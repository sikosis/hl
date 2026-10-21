#include "query/query.h"

#include "query/json_path.h"

#include <cctype>
#include <cerrno>
#include <cstdlib>
#include <initializer_list>
#include <regex>
#include <utility>
#include <vector>

namespace hl::query {
namespace {

enum class TokenKind {
    End,
    Word,
    LeftParenthesis,
    RightParenthesis,
    Comma,
    And,
    Or,
    Not,
    Equal,
    NotEqual,
    Greater,
    GreaterEqual,
    Less,
    LessEqual,
    Contains,
    NotContains,
    Match,
    NotMatch,
    Like,
    In,
    Exists,
};

struct Token {
    TokenKind kind = TokenKind::End;
    std::string text;
};

bool isOperatorCharacter(char character)
{
    return character == '(' || character == ')' || character == ',' || character == '='
        || character == '!' || character == '>' || character == '<' || character == '~'
        || character == '&' || character == '|';
}
//---------------------------------------------------------------------------------------------------------------------------------//

class Lexer {
public:
    explicit Lexer(std::string_view source)
        : source_(source)
    {
    }
//---------------------------------------------------------------------------------------------------------------------------------//

    Token next(std::string& error)
    {
        while (position_ < source_.size()
            && std::isspace(static_cast<unsigned char>(source_[position_])) != 0) {
            ++position_;
        }
        if (position_ == source_.size()) {
            return {};
        }

        constexpr struct Symbol {
            std::string_view text;
            TokenKind kind;
        } symbols[] = {
            {"!~~=", TokenKind::NotMatch},
            {"~~=", TokenKind::Match},
            {"!~=", TokenKind::NotContains},
            {"~=", TokenKind::Contains},
            {">=", TokenKind::GreaterEqual},
            {"<=", TokenKind::LessEqual},
            {"!=", TokenKind::NotEqual},
            {"&&", TokenKind::And},
            {"||", TokenKind::Or},
            {"=", TokenKind::Equal},
            {">", TokenKind::Greater},
            {"<", TokenKind::Less},
            {"!", TokenKind::Not},
            {"(", TokenKind::LeftParenthesis},
            {")", TokenKind::RightParenthesis},
            {",", TokenKind::Comma},
        };
        for (const Symbol& symbol : symbols) {
            if (source_.substr(position_, symbol.text.size()) == symbol.text) {
                position_ += symbol.text.size();
                return Token {symbol.kind, std::string(symbol.text)};
            }
        }

        if (source_[position_] == '"') {
            ++position_;
            std::string value;
            while (position_ < source_.size() && source_[position_] != '"') {
                if (source_[position_] == '\\' && position_ + 1 < source_.size()) {
                    ++position_;
                    switch (source_[position_]) {
                        case 'n': value.push_back('\n'); break;
                        case 'r': value.push_back('\r'); break;
                        case 't': value.push_back('\t'); break;
                        default: value.push_back(source_[position_]); break;
                    }
                } else {
                    value.push_back(source_[position_]);
                }
                ++position_;
            }
            if (position_ == source_.size()) {
                error = "unterminated quoted value";
                return {};
            }
            ++position_;
            return Token {TokenKind::Word, std::move(value)};
        }

        const std::size_t start = position_;
        while (position_ < source_.size()
            && std::isspace(static_cast<unsigned char>(source_[position_])) == 0
            && !isOperatorCharacter(source_[position_])) {
            ++position_;
        }
        if (start == position_) {
            error = "unexpected character '" + std::string(1, source_[position_]) + "'";
            return {};
        }
        std::string word(source_.substr(start, position_ - start));
        if (word == "and") return Token {TokenKind::And, word};
        if (word == "or") return Token {TokenKind::Or, word};
        if (word == "not") return Token {TokenKind::Not, word};
        if (word == "like") return Token {TokenKind::Like, word};
        if (word == "in") return Token {TokenKind::In, word};
        if (word == "exists" || word == "exist") return Token {TokenKind::Exists, word};
        if (word == "eq") return Token {TokenKind::Equal, word};
        if (word == "ne") return Token {TokenKind::NotEqual, word};
        if (word == "gt") return Token {TokenKind::Greater, word};
        if (word == "ge") return Token {TokenKind::GreaterEqual, word};
        if (word == "lt") return Token {TokenKind::Less, word};
        if (word == "le") return Token {TokenKind::LessEqual, word};
        if (word == "contain") return Token {TokenKind::Contains, word};
        if (word == "match") return Token {TokenKind::Match, word};
        return Token {TokenKind::Word, std::move(word)};
    }
//---------------------------------------------------------------------------------------------------------------------------------//

private:
    std::string_view source_;
    std::size_t position_ = 0;
};

void appendValue(const model::Value& value, std::vector<std::string_view>& values)
{
    if (value.defined) {
        values.push_back(value.implicit ? std::string_view("true") : value.raw);
    }
}
//---------------------------------------------------------------------------------------------------------------------------------//

void findFields(const model::Record& record, std::string_view name, std::vector<std::string_view>& values)
{
    bool sourceOnly = false;
    if (!name.empty() && name.front() == '.') {
        sourceOnly = true;
        name.remove_prefix(1);
    }
    if (!sourceOnly && name == "level") {
        if (record.level != model::Level::Unknown) values.push_back(model::levelName(record.level));
        return;
    }
    if (!sourceOnly && name == "message") { appendValue(record.message, values); return; }
    if (!sourceOnly && name == "logger") { appendValue(record.logger, values); return; }
    if (!sourceOnly && name == "caller") { appendValue(record.caller, values); return; }
    for (const model::Field& field : record.fields) {
        if (field.key == name) appendValue(field.value, values);
    }
    if (values.empty()) resolveNestedField(record, name, values);
}
//---------------------------------------------------------------------------------------------------------------------------------//

bool parseNumber(std::string_view text, double& value)
{
    std::string copy(text);
    char* end = nullptr;
    errno = 0;
    value = std::strtod(copy.c_str(), &end);
    return errno == 0 && end == copy.c_str() + copy.size() && end != copy.c_str();
}
//---------------------------------------------------------------------------------------------------------------------------------//

bool globMatches(std::string_view pattern, std::string_view value)
{
    std::size_t patternPosition = 0;
    std::size_t valuePosition = 0;
    std::size_t star = std::string_view::npos;
    std::size_t retry = 0;
    while (valuePosition < value.size()) {
        if (patternPosition < pattern.size()
            && (pattern[patternPosition] == '?' || pattern[patternPosition] == value[valuePosition])) {
            ++patternPosition;
            ++valuePosition;
        } else if (patternPosition < pattern.size() && pattern[patternPosition] == '*') {
            star = patternPosition++;
            retry = valuePosition;
        } else if (star != std::string_view::npos) {
            patternPosition = star + 1;
            valuePosition = ++retry;
        } else {
            return false;
        }
    }
    while (patternPosition < pattern.size() && pattern[patternPosition] == '*') ++patternPosition;
    return patternPosition == pattern.size();
}
//---------------------------------------------------------------------------------------------------------------------------------//

enum class Comparison {
    Equal, NotEqual, Greater, GreaterEqual, Less, LessEqual,
    Contains, NotContains, Like, NotLike, Match, NotMatch, In, NotIn,
};

bool compareText(std::string_view actual, Comparison comparison, std::string_view expected, const std::regex* regex)
{
    double actualNumber = 0;
    double expectedNumber = 0;
    const bool numeric = parseNumber(actual, actualNumber) && parseNumber(expected, expectedNumber);
    switch (comparison) {
        case Comparison::Equal: return numeric ? actualNumber == expectedNumber : actual == expected;
        case Comparison::NotEqual: return numeric ? actualNumber != expectedNumber : actual != expected;
        case Comparison::Greater: return numeric ? actualNumber > expectedNumber : actual > expected;
        case Comparison::GreaterEqual: return numeric ? actualNumber >= expectedNumber : actual >= expected;
        case Comparison::Less: return numeric ? actualNumber < expectedNumber : actual < expected;
        case Comparison::LessEqual: return numeric ? actualNumber <= expectedNumber : actual <= expected;
        case Comparison::Contains: return actual.find(expected) != std::string_view::npos;
        case Comparison::NotContains: return actual.find(expected) == std::string_view::npos;
        case Comparison::Like: return globMatches(expected, actual);
        case Comparison::NotLike: return !globMatches(expected, actual);
        case Comparison::Match: return regex != nullptr && std::regex_search(actual.begin(), actual.end(), *regex);
        case Comparison::NotMatch: return regex == nullptr || !std::regex_search(actual.begin(), actual.end(), *regex);
        case Comparison::In:
        case Comparison::NotIn:
            break;
    }
    return false;
}
//---------------------------------------------------------------------------------------------------------------------------------//

bool compareLevel(std::string_view actual, Comparison comparison, std::string_view expected, bool& compared)
{
    const model::Level actualLevel = model::levelFromText(actual);
    const model::Level expectedLevel = model::levelFromText(expected);
    if (actualLevel == model::Level::Unknown || expectedLevel == model::Level::Unknown) {
        compared = false;
        return false;
    }
    compared = true;
    const unsigned left = static_cast<unsigned>(actualLevel);
    const unsigned right = static_cast<unsigned>(expectedLevel);
    switch (comparison) {
        case Comparison::Equal: return left == right;
        case Comparison::NotEqual: return left != right;
        case Comparison::Greater: return left > right;
        case Comparison::GreaterEqual: return left >= right;
        case Comparison::Less: return left < right;
        case Comparison::LessEqual: return left <= right;
        default: compared = false; return false;
    }
}
//---------------------------------------------------------------------------------------------------------------------------------//

} // namespace

struct Query::Node {
    virtual ~Node() = default;
//---------------------------------------------------------------------------------------------------------------------------------//

