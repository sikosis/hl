#include "io/input.h"
#include "pipeline/processor.h"

#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#ifndef HL_VERSION
#define HL_VERSION "development"
#endif

namespace {

void printHelp(std::ostream& output)
{
    output << "Usage: hl [OPTIONS] [FILE]...\n"
           << "\n"
           << "A fast viewer for text, JSON and logfmt logs.\n"
           << "\n"
           << "Options:\n"
           << "  -r, --raw    Do not format structured entries\n"
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
        if (!optionsEnded && argument.size() > 1 && argument.front() == '-') {
            std::cerr << "hl: unknown option '" << argument << "'\n";
            return 2;
        }

        paths.emplace_back(argument);
    }

    const bool succeeded = raw
        ? hl::io::writeInputs(paths, std::cin, std::cout, std::cerr)
        : hl::pipeline::writeFormattedInputs(paths, std::cin, std::cout, std::cerr);
    return succeeded ? 0 : 1;
}
//---------------------------------------------------------------------------------------------------------------------------------//
