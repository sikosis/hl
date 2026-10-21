#ifndef HL_FORMAT_FORMATTER_H
#define HL_FORMAT_FORMATTER_H

#include "model/record.h"

#include <string>

namespace hl::format {

struct FormatterOptions {
    bool colour = false;
};

void formatRecord(const model::Record& record, std::string& output);
void formatRecord(const model::Record& record, const FormatterOptions& options, std::string& output);

} // namespace hl::format

#endif
