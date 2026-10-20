#include "platform/platform.h"

#include <cstdlib>
#include <string_view>

#include <FindDirectory.h>
#include <Path.h>
#include <PathFinder.h>
#include <StringList.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace hl::platform {
namespace {

std::string environmentValue(const char* name)
{
    const char* value = std::getenv(name);
    return value == nullptr ? std::string() : std::string(value);
}
//---------------------------------------------------------------------------------------------------------------------------------//

std::string directoryPath(directory_which directory, std::string_view suffix)
{
    BPath path;
    if (find_directory(directory, &path) != B_OK) {
        return {};
    }

    if (path.Append(std::string(suffix).c_str()) != B_OK) {
        return {};
    }
    return path.Path();
}
//---------------------------------------------------------------------------------------------------------------------------------//

bool isExecutable(const std::string& path)
{
    return !path.empty() && access(path.c_str(), X_OK) == 0;
}
//---------------------------------------------------------------------------------------------------------------------------------//

std::optional<std::string> findExecutable(std::string_view name)
{
    if (name.find('/') != std::string_view::npos) {
        const std::string path(name);
        return isExecutable(path) ? std::optional<std::string>(path) : std::nullopt;
    }

    const std::string searchPath = environmentValue("PATH");
    std::size_t start = 0;
    while (start <= searchPath.size()) {
        const std::size_t end = searchPath.find(':', start);
        const std::string directory = searchPath.substr(start, end - start);
        const std::string candidate = (directory.empty() ? "." : directory) + "/" + std::string(name);
        if (isExecutable(candidate)) {
            return candidate;
        }
        if (end == std::string::npos) {
            break;
        }
        start = end + 1;
    }
    return std::nullopt;
}
//---------------------------------------------------------------------------------------------------------------------------------//

ColourDepth detectColourDepth(bool isTerminal)
{
    if (!isTerminal || environmentValue("TERM") == "dumb" || std::getenv("NO_COLOR") != nullptr) {
        return ColourDepth::None;
    }
    const std::string colourTerm = environmentValue("COLORTERM");
    if (colourTerm == "truecolor" || colourTerm == "24bit") {
        return ColourDepth::TrueColour;
    }
    return environmentValue("TERM").find("256color") != std::string::npos
        ? ColourDepth::Ansi256
        : ColourDepth::Ansi16;
}
//---------------------------------------------------------------------------------------------------------------------------------//

} // namespace

bool FileId::operator==(const FileId& other) const
{
    return device == other.device && node == other.node;
}
//---------------------------------------------------------------------------------------------------------------------------------//

std::string userConfigDir()
{
    return directoryPath(B_USER_SETTINGS_DIRECTORY, "hl");
}
//---------------------------------------------------------------------------------------------------------------------------------//

std::string systemConfigDir()
{
    return directoryPath(B_SYSTEM_SETTINGS_DIRECTORY, "hl");
}
//---------------------------------------------------------------------------------------------------------------------------------//

std::vector<std::string> dataDirs()
{
    BStringList paths;
    if (BPathFinder::FindPaths(B_FIND_PATH_DATA_DIRECTORY, "hl", B_FIND_PATH_EXISTING_ONLY, paths) != B_OK) {
        return {};
    }

    std::vector<std::string> result;
    result.reserve(paths.CountStrings());
    for (int32 index = 0; index < paths.CountStrings(); ++index) {
        result.emplace_back(paths.StringAt(index).String());
    }
    return result;
}
//---------------------------------------------------------------------------------------------------------------------------------//

std::string cacheDir()
{
    return directoryPath(B_USER_CACHE_DIRECTORY, "hl");
}
//---------------------------------------------------------------------------------------------------------------------------------//

TerminalInfo terminalInfo(int fileDescriptor)
{
    TerminalInfo result;
    result.isTerminal = isatty(fileDescriptor) != 0;
    winsize size {};
    if (result.isTerminal && ioctl(fileDescriptor, TIOCGWINSZ, &size) == 0 && size.ws_col > 0) {
        result.columns = size.ws_col;
    }
    result.colourDepth = detectColourDepth(result.isTerminal);
    return result;
}
//---------------------------------------------------------------------------------------------------------------------------------//

std::optional<std::string> findPager()
{
    for (const char* variable : {"HL_PAGER", "PAGER"}) {
        const std::string configured = environmentValue(variable);
        if (!configured.empty()) {
            return configured;
        }
    }
    return findExecutable("less");
}
//---------------------------------------------------------------------------------------------------------------------------------//

std::optional<FileId> fileId(const std::string& path)
{
    struct stat status {};
    if (stat(path.c_str(), &status) != 0) {
        return std::nullopt;
    }
    return FileId {static_cast<std::uint64_t>(status.st_dev), static_cast<std::uint64_t>(status.st_ino)};
}
//---------------------------------------------------------------------------------------------------------------------------------//

} // namespace hl::platform
