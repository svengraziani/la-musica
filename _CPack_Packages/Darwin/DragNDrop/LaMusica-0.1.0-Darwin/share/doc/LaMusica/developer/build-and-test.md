# Build And Test

## Prerequisites

- macOS.
- Xcode command line tools.
- CMake.

## Commands

```sh
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

Release builds use:

```sh
cmake --preset release
cmake --build --preset release
```

## App Smoke

```sh
build/unix-debug/apps/daw/LaMusica.app/Contents/MacOS/LaMusica --smoke
```

## Formatting

```sh
find apps libs tools tests \( -name '*.cpp' -o -name '*.hpp' -o -name '*.mm' \) -exec xcrun clang-format --dry-run --Werror {} +
```

## Packaging

```sh
cpack --config build/unix-release/CPackConfig.cmake
```

The package installs the app bundle, MCP daemon, CLI, documentation, and redistributable example
projects.

## Developer References

- Architecture: `docs/architecture/architecture-baseline.md`
- Project format: `docs/schemas/project-v1.schema.json`
- Command API: `docs/developer/command-api.md`
- MCP tools: `docs/developer/mcp-tools.md`
- Performance policy: `docs/performance/realtime-policy.md`
