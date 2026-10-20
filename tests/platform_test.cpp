#include "platform/platform.h"

#include <iostream>

#include <unistd.h>

int main()
{
    bool passed = true;

    if (hl::platform::systemConfigDir().empty()) {
        std::cerr << "system config directory is empty\n";
        passed = false;
    }
    (void)hl::platform::dataDirs();
    (void)hl::platform::findPager();

    const auto identity = hl::platform::fileId(".");
    if (!identity.has_value() || !(identity.value() == identity.value())) {
        std::cerr << "could not identify the current directory\n";
        passed = false;
    }

    const auto terminal = hl::platform::terminalInfo(STDOUT_FILENO);
    if (terminal.columns == 0) {
        std::cerr << "terminal width is zero\n";
        passed = false;
    }

    return passed ? 0 : 1;
}
//---------------------------------------------------------------------------------------------------------------------------------//
