# hl

`hl` is a C++17 reimplementation of the core of
[pamburus/hl](https://github.com/pamburus/hl) for Haiku. It will turn JSON and
logfmt logs into readable, coloured, filterable output while keeping the core
portable across Haiku, Linux and macOS.

The project is in early development. The current skeleton provides the command
line entry point and platform abstraction; log parsing is not implemented yet.

## Build

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

Run `build/hl --help` or `build/hl --version` after building.

See [hl-design.md](hl-design.md) for the architecture, compatibility goals and
roadmap.
