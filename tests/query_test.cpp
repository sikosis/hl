#include "parse/scanner.h"
#include "query/query.h"

#include <iostream>
#include <string>

namespace {

bool expectQuery(const hl::model::Record& record, std::string_view expression, bool expected)
{
    hl::query::Query query;
    std::string error;
    if (!hl::query::compileQuery(expression, query, error)) {
        std::cerr << "query did not compile: " << expression << ": " << error << '\n';
        return false;
    }
    const bool actual = query.matches(record);
    if (actual != expected) {
        std::cerr << "query result differs: " << expression << '\n';
        return false;
    }
    return true;
}
//---------------------------------------------------------------------------------------------------------------------------------//

} // namespace

int main()
{
    hl::model::Record record;
    if (hl::parse::scanEntry(
            "{\"level\":\"error\",\"message\":\"request failed\",\"status\":503,"
            "\"request\":{\"method\":\"POST\"},\"tags\":[\"api\",\"urgent\"]}",
            record) != hl::parse::ScanStatus::Structured) {
        std::cerr << "query record did not parse\n";
        return 1;
    }

    bool passed = true;
    passed &= expectQuery(record, "level = error and status >= 500", true);
    passed &= expectQuery(record, "level > warning", true);
    passed &= expectQuery(record, "request.method = GET or message ~= failed", true);
    passed &= expectQuery(record, "not exists(caller)", true);
    passed &= expectQuery(record, "status in (200, 503)", true);
    passed &= expectQuery(record, "status not in (500, 501)", true);
    passed &= expectQuery(record, "message like *failed", true);
    passed &= expectQuery(record, "message match \"request.*\"", true);
    passed &= expectQuery(record, "request.method = GET and status = 503", false);
    passed &= expectQuery(record, "missing? = value", true);

    hl::query::Query invalid;
    std::string error;
    passed &= !hl::query::compileQuery("status in ()", invalid, error);
    passed &= !error.empty();
    return passed ? 0 : 1;
}
//---------------------------------------------------------------------------------------------------------------------------------//
