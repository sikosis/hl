#include "io/entry_splitter.h"

namespace hl::io {

EntrySplitter::EntrySplitter(Delimiter delimiter)
    : requestedDelimiter_(delimiter)
{
    if (delimiter != Delimiter::Auto) {
        activeDelimiter_ = delimiter;
    }
}
//---------------------------------------------------------------------------------------------------------------------------------//

bool EntrySplitter::push(std::string_view bytes, const EntryHandler& handler)
{
    pending_.append(bytes.data(), bytes.size());
    return emitAvailable(false, handler);
}
//---------------------------------------------------------------------------------------------------------------------------------//

bool EntrySplitter::finish(const EntryHandler& handler)
{
    return emitAvailable(true, handler);
}
//---------------------------------------------------------------------------------------------------------------------------------//

Delimiter EntrySplitter::delimiter() const
{
    return activeDelimiter_.value_or(requestedDelimiter_);
}
//---------------------------------------------------------------------------------------------------------------------------------//

bool EntrySplitter::detectDelimiter(bool finishing)
{
    for (std::size_t index = 0; index < pending_.size(); ++index) {
        switch (pending_[index]) {
            case '\n':
                activeDelimiter_ = Delimiter::LineFeed;
                return true;
            case '\0':
                activeDelimiter_ = Delimiter::Null;
                return true;
            case '\r':
                if (index + 1 == pending_.size() && !finishing) {
                    return false;
                }
                activeDelimiter_ = index + 1 < pending_.size() && pending_[index + 1] == '\n'
                    ? Delimiter::CarriageReturnLineFeed
                    : Delimiter::CarriageReturn;
                return true;
            default:
                break;
        }
    }
    return false;
}
//---------------------------------------------------------------------------------------------------------------------------------//

bool EntrySplitter::emitAvailable(bool finishing, const EntryHandler& handler)
{
    if (!activeDelimiter_.has_value() && !detectDelimiter(finishing)) {
        if (finishing && !pending_.empty()) {
            const bool handled = handler(pending_, {});
            pending_.clear();
            return handled;
        }
        return true;
    }

    const std::string_view separator = delimiterText();
    std::size_t consumed = 0;
    while (consumed < pending_.size()) {
        const std::size_t delimiterPosition = pending_.find(separator, consumed);
        if (delimiterPosition == std::string::npos) {
            break;
        }

        const std::string_view entry(pending_.data() + consumed, delimiterPosition - consumed);
        const std::string_view foundDelimiter(pending_.data() + delimiterPosition, separator.size());
        if (!handler(entry, foundDelimiter)) {
            return false;
        }
        consumed = delimiterPosition + separator.size();
    }

    if (consumed > 0) {
        pending_.erase(0, consumed);
    }
    if (finishing && !pending_.empty()) {
        const bool handled = handler(pending_, {});
        pending_.clear();
        return handled;
    }
    return true;
}
//---------------------------------------------------------------------------------------------------------------------------------//

std::string_view EntrySplitter::delimiterText() const
{
    switch (activeDelimiter_.value_or(requestedDelimiter_)) {
        case Delimiter::LineFeed:
            return "\n";
        case Delimiter::CarriageReturn:
            return "\r";
        case Delimiter::CarriageReturnLineFeed:
            return "\r\n";
        case Delimiter::Null:
            return std::string_view("\0", 1);
        case Delimiter::Auto:
            return {};
    }
    return {};
}
//---------------------------------------------------------------------------------------------------------------------------------//

} // namespace hl::io
