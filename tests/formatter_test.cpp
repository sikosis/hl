#include "format/formatter.h"
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

bool expectFormatted(std::string_view input, std::string_view expected)
{
    hl::model::Record record;
    if (hl::parse::scanEntry(input, record) != hl::parse::ScanStatus::Structured) {
        std::cerr << "structured test entry was not parsed: " << input << '\n';
        return false;
    }

    std::string output;
    hl::format::formatRecord(record, output);
    if (output == expected) {
        return true;
    }
    std::cerr << "expected: " << expected << "\nactual:   " << output << '\n';
    return false;
}
//---------------------------------------------------------------------------------------------------------------------------------//

} // namespace

int main()
{
    bool passed = true;
    passed &= expectFormatted(
        "{\"level\":\"error\",\"logger\":\"api\",\"message\":\"failed\\nrequest\","
        "\"status\":500,\"user\":\"Jane Doe\",\"ok\":false,\"caller\":\"main.cpp:8\"}",
        "[error] api: failed\\nrequest status=500 user=\"Jane Doe\" ok=false caller=main.cpp:8");
    passed &= expectFormatted(
        "{\"timestamp\":\"2026-10-20T12:34:56.123456789+10:00\",\"message\":\"ready\"}",
        "2026-10-20 02:34:56.123 ready");
    passed &= expectFormatted(
        "{\"time\":\"1969-12-31T23:59:59.500Z\",\"message\":\"before epoch\"}",
        "1969-12-31 23:59:59.500 before epoch");
    passed &= expectFormatted(
        "level=warn message=\"disk full\" path=/boot/home retry=3 enabled",
        "[warning] disk full path=/boot/home retry=3 enabled=true");
    passed &= expectFormatted(
        "message=ready pattern=\"a\\qb\"",
        "ready pattern=\"a\\\\qb\"");
    passed &= expectFormatted(
        "{\"message\":\"caf\\u00e9 \\ud83d\\ude00\"}",
        "caf\xc3\xa9 \xf0\x9f\x98\x80");
    passed &= expectFormatted(
        "{\"level\":\"notice\",\"message\":\"hello\"}",
        "hello level=notice");
    passed &= expectFormatted(
        "{\"message\":\"quoted field\",\"detail\":\"say \\\"hello\\\"\",\"path\":\"a\\\\b\"}",
        "quoted field detail=\"say \\\"hello\\\"\" path=\"a\\\\b\"");
    passed &= expectFormatted("{}", "{}");

    hl::model::Record plainRecord;
    const std::string plain = "ordinary text log";
    plainRecord.reset(plain, hl::model::RecordFormat::PlainText);
    std::string plainOutput;
    hl::format::formatRecord(plainRecord, plainOutput);
    passed &= expect(plainOutput == plain, "plain text changed during formatting");

    hl::model::Record colouredRecord;
    passed &= expect(
        hl::parse::scanEntry("{\"level\":\"error\",\"message\":\"failed\",\"count\":2}", colouredRecord)
            == hl::parse::ScanStatus::Structured,
        "coloured record did not parse");
    hl::format::FormatterOptions colourOptions;
    colourOptions.colour = true;
    std::string colouredOutput;
    hl::format::formatRecord(colouredRecord, colourOptions, colouredOutput);
    passed &= expect(
        colouredOutput
            == "\x1b[31m[error]\x1b[0m \x1b[37mfailed\x1b[0m \x1b[36mcount\x1b[0m=\x1b[34m2\x1b[0m",
        "coloured output does not match the built-in theme");

    return passed ? 0 : 1;
}
//---------------------------------------------------------------------------------------------------------------------------------//