    virtual bool evaluate(const model::Record& record) const = 0;
};

namespace {

class LogicalNode final : public Query::Node {
public:
    LogicalNode(bool conjunction, std::shared_ptr<const Query::Node> left, std::shared_ptr<const Query::Node> right)
        : conjunction_(conjunction), left_(std::move(left)), right_(std::move(right)) {}
//---------------------------------------------------------------------------------------------------------------------------------//

    bool evaluate(const model::Record& record) const override
    {
        return conjunction_ ? left_->evaluate(record) && right_->evaluate(record)
                            : left_->evaluate(record) || right_->evaluate(record);
    }
//---------------------------------------------------------------------------------------------------------------------------------//
private:
    bool conjunction_;
    std::shared_ptr<const Query::Node> left_;
    std::shared_ptr<const Query::Node> right_;
};

class NotNode final : public Query::Node {
public:
    explicit NotNode(std::shared_ptr<const Query::Node> child) : child_(std::move(child)) {}
//---------------------------------------------------------------------------------------------------------------------------------//

    bool evaluate(const model::Record& record) const override { return !child_->evaluate(record); }
//---------------------------------------------------------------------------------------------------------------------------------//
private:
    std::shared_ptr<const Query::Node> child_;
};

class ExistsNode final : public Query::Node {
public:
    explicit ExistsNode(std::string field) : field_(std::move(field)) {}
//---------------------------------------------------------------------------------------------------------------------------------//

