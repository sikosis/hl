#ifndef HL_IO_INPUT_H
#define HL_IO_INPUT_H

#include <iosfwd>
#include <string>
#include <vector>

namespace hl::io {

bool copyStream(std::istream& input, std::ostream& output);
bool writeInputs(
    const std::vector<std::string>& paths,
    std::istream& standardInput,
    std::ostream& output,
    std::ostream& error);

} // namespace hl::io

#endif
