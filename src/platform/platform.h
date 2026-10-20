#ifndef HL_PLATFORM_PLATFORM_H
#define HL_PLATFORM_PLATFORM_H

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace hl::platform {

enum class ColourDepth : std::uint8_t {
    None,
    Ansi16,
    Ansi256,
    TrueColour,
};

struct TerminalInfo {
    bool isTerminal = false;
    std::uint16_t columns = 80;
    ColourDepth colourDepth = ColourDepth::None;
};

struct FileId {
    std::uint64_t device = 0;
    std::uint64_t node = 0;

    bool operator==(const FileId& other) const;
};

std::string userConfigDir();
std::string systemConfigDir();
std::vector<std::string> dataDirs();
std::string cacheDir();
TerminalInfo terminalInfo(int fileDescriptor);
std::optional<std::string> findPager();
std::optional<FileId> fileId(const std::string& path);

} // namespace hl::platform

#endif
