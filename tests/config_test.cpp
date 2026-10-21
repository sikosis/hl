#include "config/config.h"

#include <iostream>
#include <sstream>
#include <string>
#include <vector>

int main()
{
    std::istringstream input(
        "# settings\n"
        "[output]\n"
        "time-format = \"%H:%M:%S.%3N\"\n"
        "hide-empty-fields = true\n"
        "[input]\n"
        "allow-prefix = true # comment\n");
    std::vector<hl::config::Setting> settings;
    std::string error;
    if (!hl::config::load(input, "test", settings, error)) {
        std::cerr << error << '\n';
        return 1;
    }
    if (settings.size() != 3
        || settings[0].key != "output.time-format"
        || settings[0].value != "%H:%M:%S.%3N"
        || settings[1].key != "output.hide-empty-fields"
        || settings[1].value != "true"
        || settings[2].key != "input.allow-prefix") {
        std::cerr << "configuration settings were parsed incorrectly\n";
        return 1;
    }

    std::istringstream invalid("broken line\n");
    settings.clear();
    error.clear();
    if (hl::config::load(invalid, "invalid", settings, error) || error.empty()) {
        std::cerr << "invalid configuration was accepted\n";
        return 1;
    }
    return 0;
}
//---------------------------------------------------------------------------------------------------------------------------------//
