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

    return passed ? 0 : 1;
}
//---------------------------------------------------------------------------------------------------------------------------------//
