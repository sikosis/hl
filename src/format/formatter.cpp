#include "format/formatter.h"

#include <cctype>
#include <cstdio>
#include <cstdint>
#include <initializer_list>

namespace hl::format {
namespace {

constexpr std::int64_t kNanosecondsPerSecond = 1'000'000'000;

void appendStyle(const char* style, const FormatterOptions& options, std::string& output)
{
    if (options.colour) {
        output += style;
    }
}
//---------------------------------------------------------------------------------------------------------------------------------//

void appendReset(const FormatterOptions& options, std::string& output)
{
    if (options.colour) {
        output += "\x1b[0m";
    }
}
//---------------------------------------------------------------------------------------------------------------------------------//

const char* levelStyle(model::Level level)
{
    switch (level) {
        case model::Level::Trace:
            return "\x1b[2;37m";
        case model::Level::Debug:
            return "\x1b[36m";
        case model::Level::Info:
            return "\x1b[32m";
        case model::Level::Warning:
            return "\x1b[33m";
        case model::Level::Error:
            return "\x1b[31m";
        case model::Level::Fatal:
            return "\x1b[1;91m";
        case model::Level::Unknown:
            return "\x1b[37m";
    }
    return "\x1b[37m";
}
//---------------------------------------------------------------------------------------------------------------------------------//

const char* valueStyle(model::ValueKind kind)
{
    switch (kind) {
        case model::ValueKind::String:
            return "\x1b[32m";
        case model::ValueKind::Number:
            return "\x1b[34m";
        case model::ValueKind::Bool:
            return "\x1b[33m";
        case model::ValueKind::Null:
            return "\x1b[2;37m";
        case model::ValueKind::Object:
        case model::ValueKind::Array:
            return "\x1b[35m";
    }
    return "\x1b[37m";
}
//---------------------------------------------------------------------------------------------------------------------------------//

void civilFromDays(std::int64_t days, int& year, unsigned& month, unsigned& day)
{
    days += 719468;
    const std::int64_t era = (days >= 0 ? days : days - 146096) / 146097;
    const unsigned dayOfEra = static_cast<unsigned>(days - era * 146097);
    const unsigned yearOfEra = (dayOfEra - dayOfEra / 1460 + dayOfEra / 36524 - dayOfEra / 146096) / 365;
    year = static_cast<int>(yearOfEra) + static_cast<int>(era * 400);
    const unsigned dayOfYear = dayOfEra - (365 * yearOfEra + yearOfEra / 4 - yearOfEra / 100);
    const unsigned monthPrime = (5 * dayOfYear + 2) / 153;
    day = dayOfYear - (153 * monthPrime + 2) / 5 + 1;
    month = static_cast<unsigned>(static_cast<int>(monthPrime) + (monthPrime < 10 ? 3 : -9));
    year += month <= 2;
}
//---------------------------------------------------------------------------------------------------------------------------------//

void appendTimestamp(const model::Timestamp& timestamp, std::string& output)
{
    std::int64_t seconds = timestamp.nanoseconds / kNanosecondsPerSecond;
    std::int64_t nanoseconds = timestamp.nanoseconds % kNanosecondsPerSecond;
    if (nanoseconds < 0) {
        nanoseconds += kNanosecondsPerSecond;
        --seconds;
    }
    std::int64_t days = seconds / 86400;
    std::int64_t secondsOfDay = seconds % 86400;
    if (secondsOfDay < 0) {
        secondsOfDay += 86400;
        --days;
    }

    int year = 0;
    unsigned month = 0;
    unsigned day = 0;
    civilFromDays(days, year, month, day);
    const int hour = static_cast<int>(secondsOfDay / 3600);
    const int minute = static_cast<int>((secondsOfDay % 3600) / 60);
    const int second = static_cast<int>(secondsOfDay % 60);
    const int milliseconds = static_cast<int>(nanoseconds / 1'000'000);
    char buffer[32];
    const int length = std::snprintf(
        buffer,
        sizeof(buffer),
        "%04d-%02u-%02u %02d:%02d:%02d.%03d",
        year,
        month,
        day,
        hour,
        minute,
        second,
        milliseconds);
    if (length > 0) {
        output.append(buffer, static_cast<std::size_t>(length));
    }
}
//---------------------------------------------------------------------------------------------------------------------------------//

void appendUtf8(std::uint32_t codePoint, std::string& output)
{
    if (codePoint <= 0x7f) {
        output.push_back(static_cast<char>(codePoint));
    } else if (codePoint <= 0x7ff) {
        output.push_back(static_cast<char>(0xc0 | (codePoint >> 6)));
        output.push_back(static_cast<char>(0x80 | (codePoint & 0x3f)));
    } else if (codePoint <= 0xffff) {
        output.push_back(static_cast<char>(0xe0 | (codePoint >> 12)));
        output.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3f)));
        output.push_back(static_cast<char>(0x80 | (codePoint & 0x3f)));
    } else {
        output.push_back(static_cast<char>(0xf0 | (codePoint >> 18)));
        output.push_back(static_cast<char>(0x80 | ((codePoint >> 12) & 0x3f)));
        output.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3f)));
        output.push_back(static_cast<char>(0x80 | (codePoint & 0x3f)));
    }
}
//---------------------------------------------------------------------------------------------------------------------------------//

