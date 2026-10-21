#include "pipeline/processor.h"

#include <iostream>
#include <sstream>
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

bool expectStream(std::string_view input, std::string_view expected)
{
    std::istringstream source {std::string(input)};
    std::ostringstream output;
    if (!hl::pipeline::formatStream(source, output)) {
        std::cerr << "pipeline rejected input\n";
        return false;
    }
    if (output.str() == expected) {
        return true;
    }
    std::cerr << "pipeline output differs\nexpected: " << expected
              << "\nactual:   " << output.str() << '\n';
    return false;
}
//---------------------------------------------------------------------------------------------------------------------------------//

} // namespace

int main()
{
    bool passed = true;
    passed &= expectStream(
        "ordinary log\n{\"level\":\"info\",\"message\":\"ready\",\"port\":80}\n"
        "level=error message=failed code=5\n{\"broken\":}\n",
        "ordinary log\n[info] ready port=80\n[error] failed code=5\n{\"broken\":}\n");
    passed &= expectStream("{\"message\":\"last\"}", "last");

    const std::string nullInput("message=one\0plain text\0", 23);
    const std::string nullExpected("one\0plain text\0", 15);
    passed &= expectStream(nullInput, nullExpected);

    std::istringstream filteredSource(
        "level=info message=starting component=api\n"
        "level=error message=failed component=worker\n"
        "ordinary text\n");
    std::ostringstream filteredOutput;
    hl::pipeline::ProcessorOptions filteredOptions;
    filteredOptions.filter.setMinimumLevel(hl::model::Level::Warning);
    passed &= hl::pipeline::formatStream(filteredSource, filteredOutput, filteredOptions);
    passed &= filteredOutput.str() == "[error] failed component=worker\n";

    std::string manyEntries;
    std::string expectedEntries;
    for (int index = 0; index < 1000; ++index) {
        manyEntries += "message=item-" + std::to_string(index) + "\n";
        expectedEntries += "item-" + std::to_string(index) + "\r\n";
    }
    std::istringstream parallelSource(manyEntries);
    std::ostringstream parallelOutput;
    hl::pipeline::ProcessorOptions parallelOptions;
    parallelOptions.concurrency = 4;
    parallelOptions.bufferSize = 17;
    parallelOptions.outputDelimiter = "\r\n";
    parallelOptions.replaceOutputDelimiter = true;
    passed &= hl::pipeline::formatStream(parallelSource, parallelOutput, parallelOptions);
    passed &= expect(
        parallelOutput.str() == expectedEntries,
        "parallel pipeline did not preserve input order or output delimiters");

    std::istringstream forcedTextSource("message=ready\n");
    std::ostringstream forcedTextOutput;
    hl::pipeline::ProcessorOptions forcedTextOptions;
    forcedTextOptions.inputFormat = hl::parse::InputFormat::PlainText;
    passed &= hl::pipeline::formatStream(forcedTextSource, forcedTextOutput, forcedTextOptions);
    passed &= expect(
        forcedTextOutput.str() == "message=ready\n",
        "forced text input was reformatted");

    std::istringstream sortedSource(
        "{\"timestamp\":\"2026-10-20T12:00:02Z\",\"message\":\"third\"}\n"
        "{\"timestamp\":\"2026-10-20T12:00:00Z\",\"message\":\"first\"}\n"
        "continuation\n"
        "{\"timestamp\":\"2026-10-20T12:00:01Z\",\"message\":\"second\"}\n");
    std::ostringstream sortedOutput;
    hl::pipeline::ProcessorOptions sortedOptions;
    sortedOptions.sort = true;
    sortedOptions.formatter.visibilityRules = {"time"};
    passed &= hl::pipeline::formatStream(sortedSource, sortedOutput, sortedOptions);
    passed &= expect(
        sortedOutput.str() == "first\ncontinuation\nsecond\nthird\n",
        "chronological sorting or untimed-entry grouping was incorrect");
    return passed ? 0 : 1;
}
//---------------------------------------------------------------------------------------------------------------------------------//
