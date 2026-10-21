#include "config/config.h"

#include <cctype>
#include <fstream>
#include <istream>
#include <string_view>
#include <utility>

namespace hl::config {
namespace {

std::string_view trim(std::string_view text)
{
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front())) != 0) {
        text.remove_prefix(1);
    }
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back())) != 0) {
        text.remove_suffix(1);
    }
    return text;
}
//---------------------------------------------------------------------------------------------------------------------------------//

std::size_t findComment(std::string_view text)
{
    bool quoted = false;
    bool escaped = false;
    for (std::size_t position = 0; position < text.size(); ++position) {
        const char character = text[position];
        if (escaped) {
            escaped = false;
        } else if (character == '\\' && quoted) {
            escaped = true;
        } else if (character == '"') {
            quoted = !quoted;
        } else if (character == '#' && !quoted) {
            return position;
        }
    }
    return std::string_view::npos;
}
//---------------------------------------------------------------------------------------------------------------------------------//

bool decodeValue(std::string_view text, std::string& value)
{
    text = trim(text);
    if (text.empty()) {
        return false;
    }
    if (text.front() != '"') {
        value.assign(text);
        return true;
    }
    if (text.size() < 2 || text.back() != '"') {
        return false;
    }
    value.clear();
    for (std::size_t position = 1; position + 1 < text.size(); ++position) {
        const char character = text[position];
        if (character != '\\' || position + 2 >= text.size()) {
            value.push_back(character);
            continue;
        }
        const char escaped = text[++position];
        switch (escaped) {
            case 'n':
                value.push_back('\n');
                break;
            case 'r':
                value.push_back('\r');
                break;
            case 't':
                value.push_back('\t');
                break;
            case '"':
            case '\\':
                value.push_back(escaped);
                break;
            default:
                return false;
        }
    }
    return true;
}
//---------------------------------------------------------------------------------------------------------------------------------//

} // namespace

bool load(
    std::istream& input,
    std::string_view source,
    std::vector<Setting>& settings,
    std::string& error)
{
    std::string section;
    std::string line;
    std::size_t lineNumber = 0;
    while (std::getline(input, line)) {
        ++lineNumber;
        std::string_view content(line);
        const std::size_t comment = findComment(content);
        if (comment != std::string_view::npos) {
            content = content.substr(0, comment);
        }
        content = trim(content);
        if (content.empty()) {
            continue;
        }
        if (content.front() == '[' && content.back() == ']') {
            section = std::string(trim(content.substr(1, content.size() - 2)));
            continue;
        }
        const std::size_t equals = content.find('=');
        if (equals == std::string_view::npos) {
            error = std::string(source) + ":" + std::to_string(lineNumber) + ": expected key = value";
            return false;
        }
        const std::string_view key = trim(content.substr(0, equals));
        std::string value;
        if (key.empty() || !decodeValue(content.substr(equals + 1), value)) {
            error = std::string(source) + ":" + std::to_string(lineNumber) + ": invalid setting";
            return false;
        }
        settings.push_back(Setting {
            section.empty() ? std::string(key) : section + "." + std::string(key),
            std::move(value)});
    }
    return true;
}
//---------------------------------------------------------------------------------------------------------------------------------//

bool loadFile(const std::string& path, std::vector<Setting>& settings, std::string& error)
{
    std::ifstream input(path);
    if (!input) {
        error = "could not open '" + path + "'";
        return false;
    }
    return load(input, path, settings, error);
}
//---------------------------------------------------------------------------------------------------------------------------------//

} // namespace hl::config
