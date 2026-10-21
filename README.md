# hl

`hl` is a fast log-file viewer for Haiku, implemented in C++17 and inspired by
[pamburus/hl](https://github.com/pamburus/hl). It reads ordinary text logs as
well as structured JSON and logfmt logs. Structured formats enable richer field
formatting and filtering, but they are not required. The core remains portable
across Haiku, Linux and macOS.

The project is in early development. It currently streams ordinary logs from
files or stdin without changing them. Entry splitting supports LF, CRLF, CR and
NUL-delimited logs, and automatic text, JSON and logfmt classification is in
place. JSON and logfmt fields are extracted into non-owning record views;
structured entries are rendered into compact readable lines while plain text is
left unchanged. Filtering and coloured themes are not implemented yet.

## Build

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

Run it with a file, several files, or stdin:

```sh
build/hl system.log
build/hl application.log server.log
tail -f system.log | build/hl
```

Use `--raw` (or `-r`) to copy every input byte without structured formatting.

`build/hl --help` and `build/hl --version` provide command information.

See [hl-design.md](hl-design.md) for the architecture, compatibility goals and
roadmap.
