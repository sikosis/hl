#include "pipeline/processor.h"

#include <iostream>
#include <sstream>
#include <string>

namespace {

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
    return passed ? 0 : 1;
}
//---------------------------------------------------------------------------------------------------------------------------------//