    bool evaluate(const model::Record& record) const override
    {
        std::vector<std::string_view> values;
        findFields(record, field_, values);
        return !values.empty();
    }
//---------------------------------------------------------------------------------------------------------------------------------//
private:
    std::string field_;
};

class ComparisonNode final : public Query::Node {
public:
    ComparisonNode(std::string field, bool keepMissing, Comparison comparison, std::vector<std::string> values)
        : field_(std::move(field)), keepMissing_(keepMissing), comparison_(comparison), values_(std::move(values))
    {
        if ((comparison_ == Comparison::Match || comparison_ == Comparison::NotMatch) && !values_.empty()) {
            regex_ = std::make_unique<std::regex>(values_.front());
        }
    }
//---------------------------------------------------------------------------------------------------------------------------------//

    bool evaluate(const model::Record& record) const override
    {
        std::vector<std::string_view> actualValues;
        findFields(record, field_, actualValues);
        if (actualValues.empty()) return keepMissing_;
        for (const std::string_view actual : actualValues) {
            bool matched = false;
            if (comparison_ == Comparison::In || comparison_ == Comparison::NotIn) {
                for (const std::string& expected : values_) {
                    if (compareText(actual, Comparison::Equal, expected, nullptr)) {
                        matched = true;
                        break;
                    }
                }
                if (comparison_ == Comparison::NotIn) matched = !matched;
            } else {
                bool comparedAsLevel = false;
                if (field_ == "level") {
                    matched = compareLevel(actual, comparison_, values_.front(), comparedAsLevel);
                }
                if (!comparedAsLevel) {
                    matched = compareText(actual, comparison_, values_.front(), regex_.get());
                }
            }
            if (matched) return true;
        }
        return false;
    }
//---------------------------------------------------------------------------------------------------------------------------------//
private:
    std::string field_;
    bool keepMissing_;
    Comparison comparison_;
    std::vector<std::string> values_;
    std::unique_ptr<std::regex> regex_;
};

class Parser {
public:
    Parser(std::string_view expression, std::string& error) : lexer_(expression), error_(error) { advance(); }
//---------------------------------------------------------------------------------------------------------------------------------//

