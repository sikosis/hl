#ifndef HL_PARSE_SCANNER_H
#define HL_PARSE_SCANNER_H

#include "model/record.h"

#include <cstdint>
#include <string_view>

namespace hl::parse {

enum class ScanStatus : std::uint8_t {
    PlainText,
    Structured,
    Malformed,
};

bool scanJson(std::string_view entry, model::Record& record);
bool scanLogfmt(std::string_view entry, model::Record& record);
ScanStatus scanEntry(std::string_view entry, model::Record& record);

} // namespace hl::parse

#endif
