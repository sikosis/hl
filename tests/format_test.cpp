#include "parse/format.h"

#include <iostream>

namespace {

bool expectFormat(std::string_view entry, hl::parse::InputFormat expected)
{
    const hl::parse::InputFormat actual = hl::parse::detectFormat(entry);
    if (actual == expected) {
        return true;
    }

    std::cerr << "expected " << hl::parse::formatName(expected)
              << " but detected " << hl::parse::formatName(actual)
              << " for '" << entry << "'\n";
    return false;
}
//---------------------------------------------------------------------------------------------------------------------------------//

} // namespace

int main()
{
    bool passed = true;
    passed &= expectFormat("a normal log line", hl::parse::InputFormat::PlainText);
    passed &= expectFormat("2026-10-20 12:30:00 service started", hl::parse::InputFormat::PlainText);
    passed &= expectFormat("", hl::parse::InputFormat::PlainText);
    passed &= expectFormat("   \t", hl::parse::InputFormat::PlainText);
    passed &= expectFormat("  {\"level\":\"info\"}", hl::parse::InputFormat::Json);
    passed &= expectFormat("level=info message=started", hl::parse::InputFormat::Logfmt);
    passed &= expectFormat("time=12:30 plain words", hl::parse::InputFormat::Logfmt);
    passed &= expectFormat("value = separated", hl::parse::InputFormat::PlainText);
    return passed ? 0 : 1;
}
//---------------------------------------------------------------------------------------------------------------------------------//
