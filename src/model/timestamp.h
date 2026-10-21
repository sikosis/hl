#ifndef HL_MODEL_TIMESTAMP_H
#define HL_MODEL_TIMESTAMP_H

#include <cstdint>

namespace hl::model {

struct Timestamp {
    std::int64_t nanoseconds = 0;
    std::int16_t sourceOffsetMinutes = 0;
};

} // namespace hl::model

#endif