int hexValue(char character)
{
    if (character >= '0' && character <= '9') {
        return character - '0';
    }
    if (character >= 'a' && character <= 'f') {
        return character - 'a' + 10;
    }
    if (character >= 'A' && character <= 'F') {
        return character - 'A' + 10;
    }
    return -1;
}
//---------------------------------------------------------------------------------------------------------------------------------//

bool readCodePoint(std::string_view raw, std::size_t& position, std::uint32_t& codePoint)
{
    if (position + 4 > raw.size()) {
        return false;
    }
    codePoint = 0;
    for (std::size_t index = 0; index < 4; ++index) {
        const int digit = hexValue(raw[position + index]);
        if (digit < 0) {
            return false;
        }
        codePoint = (codePoint << 4) | static_cast<std::uint32_t>(digit);
    }
    position += 4;
    return true;
}
//---------------------------------------------------------------------------------------------------------------------------------//

void appendEscapedCharacter(char character, std::string& output)
{
    switch (character) {
        case '\n':
            output += "\\n";
            break;
        case '\r':
            output += "\\r";
            break;
        case '\t':
            output += "\\t";
            break;
        default:
            if (static_cast<unsigned char>(character) < 0x20) {
                constexpr char digits[] = "0123456789abcdef";
                output += "\\u00";
                output.push_back(digits[(static_cast<unsigned char>(character) >> 4) & 0x0f]);
                output.push_back(digits[static_cast<unsigned char>(character) & 0x0f]);
            } else {
                output.push_back(character);
            }
            break;
    }
}
//---------------------------------------------------------------------------------------------------------------------------------//

