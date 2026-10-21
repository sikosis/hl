#include "io/input.h"
#include "pipeline/processor.h"
#include "platform/platform.h"
#include "query/filter.h"
#include "query/time_argument.h"

#include <chrono>
#include <cstdint>
#include <iostream>
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

void printHelp(std::ostream& output)
{
    output << "Usage: hl [OPTIONS] [FILE]...\n"
           << "\n"
           << "A fast viewer for text, JSON and logfmt logs.\n"
           << "\n"
           << "Options:\n"
           << "  -r, --raw    Do not format structured entries\n"
           << "  -l, --level LEVEL\n"
           << "               Keep entries at or above LEVEL\n"
           << "  -f, --filter FIELD[?](=|!=|~=|!~=)VALUE\n"
           << "               Add a field filter; ? keeps missing fields\n"
           << "  --since TIME Keep entries at or after TIME\n"
           << "  --until TIME Keep entries at or before TIME\n"
           << "  -h, --hide FIELDS\n"
           << "               Hide fields; use !FIELD to reveal and * for all\n"
           << "  -e, --hide-empty-fields\n"
           << "               Hide empty strings, objects, arrays and nulls\n"
           << "  -E, --show-empty-fields\n"
           << "               Show empty fields (default)\n"
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
    ColourMode colourMode = ColourMode::Auto;
    bool hideEmptyFields = false;
    std::vector<std::string> visibilityRules;
    hl::query::Filter filter;
    const std::int64_t nowNanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
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
        if (!optionsEnded && argument == "--help") {
            printHelp(std::cout);
            return 0;
        }
        if (!optionsEnded && (argument == "-r" || argument == "--raw")) {
            raw = true;
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

    hl::pipeline::ProcessorOptions processorOptions;
    processorOptions.formatter.colour = colourMode == ColourMode::Always
        || (colourMode == ColourMode::Auto
            && hl::platform::terminalInfo(STDOUT_FILENO).colourDepth != hl::platform::ColourDepth::None);
    processorOptions.formatter.hideEmptyFields = hideEmptyFields;
    processorOptions.formatter.visibilityRules = std::move(visibilityRules);
    processorOptions.filter = std::move(filter);
    processorOptions.raw = raw;

    const bool succeeded = raw && !processorOptions.filter.isActive()
        ? hl::io::writeInputs(paths, std::cin, std::cout, std::cerr)
        : hl::pipeline::writeFormattedInputs(paths, std::cin, std::cout, std::cerr, processorOptions);
    return succeeded ? 0 : 1;
}
//---------------------------------------------------------------------------------------------------------------------------------//
