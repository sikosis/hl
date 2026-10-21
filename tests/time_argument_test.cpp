#include "parse/timestamp.h"
#include "query/time_argument.h"

#include <iostream>

namespace {

bool expect(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << message << '\n';
    }
    return condition;
}
//---------------------------------------------------------------------------------------------------------------------------------//

} // namespace

int main()
{
    bool passed = true;
    const auto now = hl::parse::parseTimestamp("2026-10-20T12:34:56Z");
    passed &= expect(now.has_value(), "test clock did not parse");
    if (!now.has_value()) {
        return 1;
    }

    const auto threeHoursAgo = hl::query::parseTimeArgument("-3h", now->nanoseconds);
    passed &= expect(
        threeHoursAgo.has_value() && threeHoursAgo->nanoseconds == now->nanoseconds - 3LL * 3600 * 1000000000,
        "relative hours parsed incorrectly");

    const auto current = hl::query::parseTimeArgument("now", now->nanoseconds);
    passed &= expect(
        current.has_value() && current->nanoseconds == now->nanoseconds,
        "now parsed incorrectly");

    const auto tomorrow = hl::query::parseTimeArgument("+1d", now->nanoseconds);
    passed &= expect(
        tomorrow.has_value() && tomorrow->nanoseconds == now->nanoseconds + 86400LL * 1000000000,
        "relative days parsed incorrectly");

    const auto today = hl::query::parseTimeArgument("today", now->nanoseconds);
    const auto expectedToday = hl::parse::parseTimestamp("2026-10-20T00:00:00Z");
    passed &= expect(
        today.has_value() && expectedToday.has_value() && today->nanoseconds == expectedToday->nanoseconds,
        "today parsed incorrectly");

    const auto yesterday = hl::query::parseTimeArgument("yesterday", now->nanoseconds);
    const auto expectedYesterday = hl::parse::parseTimestamp("2026-10-19T00:00:00Z");
    passed &= expect(
        yesterday.has_value() && expectedYesterday.has_value()
            && yesterday->nanoseconds == expectedYesterday->nanoseconds,
        "yesterday parsed incorrectly");

    passed &= expect(
        hl::query::parseTimeArgument("2026-10-20T10:00:00Z", now->nanoseconds).has_value(),
        "absolute time argument was rejected");
    passed &= expect(
        !hl::query::parseTimeArgument("-hours", now->nanoseconds).has_value(),
        "invalid relative time was accepted");
    return passed ? 0 : 1;
}
//---------------------------------------------------------------------------------------------------------------------------------//
