#include "query/filter.h"

#include "query/json_path.h"

#include <utility>

namespace hl::query {
namespace {

void appendValueText(const model::Value& value, std::vector<std::string_view>& values)
{
    if (value.defined) {
        values.push_back(value.implicit ? std::string_view("true") : value.raw);
    }
}
//---------------------------------------------------------------------------------------------------------------------------------//

bool appendExactSourceFields(
    const model::Record& record,
    std::string_view name,
    std::vector<std::string_view>& values)
{
    for (const model::Field& field : record.fields) {
        if (field.key == name) {
            appendValueText(field.value, values);
        }
    }
    return !values.empty();
}
//---------------------------------------------------------------------------------------------------------------------------------//

void findFields(
    const model::Record& record,
    std::string_view name,
    std::vector<std::string_view>& values)
{
    bool sourceOnly = false;
    if (!name.empty() && name.front() == '.') {
        sourceOnly = true;
        name.remove_prefix(1);
    }
    if (!sourceOnly && name == "level") {
        if (record.level == model::Level::Unknown) {
            return;
        }
        values.push_back(model::levelName(record.level));
        return;
    }
    if (!sourceOnly && name == "message") {
        appendValueText(record.message, values);
        return;
    }
    if (!sourceOnly && name == "logger") {
        appendValueText(record.logger, values);
        return;
    }
    if (!sourceOnly && name == "caller") {
        appendValueText(record.caller, values);
        return;
    }
    if (appendExactSourceFields(record, name, values)) {
        return;
    }
    resolveNestedField(record, name, values);
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
        std::vector<std::string_view> values;
        findFields(record, filter.field, values);
        if (values.empty()) {
            if (filter.keepMissing) {
                continue;
            }
            return false;
        }
        bool matched = false;
        for (const std::string_view value : values) {
            if (compare(value, filter.comparison, filter.value)) {
                matched = true;
                break;
            }
        }
        if (!matched) {
            return false;
        }
    }
    return true;
}
//---------------------------------------------------------------------------------------------------------------------------------//

} // namespace hl::query
