#ifndef HL_QUERY_QUERY_H
#define HL_QUERY_QUERY_H

#include "model/record.h"

#include <memory>
#include <string>
#include <string_view>

namespace hl::query {

class Query {
public:
    struct Node;

    Query();
    explicit Query(std::shared_ptr<const Node> root);

    bool matches(const model::Record& record) const;
    bool empty() const;

private:
    std::shared_ptr<const Node> root_;
};

bool compileQuery(std::string_view expression, Query& query, std::string& error);

} // namespace hl::query

#endif