    std::shared_ptr<const Query::Node> parse()
    {
        auto root = parseOr();
        if (root && current_.kind != TokenKind::End) fail("unexpected token '" + current_.text + "'");
        return error_.empty() ? root : nullptr;
    }
//---------------------------------------------------------------------------------------------------------------------------------//
private:
    void advance() { current_ = lexer_.next(error_); }
//---------------------------------------------------------------------------------------------------------------------------------//
    void fail(std::string message) { if (error_.empty()) error_ = std::move(message); }
//---------------------------------------------------------------------------------------------------------------------------------//

    std::shared_ptr<const Query::Node> parseOr()
    {
        auto node = parseAnd();
        while (node && current_.kind == TokenKind::Or) {
            advance();
            node = std::make_shared<LogicalNode>(false, node, parseAnd());
        }
        return node;
    }
//---------------------------------------------------------------------------------------------------------------------------------//

    std::shared_ptr<const Query::Node> parseAnd()
    {
        auto node = parseNot();
        while (node && current_.kind == TokenKind::And) {
            advance();
            node = std::make_shared<LogicalNode>(true, node, parseNot());
        }
        return node;
    }
//---------------------------------------------------------------------------------------------------------------------------------//

    std::shared_ptr<const Query::Node> parseNot()
    {
        if (current_.kind == TokenKind::Not) {
            advance();
            return std::make_shared<NotNode>(parseNot());
        }
        return parsePrimary();
    }
//---------------------------------------------------------------------------------------------------------------------------------//

    std::shared_ptr<const Query::Node> parsePrimary()
    {
        if (current_.kind == TokenKind::LeftParenthesis) {
            advance();
            auto node = parseOr();
            if (current_.kind != TokenKind::RightParenthesis) {
                fail("expected ')'");
                return nullptr;
            }
            advance();
            return node;
        }
        if (current_.kind == TokenKind::Exists) {
            advance();
            if (current_.kind != TokenKind::LeftParenthesis) { fail("expected '(' after exists"); return nullptr; }
            advance();
            if (current_.kind != TokenKind::Word) { fail("expected field in exists()"); return nullptr; }
            std::string field = std::move(current_.text);
            advance();
            if (current_.kind != TokenKind::RightParenthesis) { fail("expected ')' after field"); return nullptr; }
            advance();
            return std::make_shared<ExistsNode>(std::move(field));
        }
        return parseComparison();
    }
//---------------------------------------------------------------------------------------------------------------------------------//

