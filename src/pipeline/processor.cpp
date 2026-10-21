#include "pipeline/processor.h"

#include "format/formatter.h"
#include "io/entry_splitter.h"
#include "io/input.h"
#include "model/record.h"
#include "parse/scanner.h"

#include <istream>
#include <algorithm>
#include <cstdint>
#include <future>
#include <deque>
#include <limits>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

namespace hl::pipeline {
namespace {

constexpr std::size_t kBatchSize = 256;

struct EntryData {
    std::string entry;
    std::string delimiter;
};

struct SortableEntry {
    EntryData data;
    std::int64_t timestamp = std::numeric_limits<std::int64_t>::min();
};

std::string formatBatch(std::vector<EntryData> entries, const ProcessorOptions& options)
{
    model::Record record;
    std::string formatted;
    std::string batchOutput;
    for (const EntryData& data : entries) {
        const parse::ScanStatus status = parse::scanEntry(
            data.entry, record, options.inputFormat, options.allowPrefix);
        if (!options.filter.matches(record)) {
            continue;
        }
        if (options.raw || status != parse::ScanStatus::Structured) {
            batchOutput += data.entry;
        } else {
            format::formatRecord(record, options.formatter, formatted);
            batchOutput += formatted;
        }
        batchOutput += options.replaceOutputDelimiter ? options.outputDelimiter : data.delimiter;
    }
    return batchOutput;
}
//---------------------------------------------------------------------------------------------------------------------------------//

bool writeCompleted(
    std::deque<std::future<std::string>>& work,
    std::ostream& output)
{
    if (work.empty()) {
        return true;
    }
    const std::string completed = work.front().get();
    work.pop_front();
    output.write(completed.data(), static_cast<std::streamsize>(completed.size()));
    return static_cast<bool>(output);
}
//---------------------------------------------------------------------------------------------------------------------------------//

bool collectSortedStream(
    std::istream& input,
    const ProcessorOptions& options,
    std::vector<SortableEntry>& entries)
{
    io::EntrySplitter splitter(options.inputDelimiter, options.maximumMessageSize);
    std::int64_t previousTimestamp = std::numeric_limits<std::int64_t>::min();
    const io::EntryHandler collect = [&](std::string_view entry, std::string_view delimiter) {
        model::Record record;
        parse::scanEntry(entry, record, options.inputFormat, options.allowPrefix);
        if (record.timestamp.has_value()) {
            previousTimestamp = record.timestamp->nanoseconds;
        }
        entries.push_back(SortableEntry {
            EntryData {std::string(entry), std::string(delimiter)}, previousTimestamp});
        return true;
    };

    std::vector<char> buffer(options.bufferSize == 0 ? 1 : options.bufferSize);
    while (input) {
        input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const std::streamsize bytesRead = input.gcount();
        if (bytesRead > 0 && !splitter.push(
                std::string_view(buffer.data(), static_cast<std::size_t>(bytesRead)), collect)) {
            return false;
        }
    }
    return input.eof() && splitter.finish(collect);
}
//---------------------------------------------------------------------------------------------------------------------------------//

bool writeSortedEntries(
    std::vector<SortableEntry>& entries,
    std::ostream& output,
    const ProcessorOptions& options)
{
    std::stable_sort(entries.begin(), entries.end(), [](const SortableEntry& left, const SortableEntry& right) {
        return left.timestamp < right.timestamp;
    });
    std::vector<EntryData> batch;
    batch.reserve(kBatchSize);
    for (SortableEntry& entry : entries) {
        batch.push_back(std::move(entry.data));
        if (batch.size() == kBatchSize) {
            const std::string formatted = formatBatch(std::move(batch), options);
            output.write(formatted.data(), static_cast<std::streamsize>(formatted.size()));
            if (!output) return false;
            batch.clear();
            batch.reserve(kBatchSize);
        }
    }
    if (!batch.empty()) {
        const std::string formatted = formatBatch(std::move(batch), options);
        output.write(formatted.data(), static_cast<std::streamsize>(formatted.size()));
    }
    return static_cast<bool>(output);
}
//---------------------------------------------------------------------------------------------------------------------------------//

} // namespace

bool formatStream(std::istream& input, std::ostream& output, const ProcessorOptions& options)
{
    if (options.sort) {
        std::vector<SortableEntry> sortedEntries;
        return collectSortedStream(input, options, sortedEntries)
            && writeSortedEntries(sortedEntries, output, options);
    }
    io::EntrySplitter splitter(options.inputDelimiter, options.maximumMessageSize);
    std::vector<EntryData> entries;
    entries.reserve(kBatchSize);
    std::deque<std::future<std::string>> work;
    const std::size_t concurrency = options.concurrency == 0 ? 1 : options.concurrency;

    const auto submit = [&]() {
        if (entries.empty()) {
            return true;
        }
        if (concurrency == 1) {
            const std::string completed = formatBatch(std::move(entries), options);
            entries.clear();
            entries.reserve(kBatchSize);
            output.write(completed.data(), static_cast<std::streamsize>(completed.size()));
            return static_cast<bool>(output);
        }
        work.push_back(std::async(
            std::launch::async,
            formatBatch,
            std::move(entries),
            std::cref(options)));
        entries.clear();
        entries.reserve(kBatchSize);
        return work.size() < concurrency || writeCompleted(work, output);
    };
    const io::EntryHandler handleEntry = [&](std::string_view entry, std::string_view delimiter) {
        entries.push_back(EntryData {std::string(entry), std::string(delimiter)});
        return entries.size() < kBatchSize || submit();
    };

    std::vector<char> buffer(options.bufferSize == 0 ? 1 : options.bufferSize);
    while (input) {
        input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const std::streamsize bytesRead = input.gcount();
        if (bytesRead > 0 && !splitter.push(
                std::string_view(buffer.data(), static_cast<std::size_t>(bytesRead)),
                handleEntry)) {
            return false;
        }
    }
    if (!input.eof() || !splitter.finish(handleEntry) || !submit()) {
        return false;
    }
    while (!work.empty()) {
        if (!writeCompleted(work, output)) {
            return false;
        }
    }
    return true;
}
//---------------------------------------------------------------------------------------------------------------------------------//

bool writeFormattedInputs(
    const std::vector<std::string>& paths,
    std::istream& standardInput,
    std::ostream& output,
    std::ostream& error,
    const ProcessorOptions& options)
{
    if (options.sort) {
        std::vector<SortableEntry> entries;
        const io::StreamHandler collect = [&](std::istream& input) {
            return collectSortedStream(input, options, entries);
        };
        return io::visitInputs(paths, standardInput, error, collect)
            && writeSortedEntries(entries, output, options);
    }
    const io::StreamHandler format = [&](std::istream& input) {
        return formatStream(input, output, options);
    };
    return io::visitInputs(paths, standardInput, error, format);
}
//---------------------------------------------------------------------------------------------------------------------------------//

} // namespace hl::pipeline
