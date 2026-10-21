#ifndef HL_MODEL_RECORD_H
#define HL_MODEL_RECORD_H

#include "model/level.h"
#include "model/timestamp.h"

#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace hl::model {

enum class ValueKind : std::uint8_t {
    String,
    Number,
    Bool,
    Null,
    Object,
    Array,
};

enum class RecordFormat : std::uint8_t {
    PlainText,
    Json,
    Logfmt,
};

struct Value {
    ValueKind kind = ValueKind::String;
    bool escaped = false;
    bool defined = false;
    bool implicit = false;
    std::string_view raw;
};

struct Field {
    std::string_view key;
    Value value;
};

struct Record {
    std::string_view source;
    std::string_view prefix;
    RecordFormat format = RecordFormat::PlainText;
    std::optional<Timestamp> timestamp;
    Level level = Level::Unknown;
    Value message;
    Value logger;
    Value caller;
    std::vector<Field> fields;

    void reset(std::string_view newSource, RecordFormat newFormat);
};

} // namespace hl::model

#endif
