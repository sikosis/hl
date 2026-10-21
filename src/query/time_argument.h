#ifndef HL_QUERY_TIME_ARGUMENT_H
#define HL_QUERY_TIME_ARGUMENT_H

#include "model/timestamp.h"

#include <cstdint>
#include <optional>
#include <string_view>

namespace hl::query {

std::optional<model::Timestamp> parseTimeArgument(std::string_view text, std::int64_t nowNanoseconds);

} // namespace hl::query

#endif
