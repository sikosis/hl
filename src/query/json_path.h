#ifndef HL_QUERY_JSON_PATH_H
#define HL_QUERY_JSON_PATH_H

#include "model/record.h"

#include <string_view>
#include <vector>

namespace hl::query {

void resolveNestedField(
    const model::Record& record,
    std::string_view path,
    std::vector<std::string_view>& values);

} // namespace hl::query

#endif
