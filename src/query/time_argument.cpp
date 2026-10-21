#include "query/time_argument.h"

#include "parse/timestamp.h"

#include <limits>

namespace hl::query {
namespace {

constexpr std::int64_t kNanosecondsPerSecond = 1'000'000'000;
constexpr std::int64_t kNanosecondsPerDay = 86400 * kNanosecondsPerSecond;

std::optional<std::int64_t> relativeDuration(std::string_view text)
{
    if (text.size() < 3 || (text.front() != '-' && text.front() != '+')) {
        return std::nullopt;
    }
    std::int64_t unit = 0;
    switch (text.back()) {
        case 's':
            unit = kNanosecondsPerSecond;
            break;
        case 'm':
            unit = 60 * kNanosecondsPerSecond;
            break;
        case 'h':
            unit = 3600 * kNanosecondsPerSecond;
            break;
        case 'd':
            unit = kNanosecondsPerDay;
            break;
        case 'w':
            unit = 7 * kNanosecondsPerDay;
            break;
        default:
            return std::nullopt;
    }

    std::int64_t magnitude = 0;
    for (std::size_t position = 1; position + 1 < text.size(); ++position) {
        const char character = text[position];
        if (character < '0' || character > '9') {
            return std::nullopt;
        }
        const int digit = character - '0';
        if (magnitude > (std::numeric_limits<std::int64_t>::max() - digit) / 10) {
            return std::nullopt;
        }
        magnitude = magnitude * 10 + digit;
    }
    if (magnitude > std::numeric_limits<std::int64_t>::max() / unit) {
        return std::nullopt;
    }
    const std::int64_t duration = magnitude * unit;
    return text.front() == '-' ? -duration : duration;
}
//---------------------------------------------------------------------------------------------------------------------------------//

std::int64_t startOfUtcDay(std::int64_t nanoseconds)
{
    std::int64_t days = nanoseconds / kNanosecondsPerDay;
    if (nanoseconds % kNanosecondsPerDay < 0) {
        --days;
    }
    return days * kNanosecondsPerDay;
}
//---------------------------------------------------------------------------------------------------------------------------------//

} // namespace

std::optional<model::Timestamp> parseTimeArgument(std::string_view text, std::int64_t nowNanoseconds)
{
    if (text == "now") {
        return model::Timestamp {nowNanoseconds, 0};
    }
    if (text == "today") {
        return model::Timestamp {startOfUtcDay(nowNanoseconds), 0};
    }
    if (text == "yesterday") {
        return model::Timestamp {startOfUtcDay(nowNanoseconds) - kNanosecondsPerDay, 0};
    }
    if (const auto duration = relativeDuration(text); duration.has_value()) {
        if ((duration.value() > 0 && nowNanoseconds > std::numeric_limits<std::int64_t>::max() - duration.value())
            || (duration.value() < 0 && nowNanoseconds < std::numeric_limits<std::int64_t>::min() - duration.value())) {
            return std::nullopt;
        }
        return model::Timestamp {nowNanoseconds + duration.value(), 0};
    }
    return parse::parseTimestamp(text);
}
//---------------------------------------------------------------------------------------------------------------------------------//

} // namespace hl::query
