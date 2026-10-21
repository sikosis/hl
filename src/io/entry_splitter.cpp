#include "io/entry_splitter.h"

namespace hl::io {

EntrySplitter::EntrySplitter(Delimiter delimiter, std::size_t maximumEntrySize)
    : requestedDelimiter_(delimiter)
    , maximumEntrySize_(maximumEntrySize)
{
    if (delimiter != Delimiter::Auto) {
        activeDelimiter_ = delimiter;
    }
}
//---------------------------------------------------------------------------------------------------------------------------------//

bool EntrySplitter::push(std::string_view bytes, const EntryHandler& handler)
{
    pending_.append(bytes.data(), bytes.size());
    if (!emitAvailable(false, handler)) {
        return false;
    }
    while (maximumEntrySize_ > 0 && pending_.size() > maximumEntrySize_) {
        if (!handler(std::string_view(pending_.data(), maximumEntrySize_), {})) {
            return false;
        }
        pending_.erase(0, maximumEntrySize_);
    }
    return true;
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
            const bool handled = emitEntry(pending_, {}, handler);
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
        if (!emitEntry(entry, foundDelimiter, handler)) {
            return false;
        }
        consumed = delimiterPosition + separator.size();
    }

    if (consumed > 0) {
        pending_.erase(0, consumed);
    }
    if (finishing && !pending_.empty()) {
        const bool handled = emitEntry(pending_, {}, handler);
        pending_.clear();
        return handled;
    }
    return true;
}
//---------------------------------------------------------------------------------------------------------------------------------//

bool EntrySplitter::emitEntry(
    std::string_view entry,
    std::string_view delimiter,
    const EntryHandler& handler) const
{
    if (maximumEntrySize_ == 0 || entry.size() <= maximumEntrySize_) {
        return handler(entry, delimiter);
    }
    std::size_t position = 0;
    while (entry.size() - position > maximumEntrySize_) {
        if (!handler(entry.substr(position, maximumEntrySize_), {})) {
            return false;
        }
        position += maximumEntrySize_;
    }
    return handler(entry.substr(position), delimiter);
}
//---------------------------------------------------------------------------------------------------------------------------------//

std::string_view EntrySplitter::delimiterText() const
{
    return io::delimiterText(activeDelimiter_.value_or(requestedDelimiter_));
}
//---------------------------------------------------------------------------------------------------------------------------------//

bool parseDelimiter(std::string_view text, Delimiter& delimiter)
{
    if (text == "auto") {
        delimiter = Delimiter::Auto;
        return true;
    }
    if (text == "newline" || text == "lf") {
        delimiter = Delimiter::LineFeed;
        return true;
    }
    if (text == "cr") {
        delimiter = Delimiter::CarriageReturn;
        return true;
    }
    if (text == "crlf") {
        delimiter = Delimiter::CarriageReturnLineFeed;
        return true;
    }
    if (text == "nul" || text == "null") {
        delimiter = Delimiter::Null;
        return true;
    }
    return false;
}
//---------------------------------------------------------------------------------------------------------------------------------//

std::string_view delimiterText(Delimiter delimiter)
{
    switch (delimiter) {
        case Delimiter::LineFeed:
            return "\n";
        case Delimiter::CarriageReturn:
            return "\r";
        case Delimiter::CarriageReturnLineFeed:
            return "\r\n";
        case Delimiter::Null:
            return std::string_view("\0", 1);
        case Delimiter::Auto:
            return "\n";
    }
    return "\n";
}
//---------------------------------------------------------------------------------------------------------------------------------//

} // namespace hl::io
