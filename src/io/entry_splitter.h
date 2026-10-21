#ifndef HL_IO_ENTRY_SPLITTER_H
#define HL_IO_ENTRY_SPLITTER_H

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace hl::io {

enum class Delimiter : std::uint8_t {
    Auto,
    LineFeed,
    CarriageReturn,
    CarriageReturnLineFeed,
    Null,
};

using EntryHandler = std::function<bool(std::string_view entry, std::string_view delimiter)>;

class EntrySplitter {
public:
    explicit EntrySplitter(
        Delimiter delimiter = Delimiter::Auto,
        std::size_t maximumEntrySize = 64 * 1024 * 1024);

    bool push(std::string_view bytes, const EntryHandler& handler);
    bool finish(const EntryHandler& handler);
    Delimiter delimiter() const;

private:
    bool detectDelimiter(bool finishing);
    bool emitAvailable(bool finishing, const EntryHandler& handler);
    bool emitEntry(std::string_view entry, std::string_view delimiter, const EntryHandler& handler) const;
    std::string_view delimiterText() const;

    Delimiter requestedDelimiter_;
    std::optional<Delimiter> activeDelimiter_;
    std::string pending_;
    std::size_t maximumEntrySize_;
};

bool parseDelimiter(std::string_view text, Delimiter& delimiter);
std::string_view delimiterText(Delimiter delimiter);

} // namespace hl::io

#endif
