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
    switch (detectFormat(entry)) {
        case InputFormat::Json:
            if (scanJson(entry, record)) {
                return ScanStatus::Structured;
            }
            record.reset(entry, model::RecordFormat::PlainText);
            return ScanStatus::Malformed;
        case InputFormat::Logfmt:
            if (scanLogfmt(entry, record)) {
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
