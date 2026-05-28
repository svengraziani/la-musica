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

The test preset includes the unit suite plus CLI lifecycle checks that create, validate, and inspect
a project directory through the built executable.
It also runs `lamusica_cli benchmark-smoke`, a conservative stress benchmark gate that records
machine context and fails on threshold regressions.

The unit suite also exercises recording commit, discard, latency alignment, and interrupted
temporary-file recovery paths.

Application-shell tests cover project lifecycle, crash-safe recovery, preferences, keyboard
shortcut persistence, and menu command routing through the focused primary panel.

Drum-machine tests cover MIDI triggering, choke groups, per-pad routing, velocity layers, portable
preset serialization and parsing, starter-kit license metadata, sample start/end, reverse playback,
pitch, envelopes, and filtering.

Pattern tests cover deterministic probability, swing, ratchets, accents, independent cycling
per-lane lengths, pattern placement on the arrangement timeline, pattern-to-MIDI conversion,
MIDI-to-pattern conversion, and chained patterns.

Mixer tests cover routing validation, safe send edits with rollback, send-cycle rejection,
off-thread graph update plans, metering, fader groups, and undoable mix changes.

Plugin-hosting foundation tests cover scan cache behavior, crash/timeout blacklist handling,
blacklist isolation, insert-chain save/reload, preset application, and stable parameter addresses.

Warp tests cover transient detection, beat slicing, groove extraction, tempo retargeting without
losing marker metadata, undoable marker edits, render-cache invalidation, and preview/offline plan
agreement.

## CLI Project Lifecycle

```sh
build/unix-debug/tools/cli/lamusica_cli create-project /tmp/Demo.Project.lamusica Demo
build/unix-debug/tools/cli/lamusica_cli validate /tmp/Demo.Project.lamusica
build/unix-debug/tools/cli/lamusica_cli inspect-project /tmp/Demo.Project.lamusica
```

Release builds use:

```sh
cmake --preset release
cmake --build --preset release
```

AddressSanitizer builds use:

```sh
cmake --preset asan
cmake --build --preset asan
ctest --preset asan
```

Profiling builds use:

```sh
cmake --preset profiling
cmake --build --preset profiling
```

## App Smoke

```sh
build/unix-debug/apps/daw/LaMusica.app/Contents/MacOS/LaMusica --smoke
```

## Formatting

```sh
find apps libs tools tests \( -name '*.cpp' -o -name '*.hpp' -o -name '*.mm' \) -exec xcrun clang-format --dry-run --Werror {} +
cmake -P cmake/CheckMarkdown.cmake
cmake -P cmake/CheckDependencyLock.cmake
```

Editor formatting defaults for C++, CMake, Markdown, JSON, YAML, and shell scripts are recorded in
`.editorconfig`.

Third-party dependency policy and locked external inputs are recorded in
`docs/developer/dependencies.md`.

## Packaging

```sh
cpack --config build/unix-release/CPackConfig.cmake
cmake -DPACKAGE=LaMusica-0.1.0-Darwin.tar.gz -P cmake/VerifyPackage.cmake
```

The package installs the app bundle, MCP daemon, CLI, documentation, and redistributable example
and tutorial projects.

## Developer References

- Architecture: `docs/architecture/architecture-baseline.md`
- Dependencies: `docs/developer/dependencies.md`
- Project format: `docs/developer/project-format.md`
- Project schema: `docs/schemas/project-v1.schema.json`
- Command API: `docs/developer/command-api.md`
- MCP tools: `docs/developer/mcp-tools.md`
- Performance policy: `docs/performance/realtime-policy.md`
