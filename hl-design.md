# hl for Haiku: C++17 Design Document

Oct 20, 2026

`hl` for Haiku is a C++17 log-file viewer inspired by the core of [pamburus/hl](https://github.com/pamburus/hl). It reads ordinary text logs and adds structured formatting and filtering when entries are JSON or logfmt. It is built as a portable core with a thin Haiku platform layer, so it can be developed and tested on Linux and shipped as a Haiku package.

## Goals and non-goals

v1 must read plain-text, JSON and logfmt logs from files, compressed files and stdin, and print them in a coloured, human-readable form fast enough that a multi-gigabyte log is not a wait. Plain-text entries always remain usable even when they have no recognised structure. The upstream project reports about 1.1 seconds for a 2.3 GiB, 6-million-line file on a 10-core Apple M1 Max; we use that as a reference point, not a promise.

**Goals**

- Behave like upstream `hl` for the options people use daily: level, field and query filters, time range, field hiding, time format and zone, themes, pager.
- Keep the command line, config keys and environment variables compatible with upstream where practical, so existing habits and config files carry over.
- Isolate everything Haiku-specific behind one small interface, so the same core builds and tests on Linux and macOS.
- Ship as a normal Haiku package (`hl.hpkg`) with a HaikuPorts recipe.

**Non-goals for v1**

- A graphical front end or Tracker integration.
- Windows support.
- Byte-for-byte identical output to upstream; matching behaviour matters, matching every escape sequence does not.
- BeOS-era `x86_gcc2` builds. The target is a modern GCC with full C++17 support.

## What we are porting

Upstream `hl` is a Rust tool (MIT licence) that converts JSON and logfmt logs to readable output. The table lists its features as described in its [README](https://github.com/pamburus/hl) and the phase in which each lands here (see Roadmap).

| Feature | Upstream option | Phase |
| --- | --- | --- |
| Concatenate several files, read stdin | `hl *.log`, `-` | MVP |
| Plain-text logs and unrecognised entries | automatic pass-through | MVP |
| JSON and logfmt input, auto-detected | `--input-format` | MVP |
| Level filter | `-l` | MVP |
| Field filter (`=`, `!=`, `~=`, `!~=`, `?` modifier, nested paths) | `-f` | MVP |
| Time range with relative shortcuts (`today`, `-3h`, `-14d`) | `--since`, `--until` | MVP |
| Hide or reveal fields, hide empty fields | `-h`, `-e`, `-E` | MVP |
| Time format and zone (UTC, local, named) | `-t`, `-Z`, `-L` | MVP (UTC, local); v1 (named zones) |
| ANSI colour and themes | `--color`, `-c`, `--theme` | MVP |
| Automatic pager, streaming mode | `--paging`, `-P` | MVP |
| Raw output and raw field values | `-r`, `--raw-fields` | MVP |
| Config file and environment variables | `--config`, `HL_*` | v1 |
| Field expansion (`never`, `inline`, `auto`, `always`) | `-x` | v1 |
| Compressed input (gzip, bzip2, xz, zstd) | automatic | v1 |
| Query language (`and`, `or`, `not`, `in`, `like`, `match`, `exists()`) | `-q` | v1 |
| Non-JSON prefix before entries | `--allow-prefix` | v1 |
| Chronological sort across files | `-s` | v1 (in memory); v2 (indexed) |
| Follow mode with `--tail` and sync interval | `-F`, `--tail`, `--sync-interval-ms` | v1 |
| Theme overlays, custom themes, `--list-themes` | `theme-overlays` | v1 |
| Shell completions and man page | `--shell-completions`, `--man-page` | v2 |
| Persistent sort index with block skipping | `--dump-index` | v2 |

## Architecture at a glance

```
 Settings: CLI arguments, HL_* variables, config files
      |
      | filters and theme
      v
 +--------+     +--------+     +------------------------------+
 | Inputs | --> | Reader | --> | Worker pool (N threads)      |
 | files, |     | 256KiB |     |  Scan --> Filter --> Format  |
 | stdin, |     | buffers|     +------------------------------+
 | pipes, |     | split  |                    |
 | gz bz2 |     | into   |                    | batches carry
 | xz zstd|     | entries|                    | sequence numbers
 +--------+     +--------+                    v
     ^                              +-----------------+
     | follow mode                  |  Ordered sink   |
     |                              |  input order, or|
     |      +---------------+  <--- |  by time (-s/-F)|
     |      |    Output     |       +-----------------+
     |      | pager, stdout |
     |      | or file       |
     |      +---------------+
     |             ^
 +---+-------------+---------------------------------------------+
 | Platform layer (the only code that touches Haiku APIs)         |
 |  [Terminal info] [File watcher] [Pager lookup] [Config dirs]   |
 +----------------------------------------------------------------+
```

Inputs are cut into entries, a worker pool scans, filters and formats them in parallel, and an ordered sink restores the order before output. Settings decide the filters and theme; the platform layer supplies paths, the pager, file watching and terminal facts, and is the only code with separate POSIX and Haiku versions.

## Module layout

The code is split into small libraries that only depend downwards. Everything under `platform/` is the only place allowed to include Haiku headers.

```
hl/
  CMakeLists.txt
  src/
    main.cpp               entry point, wiring
    cli/                   argument parsing, help, completions
    config/                config files, env vars, precedence
    io/                    inputs, mmap, decompression, pager, delimiter splitting
    model/                 Record, FieldPath, Level, Timestamp
    parse/                 JSON and logfmt scanners, format detection
    query/                 lexer, parser, AST, compiled predicate
    format/                formatter, themes, ANSI writer, time formatting
    pipeline/              scheduler, worker pool, ordered sink, merge sorter
    index/                 block index for sorted mode (v2)
    platform/              platform.h + posix/ and haiku/ backends
  data/                    default config.toml, themes/*.toml
  third_party/             vendored header-only libraries
  tests/                   unit/, golden/, fuzz/
  bench/                   benchmark harness
  packaging/               HaikuPorts recipe, man page
```

| Module | Responsibility | Depends on |
| --- | --- | --- |
| `model` | Plain data types shared by all stages | none |
| `parse` | Turn bytes of one entry into a `Record` view | `model` |
| `query` | Compile `-l`, `-f`, `-q`, `--since`, `--until` into one predicate | `model` |
| `format` | Render a `Record` to bytes with theme, time and field rules | `model` |
| `io` | Open inputs, split into entries, run the pager | `platform` |
| `pipeline` | Run parse, filter and format in parallel and keep order | `parse`, `query`, `format`, `io` |
| `config`, `cli` | Build one `Settings` object from files, environment and arguments | `platform` |
| `platform` | Config directories, file watching, terminal capabilities, pager lookup | OS only |

## Record model and parsers

A `Record` is a non-owning view over one log entry. Parsers never build a DOM: they scan the bytes once and record where each key and value sits, so parsing cost stays close to memory bandwidth and field order is preserved for output.

```cpp
namespace hl::model {

enum class ValueKind : uint8_t { String, Number, Bool, Null, Object, Array };

struct Value {
    ValueKind kind;
    bool escaped;             // string contains backslash escapes, decode lazily
    std::string_view raw;     // slice of the source buffer
};

struct Field { std::string_view key; Value value; };

struct Record {
    std::string_view source;          // the whole entry
    std::optional<Timestamp> ts;      // int64 nanoseconds since epoch + offset
    Level level = Level::Unknown;
    Value message, logger, caller;    // predefined fields, by configured key names
    std::vector<Field> fields;        // source order, reused per worker
};

} // namespace hl::model
```

Design rules:

- **Zero copy.** Views point into the input buffer. Strings with escapes are decoded only when a filter or the formatter needs the text, into a per-worker arena that is reset after each entry.
- **Lazy nesting.** Objects and arrays stay as raw slices. Paths such as `request.method` or `span.[1].name` descend into them on demand, which keeps flat logs cheap.
- **Structured scanners are optional enhancements.** `JsonScanner` handles one JSON object per entry; `LogfmtScanner` handles `key=value` pairs with quoted values, where a bare key means `true`. Format is detected from the entry contents unless `--input-format` forces it. Entries that do not match either structured format are plain text and pass through unchanged.
- **Prefix tolerance.** With `--allow-prefix` the JSON scanner skips bytes up to the first `{`; the skipped text is kept and printed before the formatted entry.
- **Predefined fields.** Time, level, message, logger and caller are looked up by lists of candidate key names (for example `ts`, `time`, `timestamp` for time). The lists live in the default config so users can extend them.
- **Timestamps.** A hand-written RFC 3339 parser covers the common case; numeric values are read as Unix time with the unit guessed from magnitude or forced by `--unix-timestamp-unit`.
- **Plain and malformed input.** An unrecognised entry or one that fails structured parsing is passed through unchanged rather than dropped. Forced structured input records parse failures for diagnostics; automatic mode treats unrecognised entries as normal text. Whether structured filters apply to plain entries is an open question (see Risks).

## Query and filter language

Every filter option compiles into one predicate tree: `-l error` becomes `level >= error`, `-f component=tsdb` becomes a comparison node, and `-q` is parsed from the full expression grammar below. The operator set follows upstream.

```
query      := or_expr
or_expr    := and_expr ( ("or" | "||") and_expr )*
and_expr   := not_expr ( ("and" | "&&") not_expr )*
not_expr   := ("not" | "!") not_expr | primary
primary    := "(" query ")" | exists | comparison
exists     := ("exists" | "exist") "(" field ")"
comparison := field ["?"] op value | field ["?"] ["not"] "in" set
set        := "(" value ("," value)* ")" | "@" filename | "@-"
field      := ["."] name ( "." name | ".[" [index] "]" )* | json_string
value      := bare_word | number | json_string
```

| Group | Operators |
| --- | --- |
| Logical | `and` `&&`, `or` `\|\|`, `not` `!` |
| Comparison | `=` `eq`, `!=` `ne`, `>` `gt`, `>=` `ge`, `<` `lt`, `<=` `le` |
| Substring | `~=` `contain`, `!~=` `not contain` |
| Wildcard | `like`, `not like` (`*` any run, `?` one character) |
| Regular expression | `~~=` `match`, `!~~=` `not match` |
| Sets | `in (a, b)`, `in @file`, `in @-`, and their `not in` forms |
| Presence | `exists(.field)` |

Semantics to preserve:

- A comparison on a missing field skips the entry; a `?` after the field name (`.price?=3`) keeps entries where the field is absent.
- `level`, `message`, `caller` and `logger` mean the predefined fields. A leading period (`.level`) addresses the source field with that name; a JSON string (`".level"`) addresses a field by exact name.
- Numbers compare numerically when both sides parse as numbers, levels compare by rank (`level > info`), everything else compares as text.
- Nested paths work on objects (`request.method`) and arrays (`span.[].name` for any element, `span.[1].name` for the second).

**Compilation.** The parser is a small recursive-descent parser over a hand-written lexer. The AST is lowered into final-class nodes with a single `bool eval(const Record&, Arena&)` method, short-circuiting `and` and `or`, with field paths, regular expressions and `in` sets prepared once at start-up. Time-range checks run before everything else because they only need the timestamp.

**Time arguments** for `--since` and `--until` accept RFC 3339, the current output time format, `today`, `yesterday`, weekday names and relative offsets such as `-3h` or `-14d`.

**Regular expressions** sit behind a `Regex` interface. The default backend is `std::regex` (no dependency, slow); PCRE2 or RE2 can replace it if they are available as Haiku packages. Pattern syntax differs from upstream's Rust engine, and the manual must say so.

## Output formatting and themes

The formatter turns a `Record` into one block of bytes: timestamp, level tag, logger, message, then remaining fields as `key=value`, then caller. Each part is styled by a theme *element*, and the layout and glyphs are configurable so output can be made plain ASCII with `--ascii`.

**Themes.** The theme model follows upstream's version 1.0 schema, which lets existing theme files be reused.

- A theme file has `version`, optional `tags` (`dark`, `light`, `16color`, `256color`, `truecolor`, plus `base` and `overlay`), and three tables: `styles` (reusable roles that can inherit from each other), `elements` (styles for parts of an entry such as `time`, `level`, `message`, `key`, `string`, `number`) and `levels` (per-level overrides).
- Resolution order: built-in `@base` theme, then the chosen theme, then each overlay in list order, then explicit properties on an element.
- Colours are `default`, the 16 named ANSI colours, a 0 to 255 palette index, or `#RRGGBB`. Modes are `bold`, `faint`, `italic`, `underline`, `slow-blink`, `rapid-blink`, `reverse`, `conceal` and `crossed-out`.
- Themes are compiled once into a flat table of ready-made escape sequences per element and level, so formatting an entry does no theme lookups.

**Colour capability.** `--color auto` enables colour only when stdout is a terminal and `TERM` is not `dumb`. Colour depth comes from the terminal: truecolor values are down-converted to the 256-colour palette and then to 16 colours when needed. Which depth Haiku's Terminal actually reports has to be checked on a real system (see Risks).

**Fields.** `--hide` takes keys, `!key` reveals, `*` and `!*` select all; `--hide-empty-fields` drops empty strings, objects, arrays and nulls; `--flatten` joins nested keys with a dot. `-x` selects expansion: `never` escapes newlines on one line, `inline` keeps them, `auto` expands only multi-line values, `always` expands every field onto its own line.

**Time.** A small formatter implements the `strftime` conversions people use (`%Y %m %d %H %M %S %T %y %b %e %Z %z`) plus fractional seconds as `%3N`, `%6N`, `%9N`; the default is `%Y-%m-%d %T.%3N`. UTC is the default zone; `-L` uses the system zone through `localtime_r`. Named zones (`-Z Europe/Berlin`) need a tz database and are the one place C++17 has no standard answer, so they use a pluggable `ZoneProvider`: UTC and local first, then either the Howard Hinnant `date` library with bundled tzdata or ICU, chosen after checking what Haiku ships.

**Text encoding.** Haiku's native encoding is UTF-8, so field values are passed through as UTF-8 and only control characters are escaped.

## Concurrency and I/O pipeline

The pipeline keeps all cores busy on parsing, filtering and formatting while output order stays identical to input order. It uses plain `std::thread` (pthreads on Haiku), with no dependency on a task library.

1. **Reader.** Fills fixed-size buffers (default 256 KiB, `--buffer-size`) from a memory-mapped file, a decompressor stream or a pipe. Regular uncompressed files are mapped with `mmap`; everything else uses `read`.
2. **Splitter.** Cuts each buffer at entry delimiters (`--delimiter`: newline, CR, CRLF, NUL or auto) and carries a partial last entry into the next buffer. An entry larger than `--max-message-size` (default 64 MiB) is cut and flagged instead of growing memory without bound.
3. **Workers.** `--concurrency` threads (default: `std::thread::hardware_concurrency()`) each take a batch of entries, then parse, filter and format it into a private output buffer.
4. **Ordered sink.** Batches carry a sequence number; the sink writes them in sequence, parking early arrivals. Output goes out in large `write` calls to stdout, the pager pipe or `--output`.

Back-pressure: the reader may run at most a fixed number of batches ahead of the sink (a small multiple of the worker count), so memory use is bounded no matter how slow the pager is.

Signals: `SIGPIPE` is ignored and `EPIPE` ends the run quietly, which is what happens when the user quits the pager. `SIGINT` is counted against `--interrupt-ignore-count` (default 3) so a first Ctrl-C reaches the pager rather than killing `hl`.

Allocation policy: workers reuse their `Record`, arena and output buffer between batches, so steady-state processing does no heap allocation.

## Sorting, indexing and follow mode

**Sorted mode (`-s`), v1.** For each input, the scanner extracts the timestamp of every entry that passes the time-range check and records `(timestamp, source, offset, length)`. The tuples are sorted with a stable sort and the entries are then re-read from the mapped files and formatted in order. An entry with no parseable timestamp inherits the previous entry's timestamp so multi-line records stay together. At 6 million entries the tuple table is a few hundred megabytes, which is acceptable for v1.

**Indexed mode, v2.** Each file is divided into blocks of a few MiB. For each block the index stores offset, minimum and maximum timestamp, a bitmask of levels present and a flag for "already sorted". Queries skip blocks whose range or levels cannot match. Index files live in the user cache directory (`~/config/cache/hl/` on Haiku), keyed by device, node number, size and modification time. A file that only grew is re-indexed from its last block onward. Upstream reports a first scan of about 2 GiB/s and re-indexing of up to roughly 10 GiB/s on its test machine; this design makes no speed claim until it is measured on Haiku.

**Follow mode (`-F`).** Inputs are files and pipes (so `hl -F <(cmd1) <(cmd2)` works). The coordinator works in windows of `--sync-interval-ms` (default 100):

1. At start, `--tail N` (default 10) preloads the last N entries of each file.
2. Reader threads push new entries into per-source queues.
3. Every interval the coordinator takes all entries older than the window edge, sorts them by timestamp and writes them out. The pager is off by default in this mode.

**Detecting file changes.** A `FileWatcher` interface hides the mechanism, with two implementations:

| Implementation | How it works | Use |
| --- | --- | --- |
| `PollingWatcher` | `stat` each file every sync interval and compare size, modification time and node number | Default everywhere, no extra libraries |
| `HaikuNodeWatcher` | `watch_node()` with `B_WATCH_STAT`; a `B_STAT_CHANGED` notification with the size field set means the file grew | Optional, enabled by a CMake flag |

Haiku's node monitor delivers notifications as messages to a `BMessenger` target, so using it means linking `libbe` and running a `BLooper` in the process. That is why it is an optional optimisation to cut idle wake-ups and not the default. The API also has a `B_WATCH_INTERIM_STAT` flag that lets a file system rate-limit notifications for frequent small writes, which suits a log being appended continuously. See the Haiku [node monitoring notes](https://www.haiku-os.org/docs/develop/file_systems/node_monitoring.html) and the [`NodeMonitor.h` reference](https://api.haiku-os.org/NodeMonitor_8h.html).

Both watchers must handle truncation (size smaller than the read offset: restart at 0) and rotation (a different node at the same path: finish the old file, then open the new one).

## Haiku platform layer

All operating-system differences sit behind `platform/platform.h`. The core calls these functions and never includes a Haiku header. Two backends exist: `posix/` (Linux and macOS, used for development and CI) and `haiku/`.

| Concern | Interface | POSIX backend | Haiku backend |
| --- | --- | --- | --- |
| User settings directory | `userConfigDir()` | `$XDG_CONFIG_HOME/hl` or `~/.config/hl` | `find_directory(B_USER_SETTINGS_DIRECTORY)` plus `hl/`, i.e. `~/config/settings/hl/` |
| System settings directory | `systemConfigDir()` | `/etc/hl` | `find_directory(B_SYSTEM_SETTINGS_DIRECTORY)` plus `hl/` |
| Packaged defaults (themes, default config) | `dataDirs()` | `/usr/share/hl` | `BPathFinder` with `B_FIND_PATH_DATA_DIRECTORY`, which searches user and system, non-packaged before packaged |
| Cache directory for indexes | `cacheDir()` | `$XDG_CACHE_HOME/hl` | `find_directory(B_USER_CACHE_DIRECTORY)` plus `hl/` |
| Terminal width and capabilities | `terminalInfo()` | `ioctl(TIOCGWINSZ)`, `TERM`, `COLORTERM` | same calls, with Haiku-specific defaults confirmed on a real system |
| Pager | `findPager()` | `HL_PAGER`, `PAGER`, then `less` | same; fall back to plain output if no pager is installed |
| File watching | `makeFileWatcher()` | polling | polling, or `watch_node()` when built with the node-monitor option |
| File identity | `fileId(path)` | `st_dev` and `st_ino` | `st_dev` and `st_ino` (Haiku maps these to its device and node numbers) |

**Why `find_directory` and `BPathFinder`.** Haiku's `find_directory()` constants give the user and system settings locations, and the [API reference](https://www.haiku-os.org/docs/api/FindDirectory_8h.html) recommends the C++ `BPathFinder` class for new code. Packaged files under `/boot/system` are read-only, so the design never writes there: defaults ship inside the package and are only read; user changes go to `~/config/settings/hl/`. A [forum thread](https://discuss.haiku-os.org/t/my-first-publicly-available-app-for-haiku/8633?page=2) from Haiku developers gives the same advice: find shipped data with `BPathFinder`, keep user settings in the user settings directory.

**Config lookup order on Haiku**, lowest to highest priority: packaged default, system settings (`/boot/system/settings/hl/config.toml`), user settings (`~/config/settings/hl/config.toml`), `HL_CONFIG`, then each `--config` option. This mirrors upstream's system-then-user-then-explicit order.

**Pager.** `hl` starts the pager with `fork` and `exec` on a pipe, sets `LESS` to include `-R` when the user has not set it (so colour passes through), and waits for it on exit. If the pager cannot be started, output goes straight to the terminal. When hum is installed it can serve as the default pager (see Optional integration: hum below).

**Threads and memory.** CMake's `Threads` package supplies whatever thread library flag the platform needs, `mmap` has a `read`-based fallback, and file offsets use 64-bit `off_t`.

**Package contents** (`hl-<version>-<arch>.hpkg`): `bin/hl`, `data/hl/config.toml`, `data/hl/themes/*.toml`, `documentation/man/man1/hl.1`, and a `.PackageInfo` that lists the runtime requirements (the compression libraries, if linked dynamically).

## Optional integration: hum

[hum](https://github.com/sikosis/hum) is a dependency-free C++17 terminal toolkit for Haiku (MIT licence) that offers subcommands such as `pager`, `filter`, `choose`, `style` and `version-check`. Its README documents a command-line interface and no library API, so `hl` treats it as an external program: it is never linked, and `hl` works identically without it.

**Rules for the integration**

- **Optional and lazy.** `hl` looks for `hum` only when a feature that can use it is about to run, so the normal formatting path pays nothing. Detection searches `PATH`, which includes `~/config/non-packaged/bin`, where hum's README says a hand-built binary is installed, then runs `hum version-check` against the minimum version `hl` was tested with.
- **Always a fallback.** Every use of hum has a built-in alternative, and the bytes `hl` writes do not depend on it.
- **User control.** `HL_HUM=auto|never` (default `auto`) and the config key `integrations.hum` turn it off; an explicit `HL_PAGER` or `PAGER` always wins over hum.
- **Colour etiquette.** hum disables colour when `NO_COLOR=1` is set; `hl --color auto` honours the same variable.

| Feature | With hum installed | Without hum | Phase |
| --- | --- | --- | --- |
| Paging | `hum pager` is the default pager when `HL_PAGER` and `PAGER` are unset and it passes ANSI colour through | `less`, then plain output | v1 |
| Theme picker (`--pick-theme`) | `hl --list-themes` piped to `hum filter`; the chosen name is printed so the user can put it in their config | Numbered list on the terminal | v2 |

Pager order becomes: `HL_PAGER`, `PAGER`, `hum pager` (only when hum is detected and `HL_HUM` is not `never`), `less`, plain output. The theme picker is the Haiku-native counterpart of upstream's documented fzf recipe; whether hum can show a live colour preview is not documented, so none is assumed.

## CLI, configuration and environment

Usage is `hl [OPTIONS] [FILE]...`; with no files, or with `-`, input comes from stdin. Option names, short flags, defaults and `HL_*` variables follow upstream's [`--help` output](https://github.com/pamburus/hl#complete-set-of-options-and-flags) so existing muscle memory and config files keep working.

| Group | Options | Environment variable | Default |
| --- | --- | --- | --- |
| Input | `--input-format auto\|text\|json\|logfmt` | `HL_INPUT_FORMAT` | `auto` |
| Input | `--delimiter`, `--allow-prefix`, `--unix-timestamp-unit` | `HL_DELIMITER`, `HL_ALLOW_PREFIX`, `HL_UNIX_TIMESTAMP_UNIT` | `auto`, off, `auto` |
| Filter | `-l/--level`, `--since`, `--until`, `-f/--filter`, `-q/--query` | `HL_LEVEL` | none |
| Output | `--color auto\|always\|never`, `-c`, `--theme` | `HL_COLOR`, `HL_THEME` | `auto`, `uni` |
| Output | `-t/--time-format`, `-Z/--time-zone`, `-L/--local` | `HL_TIME_FORMAT`, `HL_TIME_ZONE` | `%Y-%m-%d %T.%3N`, UTC |
| Output | `-h/--hide`, `-e`, `-E`, `--flatten`, `-x/--expansion`, `--ascii` | `HL_HIDE_EMPTY_FIELDS`, `HL_FLATTEN`, `HL_EXPANSION`, `HL_ASCII` | none, `always`, `auto`, `auto` |
| Output | `-r/--raw`, `--raw-fields`, `-o/--output`, `--output-delimiter` | `HL_OUTPUT_DELIMITER` | off, off, stdout, newline |
| Mode | `-s/--sort`, `-F/--follow`, `--tail`, `--sync-interval-ms` | none | off, off, 10, 100 |
| Paging | `--paging auto\|always\|never`, `-P` | `HL_PAGING`, `HL_PAGER`, `PAGER` | `auto` |
| Tuning | `-C/--concurrency`, `--buffer-size`, `--max-message-size` | `HL_CONCURRENCY`, `HL_BUFFER_SIZE`, `HL_MAX_MESSAGE_SIZE` | cores, 256 KiB, 64 MiB |
| Meta | `--config`, `--list-themes`, `--shell-completions`, `--man-page`, `--help`, `-V` | `HL_CONFIG` | none |
| Integration | `--pick-theme` (v2) | `HL_HUM` | off, auto |

**Precedence**, lowest to highest: built-in defaults, configuration files, environment variables, command-line arguments. Within configuration files, later files override only the keys they contain. `--config -` discards all default locations and starts over; `HL_CONFIG=-` does the same.

**Parsing library.** CLI11 (header-only, C++11 and later) handles the command line. Two upstream quirks need explicit handling: `-h` means `--hide`, so CLI11's automatic help flag is replaced by `--help`; and options such as `--color` and `-x` take an optional value, which maps to CLI11's `expected(0, 1)`. Repeatable options (`-f`, `-q`, `-h`, `--config`) are collected in order.

**Config format.** Config files and theme files are read with toml++ (header-only, C++17). JSON config reuses the same tree loader. YAML, which upstream also accepts, is deferred until a YAML library is confirmed available on Haiku.

**Exit status.** 0 on success, including when a pager is closed early; 1 for runtime errors such as an unreadable file (other inputs are still processed and the error is printed to stderr); 2 for usage errors.

## Build, dependencies and packaging

**Toolchain.** CMake 3.16 or later, `-std=c++17`, `-Wall -Wextra`, GCC or Clang. Haiku's repositories carried GCC 13.2.0 packages in late 2023 (reported in a [Swift-on-Haiku build page](https://www.github.com/return/swift-haiku-build)), which is well past what C++17 needs; confirm the compiler on the target Haiku release before the first build. The legacy 32-bit `x86_gcc2` toolchain is not a target.

**Dependencies.** A HaikuPorts recipe declares its requirements as packages and builds in a controlled environment, so the project keeps its dependency list short:

| Dependency | Purpose | How it is obtained |
| --- | --- | --- |
| CLI11 | Command-line parsing | Vendored in `third_party/` (header-only) |
| toml++ | Config and theme files | Vendored (header-only, C++17) |
| doctest | Unit tests | Vendored, test builds only |
| zlib, bzip2, liblzma, zstd | gzip, bzip2, xz and zstd input | System packages, each optional (`HL_WITH_ZLIB` and so on); package names to be confirmed in HaikuPorts |
| PCRE2 or RE2 | Faster regular expressions | Optional; `std::regex` is the fallback |
| `libbe` | Node monitor watcher | Only with `HL_USE_NODE_MONITOR=ON` |

**Recipe sketch** (HaikuPorts recipes are shell scripts with `BUILD()` and `INSTALL()` functions; see the [recipe reference](https://www.github.com/haikuports/haikuports/wiki/HaikuPorter-BuildRecipes) and check field names against it before submitting):

```sh
SUMMARY="Fast viewer for text and structured logs"
LICENSE="MIT"
ARCHITECTURES="all !x86_gcc2"
PROVIDES="
	hl = $portVersion
	cmd:hl = $portVersion
	"
REQUIRES="
	haiku
	"
BUILD_REQUIRES="
	haiku_devel
	"
BUILD_PREREQUIRES="
	cmd:cmake
	cmd:g++
	cmd:make
	"

BUILD()
{
	cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=$prefix
	cmake --build build -j$jobs
}

INSTALL()
{
	cmake --install build
}

TEST()
{
	ctest --test-dir build
}
```

**Local build and CI.** Day-to-day work happens on Linux with the POSIX backend. CI runs on every push: GCC and Clang builds, `ctest`, AddressSanitizer and UBSan runs, and a short fuzz job on the parsers. A second job builds and tests on a Haiku image (nightly or on tags) so Haiku-only code paths are not left untested; Haiku's [nightly images](https://download.haiku-os.org/) can be booted in a virtual machine for this.

**Licence.** MIT, matching upstream. If any upstream theme files or text are copied rather than rewritten, their copyright notice is kept in the repository and in the package.

## Testing and benchmarking

**Unit tests** (doctest) cover the pieces with sharp edges: scanners (escapes, Unicode, deep nesting, truncated input), timestamp parsing, level mapping, the time formatter, the query lexer and parser, predicate semantics (missing fields, the `?` modifier, numeric versus text comparison) and theme resolution with overlays.

**Golden tests** run the binary over a small corpus (JSON, logfmt, mixed, prefixed, multi-line values) with `--color never`, a fixed time format and `-Z UTC`, and compare against stored output. A second set keeps coloured output for the default theme.

**Differential tests** run the upstream `hl` binary and this one over the same corpus on Linux and compare which entries pass each filter, not the exact bytes. This catches query-semantics drift cheaply.

**Fuzzing.** libFuzzer targets for `JsonScanner`, `LogfmtScanner`, the query parser and the time-argument parser, run in CI on Linux with sanitizers.

**Pipeline tests** use tiny buffers so entries straddle buffer boundaries, random worker delays to prove output order, an oversized entry to exercise `--max-message-size`, and a closed pipe to exercise `EPIPE`.

**Follow-mode tests** append to, truncate and rotate a file under both watchers.

**Haiku-only tests**, run in the Haiku VM job: directory resolution through `find_directory` and `BPathFinder`, pager start-up, and the node-monitor watcher.

**Benchmarks.** `bench/` generates a synthetic corpus with the same shape as upstream's reference file (about 2.3 GiB, 6 million lines) and follows the method upstream uses: `time hl --config - FILE -c -o /dev/null`, with the measurement recipe in upstream [issue #132](https://github.com/pamburus/hl/issues/132). The targets below are proposals for this project, to be revised once Haiku numbers exist.

| Scenario | Command | Target |
| --- | --- | --- |
| Plain formatting | `hl FILE -c -o /dev/null` | MVP: within 3x of upstream on the same machine; v1: within 1.5x |
| Forced expansion | `hl FILE -c -x always -o /dev/null` | Within 25% of the plain case |
| Selective filter | `hl FILE -l e -o /dev/null` | Faster than plain formatting |
| Sorted mode | `hl -s FILE -o /dev/null` | Linear in entry count; memory under 100 bytes per passing entry |
| Streaming memory | `hl FILE -P -o /dev/null` | Resident memory independent of file size |

## Roadmap

Four phases, each ending in something that can be installed and used. A phase is done only when its exit criteria hold on a real Haiku system, not just on Linux.

| Phase | Scope | Exit criteria |
| --- | --- | --- |
| 0. Skeleton | CMake project, `platform/` interface with POSIX and Haiku backends, CI on Linux, Haiku VM job, vendored libraries | `hl --version` runs on Linux and in the Haiku VM; CI green on both |
| 1. MVP | Plain-text pass-through, JSON and logfmt scanners, level, field and time-range filters, field hiding, UTC and local time, default theme, colour, pager, multiple files and stdin, parallel pipeline | Golden tests pass for plain and structured logs; formats a 1 GiB log on Haiku with all cores busy; output order identical to input |
| 2. v1 | Query language, config files and `HL_*` variables, themes and overlays, field expansion, compressed input, `--allow-prefix`, named time zones, in-memory sort, follow mode with polling | Differential tests agree with upstream on the query corpus; `hl -F` merges two live logs in timestamp order; HaikuPorts recipe submitted |
| 3. v2 | Persistent block index for `-s`, node-monitor watcher, shell completions, man page, benchmark report | Re-sorting an unchanged large file is much faster than the first run; benchmark table published with Haiku numbers |

Within phase 1 the order of work is: streaming reader and plain-text pass-through, model and structured scanners, formatter and theme engine, filters, parallel pipeline, pager, then the full CLI. This establishes useful behaviour for every log file before adding format-specific features.

## Risks and open questions

These are the decisions that need a real Haiku system or a maintainer's call before the affected phase starts.

| # | Question or risk | Why it matters | Proposed answer | Needed by |
| --- | --- | --- | --- | --- |
| 1 | Which terminal colour depth does Haiku's Terminal report and support? | Decides the default colour palette and the truecolor down-conversion | Test on a Haiku VM; default to 256 colours if unclear | Phase 1 |
| 2 | Are zlib, bzip2, liblzma and zstd development packages available under the names the recipe needs? | Compressed input and the recipe's `BUILD_REQUIRES` | Check HaikuPorts; make each library optional | Phase 2 |
| 3 | Which regular-expression engine is available (PCRE2, RE2)? | `std::regex` is slow on large logs | Keep the `Regex` interface; start with `std::regex` | Phase 2 |
| 4 | Named time zones: Hinnant `date` with bundled tzdata, or ICU? | Only way to support `-Z Europe/Berlin` in C++17 | Prototype both; pick the smaller dependency | Phase 2 |
| 5 | Do filters apply to entries that fail to parse? | Changes what `-l error` hides when a file has plain-text lines | Compare with upstream's behaviour using the differential tests, then copy it | Phase 1 |
| 6 | Is `less` present on a default Haiku install, and does it pass colour sequences with `-R`? | Pager default | Verify in the VM; fall back to plain output | Phase 1 |
| 7 | Where should shell completion scripts be installed for Haiku's bash? | Phase 3 packaging | Ask on the Haiku developer forum | Phase 3 |
| 8 | Is the node-monitor watcher worth a `libbe` dependency? | Idle wake-ups in follow mode | Measure polling cost at 100 ms before deciding | Phase 3 |
| 9 | Upstream is under active development, so options and theme schema will change | Drift from the port | Pin to an upstream release for each phase and record it in the docs | Ongoing |
| 10 | Does hum pager read stdin, keep ANSI colours and cope with a multi-gigabyte stream? | Decides whether it can be the default pager | Test it; if it fails, keep less as the default and offer hum only through PAGER | Phase 2 |
| 11 | hum is a command-line tool with no documented API, so its subcommands may change | A change could break paging or the theme picker | Gate every use on hum version-check against a tested minimum version; fall back silently | Phase 2 |

**Performance risk.** Matching a Rust implementation that has been tuned over 1,700 commits is the largest technical risk. The mitigations are the zero-copy record model, the allocation-free steady state and early benchmarking in phase 1, so slow designs are found before the query language and indexing depend on them.

## Sources

Pages opened or searched on 2026-10-20. Upstream option names and defaults are as shown in its README at that date; Haiku details come from the pages below.

- [pamburus/hl README](https://github.com/pamburus/hl): features, options, config, themes, performance figures
- [sikosis/hum README](https://github.com/sikosis/hum): subcommands, install location, version-check, NO_COLOR
- [Haiku node monitoring notes](https://www.haiku-os.org/docs/develop/file_systems/node_monitoring.html) and [NodeMonitor.h reference](https://api.haiku-os.org/NodeMonitor_8h.html)
- [FindDirectory.h reference](https://www.haiku-os.org/docs/api/FindDirectory_8h.html)
- [Haiku forum: finding data and settings with BPathFinder](https://discuss.haiku-os.org/t/my-first-publicly-available-app-for-haiku/8633?page=2)
- [HaikuPorter build recipes](https://www.github.com/haikuports/haikuports/wiki/HaikuPorter-BuildRecipes)
- [Swift-on-Haiku build page](https://www.github.com/return/swift-haiku-build) (GCC 13.2.0 packages, late 2023)
