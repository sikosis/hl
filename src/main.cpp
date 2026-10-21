#include "config/config.h"
#include "io/input.h"
#include "io/pager.h"
#include "pipeline/processor.h"
#include "platform/platform.h"
#include "query/filter.h"
#include "query/query.h"
#include "query/time_argument.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iostream>
#include <limits>
#include <thread>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <unistd.h>

#ifndef HL_VERSION
#define HL_VERSION "development"
#endif

namespace {

enum class ColourMode {
    Auto,
    Always,
    Never,
};

enum class PagingMode {
    Auto,
    Always,
    Never,
};

bool parseColourMode(std::string_view value, ColourMode& mode)
{
    if (value == "auto") {
        mode = ColourMode::Auto;
        return true;
    }
    if (value == "always") {
        mode = ColourMode::Always;
        return true;
    }
    if (value == "never") {
        mode = ColourMode::Never;
        return true;
    }
    return false;
}
//---------------------------------------------------------------------------------------------------------------------------------//

bool parsePagingMode(std::string_view value, PagingMode& mode)
{
    if (value == "auto") {
        mode = PagingMode::Auto;
        return true;
    }
    if (value == "always") {
        mode = PagingMode::Always;
        return true;
    }
    if (value == "never") {
        mode = PagingMode::Never;
        return true;
    }
    return false;
}
//---------------------------------------------------------------------------------------------------------------------------------//

bool addVisibilityRules(std::string_view value, std::vector<std::string>& rules)
{
    std::size_t start = 0;
    while (start <= value.size()) {
        const std::size_t end = value.find(',', start);
        const std::string_view rule = value.substr(start, end - start);
        if (rule.empty() || rule == "!") {
            return false;
        }
        rules.emplace_back(rule);
        if (end == std::string_view::npos) {
            break;
        }
        start = end + 1;
    }
    return true;
}
//---------------------------------------------------------------------------------------------------------------------------------//

bool parseSize(std::string_view text, std::size_t& size)
{
    if (text.empty()) {
        return false;
    }
    std::size_t position = 0;
    std::uint64_t value = 0;
    while (position < text.size() && text[position] >= '0' && text[position] <= '9') {
        const unsigned digit = static_cast<unsigned>(text[position] - '0');
        if (value > (std::numeric_limits<std::uint64_t>::max() - digit) / 10) {
            return false;
        }
        value = value * 10 + digit;
        ++position;
    }
    if (position == 0) {
        return false;
    }

    const std::string_view suffix = text.substr(position);
    std::uint64_t multiplier = 1;
    if (suffix == "k" || suffix == "K" || suffix == "KB" || suffix == "KiB") {
        multiplier = 1024;
    } else if (suffix == "m" || suffix == "M" || suffix == "MB" || suffix == "MiB") {
        multiplier = 1024 * 1024;
    } else if (suffix == "g" || suffix == "G" || suffix == "GB" || suffix == "GiB") {
        multiplier = 1024ULL * 1024ULL * 1024ULL;
    } else if (!suffix.empty()) {
        return false;
    }
    if (value == 0 || value > std::numeric_limits<std::size_t>::max() / multiplier) {
        return false;
    }
    size = static_cast<std::size_t>(value * multiplier);
    return true;
}
//---------------------------------------------------------------------------------------------------------------------------------//

bool parseOutputDelimiter(std::string_view text, std::string& delimiter)
{
    hl::io::Delimiter parsed;
    if (!hl::io::parseDelimiter(text, parsed) || parsed == hl::io::Delimiter::Auto) {
        return false;
    }
    delimiter.assign(hl::io::delimiterText(parsed));
    return true;
}
//---------------------------------------------------------------------------------------------------------------------------------//

bool parseBoolean(std::string_view text, bool& value)
{
    if (text == "true" || text == "yes" || text == "on" || text == "1") {
        value = true;
        return true;
    }
    if (text == "false" || text == "no" || text == "off" || text == "0") {
        value = false;
        return true;
    }
    return false;
}
//---------------------------------------------------------------------------------------------------------------------------------//

std::string environmentValue(const char* name)
{
    const char* value = std::getenv(name);
    return value == nullptr ? std::string() : std::string(value);
}
//---------------------------------------------------------------------------------------------------------------------------------//

void printHelp(std::ostream& output)
{
    output << "Usage: hl [OPTIONS] [FILE]...\n"
           << "\n"
           << "A fast viewer for text, JSON and logfmt logs.\n"
           << "\n"
           << "Options:\n"
           << "  -r, --raw    Do not format structured entries\n"
           << "  --input-format FORMAT\n"
           << "               Force auto, text, json or logfmt input\n"
           << "  --delimiter DELIMITER\n"
           << "               Split on auto, lf, cr, crlf or nul\n"
           << "  --allow-prefix\n"
           << "               Allow text before a JSON object\n"
           << "  -l, --level LEVEL\n"
           << "               Keep entries at or above LEVEL\n"
           << "  -f, --filter FIELD[?](=|!=|~=|!~=)VALUE\n"
           << "               Add a field filter; ? keeps missing fields\n"
           << "  -q, --query EXPRESSION\n"
           << "               Add a boolean query expression\n"
           << "  --since TIME Keep entries at or after TIME\n"
           << "  --until TIME Keep entries at or before TIME\n"
           << "  -h, --hide FIELDS\n"
           << "               Hide fields; use !FIELD to reveal and * for all\n"
           << "  -e, --hide-empty-fields\n"
           << "               Hide empty strings, objects, arrays and nulls\n"
           << "  -E, --show-empty-fields\n"
           << "               Show empty fields (default)\n"
           << "  -t, --time-format FORMAT\n"
           << "               Set strftime-style time format (supports %3N/%6N/%9N)\n"
           << "  -L, --local  Format timestamps in the local time zone\n"
           << "  -Z, --time-zone ZONE\n"
           << "               Select UTC, local or a system named time zone\n"
           << "  -x, --expansion MODE\n"
           << "               Expand fields: never, inline, auto or always\n"
           << "  --ascii      Use ASCII layout characters\n"
           << "  -o, --output FILE\n"
           << "               Write output to FILE\n"
           << "  --output-delimiter DELIMITER\n"
           << "               Write lf, cr, crlf or nul between entries\n"
           << "  --buffer-size SIZE\n"
           << "               Set read buffer size (for example 256KiB)\n"
           << "  --max-message-size SIZE\n"
           << "               Bound an individual entry (default: 64MiB)\n"
           << "  -C, --concurrency COUNT\n"
           << "               Set formatter workers (default: processor count)\n"
           << "  --paging [auto|always|never]\n"
           << "               Control use of a terminal pager\n"
           << "  -P           Disable paging\n"
           << "  -s, --sort   Sort all input entries chronologically\n"
           << "  --config FILE\n"
           << "               Load a TOML configuration file; - skips defaults\n"
           << "  -c           Always use ANSI colour\n"
           << "  --color [auto|always|never]\n"
           << "               Control ANSI colour (default: auto)\n"
           << "  --help       Print help\n"
           << "  -V, --version\n"
           << "               Print version\n";
}
//---------------------------------------------------------------------------------------------------------------------------------//

} // namespace

