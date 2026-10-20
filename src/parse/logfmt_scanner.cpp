#include "parse/scanner.h"

#include "parse/scanner_internal.h"

#include <cctype>

namespace hl::parse {
namespace {

bool isSpace(char character)
{
    return std::isspace(static_cast<unsigned char>(character)) != 0;
}
//---------------------------------------------------------------------------------------------------------------------------------//

bool isDigit(char character)
{
    return character >= '0' && character <= '9';
}
//---------------------------------------------------------------------------------------------------------------------------------//

bool isNumber(std::string_view value)
{
    if (value.empty()) {
        return false;
    }
    std::size_t position = 0;
    if (value[position] == '-' || value[position] == '+') {
        ++position;
    }
    const std::size_t integerStart = position;
    while (position < value.size() && isDigit(value[position])) {
        ++position;
    }
    if (position == integerStart) {
        return false;
    }
    if (position < value.size() && value[position] == '.') {
        ++position;
        const std::size_t fractionStart = position;
        while (position < value.size() && isDigit(value[position])) {
            ++position;
        }
        if (position == fractionStart) {
            return false;
        }
    }
    if (position < value.size() && (value[position] == 'e' || value[position] == 'E')) {
        ++position;
        if (position < value.size() && (value[position] == '+' || value[position] == '-')) {
            ++position;
        }
        const std::size_t exponentStart = position;
        while (position < value.size() && isDigit(value[position])) {
            ++position;
        }
        if (position == exponentStart) {
            return false;
        }
    }
    return position == value.size();
}
//---------------------------------------------------------------------------------------------------------------------------------//

model::Value unquotedValue(std::string_view raw)
{
    if (raw == "true" || raw == "false") {
        return {model::ValueKind::Bool, false, true, false, raw};
    }
    if (raw == "null") {
        return {model::ValueKind::Null, false, true, false, raw};
    }
    if (isNumber(raw)) {
        return {model::ValueKind::Number, false, true, false, raw};
    }
    return {model::ValueKind::String, false, true, false, raw};
}
//---------------------------------------------------------------------------------------------------------------------------------//

bool parseQuotedValue(std::string_view entry, std::size_t& position, model::Value& value)
{
    ++position;
    const std::size_t start = position;
    bool escaped = false;
    while (position < entry.size()) {
        const unsigned char character = static_cast<unsigned char>(entry[position]);
        if (character == '"') {
            value = {model::ValueKind::String, escaped, true, false, entry.substr(start, position - start)};
            ++position;
            return true;
        }
        if (character < 0x20 && character != '\t') {
            return false;
        }
        if (character == '\\') {
            escaped = true;
            ++position;
            if (position >= entry.size()) {
                return false;
            }
        }
        ++position;
    }
    return false;
}
//---------------------------------------------------------------------------------------------------------------------------------//

} // namespace

bool scanLogfmt(std::string_view entry, model::Record& record)
{
    record.reset(entry, model::RecordFormat::Logfmt);
    std::size_t position = 0;
    while (position < entry.size()) {
        while (position < entry.size() && isSpace(entry[position])) {
            ++position;
        }
        if (position == entry.size()) {
            break;
        }

        const std::size_t keyStart = position;
        while (position < entry.size() && !isSpace(entry[position]) && entry[position] != '=') {
            if (entry[position] == '"') {
                record.reset(entry, model::RecordFormat::Logfmt);
                return false;
            }
            ++position;
        }
        if (position == keyStart) {
            record.reset(entry, model::RecordFormat::Logfmt);
            return false;
        }
        const std::string_view key = entry.substr(keyStart, position - keyStart);

        model::Value value;
        if (position < entry.size() && entry[position] == '=') {
            ++position;
            if (position < entry.size() && entry[position] == '"') {
                if (!parseQuotedValue(entry, position, value)) {
                    record.reset(entry, model::RecordFormat::Logfmt);
                    return false;
                }
                if (position < entry.size() && !isSpace(entry[position])) {
                    record.reset(entry, model::RecordFormat::Logfmt);
                    return false;
                }
            } else {
                const std::size_t valueStart = position;
                while (position < entry.size() && !isSpace(entry[position])) {
                    ++position;
                }
                value = unquotedValue(entry.substr(valueStart, position - valueStart));
            }
        } else {
            value = {model::ValueKind::Bool, false, true, true, {}};
        }
        record.fields.push_back({key, value});
    }

    if (record.fields.empty()) {
        return false;
    }
    populatePredefinedFields(record);
    return true;
}
//---------------------------------------------------------------------------------------------------------------------------------//

} // namespace hl::parse
