#include "model/record.h"

namespace hl::model {

void Record::reset(std::string_view newSource, RecordFormat newFormat)
{
    source = newSource;
    prefix = {};
    format = newFormat;
    timestamp.reset();
    level = Level::Unknown;
    message = {};
    logger = {};
    caller = {};
    fields.clear();
}
//---------------------------------------------------------------------------------------------------------------------------------//

} // namespace hl::model
