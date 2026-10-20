#include "io/input.h"

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

    return passed ? 0 : 1;
}
//---------------------------------------------------------------------------------------------------------------------------------//
