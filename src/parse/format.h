#ifndef HL_PARSE_FORMAT_H
#define HL_PARSE_FORMAT_H

#include <cstdint>
#include <string_view>

namespace hl::parse {

enum class InputFormat : std::uint8_t {
    Auto,
    PlainText,
    Json,
    Logfmt,
};

InputFormat detectFormat(std::string_view entry);
bool parseInputFormat(std::string_view text, InputFormat& format);
std::string_view formatName(InputFormat format);

} // namespace hl::parse

#endif
