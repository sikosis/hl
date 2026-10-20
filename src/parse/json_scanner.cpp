#include "parse/scanner.h"

#include "parse/scanner_internal.h"

#include <string_view>

namespace hl::parse {
namespace {

struct StringToken {
    std::string_view raw;
    bool escaped = false;
};

class JsonParser {
public:
    explicit JsonParser(std::string_view input);

    bool parse(model::Record& record);

private:
    bool parseObject();
    bool parseArray();
    bool parseValue(model::Value& value);
    bool parseString(StringToken& token);
    bool parseNumber(std::string_view& raw);
    bool parseLiteral(std::string_view literal);
    bool consume(char expected);
    void skipWhitespace();

    std::string_view input_;
    std::size_t position_ = 0;
    std::size_t depth_ = 0;
};

bool isDigit(char character)
{
    return character >= '0' && character <= '9';
}
//---------------------------------------------------------------------------------------------------------------------------------//

bool isHexDigit(char character)
{
    return isDigit(character)
        || (character >= 'a' && character <= 'f')
        || (character >= 'A' && character <= 'F');
}
//---------------------------------------------------------------------------------------------------------------------------------//

JsonParser::JsonParser(std::string_view input)
    : input_(input)
{
}
//---------------------------------------------------------------------------------------------------------------------------------//

bool JsonParser::parse(model::Record& record)
{
    skipWhitespace();
    if (!consume('{')) {
        return false;
    }

    skipWhitespace();
    if (consume('}')) {
        skipWhitespace();
        return position_ == input_.size();
    }

    while (position_ < input_.size()) {
        StringToken key;
        if (!parseString(key)) {
            return false;
        }
        skipWhitespace();
        if (!consume(':')) {
            return false;
        }

        model::Value value;
        if (!parseValue(value)) {
            return false;
        }
        record.fields.push_back({key.raw, value});

        skipWhitespace();
        if (consume('}')) {
            skipWhitespace();
            return position_ == input_.size();
        }
        if (!consume(',')) {
            return false;
        }
        skipWhitespace();
    }
    return false;
}
//---------------------------------------------------------------------------------------------------------------------------------//

bool JsonParser::parseObject()
{
    if (!consume('{')) {
        return false;
    }
    skipWhitespace();
    if (consume('}')) {
        return true;
    }

    while (position_ < input_.size()) {
        StringToken key;
        if (!parseString(key)) {
            return false;
        }
        skipWhitespace();
        if (!consume(':')) {
            return false;
        }
        model::Value ignored;
        if (!parseValue(ignored)) {
            return false;
        }
        skipWhitespace();
        if (consume('}')) {
            return true;
        }
        if (!consume(',')) {
            return false;
        }
        skipWhitespace();
    }
    return false;
}
//---------------------------------------------------------------------------------------------------------------------------------//

bool JsonParser::parseArray()
{
    if (!consume('[')) {
        return false;
    }
    skipWhitespace();
    if (consume(']')) {
        return true;
    }

    while (position_ < input_.size()) {
        model::Value ignored;
        if (!parseValue(ignored)) {
            return false;
        }
        skipWhitespace();
        if (consume(']')) {
            return true;
        }
        if (!consume(',')) {
            return false;
        }
        skipWhitespace();
    }
    return false;
}
//---------------------------------------------------------------------------------------------------------------------------------//

bool JsonParser::parseValue(model::Value& value)
{
    skipWhitespace();
    if (position_ >= input_.size()) {
        return false;
    }

    const std::size_t start = position_;
    if (input_[position_] == '"') {
        StringToken token;
        if (!parseString(token)) {
            return false;
        }
        value = {model::ValueKind::String, token.escaped, true, false, token.raw};
        return true;
    }
    if (input_[position_] == '{') {
        if (depth_ >= 256) {
            return false;
        }
        ++depth_;
        const bool parsed = parseObject();
        --depth_;
        if (!parsed) {
            return false;
        }
        value = {model::ValueKind::Object, false, true, false, input_.substr(start, position_ - start)};
        return true;
    }
    if (input_[position_] == '[') {
        if (depth_ >= 256) {
            return false;
        }
        ++depth_;
        const bool parsed = parseArray();
        --depth_;
        if (!parsed) {
            return false;
        }
        value = {model::ValueKind::Array, false, true, false, input_.substr(start, position_ - start)};
        return true;
    }
    if (parseLiteral("true") || parseLiteral("false")) {
        value = {model::ValueKind::Bool, false, true, false, input_.substr(start, position_ - start)};
        return true;
    }
    if (parseLiteral("null")) {
        value = {model::ValueKind::Null, false, true, false, input_.substr(start, position_ - start)};
        return true;
    }

    std::string_view number;
    if (parseNumber(number)) {
        value = {model::ValueKind::Number, false, true, false, number};
        return true;
    }
    return false;
}
//---------------------------------------------------------------------------------------------------------------------------------//

bool JsonParser::parseString(StringToken& token)
{
    if (!consume('"')) {
        return false;
    }
    const std::size_t start = position_;
    bool escaped = false;
    while (position_ < input_.size()) {
        const unsigned char character = static_cast<unsigned char>(input_[position_]);
        if (character == '"') {
            token = {input_.substr(start, position_ - start), escaped};
            ++position_;
            return true;
        }
        if (character < 0x20) {
            return false;
        }
        if (character != '\\') {
            ++position_;
            continue;
        }

        escaped = true;
        ++position_;
        if (position_ >= input_.size()) {
            return false;
        }
        const char escape = input_[position_++];
        if (escape == 'u') {
            if (position_ + 4 > input_.size()) {
                return false;
            }
            for (std::size_t index = 0; index < 4; ++index) {
                if (!isHexDigit(input_[position_ + index])) {
                    return false;
                }
            }
            position_ += 4;
        } else if (std::string_view("\"\\/bfnrt").find(escape) == std::string_view::npos) {
            return false;
        }
    }
    return false;
}
//---------------------------------------------------------------------------------------------------------------------------------//

bool JsonParser::parseNumber(std::string_view& raw)
{
    const std::size_t start = position_;
    if (consume('-') && position_ >= input_.size()) {
        position_ = start;
        return false;
    }

    if (consume('0')) {
        if (position_ < input_.size() && isDigit(input_[position_])) {
            position_ = start;
            return false;
        }
    } else {
        if (position_ >= input_.size() || input_[position_] < '1' || input_[position_] > '9') {
            position_ = start;
            return false;
        }
        while (position_ < input_.size() && isDigit(input_[position_])) {
            ++position_;
        }
    }

    if (consume('.')) {
        const std::size_t fractionStart = position_;
        while (position_ < input_.size() && isDigit(input_[position_])) {
            ++position_;
        }
        if (position_ == fractionStart) {
            position_ = start;
            return false;
        }
    }
    if (position_ < input_.size() && (input_[position_] == 'e' || input_[position_] == 'E')) {
        ++position_;
        if (position_ < input_.size() && (input_[position_] == '+' || input_[position_] == '-')) {
            ++position_;
        }
        const std::size_t exponentStart = position_;
        while (position_ < input_.size() && isDigit(input_[position_])) {
            ++position_;
        }
        if (position_ == exponentStart) {
            position_ = start;
            return false;
        }
    }

    raw = input_.substr(start, position_ - start);
    return true;
}
//---------------------------------------------------------------------------------------------------------------------------------//

bool JsonParser::parseLiteral(std::string_view literal)
{
    if (input_.substr(position_, literal.size()) != literal) {
        return false;
    }
    position_ += literal.size();
    return true;
}
//---------------------------------------------------------------------------------------------------------------------------------//

bool JsonParser::consume(char expected)
{
    if (position_ >= input_.size() || input_[position_] != expected) {
        return false;
    }
    ++position_;
    return true;
}
//---------------------------------------------------------------------------------------------------------------------------------//

void JsonParser::skipWhitespace()
{
    while (position_ < input_.size()) {
        const char character = input_[position_];
        if (character != ' ' && character != '\t' && character != '\r' && character != '\n') {
            return;
        }
        ++position_;
    }
}
//---------------------------------------------------------------------------------------------------------------------------------//

} // namespace

bool scanJson(std::string_view entry, model::Record& record)
{
    record.reset(entry, model::RecordFormat::Json);
    JsonParser parser(entry);
    if (!parser.parse(record)) {
        record.reset(entry, model::RecordFormat::Json);
        return false;
    }
    populatePredefinedFields(record);
    return true;
}
//---------------------------------------------------------------------------------------------------------------------------------//

} // namespace hl::parse
