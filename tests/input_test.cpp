#include "io/input.h"
#include "io/entry_splitter.h"

#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

bool expect(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << message << '\n';
    }
    return condition;
}
//---------------------------------------------------------------------------------------------------------------------------------//

} // namespace

int main()
{
    bool passed = true;

    std::istringstream plainInput("first line\nnot structured at all\n");
    std::ostringstream plainOutput;
    passed &= expect(
        hl::io::copyStream(plainInput, plainOutput),
        "plain-text stream copy failed");
    passed &= expect(
        plainOutput.str() == "first line\nnot structured at all\n",
        "plain-text stream changed during copy");

    std::string largeInput(300 * 1024, 'x');
    largeInput.push_back('\n');
    std::istringstream largeStream(largeInput);
    std::ostringstream largeOutput;
    passed &= expect(hl::io::copyStream(largeStream, largeOutput), "large stream copy failed");
    passed &= expect(largeOutput.str() == largeInput, "buffer boundary changed the stream");

    std::istringstream standardInput("stdin entry\n");
    std::ostringstream standardOutput;
    std::ostringstream standardError;
    passed &= expect(
        hl::io::writeInputs({}, standardInput, standardOutput, standardError),
        "implicit stdin read failed");
    passed &= expect(standardOutput.str() == "stdin entry\n", "implicit stdin output changed");
    passed &= expect(standardError.str().empty(), "implicit stdin wrote an error");

    std::istringstream missingInput;
    std::ostringstream missingOutput;
    std::ostringstream missingError;
    const std::vector<std::string> missingPaths {"/path/that/does/not/exist/hl.log"};
    passed &= expect(
        !hl::io::writeInputs(missingPaths, missingInput, missingOutput, missingError),
        "missing input unexpectedly succeeded");
    passed &= expect(!missingError.str().empty(), "missing input did not report an error");

    hl::io::EntrySplitter lineSplitter;
    std::vector<std::string> entries;
    std::vector<std::string> delimiters;
    const hl::io::EntryHandler collect = [&](std::string_view entry, std::string_view delimiter) {
        entries.emplace_back(entry);
        delimiters.emplace_back(delimiter);
        return true;
    };
    passed &= expect(lineSplitter.push("one\r", collect), "first split chunk failed");
    passed &= expect(lineSplitter.push("\ntwo\r\nthree", collect), "second split chunk failed");
    passed &= expect(lineSplitter.finish(collect), "splitter finish failed");
    passed &= expect(lineSplitter.delimiter() == hl::io::Delimiter::CarriageReturnLineFeed, "CRLF was not detected");
    passed &= expect(entries == std::vector<std::string>({"one", "two", "three"}), "CRLF entries were split incorrectly");
    passed &= expect(delimiters == std::vector<std::string>({"\r\n", "\r\n", ""}), "CRLF delimiters were not preserved");

    hl::io::EntrySplitter lineFeedSplitter;
    entries.clear();
    delimiters.clear();
    passed &= expect(lineFeedSplitter.push("one\ntwo\n", collect), "LF-delimited split failed");
    passed &= expect(lineFeedSplitter.finish(collect), "LF splitter finish failed");
    passed &= expect(lineFeedSplitter.delimiter() == hl::io::Delimiter::LineFeed, "LF delimiter was not detected");
    passed &= expect(entries == std::vector<std::string>({"one", "two"}), "LF entries were split incorrectly");

    hl::io::EntrySplitter carriageReturnSplitter;
    entries.clear();
    delimiters.clear();
    passed &= expect(carriageReturnSplitter.push("one\rtwo\r", collect), "CR-delimited split failed");
    passed &= expect(carriageReturnSplitter.finish(collect), "CR splitter finish failed");
    passed &= expect(carriageReturnSplitter.delimiter() == hl::io::Delimiter::CarriageReturn, "CR delimiter was not detected");
    passed &= expect(entries == std::vector<std::string>({"one", "two"}), "CR entries were split incorrectly");

    hl::io::EntrySplitter nullSplitter;
    entries.clear();
    delimiters.clear();
    const std::string nullEntries("alpha\0beta\0", 11);
    passed &= expect(nullSplitter.push(nullEntries, collect), "NUL-delimited split failed");
    passed &= expect(nullSplitter.finish(collect), "NUL splitter finish failed");
    passed &= expect(nullSplitter.delimiter() == hl::io::Delimiter::Null, "NUL delimiter was not detected");
    passed &= expect(entries == std::vector<std::string>({"alpha", "beta"}), "NUL entries were split incorrectly");

    hl::io::EntrySplitter rejectedSplitter;
    const hl::io::EntryHandler reject = [](std::string_view, std::string_view) {
        return false;
    };
    passed &= expect(!rejectedSplitter.push("entry\n", reject), "handler rejection was ignored");

    return passed ? 0 : 1;
}
//---------------------------------------------------------------------------------------------------------------------------------//
