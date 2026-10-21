#ifndef HL_PIPELINE_PROCESSOR_H
#define HL_PIPELINE_PROCESSOR_H

#include <iosfwd>
#include <string>
#include <vector>

namespace hl::pipeline {

bool formatStream(std::istream& input, std::ostream& output);
bool writeFormattedInputs(
    const std::vector<std::string>& paths,
    std::istream& standardInput,
    std::ostream& output,
    std::ostream& error);

} // namespace hl::pipeline

#endif
