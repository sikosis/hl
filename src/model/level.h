#ifndef HL_MODEL_LEVEL_H
#define HL_MODEL_LEVEL_H

#include <cstdint>
#include <string_view>

namespace hl::model {

enum class Level : std::uint8_t {
    Unknown,
    Trace,
    Debug,
    Info,
    Warning,
    Error,
    Fatal,
};

Level levelFromText(std::string_view text);
std::string_view levelName(Level level);

} // namespace hl::model

#endif
