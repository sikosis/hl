#include "io/input.h"

#include <array>
#include <fstream>
#include <istream>
#include <ostream>

namespace hl::io {
namespace {

constexpr std::size_t kBufferSize = 256 * 1024;

bool writePath(
    const std::string& path,
    std::istream& standardInput,
    std::ostream& error,
    const StreamHandler& handler)
{
    if (path == "-") {
        if (!handler(standardInput)) {
            error << "hl: could not process stdin\n";
            return false;
        }
        return true;
    }

    std::ifstream input(path, std::ios::binary);
    if (!input) {
        error << "hl: could not open '" << path << "'\n";
        return false;
    }

    if (!handler(input)) {
        error << "hl: could not process '" << path << "'\n";
        return false;
    }
    return true;
}
//---------------------------------------------------------------------------------------------------------------------------------//

} // namespace

bool copyStream(std::istream& input, std::ostream& output)
{
    std::array<char, kBufferSize> buffer {};
    while (input) {
        input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const std::streamsize bytesRead = input.gcount();
        if (bytesRead > 0) {
            output.write(buffer.data(), bytesRead);
            if (!output) {
                return false;
            }
        }
    }

    return input.eof();
}
//---------------------------------------------------------------------------------------------------------------------------------//

bool visitInputs(
    const std::vector<std::string>& paths,
    std::istream& standardInput,
    std::ostream& error,
    const StreamHandler& handler)
{
    bool succeeded = true;
    if (paths.empty()) {
        return writePath("-", standardInput, error, handler);
    }

    for (const std::string& path : paths) {
        if (!writePath(path, standardInput, error, handler)) {
            succeeded = false;
        }
    }
    return succeeded;
}
//---------------------------------------------------------------------------------------------------------------------------------//

bool writeInputs(
    const std::vector<std::string>& paths,
    std::istream& standardInput,
    std::ostream& output,
    std::ostream& error)
{
    const StreamHandler copy = [&](std::istream& input) {
        return copyStream(input, output);
    };
    return visitInputs(paths, standardInput, error, copy);
}
//---------------------------------------------------------------------------------------------------------------------------------//

} // namespace hl::io
