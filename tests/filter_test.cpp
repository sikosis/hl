#include "parse/scanner.h"
#include "query/filter.h"

#include <iostream>
#include <string>

namespace {

bool expect(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << message << '\n';
    }
    return condition;
}
//---------------------------------------------------------------------------------------------------------------------------------//

hl::model::Record scan(std::string_view entry)
{
    hl::model::Record record;
    (void)hl::parse::scanEntry(entry, record);
    return record;
}
//---------------------------------------------------------------------------------------------------------------------------------//

} // namespace

int main()
{
    bool passed = true;

    hl::query::FieldFilter parsed;
    std::string error;
    passed &= expect(hl::query::parseFieldFilter("component=api", parsed, error), "equality filter did not parse");
    passed &= expect(parsed.field == "component" && parsed.value == "api", "equality filter parsed incorrectly");
    passed &= expect(hl::query::parseFieldFilter("message?~=ready", parsed, error), "optional contains filter did not parse");
    passed &= expect(parsed.keepMissing && parsed.comparison == hl::query::ComparisonOperator::Contains, "optional filter parsed incorrectly");
    passed &= expect(!hl::query::parseFieldFilter("missing-operator", parsed, error), "invalid filter was accepted");

    const hl::model::Record info = scan(
        "{\"timestamp\":\"2026-10-20T10:00:00Z\",\"level\":\"info\","
        "\"message\":\"service ready\",\"component\":\"api\",\"code\":200}");
    const hl::model::Record errorRecord = scan(
        "{\"timestamp\":\"2026-10-20T11:00:00Z\",\"level\":\"error\","
        "\"message\":\"service failed\",\"component\":\"worker\",\"code\":500}");
    const hl::model::Record plain = scan("ordinary log entry");

    hl::query::Filter levelFilter;
    levelFilter.setMinimumLevel(hl::model::Level::Warning);
    passed &= expect(!levelFilter.matches(info), "level filter kept info");
    passed &= expect(levelFilter.matches(errorRecord), "level filter removed error");
    passed &= expect(!levelFilter.matches(plain), "level filter kept an entry without a level");

    hl::query::Filter fieldFilter;
    passed &= expect(hl::query::parseFieldFilter("component=api", parsed, error), "field filter setup failed");
    fieldFilter.addFieldFilter(parsed);
    passed &= expect(fieldFilter.matches(info), "field equality removed matching entry");
    passed &= expect(!fieldFilter.matches(errorRecord), "field equality kept non-matching entry");

    hl::query::Filter containsFilter;
    passed &= expect(hl::query::parseFieldFilter("message~=failed", parsed, error), "contains filter setup failed");
    containsFilter.addFieldFilter(parsed);
    passed &= expect(containsFilter.matches(errorRecord), "contains filter removed matching message");
    passed &= expect(!containsFilter.matches(info), "contains filter kept non-matching message");

    hl::query::Filter missingFilter;
    passed &= expect(hl::query::parseFieldFilter("request_id?=123", parsed, error), "missing-field filter setup failed");
    missingFilter.addFieldFilter(parsed);
    passed &= expect(missingFilter.matches(info), "? modifier did not keep a missing field");
    passed &= expect(missingFilter.matches(plain), "? modifier did not keep a plain entry");

    hl::query::Filter timeFilter;
    timeFilter.setSince(hl::model::Timestamp {1792492200000000000LL, 0});
    timeFilter.setUntil(hl::model::Timestamp {1792495800000000000LL, 0});
    passed &= expect(!timeFilter.matches(info), "since filter kept an older entry");
    passed &= expect(timeFilter.matches(errorRecord), "time range removed matching entry");
    passed &= expect(!timeFilter.matches(plain), "time range kept entry without a timestamp");

    return passed ? 0 : 1;
}
//---------------------------------------------------------------------------------------------------------------------------------//
