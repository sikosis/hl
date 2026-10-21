#ifndef HL_IO_INPUT_H
#define HL_IO_INPUT_H

#include <functional>
#include <iosfwd>
#include <string>
#include <vector>

namespace hl::io {

using StreamHandler = std::function<bool(std::istream& input)>;

bool copyStream(std::istream& input, std::ostream& output);
bool visitInputs(
    const std::vector<std::string>& paths,
    std::istream& standardInput,
    std::ostream& error,
    const StreamHandler& handler);
bool writeInputs(
    const std::vector<std::string>& paths,
    std::istream& standardInput,
    std::ostream& output,
    std::ostream& error);

} // namespace hl::io

#endif
