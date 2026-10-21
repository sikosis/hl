#include "pipeline/processor.h"

#include "format/formatter.h"
#include "io/entry_splitter.h"
#include "io/input.h"
#include "model/record.h"
#include "parse/scanner.h"

#include <array>
#include <istream>
#include <ostream>

namespace hl::pipeline {
namespace {

constexpr std::size_t kBufferSize = 256 * 1024;

} // namespace

bool formatStream(std::istream& input, std::ostream& output, const ProcessorOptions& options)
{
    io::EntrySplitter splitter;
    model::Record record;
    std::string formatted;
    const io::EntryHandler handleEntry = [&](std::string_view entry, std::string_view delimiter) {
        const parse::ScanStatus status = parse::scanEntry(entry, record);
        if (status == parse::ScanStatus::Structured) {
            format::formatRecord(record, options.formatter, formatted);
            output.write(formatted.data(), static_cast<std::streamsize>(formatted.size()));
        } else {
            output.write(entry.data(), static_cast<std::streamsize>(entry.size()));
        }
        output.write(delimiter.data(), static_cast<std::streamsize>(delimiter.size()));
        return static_cast<bool>(output);
    };

    std::array<char, kBufferSize> buffer {};
    while (input) {
        input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const std::streamsize bytesRead = input.gcount();
        if (bytesRead > 0 && !splitter.push(
                std::string_view(buffer.data(), static_cast<std::size_t>(bytesRead)),
                handleEntry)) {
            return false;
        }
    }
    return input.eof() && splitter.finish(handleEntry);
}
//---------------------------------------------------------------------------------------------------------------------------------//

bool writeFormattedInputs(
    const std::vector<std::string>& paths,
    std::istream& standardInput,
    std::ostream& output,
    std::ostream& error,
    const ProcessorOptions& options)
{
    const io::StreamHandler format = [&](std::istream& input) {
        return formatStream(input, output, options);
    };
    return io::visitInputs(paths, standardInput, error, format);
}
//---------------------------------------------------------------------------------------------------------------------------------//

} // namespace hl::pipeline
