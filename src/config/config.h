#ifndef HL_CONFIG_CONFIG_H
#define HL_CONFIG_CONFIG_H

#include <iosfwd>
#include <string>
#include <string_view>
#include <vector>

namespace hl::config {

struct Setting {
    std::string key;
    std::string value;
};

bool loadFile(const std::string& path, std::vector<Setting>& settings, std::string& error);
bool load(std::istream& input, std::string_view source, std::vector<Setting>& settings, std::string& error);

} // namespace hl::config

#endif
