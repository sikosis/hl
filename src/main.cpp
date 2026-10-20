#include <iostream>
#include <string_view>

#ifndef HL_VERSION
#define HL_VERSION "development"
#endif

namespace {

void printHelp(std::ostream& output)
{
    output << "Usage: hl [OPTIONS] [FILE]...\n"
           << "\n"
           << "A fast viewer for JSON and logfmt logs.\n"
           << "\n"
           << "Options:\n"
           << "  --help       Print help\n"
           << "  -V, --version\n"
           << "               Print version\n";
}
//---------------------------------------------------------------------------------------------------------------------------------//

} // namespace

int main(int argumentCount, char* arguments[])
{
    if (argumentCount == 2) {
        const std::string_view argument(arguments[1]);
        if (argument == "-V" || argument == "--version") {
            std::cout << "hl " << HL_VERSION << '\n';
            return 0;
        }
        if (argument == "--help") {
            printHelp(std::cout);
            return 0;
        }
    }

    if (argumentCount > 1) {
        std::cerr << "hl: log processing is not implemented yet\n";
        return 2;
    }

    printHelp(std::cout);
    return 0;
}
//---------------------------------------------------------------------------------------------------------------------------------//
