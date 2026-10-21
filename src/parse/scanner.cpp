#include "parse/scanner.h"

#include "parse/format.h"
#include "parse/scanner_internal.h"
#include "parse/timestamp.h"

#include <initializer_list>

namespace hl::parse {
namespace {

bool isOneOf(std::string_view value, std::initializer_list<std::string_view> candidates)
{
    for (const std::string_view candidate : candidates) {
        if (value == candidate) {
            return true;
        }
    }
    return false;
}
//---------------------------------------------------------------------------------------------------------------------------------//

} // namespace

void populatePredefinedFields(model::Record& record)
{
    for (const model::Field& field : record.fields) {
        if (isOneOf(field.key, {"time", "ts", "timestamp", "@timestamp"})) {
            record.timestamp = parseTimestamp(field.value.raw);
        } else if (isOneOf(field.key, {"level", "lvl", "severity"})) {
            record.level = model::levelFromText(field.value.raw);
        } else if (isOneOf(field.key, {"message", "msg"})) {
            record.message = field.value;
        } else if (isOneOf(field.key, {"logger", "target"})) {
            record.logger = field.value;
        } else if (isOneOf(field.key, {"caller", "source"})) {
            record.caller = field.value;
        }
    }
}
//---------------------------------------------------------------------------------------------------------------------------------//

ScanStatus scanEntry(std::string_view entry, model::Record& record)
{
    return scanEntry(entry, record, InputFormat::Auto, false);
}
//---------------------------------------------------------------------------------------------------------------------------------//

ScanStatus scanEntry(
    std::string_view entry,
    model::Record& record,
    InputFormat inputFormat,
    bool allowPrefix)
{
    std::string_view structuredEntry = entry;
    std::string_view prefix;
    if (allowPrefix && inputFormat != InputFormat::PlainText) {
        const std::size_t objectStart = entry.find('{');
        if (objectStart != std::string_view::npos && objectStart > 0) {
            prefix = entry.substr(0, objectStart);
            structuredEntry = entry.substr(objectStart);
        }
    }

    const InputFormat detected = inputFormat == InputFormat::Auto
        ? detectFormat(structuredEntry)
        : inputFormat;
    switch (detected) {
        case InputFormat::Auto:
            break;
        case InputFormat::Json:
            if (scanJson(structuredEntry, record)) {
                record.source = entry;
                record.prefix = prefix;
                return ScanStatus::Structured;
            }
            record.reset(entry, model::RecordFormat::PlainText);
            return ScanStatus::Malformed;
        case InputFormat::Logfmt:
            if (scanLogfmt(structuredEntry, record)) {
                record.source = entry;
                record.prefix = prefix;
                return ScanStatus::Structured;
            }
            record.reset(entry, model::RecordFormat::PlainText);
            return ScanStatus::Malformed;
        case InputFormat::PlainText:
            record.reset(entry, model::RecordFormat::PlainText);
            return ScanStatus::PlainText;
    }
    record.reset(entry, model::RecordFormat::PlainText);
    return ScanStatus::PlainText;
}
//---------------------------------------------------------------------------------------------------------------------------------//

} // namespace hl::parse