void appendDecodedString(
    std::string_view raw,
    model::RecordFormat format,
    bool protectQuotedValue,
    std::string& output)
{
    for (std::size_t position = 0; position < raw.size(); ++position) {
        const char character = raw[position];
        if (character != '\\' || position + 1 >= raw.size()) {
            appendEscapedCharacter(character, output);
            continue;
        }

        const char escape = raw[++position];
        switch (escape) {
            case 'n':
                output += "\\n";
                break;
            case 'r':
                output += "\\r";
                break;
            case 't':
                output += "\\t";
                break;
            case 'b':
                output += "\\b";
                break;
            case 'f':
                output += "\\f";
                break;
            case '"':
                output += protectQuotedValue ? "\\\"" : "\"";
                break;
            case '\\':
                output += protectQuotedValue ? "\\\\" : "\\";
                break;
            case '/':
                output.push_back('/');
                break;
            case 'u': {
                if (format != model::RecordFormat::Json) {
                    output += "\\u";
                    break;
                }
                ++position;
                std::uint32_t codePoint = 0;
                if (!readCodePoint(raw, position, codePoint)) {
                    output += "\xef\xbf\xbd";
                    position = raw.size();
                    break;
                }
                --position;
                if (codePoint >= 0xd800 && codePoint <= 0xdbff
                    && position + 6 < raw.size()
                    && raw[position + 1] == '\\'
                    && raw[position + 2] == 'u') {
                    std::size_t lowPosition = position + 3;
                    std::uint32_t low = 0;
                    if (readCodePoint(raw, lowPosition, low) && low >= 0xdc00 && low <= 0xdfff) {
                        codePoint = 0x10000 + ((codePoint - 0xd800) << 10) + (low - 0xdc00);
                        position = lowPosition - 1;
                    }
                }
                if (codePoint >= 0xd800 && codePoint <= 0xdfff) {
                    codePoint = 0xfffd;
                }
                if (protectQuotedValue && codePoint == '"') {
                    output += "\\\"";
                } else if (protectQuotedValue && codePoint == '\\') {
                    output += "\\\\";
                } else {
                    appendUtf8(codePoint, output);
                }
                break;
            }
            default:
                if (format == model::RecordFormat::Logfmt) {
                    output += protectQuotedValue ? "\\\\" : "\\";
                }
                output.push_back(escape);
                break;
        }
    }
}
//---------------------------------------------------------------------------------------------------------------------------------//

bool needsQuotes(std::string_view raw)
{
    if (raw.empty()) {
        return true;
    }
    for (const char character : raw) {
        if (std::isspace(static_cast<unsigned char>(character)) != 0
            || character == '"'
            || character == '\\') {
            return true;
        }
    }
    return false;
}
//---------------------------------------------------------------------------------------------------------------------------------//

void appendValue(const model::Value& value, model::RecordFormat format, bool quoteStrings, std::string& output)
{
    if (value.implicit) {
        output += "true";
        return;
    }
    if (value.kind != model::ValueKind::String) {
        output.append(value.raw.data(), value.raw.size());
        return;
    }

    const bool quoted = quoteStrings && needsQuotes(value.raw);
    if (quoted) {
        output.push_back('"');
    }
    appendDecodedString(value.raw, format, quoted, output);
    if (quoted) {
        output.push_back('"');
    }
}
//---------------------------------------------------------------------------------------------------------------------------------//

void appendStyledValue(
    const model::Value& value,
    model::RecordFormat format,
    bool quoteStrings,
    const FormatterOptions& options,
    std::string& output)
{
    appendStyle(valueStyle(value.kind), options, output);
    appendValue(value, format, quoteStrings, output);
    appendReset(options, output);
}
//---------------------------------------------------------------------------------------------------------------------------------//

void appendPartSeparator(std::string& output)
{
    if (!output.empty()) {
        output.push_back(' ');
    }
}
//---------------------------------------------------------------------------------------------------------------------------------//

bool isKey(std::string_view key, std::initializer_list<std::string_view> candidates)
{
    for (const std::string_view candidate : candidates) {
        if (key == candidate) {
            return true;
        }
    }
    return false;
}
//---------------------------------------------------------------------------------------------------------------------------------//

bool isHidden(std::string_view key, const FormatterOptions& options)
{
    bool hidden = false;
    for (const std::string& ruleText : options.visibilityRules) {
        std::string_view rule(ruleText);
        bool reveal = false;
        if (!rule.empty() && rule.front() == '!') {
            reveal = true;
            rule.remove_prefix(1);
        }
        if (rule == "*" || rule == key) {
            hidden = !reveal;
        }
    }
    return hidden;
}
//---------------------------------------------------------------------------------------------------------------------------------//

