#include "query/filter.h"

#include <utility>

namespace hl::query {
namespace {

struct FieldValue {
    bool exists = false;
    std::string_view text;
};

FieldValue valueText(const model::Value& value)
{
    if (!value.defined) {
        return {};
    }
    return {true, value.implicit ? std::string_view("true") : value.raw};
}
//---------------------------------------------------------------------------------------------------------------------------------//

FieldValue findSourceField(const model::Record& record, std::string_view name)
{
    for (const model::Field& field : record.fields) {
        if (field.key == name) {
            return valueText(field.value);
        }
    }
    return {};
}
//---------------------------------------------------------------------------------------------------------------------------------//

FieldValue findField(const model::Record& record, std::string_view name)
{
    if (!name.empty() && name.front() == '.') {
        return findSourceField(record, name.substr(1));
    }
    if (name == "level") {
        if (record.level == model::Level::Unknown) {
            return {};
        }
        return {true, model::levelName(record.level)};
    }
    if (name == "message") {
        return valueText(record.message);
    }
    if (name == "logger") {
        return valueText(record.logger);
    }
    if (name == "caller") {
        return valueText(record.caller);
    }
    return findSourceField(record, name);
}
//---------------------------------------------------------------------------------------------------------------------------------//

bool compare(std::string_view actual, ComparisonOperator comparison, std::string_view expected)
{
    switch (comparison) {
        case ComparisonOperator::Equal:
            return actual == expected;
        case ComparisonOperator::NotEqual:
            return actual != expected;
        case ComparisonOperator::Contains:
            return actual.find(expected) != std::string_view::npos;
        case ComparisonOperator::NotContains:
            return actual.find(expected) == std::string_view::npos;
    }
    return false;
}
//---------------------------------------------------------------------------------------------------------------------------------//

} // namespace

bool parseFieldFilter(std::string_view expression, FieldFilter& filter, std::string& error)
{
    struct OperatorDefinition {
        std::string_view text;
        ComparisonOperator comparison;
    };
    constexpr OperatorDefinition operators[] = {
        {"!~=", ComparisonOperator::NotContains},
        {"~=", ComparisonOperator::Contains},
        {"!=", ComparisonOperator::NotEqual},
        {"=", ComparisonOperator::Equal},
    };

    for (const OperatorDefinition& definition : operators) {
        const std::size_t position = expression.find(definition.text);
        if (position == std::string_view::npos) {
            continue;
        }
        std::string_view field = expression.substr(0, position);
        bool keepMissing = false;
        if (!field.empty() && field.back() == '?') {
            keepMissing = true;
            field.remove_suffix(1);
        }
        if (field.empty()) {
            error = "field filter has no field name";
            return false;
        }
        filter.field = std::string(field);
        filter.comparison = definition.comparison;
        filter.value = std::string(expression.substr(position + definition.text.size()));
        filter.keepMissing = keepMissing;
        error.clear();
        return true;
    }
    error = "field filter needs one of =, !=, ~= or !~=";
    return false;
}
//---------------------------------------------------------------------------------------------------------------------------------//

void Filter::setMinimumLevel(model::Level level)
{
    minimumLevel_ = level;
}
//---------------------------------------------------------------------------------------------------------------------------------//

void Filter::setSince(model::Timestamp timestamp)
{
    since_ = timestamp;
}
//---------------------------------------------------------------------------------------------------------------------------------//

void Filter::setUntil(model::Timestamp timestamp)
{
    until_ = timestamp;
}
//---------------------------------------------------------------------------------------------------------------------------------//

void Filter::addFieldFilter(FieldFilter filter)
{
    fieldFilters_.push_back(std::move(filter));
}
//---------------------------------------------------------------------------------------------------------------------------------//

bool Filter::isActive() const
{
    return minimumLevel_.has_value() || since_.has_value() || until_.has_value() || !fieldFilters_.empty();
}
//---------------------------------------------------------------------------------------------------------------------------------//

bool Filter::matches(const model::Record& record) const
{
    if (minimumLevel_.has_value()) {
        if (record.level == model::Level::Unknown
            || static_cast<unsigned>(record.level) < static_cast<unsigned>(minimumLevel_.value())) {
            return false;
        }
    }
    if (since_.has_value()
        && (!record.timestamp.has_value() || record.timestamp->nanoseconds < since_->nanoseconds)) {
        return false;
    }
    if (until_.has_value()
        && (!record.timestamp.has_value() || record.timestamp->nanoseconds > until_->nanoseconds)) {
        return false;
    }
    for (const FieldFilter& filter : fieldFilters_) {
        const FieldValue actual = findField(record, filter.field);
        if (!actual.exists) {
            if (filter.keepMissing) {
                continue;
            }
            return false;
        }
        if (!compare(actual.text, filter.comparison, filter.value)) {
            return false;
        }
    }
    return true;
}
//---------------------------------------------------------------------------------------------------------------------------------//

} // namespace hl::query
