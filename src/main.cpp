#include "io/input.h"
#include "pipeline/processor.h"
#include "platform/platform.h"

#include <iostream>
#include <string>
#include <string_view>
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

void printHelp(std::ostream& output)
{
    output << "Usage: hl [OPTIONS] [FILE]...\n"
           << "\n"
           << "A fast viewer for text, JSON and logfmt logs.\n"
           << "\n"
           << "Options:\n"
           << "  -r, --raw    Do not format structured entries\n"
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

    const bool succeeded = raw
        ? hl::io::writeInputs(paths, std::cin, std::cout, std::cerr)
        : hl::pipeline::writeFormattedInputs(paths, std::cin, std::cout, std::cerr, processorOptions);
    return succeeded ? 0 : 1;
}
//---------------------------------------------------------------------------------------------------------------------------------//