bool isEmpty(const model::Value& value)
{
    if (value.kind == model::ValueKind::Null) {
        return true;
    }
    if (value.kind == model::ValueKind::String) {
        return value.raw.empty();
    }
    if (value.kind != model::ValueKind::Object && value.kind != model::ValueKind::Array) {
        return false;
    }

    const char closing = value.kind == model::ValueKind::Object ? '}' : ']';
    if (value.raw.size() < 2 || value.raw.front() != (value.kind == model::ValueKind::Object ? '{' : '[')
        || value.raw.back() != closing) {
        return false;
    }
    for (std::size_t position = 1; position + 1 < value.raw.size(); ++position) {
        if (std::isspace(static_cast<unsigned char>(value.raw[position])) == 0) {
            return false;
        }
    }
    return true;
}
//---------------------------------------------------------------------------------------------------------------------------------//

bool shouldHide(std::string_view key, const model::Value& value, const FormatterOptions& options)
{
    return isHidden(key, options) || (options.hideEmptyFields && isEmpty(value));
}
//---------------------------------------------------------------------------------------------------------------------------------//

bool isRenderedPredefined(const model::Field& field, const model::Record& record)
{
    if (record.timestamp.has_value() && isKey(field.key, {"time", "ts", "timestamp", "@timestamp"})) {
        return true;
    }
    if (record.level != model::Level::Unknown && isKey(field.key, {"level", "lvl", "severity"})) {
        return true;
    }
    if (record.message.defined && isKey(field.key, {"message", "msg"})) {
        return true;
    }
    if (record.logger.defined && isKey(field.key, {"logger", "target"})) {
        return true;
    }
    return record.caller.defined && isKey(field.key, {"caller", "source"});
}
//---------------------------------------------------------------------------------------------------------------------------------//

} // namespace

void formatRecord(const model::Record& record, std::string& output)
{
    formatRecord(record, FormatterOptions {}, output);
}
//---------------------------------------------------------------------------------------------------------------------------------//

void formatRecord(const model::Record& record, const FormatterOptions& options, std::string& output)
{
    output.clear();
    if (record.format == model::RecordFormat::PlainText) {
        output.append(record.source.data(), record.source.size());
        return;
    }

    if (record.timestamp.has_value() && !isHidden("time", options)) {
        appendStyle("\x1b[2;36m", options, output);
        appendTimestamp(record.timestamp.value(), output);
        appendReset(options, output);
    }
    if (record.level != model::Level::Unknown && !isHidden("level", options)) {
        appendPartSeparator(output);
        appendStyle(levelStyle(record.level), options, output);
        output.push_back('[');
        output += model::levelName(record.level);
        output.push_back(']');
        appendReset(options, output);
    }
    if (record.logger.defined && !shouldHide("logger", record.logger, options)) {
        appendPartSeparator(output);
        appendStyle("\x1b[35m", options, output);
        appendValue(record.logger, record.format, false, output);
        output.push_back(':');
        appendReset(options, output);
    }
    if (record.message.defined && !shouldHide("message", record.message, options)) {
        appendPartSeparator(output);
        appendStyle("\x1b[37m", options, output);
        appendValue(record.message, record.format, false, output);
        appendReset(options, output);
    }

    for (const model::Field& field : record.fields) {
        if (isRenderedPredefined(field, record)
            || shouldHide(field.key, field.value, options)) {
            continue;
        }
        appendPartSeparator(output);
        appendStyle("\x1b[36m", options, output);
        output.append(field.key.data(), field.key.size());
        appendReset(options, output);
        output.push_back('=');
        appendStyledValue(field.value, record.format, true, options, output);
    }
    if (record.caller.defined && !shouldHide("caller", record.caller, options)) {
        appendPartSeparator(output);
        appendStyle("\x1b[36m", options, output);
        output += "caller=";
        appendReset(options, output);
        appendStyledValue(record.caller, record.format, true, options, output);
    }
    if (output.empty() && record.fields.empty()) {
        output.append(record.source.data(), record.source.size());
    }
}
//---------------------------------------------------------------------------------------------------------------------------------//

} // namespace hl::format
