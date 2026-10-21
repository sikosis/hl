#ifndef HL_FORMAT_FORMATTER_H
#define HL_FORMAT_FORMATTER_H

#include "model/record.h"

#include <string>
#include <vector>

namespace hl::format {

struct FormatterOptions {
    bool colour = false;
    bool hideEmptyFields = false;
    std::vector<std::string> visibilityRules;
};

void formatRecord(const model::Record& record, std::string& output);
void formatRecord(const model::Record& record, const FormatterOptions& options, std::string& output);

} // namespace hl::format

#endif
