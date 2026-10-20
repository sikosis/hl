#include "parse/format.h"

#include <cctype>

namespace hl::parse {
namespace {

bool isSpace(char character)
{
    return std::isspace(static_cast<unsigned char>(character)) != 0;
}
//---------------------------------------------------------------------------------------------------------------------------------//

bool isLogfmtKeyCharacter(char character)
{
    const unsigned char value = static_cast<unsigned char>(character);
    return !std::isspace(value) && character != '=' && character != '"';
}
//---------------------------------------------------------------------------------------------------------------------------------//

bool containsLogfmtPair(std::string_view entry)
{
    std::size_t position = 0;
    while (position < entry.size()) {
        while (position < entry.size() && isSpace(entry[position])) {
            ++position;
        }

        const std::size_t keyStart = position;
        while (position < entry.size() && isLogfmtKeyCharacter(entry[position])) {
            ++position;
        }
        if (position > keyStart && position < entry.size() && entry[position] == '=') {
            return true;
        }

        while (position < entry.size() && !isSpace(entry[position])) {
            ++position;
        }
    }
    return false;
}
//---------------------------------------------------------------------------------------------------------------------------------//

} // namespace

InputFormat detectFormat(std::string_view entry)
{
    std::size_t first = 0;
    while (first < entry.size() && isSpace(entry[first])) {
        ++first;
    }
    if (first == entry.size()) {
        return InputFormat::PlainText;
    }
    if (entry[first] == '{') {
        return InputFormat::Json;
    }
    return containsLogfmtPair(entry.substr(first)) ? InputFormat::Logfmt : InputFormat::PlainText;
}
//---------------------------------------------------------------------------------------------------------------------------------//

std::string_view formatName(InputFormat format)
{
    switch (format) {
        case InputFormat::PlainText:
            return "text";
        case InputFormat::Json:
            return "json";
        case InputFormat::Logfmt:
            return "logfmt";
    }
    return "text";
}
//---------------------------------------------------------------------------------------------------------------------------------//

} // namespace hl::parse
