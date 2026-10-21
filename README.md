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
left unchanged. RFC 3339 and Unix timestamps are normalised to UTC, and a
built-in ANSI theme colours structured output when stdout is a terminal.
Filtering and configurable themes are not implemented yet.

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
Colour defaults to automatic terminal detection. Use `-c` or `--color=always`
to force it, and `--color=never` to disable it.

Filter structured logs by minimum level, fields, or time:

```sh
build/hl -l warning application.log
build/hl -f component=api -f 'message~=failed' application.log
build/hl --since=-3h --until=now application.log
```

Field filters support `=`, `!=`, `~=` (contains), and `!~=` (does not
contain). Add `?` after a field name, such as `request_id?=123`, to keep entries
where that field is absent. Time bounds accept RFC 3339, Unix timestamps, `now`,
`today`, `yesterday`, and relative values such as `-3h`, `-14d`, or `+1w`.

Nested JSON paths work in field filters, including array indexes and wildcards:

```sh
build/hl -f request.method=POST application.log
build/hl -f spans.[1].name=backend application.log
build/hl -f spans.[].name=backend application.log
```

Hide fields with repeatable `-h`/`--hide` options or comma-separated rules.
Rules are applied in order; prefix a name with `!` to reveal it again. `*`
matches every field. Use `-e` to hide empty strings, objects, arrays and nulls,
or `-E` to show them again.

```sh
build/hl -h caller,request_id application.log
build/hl -h '*' -h '!message' -h '!code' application.log
build/hl -e application.log
```

`build/hl --help` and `build/hl --version` provide command information.

See [hl-design.md](hl-design.md) for the architecture, compatibility goals and
roadmap.
