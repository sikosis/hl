#ifndef HL_QUERY_FILTER_H
#define HL_QUERY_FILTER_H

#include "model/level.h"
#include "model/record.h"
#include "model/timestamp.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace hl::query {

enum class ComparisonOperator : std::uint8_t {
    Equal,
    NotEqual,
    Contains,
    NotContains,
};

struct FieldFilter {
    std::string field;
    ComparisonOperator comparison = ComparisonOperator::Equal;
    std::string value;
    bool keepMissing = false;
};

bool parseFieldFilter(std::string_view expression, FieldFilter& filter, std::string& error);

class Filter {
public:
    void setMinimumLevel(model::Level level);
    void setSince(model::Timestamp timestamp);
    void setUntil(model::Timestamp timestamp);
    void addFieldFilter(FieldFilter filter);

    bool isActive() const;
    bool matches(const model::Record& record) const;

private:
    std::optional<model::Level> minimumLevel_;
    std::optional<model::Timestamp> since_;
    std::optional<model::Timestamp> until_;
    std::vector<FieldFilter> fieldFilters_;
};

} // namespace hl::query

#endif
