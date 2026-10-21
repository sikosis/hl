#include "parse/timestamp.h"

#include <cstdint>
#include <limits>

namespace hl::parse {
namespace {

constexpr std::int64_t kNanosecondsPerSecond = 1'000'000'000;

bool readDigits(std::string_view text, std::size_t& position, std::size_t count, int& value)
{
    if (position + count > text.size()) {
        return false;
    }
    value = 0;
    for (std::size_t index = 0; index < count; ++index) {
        const char character = text[position + index];
        if (character < '0' || character > '9') {
            return false;
        }
        value = value * 10 + (character - '0');
    }
    position += count;
    return true;
}
//---------------------------------------------------------------------------------------------------------------------------------//

bool consume(std::string_view text, std::size_t& position, char expected)
{
    if (position >= text.size() || text[position] != expected) {
        return false;
    }
    ++position;
    return true;
}
//---------------------------------------------------------------------------------------------------------------------------------//

bool isLeapYear(int year)
{
    return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
}
//---------------------------------------------------------------------------------------------------------------------------------//

int daysInMonth(int year, int month)
{
    constexpr int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month == 2 && isLeapYear(year)) {
        return 29;
    }
    return days[month - 1];
}
//---------------------------------------------------------------------------------------------------------------------------------//

std::int64_t daysFromCivil(int year, unsigned month, unsigned day)
{
    year -= month <= 2;
    const int era = (year >= 0 ? year : year - 399) / 400;
    const unsigned yearOfEra = static_cast<unsigned>(year - era * 400);
    const int adjustedMonth = static_cast<int>(month) + (month > 2 ? -3 : 9);
    const unsigned dayOfYear = (153 * static_cast<unsigned>(adjustedMonth) + 2) / 5 + day - 1;
    const unsigned dayOfEra = yearOfEra * 365 + yearOfEra / 4 - yearOfEra / 100 + dayOfYear;
    return static_cast<std::int64_t>(era) * 146097 + static_cast<std::int64_t>(dayOfEra) - 719468;
}
//---------------------------------------------------------------------------------------------------------------------------------//

std::optional<model::Timestamp> makeTimestamp(
    std::int64_t seconds,
    std::int64_t fractionalNanoseconds,
    int offsetMinutes)
{
    const std::int64_t maximum = std::numeric_limits<std::int64_t>::max();
    const std::int64_t minimum = std::numeric_limits<std::int64_t>::min();
    if (seconds > (maximum - fractionalNanoseconds) / kNanosecondsPerSecond
        || seconds < minimum / kNanosecondsPerSecond) {
        return std::nullopt;
    }
    return model::Timestamp {
        seconds * kNanosecondsPerSecond + fractionalNanoseconds,
        static_cast<std::int16_t>(offsetMinutes),
    };
}
//---------------------------------------------------------------------------------------------------------------------------------//

std::optional<model::Timestamp> parseRfc3339(std::string_view text)
{
    std::size_t position = 0;
    int year = 0;
    int month = 0;
    int day = 0;
    int hour = 0;
    int minute = 0;
    int second = 0;
    if (!readDigits(text, position, 4, year)
        || !consume(text, position, '-')
        || !readDigits(text, position, 2, month)
        || !consume(text, position, '-')
        || !readDigits(text, position, 2, day)
        || position >= text.size()
        || (text[position] != 'T' && text[position] != 't' && text[position] != ' ')) {
        return std::nullopt;
    }
    ++position;
    if (!readDigits(text, position, 2, hour)
        || !consume(text, position, ':')
        || !readDigits(text, position, 2, minute)
        || !consume(text, position, ':')
        || !readDigits(text, position, 2, second)) {
        return std::nullopt;
    }
    if (month < 1 || month > 12 || day < 1 || day > daysInMonth(year, month)
        || hour > 23 || minute > 59 || second > 59) {
        return std::nullopt;
    }

    std::int64_t fractionalNanoseconds = 0;
    if (position < text.size() && text[position] == '.') {
        ++position;
        std::size_t digits = 0;
        while (position < text.size() && text[position] >= '0' && text[position] <= '9') {
            if (digits < 9) {
                fractionalNanoseconds = fractionalNanoseconds * 10 + (text[position] - '0');
            }
            ++digits;
            ++position;
        }
        if (digits == 0) {
            return std::nullopt;
        }
        for (std::size_t index = digits; index < 9; ++index) {
            fractionalNanoseconds *= 10;
        }
    }

    int offsetMinutes = 0;
    if (position < text.size() && (text[position] == 'Z' || text[position] == 'z')) {
        ++position;
    } else if (position < text.size() && (text[position] == '+' || text[position] == '-')) {
        const bool negative = text[position++] == '-';
        int offsetHour = 0;
        int offsetMinute = 0;
        if (!readDigits(text, position, 2, offsetHour)
            || !consume(text, position, ':')
            || !readDigits(text, position, 2, offsetMinute)
            || offsetHour > 23
            || offsetMinute > 59) {
            return std::nullopt;
        }
        offsetMinutes = offsetHour * 60 + offsetMinute;
        if (negative) {
            offsetMinutes = -offsetMinutes;
        }
    } else {
        return std::nullopt;
    }
    if (position != text.size()) {
        return std::nullopt;
    }

    const std::int64_t seconds = daysFromCivil(year, static_cast<unsigned>(month), static_cast<unsigned>(day)) * 86400
        + hour * 3600 + minute * 60 + second - offsetMinutes * 60;
    return makeTimestamp(seconds, fractionalNanoseconds, offsetMinutes);
}
//---------------------------------------------------------------------------------------------------------------------------------//

std::optional<model::Timestamp> parseUnixTimestamp(std::string_view text)
{
    if (text.empty()) {
        return std::nullopt;
    }
    std::size_t position = 0;
    const bool negative = text[position] == '-';
    if (negative || text[position] == '+') {
        ++position;
    }
    if (position == text.size()) {
        return std::nullopt;
    }

    std::uint64_t magnitude = 0;
    for (; position < text.size(); ++position) {
        const char character = text[position];
        if (character < '0' || character > '9') {
            return std::nullopt;
        }
        const unsigned digit = static_cast<unsigned>(character - '0');
        if (magnitude > (std::numeric_limits<std::uint64_t>::max() - digit) / 10) {
            return std::nullopt;
        }
        magnitude = magnitude * 10 + digit;
    }

    std::uint64_t multiplier = 1;
    if (magnitude < 100'000'000'000ULL) {
        multiplier = 1'000'000'000ULL;
    } else if (magnitude < 100'000'000'000'000ULL) {
        multiplier = 1'000'000ULL;
    } else if (magnitude < 100'000'000'000'000'000ULL) {
        multiplier = 1'000ULL;
    }
    const std::uint64_t limit = negative
        ? static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()) + 1
        : static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
    if (magnitude > limit / multiplier) {
        return std::nullopt;
    }
    const std::uint64_t scaled = magnitude * multiplier;
    const std::int64_t nanoseconds = negative
        ? (scaled == limit ? std::numeric_limits<std::int64_t>::min() : -static_cast<std::int64_t>(scaled))
        : static_cast<std::int64_t>(scaled);
    return model::Timestamp {nanoseconds, 0};
}
//---------------------------------------------------------------------------------------------------------------------------------//

} // namespace

std::optional<model::Timestamp> parseTimestamp(std::string_view text)
{
    if (text.size() >= 20 && text[4] == '-') {
        return parseRfc3339(text);
    }
    return parseUnixTimestamp(text);
}
//---------------------------------------------------------------------------------------------------------------------------------//

} // namespace hl::parse
