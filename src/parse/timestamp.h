#ifndef HL_PARSE_TIMESTAMP_H
#define HL_PARSE_TIMESTAMP_H

#include "model/timestamp.h"

#include <optional>
#include <string_view>

namespace hl::parse {

std::optional<model::Timestamp> parseTimestamp(std::string_view text);

} // namespace hl::parse

#endif
