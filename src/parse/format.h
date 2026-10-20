#ifndef HL_PARSE_FORMAT_H
#define HL_PARSE_FORMAT_H

#include <cstdint>
#include <string_view>

namespace hl::parse {

enum class InputFormat : std::uint8_t {
    PlainText,
    Json,
    Logfmt,
};

InputFormat detectFormat(std::string_view entry);
std::string_view formatName(InputFormat format);

} // namespace hl::parse

#endif
