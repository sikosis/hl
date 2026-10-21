#include "model/level.h"

#include <cctype>

namespace hl::model {
namespace {

bool equalsIgnoringCase(std::string_view left, std::string_view right)
{
    if (left.size() != right.size()) {
        return false;
    }
    for (std::size_t index = 0; index < left.size(); ++index) {
        const unsigned char leftCharacter = static_cast<unsigned char>(left[index]);
        const unsigned char rightCharacter = static_cast<unsigned char>(right[index]);
        if (std::tolower(leftCharacter) != std::tolower(rightCharacter)) {
            return false;
        }
    }
    return true;
}
//---------------------------------------------------------------------------------------------------------------------------------//

} // namespace

Level levelFromText(std::string_view text)
{
    if (equalsIgnoringCase(text, "trace") || equalsIgnoringCase(text, "trc") || equalsIgnoringCase(text, "t")) {
        return Level::Trace;
    }
    if (equalsIgnoringCase(text, "debug") || equalsIgnoringCase(text, "dbg") || equalsIgnoringCase(text, "d")) {
        return Level::Debug;
    }
    if (equalsIgnoringCase(text, "info") || equalsIgnoringCase(text, "information") || equalsIgnoringCase(text, "i")) {
        return Level::Info;
    }
    if (equalsIgnoringCase(text, "warn") || equalsIgnoringCase(text, "warning") || equalsIgnoringCase(text, "w")) {
        return Level::Warning;
    }
    if (equalsIgnoringCase(text, "error") || equalsIgnoringCase(text, "err") || equalsIgnoringCase(text, "e")) {
        return Level::Error;
    }
    if (equalsIgnoringCase(text, "fatal") || equalsIgnoringCase(text, "critical") || equalsIgnoringCase(text, "crit")
        || equalsIgnoringCase(text, "f")) {
        return Level::Fatal;
    }
    return Level::Unknown;
}
//---------------------------------------------------------------------------------------------------------------------------------//

std::string_view levelName(Level level)
{
    switch (level) {
        case Level::Trace:
            return "trace";
        case Level::Debug:
            return "debug";
        case Level::Info:
            return "info";
        case Level::Warning:
            return "warning";
        case Level::Error:
            return "error";
        case Level::Fatal:
            return "fatal";
        case Level::Unknown:
            return "unknown";
    }
    return "unknown";
}
//---------------------------------------------------------------------------------------------------------------------------------//

} // namespace hl::model
