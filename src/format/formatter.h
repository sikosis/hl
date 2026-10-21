#ifndef HL_FORMAT_FORMATTER_H
#define HL_FORMAT_FORMATTER_H

#include "model/record.h"

#include <string>
#include <vector>

namespace hl::format {

enum class Expansion {
    Never,
    Inline,
    Auto,
    Always,
};

struct FormatterOptions {
    bool colour = false;
    bool hideEmptyFields = false;
    bool localTime = false;
    bool ascii = false;
    Expansion expansion = Expansion::Never;
    std::string timeFormat = "%Y-%m-%d %T.%3N";
    std::vector<std::string> visibilityRules;
};

bool parseExpansion(std::string_view text, Expansion& expansion);

void formatRecord(const model::Record& record, std::string& output);
void formatRecord(const model::Record& record, const FormatterOptions& options, std::string& output);

} // namespace hl::format

#endif
