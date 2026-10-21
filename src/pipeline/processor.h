#ifndef HL_PIPELINE_PROCESSOR_H
#define HL_PIPELINE_PROCESSOR_H

#include "format/formatter.h"

#include <iosfwd>
#include <string>
#include <vector>

namespace hl::pipeline {

struct ProcessorOptions {
    format::FormatterOptions formatter;
};

bool formatStream(std::istream& input, std::ostream& output, const ProcessorOptions& options = {});
bool writeFormattedInputs(
    const std::vector<std::string>& paths,
    std::istream& standardInput,
    std::ostream& output,
    std::ostream& error,
    const ProcessorOptions& options = {});

} // namespace hl::pipeline

#endif
