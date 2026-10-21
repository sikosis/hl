#include "format/formatter.h"

#include <cctype>
#include <cstdint>
#include <initializer_list>

namespace hl::format {
namespace {

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

bool isRenderedPredefined(const model::Field& field, const model::Record& record)
{
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
    output.clear();
    if (record.format == model::RecordFormat::PlainText) {
        output.append(record.source.data(), record.source.size());
        return;
    }

    if (record.level != model::Level::Unknown) {
        output.push_back('[');
        output += model::levelName(record.level);
        output.push_back(']');
    }
    if (record.logger.defined) {
        appendPartSeparator(output);
        appendValue(record.logger, record.format, false, output);
        output.push_back(':');
    }
    if (record.message.defined) {
        appendPartSeparator(output);
        appendValue(record.message, record.format, false, output);
    }

    for (const model::Field& field : record.fields) {
        if (isRenderedPredefined(field, record)) {
            continue;
        }
        appendPartSeparator(output);
        output.append(field.key.data(), field.key.size());
        output.push_back('=');
        appendValue(field.value, record.format, true, output);
    }
    if (record.caller.defined) {
        appendPartSeparator(output);
        output += "caller=";
        appendValue(record.caller, record.format, true, output);
    }
    if (output.empty()) {
        output.append(record.source.data(), record.source.size());
    }
}
//---------------------------------------------------------------------------------------------------------------------------------//

} // namespace hl::format
