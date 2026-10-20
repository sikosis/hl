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
    std::ostream& output,
    std::ostream& error)
{
    if (path == "-") {
        if (!copyStream(standardInput, output)) {
            error << "hl: could not read stdin\n";
            return false;
        }
        return true;
    }

    std::ifstream input(path, std::ios::binary);
    if (!input) {
        error << "hl: could not open '" << path << "'\n";
        return false;
    }

    if (!copyStream(input, output)) {
        error << "hl: could not read '" << path << "'\n";
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

bool writeInputs(
    const std::vector<std::string>& paths,
    std::istream& standardInput,
    std::ostream& output,
    std::ostream& error)
{
    bool succeeded = true;
    if (paths.empty()) {
        return writePath("-", standardInput, output, error);
    }

    for (const std::string& path : paths) {
        if (!writePath(path, standardInput, output, error)) {
            succeeded = false;
        }
    }
    return succeeded;
}
//---------------------------------------------------------------------------------------------------------------------------------//

} // namespace hl::io
