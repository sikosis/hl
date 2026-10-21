#ifndef HL_PIPELINE_PROCESSOR_H
#define HL_PIPELINE_PROCESSOR_H

#include "format/formatter.h"
#include "io/entry_splitter.h"
#include "parse/format.h"
#include "query/filter.h"

#include <cstddef>
#include <iosfwd>
#include <string>
#include <vector>

namespace hl::pipeline {

struct ProcessorOptions {
    format::FormatterOptions formatter;
    query::Filter filter;
    bool raw = false;
    bool sort = false;
    bool allowPrefix = false;
    parse::InputFormat inputFormat = parse::InputFormat::Auto;
    io::Delimiter inputDelimiter = io::Delimiter::Auto;
    std::string outputDelimiter;
    bool replaceOutputDelimiter = false;
    std::size_t bufferSize = 256 * 1024;
    std::size_t maximumMessageSize = 64 * 1024 * 1024;
    std::size_t concurrency = 1;
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