int main(int argumentCount, char* arguments[])
{
    std::vector<std::string> paths;
    bool optionsEnded = false;
    bool raw = false;
    bool sort = false;
    ColourMode colourMode = ColourMode::Auto;
    PagingMode pagingMode = PagingMode::Auto;
    bool hideEmptyFields = false;
    bool localTime = false;
    std::string timeZone = "UTC";
    bool ascii = false;
    bool allowPrefix = false;
    std::string outputPath;
    std::string timeFormat = "%Y-%m-%d %T.%3N";
    std::string outputDelimiter;
    bool replaceOutputDelimiter = false;
    std::size_t bufferSize = 256 * 1024;
    std::size_t maximumMessageSize = 64 * 1024 * 1024;
    std::size_t concurrency = std::thread::hardware_concurrency();
    if (concurrency == 0) {
        concurrency = 1;
    }
    hl::format::Expansion expansion = hl::format::Expansion::Never;
    hl::parse::InputFormat inputFormat = hl::parse::InputFormat::Auto;
    hl::io::Delimiter inputDelimiter = hl::io::Delimiter::Auto;
    std::vector<std::string> visibilityRules;
    hl::query::Filter filter;
    const std::int64_t nowNanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    const auto applySetting = [&](std::string_view key, std::string_view value, std::string& error) {
        if (key == "input-format" || key == "input.format") {
            if (!hl::parse::parseInputFormat(value, inputFormat)) {
                error = "invalid input format '" + std::string(value) + "'";
                return false;
            }
        } else if (key == "delimiter" || key == "input.delimiter") {
            if (!hl::io::parseDelimiter(value, inputDelimiter)) {
                error = "invalid delimiter '" + std::string(value) + "'";
                return false;
            }
        } else if (key == "allow-prefix" || key == "input.allow-prefix") {
            if (!parseBoolean(value, allowPrefix)) {
                error = "invalid boolean '" + std::string(value) + "'";
                return false;
            }
        } else if (key == "level" || key == "filter.level") {
            const hl::model::Level level = hl::model::levelFromText(value);
            if (level == hl::model::Level::Unknown) {
                error = "unknown level '" + std::string(value) + "'";
                return false;
            }
            filter.setMinimumLevel(level);
        } else if (key == "color" || key == "output.color") {
            if (!parseColourMode(value, colourMode)) {
                error = "invalid colour mode '" + std::string(value) + "'";
                return false;
            }
        } else if (key == "time-format" || key == "output.time-format") {
            timeFormat = value;
        } else if (key == "time-zone" || key == "output.time-zone") {
            if (value == "UTC" || value == "utc" || value == "Z") {
                localTime = false;
                timeZone = "UTC";
            } else if (value == "local") {
                localTime = true;
                timeZone = "local";
            } else {
                localTime = true;
                timeZone = value;
            }
        } else if (key == "hide-empty-fields" || key == "output.hide-empty-fields") {
            if (!parseBoolean(value, hideEmptyFields)) {
                error = "invalid boolean '" + std::string(value) + "'";
                return false;
            }
        } else if (key == "ascii" || key == "output.ascii") {
            if (!parseBoolean(value, ascii)) {
                error = "invalid boolean '" + std::string(value) + "'";
                return false;
            }
        } else if (key == "expansion" || key == "output.expansion") {
            if (!hl::format::parseExpansion(value, expansion)) {
                error = "invalid expansion mode '" + std::string(value) + "'";
                return false;
            }
        } else if (key == "output-delimiter" || key == "output.delimiter") {
            if (!parseOutputDelimiter(value, outputDelimiter)) {
                error = "invalid output delimiter '" + std::string(value) + "'";
                return false;
            }
            replaceOutputDelimiter = true;
        } else if (key == "paging") {
            if (!parsePagingMode(value, pagingMode)) {
                error = "invalid paging mode '" + std::string(value) + "'";
                return false;
            }
        } else if (key == "buffer-size" || key == "tuning.buffer-size") {
            if (!parseSize(value, bufferSize)) {
                error = "invalid buffer size '" + std::string(value) + "'";
                return false;
            }
        } else if (key == "max-message-size" || key == "tuning.max-message-size") {
            if (!parseSize(value, maximumMessageSize)) {
                error = "invalid maximum message size '" + std::string(value) + "'";
                return false;
            }
        } else if (key == "concurrency" || key == "tuning.concurrency") {
            if (!parseSize(value, concurrency)) {
                error = "invalid concurrency '" + std::string(value) + "'";
                return false;
            }
        }
        error.clear();
        return true;
    };

    std::vector<std::string> explicitConfigs;
    const std::string environmentConfig = environmentValue("HL_CONFIG");
    if (!environmentConfig.empty()) {
        explicitConfigs.push_back(environmentConfig);
    }
    for (int index = 1; index < argumentCount; ++index) {
        const std::string_view argument(arguments[index]);
        if (argument == "--config" && index + 1 < argumentCount) {
            explicitConfigs.emplace_back(arguments[++index]);
        } else if (argument.substr(0, 9) == "--config=") {
            explicitConfigs.emplace_back(argument.substr(9));
        }
    }

    std::vector<std::string> configPaths;
    bool skipDefaults = false;
    for (const std::string& path : explicitConfigs) {
        if (path == "-") {
            skipDefaults = true;
            configPaths.clear();
        } else {
            configPaths.push_back(path);
        }
    }
    if (!skipDefaults) {
        std::vector<std::string> defaults;
        const std::string systemConfig = hl::platform::systemConfigDir();
        const std::string userConfig = hl::platform::userConfigDir();
        if (!systemConfig.empty()) {
            defaults.push_back(systemConfig + "/config.toml");
        }
        if (!userConfig.empty()) {
            defaults.push_back(userConfig + "/config.toml");
        }
        configPaths.insert(configPaths.begin(), defaults.begin(), defaults.end());
    }

    for (const std::string& path : configPaths) {
        std::ifstream check(path);
        const bool explicitPath = std::find(explicitConfigs.begin(), explicitConfigs.end(), path) != explicitConfigs.end();
        if (!check && !explicitPath) {
            continue;
        }
        check.close();
        std::vector<hl::config::Setting> settings;
        std::string error;
        if (!hl::config::loadFile(path, settings, error)) {
            std::cerr << "hl: " << error << '\n';
            return 2;
        }
        for (const hl::config::Setting& setting : settings) {
            if (!applySetting(setting.key, setting.value, error)) {
                std::cerr << "hl: " << path << ": " << error << '\n';
                return 2;
            }
        }
    }

    const struct EnvironmentSetting {
        const char* name;
        const char* key;
    } environmentSettings[] = {
        {"HL_INPUT_FORMAT", "input-format"},
        {"HL_DELIMITER", "delimiter"},
        {"HL_ALLOW_PREFIX", "allow-prefix"},
        {"HL_LEVEL", "level"},
        {"HL_COLOR", "color"},
        {"HL_TIME_FORMAT", "time-format"},
        {"HL_TIME_ZONE", "time-zone"},
        {"HL_HIDE_EMPTY_FIELDS", "hide-empty-fields"},
        {"HL_EXPANSION", "expansion"},
        {"HL_ASCII", "ascii"},
        {"HL_OUTPUT_DELIMITER", "output-delimiter"},
        {"HL_PAGING", "paging"},
        {"HL_CONCURRENCY", "concurrency"},
        {"HL_BUFFER_SIZE", "buffer-size"},
        {"HL_MAX_MESSAGE_SIZE", "max-message-size"},
    };
    for (const EnvironmentSetting& setting : environmentSettings) {
        const std::string value = environmentValue(setting.name);
        if (value.empty()) {
            continue;
        }
        std::string error;
        if (!applySetting(setting.key, value, error)) {
            std::cerr << "hl: " << setting.name << ": " << error << '\n';
            return 2;
        }
    }

    for (int index = 1; index < argumentCount; ++index) {
        const std::string_view argument(arguments[index]);
        if (!optionsEnded && argument == "--") {
            optionsEnded = true;
            continue;
        }
        if (!optionsEnded && (argument == "-V" || argument == "--version")) {
            std::cout << "hl " << HL_VERSION << '\n';
            return 0;
        }
        if (!optionsEnded && argument == "--config") {
            if (index + 1 >= argumentCount) {
                std::cerr << "hl: --config needs a file\n";
                return 2;
            }
            ++index;
            continue;
        }
        if (!optionsEnded && argument.substr(0, 9) == "--config=") {
            continue;
        }
        if (!optionsEnded && argument == "--help") {
            printHelp(std::cout);
            return 0;
        }
        if (!optionsEnded && (argument == "-r" || argument == "--raw")) {
            raw = true;
            continue;
        }
        if (!optionsEnded && (argument == "-s" || argument == "--sort")) {
            sort = true;
            continue;
        }
        if (!optionsEnded && argument == "--allow-prefix") {
            allowPrefix = true;
            continue;
        }
        if (!optionsEnded && (argument == "--ascii")) {
            ascii = true;
            continue;
        }
        if (!optionsEnded && (argument == "-L" || argument == "--local")) {
            localTime = true;
            timeZone = "local";
            continue;
        }
        if (!optionsEnded && (argument == "--input-format" || argument == "--delimiter"
                || argument == "--output-delimiter" || argument == "--buffer-size"
                || argument == "--max-message-size" || argument == "-t"
                || argument == "--time-format" || argument == "-Z"
                || argument == "--time-zone" || argument == "-o" || argument == "--output"
                || argument == "-x" || argument == "--expansion" || argument == "-C"
                || argument == "--concurrency")) {
            if (index + 1 >= argumentCount) {
                std::cerr << "hl: " << argument << " needs a value\n";
                return 2;
            }
            const std::string_view value(arguments[++index]);
            if (argument == "--input-format" && !hl::parse::parseInputFormat(value, inputFormat)) {
                std::cerr << "hl: invalid input format '" << value << "'\n";
                return 2;
            } else if (argument == "--delimiter" && !hl::io::parseDelimiter(value, inputDelimiter)) {
                std::cerr << "hl: invalid delimiter '" << value << "'\n";
                return 2;
            } else if (argument == "--output-delimiter") {
                if (!parseOutputDelimiter(value, outputDelimiter)) {
                    std::cerr << "hl: invalid output delimiter '" << value << "'\n";
                    return 2;
                }
                replaceOutputDelimiter = true;
            } else if (argument == "--buffer-size" && !parseSize(value, bufferSize)) {
                std::cerr << "hl: invalid buffer size '" << value << "'\n";
                return 2;
            } else if (argument == "--max-message-size" && !parseSize(value, maximumMessageSize)) {
                std::cerr << "hl: invalid maximum message size '" << value << "'\n";
                return 2;
            } else if (argument == "-C" || argument == "--concurrency") {
                if (!parseSize(value, concurrency)) {
                    std::cerr << "hl: invalid concurrency '" << value << "'\n";
                    return 2;
                }
            } else if (argument == "-t" || argument == "--time-format") {
                timeFormat = value;
            } else if (argument == "-Z" || argument == "--time-zone") {
                if (value == "UTC" || value == "utc" || value == "Z") {
                    localTime = false;
                    timeZone = "UTC";
                } else if (value == "local") {
                    localTime = true;
                    timeZone = "local";
                } else {
                    localTime = true;
                    timeZone = value;
                }
            } else if (argument == "-o" || argument == "--output") {
                outputPath = value;
            } else if ((argument == "-x" || argument == "--expansion")
                && !hl::format::parseExpansion(value, expansion)) {
                std::cerr << "hl: invalid expansion mode '" << value << "'\n";
                return 2;
            }
            continue;
        }
        if (!optionsEnded && (argument == "-h" || argument == "--hide")) {
            if (index + 1 >= argumentCount
                || !addVisibilityRules(arguments[++index], visibilityRules)) {
                std::cerr << "hl: " << argument << " needs a valid field list\n";
                return 2;
            }
            continue;
        }
        if (!optionsEnded && argument.substr(0, 7) == "--hide=") {
            if (!addVisibilityRules(argument.substr(7), visibilityRules)) {
                std::cerr << "hl: --hide needs a valid field list\n";
                return 2;
            }
            continue;
        }
        if (!optionsEnded && (argument == "-e" || argument == "--hide-empty-fields")) {
            hideEmptyFields = true;
            continue;
        }
        if (!optionsEnded && (argument == "-E" || argument == "--show-empty-fields")) {
            hideEmptyFields = false;
            continue;
        }
        if (!optionsEnded && argument == "-c") {
            colourMode = ColourMode::Always;
            continue;
        }
        if (!optionsEnded && argument == "-P") {
            pagingMode = PagingMode::Never;
            continue;
        }
        if (!optionsEnded && argument == "--paging") {
            pagingMode = PagingMode::Always;
            if (index + 1 < argumentCount) {
                PagingMode requestedMode;
                if (parsePagingMode(arguments[index + 1], requestedMode)) {
                    pagingMode = requestedMode;
                    ++index;
                }
            }
            continue;
        }
        if (!optionsEnded && argument.substr(0, 9) == "--paging=") {
            if (!parsePagingMode(argument.substr(9), pagingMode)) {
                std::cerr << "hl: invalid paging mode '" << argument.substr(9) << "'\n";
                return 2;
            }
            continue;
        }
        if (!optionsEnded && argument == "--color") {
            colourMode = ColourMode::Always;
            if (index + 1 < argumentCount) {
                ColourMode requestedMode;
                if (parseColourMode(arguments[index + 1], requestedMode)) {
                    colourMode = requestedMode;
                    ++index;
                }
            }
            continue;
        }
        if (!optionsEnded && argument.substr(0, 8) == "--color=") {
            if (!parseColourMode(argument.substr(8), colourMode)) {
                std::cerr << "hl: invalid colour mode '" << argument.substr(8) << "'\n";
                return 2;
            }
            continue;
        }
        if (!optionsEnded && (argument == "-l" || argument == "--level")) {
            if (index + 1 >= argumentCount) {
                std::cerr << "hl: " << argument << " needs a level\n";
                return 2;
            }
            const hl::model::Level level = hl::model::levelFromText(arguments[++index]);
            if (level == hl::model::Level::Unknown) {
                std::cerr << "hl: unknown level '" << arguments[index] << "'\n";
                return 2;
            }
            filter.setMinimumLevel(level);
            continue;
        }
        if (!optionsEnded && argument.substr(0, 8) == "--level=") {
            const hl::model::Level level = hl::model::levelFromText(argument.substr(8));
            if (level == hl::model::Level::Unknown) {
                std::cerr << "hl: unknown level '" << argument.substr(8) << "'\n";
                return 2;
            }
            filter.setMinimumLevel(level);
            continue;
        }
        if (!optionsEnded && (argument == "-f" || argument == "--filter")) {
            if (index + 1 >= argumentCount) {
                std::cerr << "hl: " << argument << " needs an expression\n";
                return 2;
            }
            hl::query::FieldFilter fieldFilter;
            std::string error;
            const std::string_view expression(arguments[++index]);
            if (!hl::query::parseFieldFilter(expression, fieldFilter, error)) {
                std::cerr << "hl: invalid field filter '" << expression << "': " << error << '\n';
                return 2;
            }
            filter.addFieldFilter(std::move(fieldFilter));
            continue;
        }
        if (!optionsEnded && (argument == "-q" || argument == "--query")) {
            if (index + 1 >= argumentCount) {
                std::cerr << "hl: " << argument << " needs an expression\n";
                return 2;
            }
            hl::query::Query query;
            std::string error;
            const std::string_view expression(arguments[++index]);
            if (!hl::query::compileQuery(expression, query, error)) {
                std::cerr << "hl: invalid query '" << expression << "': " << error << '\n';
                return 2;
            }
            filter.addQuery(std::move(query));
            continue;
        }
        if (!optionsEnded && argument.substr(0, 8) == "--query=") {
            hl::query::Query query;
            std::string error;
            if (!hl::query::compileQuery(argument.substr(8), query, error)) {
                std::cerr << "hl: invalid query '" << argument.substr(8) << "': " << error << '\n';
                return 2;
            }
            filter.addQuery(std::move(query));
            continue;
        }
        if (!optionsEnded && argument.substr(0, 9) == "--filter=") {
            hl::query::FieldFilter fieldFilter;
            std::string error;
            if (!hl::query::parseFieldFilter(argument.substr(9), fieldFilter, error)) {
                std::cerr << "hl: invalid field filter '" << argument.substr(9) << "': " << error << '\n';
                return 2;
            }
            filter.addFieldFilter(std::move(fieldFilter));
            continue;
        }
        if (!optionsEnded && (argument == "--since" || argument == "--until")) {
            if (index + 1 >= argumentCount) {
                std::cerr << "hl: " << argument << " needs a time\n";
                return 2;
            }
            const std::string_view value(arguments[++index]);
            const auto timestamp = hl::query::parseTimeArgument(value, nowNanoseconds);
            if (!timestamp.has_value()) {
                std::cerr << "hl: invalid time '" << value << "'\n";
                return 2;
            }
            if (argument == "--since") {
                filter.setSince(timestamp.value());
            } else {
                filter.setUntil(timestamp.value());
            }
            continue;
        }
        if (!optionsEnded && (argument.substr(0, 8) == "--since=" || argument.substr(0, 8) == "--until=")) {
            const bool since = argument.substr(0, 8) == "--since=";
            const std::string_view value = argument.substr(8);
            const auto timestamp = hl::query::parseTimeArgument(value, nowNanoseconds);
            if (!timestamp.has_value()) {
                std::cerr << "hl: invalid time '" << value << "'\n";
                return 2;
            }
            if (since) {
                filter.setSince(timestamp.value());
            } else {
                filter.setUntil(timestamp.value());
            }
            continue;
        }
        if (!optionsEnded && argument.size() > 1 && argument.front() == '-') {
            std::cerr << "hl: unknown option '" << argument << "'\n";
            return 2;
        }

        paths.emplace_back(argument);
    }

    if (localTime && timeZone != "local") {
        setenv("TZ", timeZone.c_str(), 1);
        tzset();
    }

    const hl::platform::TerminalInfo terminal = hl::platform::terminalInfo(STDOUT_FILENO);
    hl::pipeline::ProcessorOptions processorOptions;
    processorOptions.formatter.colour = colourMode == ColourMode::Always
        || (colourMode == ColourMode::Auto
            && outputPath.empty()
            && terminal.colourDepth != hl::platform::ColourDepth::None);
    processorOptions.formatter.hideEmptyFields = hideEmptyFields;
    processorOptions.formatter.localTime = localTime;
    processorOptions.formatter.ascii = ascii;
    processorOptions.formatter.expansion = expansion;
    processorOptions.formatter.timeFormat = std::move(timeFormat);
    processorOptions.formatter.visibilityRules = std::move(visibilityRules);
    processorOptions.filter = std::move(filter);
    processorOptions.raw = raw;
    processorOptions.sort = sort;
    processorOptions.allowPrefix = allowPrefix;
    processorOptions.inputFormat = inputFormat;
    processorOptions.inputDelimiter = inputDelimiter;
    processorOptions.outputDelimiter = std::move(outputDelimiter);
    processorOptions.replaceOutputDelimiter = replaceOutputDelimiter;
    processorOptions.bufferSize = bufferSize;
    processorOptions.maximumMessageSize = maximumMessageSize;
    processorOptions.concurrency = concurrency;

    std::ofstream outputFile;
    std::ostream* output = &std::cout;
    hl::io::Pager pager;
    if (!outputPath.empty() && outputPath != "-") {
        outputFile.open(outputPath, std::ios::binary);
        if (!outputFile) {
            std::cerr << "hl: could not open output '" << outputPath << "'\n";
            return 1;
        }
        output = &outputFile;
    } else {
        const bool usePager = pagingMode == PagingMode::Always
            || (pagingMode == PagingMode::Auto && terminal.isTerminal);
        if (usePager) {
            const auto pagerCommand = hl::platform::findPager();
            if (pagerCommand.has_value()) {
                std::string pagerError;
                if (pager.start(pagerCommand.value(), pagerError)) {
                    output = &pager.output();
                } else if (pagingMode == PagingMode::Always) {
                    std::cerr << "hl: could not start pager: " << pagerError << '\n';
                    return 1;
                }
            } else if (pagingMode == PagingMode::Always) {
                std::cerr << "hl: no pager was found\n";
                return 1;
            }
        }
    }

    const bool byteForByteRaw = raw && !sort && !processorOptions.filter.isActive()
        && inputDelimiter == hl::io::Delimiter::Auto && !replaceOutputDelimiter
        && inputFormat == hl::parse::InputFormat::Auto && !allowPrefix;
    const bool succeeded = byteForByteRaw
        ? hl::io::writeInputs(paths, std::cin, *output, std::cerr)
        : hl::pipeline::writeFormattedInputs(paths, std::cin, *output, std::cerr, processorOptions);
    const bool pagerSucceeded = pager.finish();
    return (succeeded || pager.closedEarly()) && pagerSucceeded ? 0 : 1;
}
//---------------------------------------------------------------------------------------------------------------------------------//