    std::shared_ptr<const Query::Node> parseComparison()
    {
        if (current_.kind != TokenKind::Word) { fail("expected field name"); return nullptr; }
        std::string field = std::move(current_.text);
        bool keepMissing = false;
        if (!field.empty() && field.back() == '?') { keepMissing = true; field.pop_back(); }
        advance();

        bool negatedWordOperator = false;
        if (current_.kind == TokenKind::Not) { negatedWordOperator = true; advance(); }
        Comparison comparison;
        switch (current_.kind) {
            case TokenKind::Equal: comparison = Comparison::Equal; break;
            case TokenKind::NotEqual: comparison = Comparison::NotEqual; break;
            case TokenKind::Greater: comparison = Comparison::Greater; break;
            case TokenKind::GreaterEqual: comparison = Comparison::GreaterEqual; break;
            case TokenKind::Less: comparison = Comparison::Less; break;
            case TokenKind::LessEqual: comparison = Comparison::LessEqual; break;
            case TokenKind::Contains: comparison = negatedWordOperator ? Comparison::NotContains : Comparison::Contains; break;
            case TokenKind::NotContains: comparison = Comparison::NotContains; break;
            case TokenKind::Like: comparison = negatedWordOperator ? Comparison::NotLike : Comparison::Like; break;
            case TokenKind::Match: comparison = negatedWordOperator ? Comparison::NotMatch : Comparison::Match; break;
            case TokenKind::NotMatch: comparison = Comparison::NotMatch; break;
            case TokenKind::In: comparison = negatedWordOperator ? Comparison::NotIn : Comparison::In; break;
            default: fail("expected comparison operator after field"); return nullptr;
        }
        advance();

        std::vector<std::string> values;
        if (comparison == Comparison::In || comparison == Comparison::NotIn) {
            if (current_.kind != TokenKind::LeftParenthesis) { fail("expected '(' after in"); return nullptr; }
            advance();
            while (current_.kind == TokenKind::Word) {
                values.push_back(std::move(current_.text));
                advance();
                if (current_.kind != TokenKind::Comma) break;
                advance();
            }
            if (values.empty() || current_.kind != TokenKind::RightParenthesis) { fail("invalid in set"); return nullptr; }
            advance();
        } else {
            if (current_.kind != TokenKind::Word) { fail("expected comparison value"); return nullptr; }
            values.push_back(std::move(current_.text));
            advance();
        }
        try {
            return std::make_shared<ComparisonNode>(std::move(field), keepMissing, comparison, std::move(values));
        } catch (const std::regex_error&) {
            fail("invalid regular expression");
            return nullptr;
        }
    }
//---------------------------------------------------------------------------------------------------------------------------------//

    Lexer lexer_;
    std::string& error_;
    Token current_;
};

} // namespace

Query::Query() = default;
//---------------------------------------------------------------------------------------------------------------------------------//

Query::Query(std::shared_ptr<const Node> root)
    : root_(std::move(root))
{
}
//---------------------------------------------------------------------------------------------------------------------------------//

bool Query::matches(const model::Record& record) const
{
    return root_ == nullptr || root_->evaluate(record);
}
//---------------------------------------------------------------------------------------------------------------------------------//

bool Query::empty() const
{
    return root_ == nullptr;
}
//---------------------------------------------------------------------------------------------------------------------------------//

bool compileQuery(std::string_view expression, Query& query, std::string& error)
{
    error.clear();
    Parser parser(expression, error);
    const auto root = parser.parse();
    if (!root) {
        if (error.empty()) error = "empty query";
        return false;
    }
    query = Query(root);
    return true;
}
//---------------------------------------------------------------------------------------------------------------------------------//

} // namespace hl::query
