#include "parse/scanner.h"

#include <iostream>
#include <string>

namespace {

bool expect(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << message << '\n';
    }
    return condition;
}
//---------------------------------------------------------------------------------------------------------------------------------//

const hl::model::Field* findField(const hl::model::Record& record, std::string_view key)
{
    for (const hl::model::Field& field : record.fields) {
        if (field.key == key) {
            return &field;
        }
    }
    return nullptr;
}
//---------------------------------------------------------------------------------------------------------------------------------//

} // namespace

int main()
{
    bool passed = true;
    hl::model::Record record;

    const std::string json =
        "{\"level\":\"ERROR\",\"message\":\"hello\\nworld\",\"count\":12.5,"
        "\"ok\":true,\"nothing\":null,\"context\":{\"a\":[1,2]},\"items\":[1,\"x\"]}";
    passed &= expect(hl::parse::scanJson(json, record), "valid JSON was rejected");
    passed &= expect(record.source == json, "JSON source view changed");
    passed &= expect(record.format == hl::model::RecordFormat::Json, "JSON format was not recorded");
    passed &= expect(record.fields.size() == 7, "JSON field count is incorrect");
    passed &= expect(record.level == hl::model::Level::Error, "JSON level was not recognised");
    passed &= expect(record.message.defined, "JSON message was not recognised");
    passed &= expect(record.message.raw == "hello\\nworld", "JSON message raw value changed");
    passed &= expect(record.message.escaped, "JSON escaped string was not marked");

    const hl::model::Field* count = findField(record, "count");
    passed &= expect(count != nullptr, "JSON number field is missing");
    passed &= expect(count != nullptr && count->value.kind == hl::model::ValueKind::Number, "JSON number kind is incorrect");
    passed &= expect(count != nullptr && count->value.raw == "12.5", "JSON number raw value changed");

    const hl::model::Field* context = findField(record, "context");
    passed &= expect(context != nullptr && context->value.kind == hl::model::ValueKind::Object, "JSON object kind is incorrect");
    passed &= expect(context != nullptr && context->value.raw == "{\"a\":[1,2]}", "JSON object raw value changed");

    const hl::model::Field* items = findField(record, "items");
    passed &= expect(items != nullptr && items->value.kind == hl::model::ValueKind::Array, "JSON array kind is incorrect");

    passed &= expect(hl::parse::scanJson(" { } \t", record), "empty JSON object was rejected");
    passed &= expect(record.fields.empty(), "empty JSON object produced fields");
    passed &= expect(!hl::parse::scanJson("{\"value\":01}", record), "invalid JSON number was accepted");
    passed &= expect(record.fields.empty(), "failed JSON scan retained partial fields");
    passed &= expect(!hl::parse::scanJson("{\"nested\":{bad}}", record), "invalid nested JSON was accepted");
    passed &= expect(!hl::parse::scanJson("{\"value\":\"bad\\q\"}", record), "invalid JSON escape was accepted");

    const std::string logfmt = "level=warn message=\"disk full\" count=2.5 enabled empty= nil=null";
    passed &= expect(hl::parse::scanLogfmt(logfmt, record), "valid logfmt was rejected");
    passed &= expect(record.format == hl::model::RecordFormat::Logfmt, "logfmt format was not recorded");
    passed &= expect(record.fields.size() == 6, "logfmt field count is incorrect");
    passed &= expect(record.level == hl::model::Level::Warning, "logfmt level was not recognised");
    passed &= expect(record.message.raw == "disk full", "quoted logfmt value changed");

    const hl::model::Field* enabled = findField(record, "enabled");
    passed &= expect(enabled != nullptr && enabled->value.kind == hl::model::ValueKind::Bool, "bare logfmt key is not boolean");
    passed &= expect(enabled != nullptr && enabled->value.implicit, "bare logfmt key was not marked implicit");

    const hl::model::Field* empty = findField(record, "empty");
    passed &= expect(empty != nullptr && empty->value.defined, "empty logfmt value was not retained");
    passed &= expect(empty != nullptr && empty->value.raw.empty(), "empty logfmt value changed");

    passed &= expect(!hl::parse::scanLogfmt("key=\"unterminated", record), "unterminated logfmt quote was accepted");
    passed &= expect(record.fields.empty(), "failed logfmt scan retained partial fields");

    const std::string plain = "2026-10-20 service started normally";
    passed &= expect(
        hl::parse::scanEntry(plain, record) == hl::parse::ScanStatus::PlainText,
        "plain entry was not classified as plain text");
    passed &= expect(record.source == plain, "plain entry source changed");
    passed &= expect(record.format == hl::model::RecordFormat::PlainText, "plain entry format is incorrect");
    passed &= expect(record.fields.empty(), "plain entry produced fields");

    passed &= expect(
        hl::parse::scanEntry("{\"broken\":}", record) == hl::parse::ScanStatus::Malformed,
        "malformed JSON was not reported");
    passed &= expect(record.format == hl::model::RecordFormat::PlainText, "malformed JSON did not fall back to plain text");
    passed &= expect(record.source == "{\"broken\":}", "malformed JSON source changed");

    passed &= expect(
        hl::parse::scanEntry("key=\"unterminated", record) == hl::parse::ScanStatus::Malformed,
        "malformed logfmt was not reported");
    passed &= expect(record.format == hl::model::RecordFormat::PlainText, "malformed logfmt did not fall back to plain text");

    const std::string prefixed = "service: {\"message\":\"ready\"}";
    passed &= expect(
        hl::parse::scanEntry(prefixed, record, hl::parse::InputFormat::Auto, true)
            == hl::parse::ScanStatus::Structured,
        "prefixed JSON was not recognised when prefixes were allowed");
    passed &= expect(record.prefix == "service: ", "JSON prefix was not retained");
    passed &= expect(record.source == prefixed, "prefixed JSON source changed");

    passed &= expect(
        hl::parse::scanEntry("key=value", record, hl::parse::InputFormat::PlainText)
            == hl::parse::ScanStatus::PlainText,
        "forced text input was parsed as structured data");

    return passed ? 0 : 1;
}
//---------------------------------------------------------------------------------------------------------------------------------//
