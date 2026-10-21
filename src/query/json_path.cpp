#include "query/json_path.h"

#include <cctype>
#include <cstddef>
#include <limits>
#include <vector>

namespace hl::query {
namespace {

enum class SegmentKind {
    Key,
    Index,
    AnyIndex,
};

struct Segment {
    SegmentKind kind = SegmentKind::Key;
    std::string_view key;
    std::size_t index = 0;
};

class JsonCursor {
public:
    explicit JsonCursor(std::string_view input);

    bool parseString(std::string_view& raw);
    bool parseValue(model::Value& value);
    bool consume(char expected);
    void skipWhitespace();

private:
    bool skipCompound();

    std::string_view input_;
    std::size_t position_ = 0;
};

JsonCursor::JsonCursor(std::string_view input)
    : input_(input)
{
}
//---------------------------------------------------------------------------------------------------------------------------------//

bool JsonCursor::parseString(std::string_view& raw)
{
    if (!consume('"')) {
        return false;
    }
    const std::size_t start = position_;
    while (position_ < input_.size()) {
        if (input_[position_] == '"') {
            raw = input_.substr(start, position_ - start);
            ++position_;
            return true;
        }
        if (input_[position_] == '\\') {
            ++position_;
            if (position_ >= input_.size()) {
                return false;
            }
        }
        ++position_;
    }
    return false;
}
//---------------------------------------------------------------------------------------------------------------------------------//

bool JsonCursor::parseValue(model::Value& value)
{
    skipWhitespace();
    if (position_ >= input_.size()) {
        return false;
    }
    if (input_[position_] == '"') {
        std::string_view raw;
        if (!parseString(raw)) {
            return false;
        }
        value = {model::ValueKind::String, raw.find('\\') != std::string_view::npos, true, false, raw};
        return true;
    }
    if (input_[position_] == '{' || input_[position_] == '[') {
        const std::size_t start = position_;
        const model::ValueKind kind = input_[position_] == '{'
            ? model::ValueKind::Object
            : model::ValueKind::Array;
        if (!skipCompound()) {
            return false;
        }
        value = {kind, false, true, false, input_.substr(start, position_ - start)};
        return true;
    }

    const std::size_t start = position_;
    while (position_ < input_.size()
        && input_[position_] != ','
        && input_[position_] != ']'
        && input_[position_] != '}'
        && std::isspace(static_cast<unsigned char>(input_[position_])) == 0) {
        ++position_;
    }
    if (position_ == start) {
        return false;
    }
    const std::string_view raw = input_.substr(start, position_ - start);
    model::ValueKind kind = model::ValueKind::Number;
    if (raw == "true" || raw == "false") {
        kind = model::ValueKind::Bool;
    } else if (raw == "null") {
        kind = model::ValueKind::Null;
    }
    value = {kind, false, true, false, raw};
    return true;
}
//---------------------------------------------------------------------------------------------------------------------------------//

bool JsonCursor::consume(char expected)
{
    if (position_ >= input_.size() || input_[position_] != expected) {
        return false;
    }
    ++position_;
    return true;
}
//---------------------------------------------------------------------------------------------------------------------------------//

void JsonCursor::skipWhitespace()
{
    while (position_ < input_.size()
        && std::isspace(static_cast<unsigned char>(input_[position_])) != 0) {
        ++position_;
    }
}
//---------------------------------------------------------------------------------------------------------------------------------//

bool JsonCursor::skipCompound()
{
    std::vector<char> closingCharacters;
    closingCharacters.push_back(input_[position_] == '{' ? '}' : ']');
    ++position_;
    while (position_ < input_.size() && !closingCharacters.empty()) {
        if (input_[position_] == '"') {
            std::string_view ignored;
            if (!parseString(ignored)) {
                return false;
            }
            continue;
        }
        if (input_[position_] == '{') {
            closingCharacters.push_back('}');
        } else if (input_[position_] == '[') {
            closingCharacters.push_back(']');
        } else if (input_[position_] == closingCharacters.back()) {
            closingCharacters.pop_back();
        }
        ++position_;
    }
    return closingCharacters.empty();
}
//---------------------------------------------------------------------------------------------------------------------------------//

bool parsePath(std::string_view path, std::vector<Segment>& segments)
{
    std::size_t start = 0;
    while (start <= path.size()) {
        const std::size_t end = path.find('.', start);
        const std::string_view part = path.substr(start, end - start);
        if (part.empty()) {
            return false;
        }
        if (part == "[]") {
            segments.push_back({SegmentKind::AnyIndex, {}, 0});
        } else if (part.size() >= 3 && part.front() == '[' && part.back() == ']') {
            std::size_t index = 0;
            for (std::size_t position = 1; position + 1 < part.size(); ++position) {
                const char character = part[position];
                if (character < '0' || character > '9') {
                    return false;
                }
                const unsigned digit = static_cast<unsigned>(character - '0');
                if (index > (std::numeric_limits<std::size_t>::max() - digit) / 10) {
                    return false;
                }
                index = index * 10 + digit;
            }
            segments.push_back({SegmentKind::Index, {}, index});
        } else {
            segments.push_back({SegmentKind::Key, part, 0});
        }
        if (end == std::string_view::npos) {
            break;
        }
        start = end + 1;
    }
    return !segments.empty() && segments.front().kind == SegmentKind::Key;
}
//---------------------------------------------------------------------------------------------------------------------------------//

void appendValueText(const model::Value& value, std::vector<std::string_view>& values)
{
    values.push_back(value.implicit ? std::string_view("true") : value.raw);
}
//---------------------------------------------------------------------------------------------------------------------------------//

void resolveValue(
    const model::Value& value,
    const std::vector<Segment>& segments,
    std::size_t segmentIndex,
    std::vector<std::string_view>& values);

void resolveObject(
    const model::Value& value,
    const std::vector<Segment>& segments,
    std::size_t segmentIndex,
    std::vector<std::string_view>& values)
{
    JsonCursor cursor(value.raw);
    if (!cursor.consume('{')) {
        return;
    }
    cursor.skipWhitespace();
    while (!cursor.consume('}')) {
        std::string_view key;
        if (!cursor.parseString(key)) {
            return;
        }
        cursor.skipWhitespace();
        if (!cursor.consume(':')) {
            return;
        }
        model::Value child;
        if (!cursor.parseValue(child)) {
            return;
        }
        if (key == segments[segmentIndex].key) {
            resolveValue(child, segments, segmentIndex + 1, values);
        }
        cursor.skipWhitespace();
        if (cursor.consume('}')) {
            return;
        }
        if (!cursor.consume(',')) {
            return;
        }
        cursor.skipWhitespace();
    }
}
//---------------------------------------------------------------------------------------------------------------------------------//

void resolveArray(
    const model::Value& value,
    const std::vector<Segment>& segments,
    std::size_t segmentIndex,
    std::vector<std::string_view>& values)
{
    JsonCursor cursor(value.raw);
    if (!cursor.consume('[')) {
        return;
    }
    cursor.skipWhitespace();
    std::size_t index = 0;
    while (!cursor.consume(']')) {
        model::Value child;
        if (!cursor.parseValue(child)) {
            return;
        }
        if (segments[segmentIndex].kind == SegmentKind::AnyIndex
            || segments[segmentIndex].index == index) {
            resolveValue(child, segments, segmentIndex + 1, values);
        }
        ++index;
        cursor.skipWhitespace();
        if (cursor.consume(']')) {
            return;
        }
        if (!cursor.consume(',')) {
            return;
        }
        cursor.skipWhitespace();
    }
}
//---------------------------------------------------------------------------------------------------------------------------------//

void resolveValue(
    const model::Value& value,
    const std::vector<Segment>& segments,
    std::size_t segmentIndex,
    std::vector<std::string_view>& values)
{
    if (segmentIndex == segments.size()) {
        appendValueText(value, values);
        return;
    }
    const Segment& segment = segments[segmentIndex];
    if (segment.kind == SegmentKind::Key && value.kind == model::ValueKind::Object) {
        resolveObject(value, segments, segmentIndex, values);
    } else if (segment.kind != SegmentKind::Key && value.kind == model::ValueKind::Array) {
        resolveArray(value, segments, segmentIndex, values);
    }
}
//---------------------------------------------------------------------------------------------------------------------------------//

} // namespace

void resolveNestedField(
    const model::Record& record,
    std::string_view path,
    std::vector<std::string_view>& values)
{
    std::vector<Segment> segments;
    if (!parsePath(path, segments)) {
        return;
    }
    for (const model::Field& field : record.fields) {
        if (field.key == segments.front().key) {
            resolveValue(field.value, segments, 1, values);
        }
    }
}
//---------------------------------------------------------------------------------------------------------------------------------//

} // namespace hl::query
