#include "parse/timestamp.h"

#include <cstdint>
#include <iostream>
#include <string_view>

namespace {

bool expectTimestamp(std::string_view text, std::int64_t expected)
{
    const auto timestamp = hl::parse::parseTimestamp(text);
    if (timestamp.has_value() && timestamp->nanoseconds == expected) {
        return true;
    }
    std::cerr << "timestamp did not match: " << text << '\n';
    return false;
}
//---------------------------------------------------------------------------------------------------------------------------------//

bool expectEquivalent(std::string_view left, std::string_view right)
{
    const auto leftTimestamp = hl::parse::parseTimestamp(left);
    const auto rightTimestamp = hl::parse::parseTimestamp(right);
    if (leftTimestamp.has_value()
        && rightTimestamp.has_value()
        && leftTimestamp->nanoseconds == rightTimestamp->nanoseconds) {
        return true;
    }
    std::cerr << "timestamps were not equivalent: " << left << " and " << right << '\n';
    return false;
}
//---------------------------------------------------------------------------------------------------------------------------------//

} // namespace

int main()
{
    bool passed = true;
    passed &= expectTimestamp("1970-01-01T00:00:00Z", 0);
    passed &= expectTimestamp("1970-01-01T00:00:00.123456789Z", 123456789);
    passed &= expectTimestamp("1969-12-31T23:59:59.500Z", -500000000);
    passed &= expectEquivalent("2026-10-20T12:34:56.123456789+10:00", "2026-10-20T02:34:56.123456789Z");
    passed &= expectEquivalent("2026-10-20T01:00:00-01:30", "2026-10-20T02:30:00Z");
    passed &= expectTimestamp("1700000000", 1700000000000000000LL);
    passed &= expectTimestamp("1700000000000", 1700000000000000000LL);
    passed &= expectTimestamp("1700000000000000", 1700000000000000000LL);
    passed &= expectTimestamp("1700000000000000000", 1700000000000000000LL);
    passed &= !hl::parse::parseTimestamp("2026-02-29T00:00:00Z").has_value();
    passed &= !hl::parse::parseTimestamp("2026-10-20T25:00:00Z").has_value();
    passed &= !hl::parse::parseTimestamp("2026-10-20T12:00:00").has_value();
    passed &= !hl::parse::parseTimestamp("not-a-time").has_value();
    return passed ? 0 : 1;
}
//---------------------------------------------------------------------------------------------------------------------------------//
