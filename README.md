# hl

`hl` (Haiku Log) is a fast Haiku log-file viewer, implemented in C++17 and inspired by
[pamburus/hl](https://github.com/pamburus/hl). It reads ordinary text logs as
well as structured JSON and logfmt logs. Structured formats enable richer field
formatting and filtering, but they are not required. It is built specifically
for current Haiku systems and uses Haiku APIs directly.

The project is in active development. It streams ordinary logs from files or
stdin and automatically recognises text, JSON and logfmt entries. Structured
entries gain readable formatting, level/field/time/query filtering and nested
JSON paths, while plain text remains usable. The ordered worker pipeline uses
multiple processor cores without reordering entries. gzip, bzip2, xz and zstd
files are decompressed automatically when the corresponding command is
installed.

## Build

```sh
make
make test
make install
make clean
```

`make install` defaults to the current user's non-packaged binary directory,
`/boot/home/config/non-packaged/bin`. Override the prefix when needed:

```sh
make install PREFIX=/boot/system/non-packaged
```

For package staging, use `DESTDIR`:

```sh
make install DESTDIR=/tmp/hl-package-root
```

Run it with a file, several files, or stdin:

```sh
./hl system.log
./hl application.log server.log
tail -f system.log | ./hl
./hl application.log.gz
```

Use `--raw` (or `-r`) to copy every input byte without structured formatting.
Colour defaults to automatic terminal detection. Use `-c` or `--color=always`
to force it, and `--color=never` to disable it.

Filter structured logs by minimum level, fields, or time:

```sh
./hl -l warning application.log
./hl -f component=api -f 'message~=failed' application.log
./hl --since=-3h --until=now application.log
./hl -q 'level = error and (status >= 500 or exists(exception))' application.log
```

Field filters support `=`, `!=`, `~=` (contains), and `!~=` (does not
contain). Add `?` after a field name, such as `request_id?=123`, to keep entries
where that field is absent. Time bounds accept RFC 3339, Unix timestamps, `now`,
`today`, `yesterday`, and relative values such as `-3h`, `-14d`, or `+1w`.

Nested JSON paths work in field filters, including array indexes and wildcards:

```sh
./hl -f request.method=POST application.log
./hl -f spans.[1].name=backend application.log
./hl -f spans.[].name=backend application.log
```

Hide fields with repeatable `-h`/`--hide` options or comma-separated rules.
Rules are applied in order; prefix a name with `!` to reveal it again. `*`
matches every field. Use `-e` to hide empty strings, objects, arrays and nulls,
or `-E` to show them again.

```sh
./hl -h caller,request_id application.log
./hl -h '*' -h '!message' -h '!code' application.log
./hl -e application.log
```

Force an input format or delimiter, tolerate a prefix before JSON, or merge
several files chronologically:

```sh
./hl --input-format logfmt application.log
./hl --delimiter nul --output-delimiter lf records.bin
./hl --allow-prefix prefixed-json.log
./hl --sort api.log worker.log
```

Timestamp output supports normal `strftime` conversions plus `%3N`, `%6N` and
`%9N` fractions. UTC is the default; `-L` selects local time and `-Z` accepts a
system time-zone name.

```sh
./hl -t '%H:%M:%S.%6N' application.log
./hl -Z Australia/Brisbane application.log
./hl -x auto application.log
```

Paging is automatic on a terminal. `-P` disables it; `--paging=always` forces
it. Pager selection uses `HL_PAGER`, then `PAGER`, then `hum pager` when hum is
available, then `less`. Use `-o FILE` for direct output. Tuning options include
`-C/--concurrency`, `--buffer-size`, and `--max-message-size`.

## Configuration

Settings are applied in this order: system and user `config.toml`, files named
by `HL_CONFIG` and `--config`, `HL_*` environment variables, then command-line
options. `--config -` skips the default files. A minimal configuration is:

```toml
[input]
allow-prefix = true

[output]
color = "auto"
time-format = "%Y-%m-%d %T.%3N"
time-zone = "UTC"
hide-empty-fields = true
expansion = "auto"

[tuning]
buffer-size = "256KiB"
max-message-size = "64MiB"
concurrency = 4
```

Supported environment settings include `HL_INPUT_FORMAT`, `HL_DELIMITER`,
`HL_ALLOW_PREFIX`, `HL_LEVEL`, `HL_COLOR`, `HL_TIME_FORMAT`, `HL_TIME_ZONE`,
`HL_HIDE_EMPTY_FIELDS`, `HL_EXPANSION`, `HL_OUTPUT_DELIMITER`, `HL_PAGING`,
`HL_CONCURRENCY`, `HL_BUFFER_SIZE`, and `HL_MAX_MESSAGE_SIZE`.

`./hl --help` and `./hl --version` provide command information.

See [hl-design.md](hl-design.md) for the architecture, compatibility goals and
roadmap.
