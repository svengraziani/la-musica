# Build And Test

## Prerequisites

- macOS.
- Xcode command line tools.
- CMake.
- JUCE 8.0.13 checkout supplied through `LAMUSICA_JUCE_PATH`.

## Commands

```sh
cmake --preset debug -DLAMUSICA_JUCE_PATH=/path/to/JUCE-8.0.13
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
cmake --preset release -DLAMUSICA_JUCE_PATH=/path/to/JUCE-8.0.13
cmake --build --preset release
ctest --preset release
```

The release test preset runs onboarding, accessibility, localization, generic CLI, determinism,
audio-correctness, plugin-hosting, privacy, and legacy first-track compatibility gates against
optimized binaries. Before shipping, confirm both the current template/onboarding flow and the
legacy first-track release fixtures:

```sh
ctest --preset release -L 'onboarding|a11y|i18n'
ctest --preset release -R 'lamusica_daw_diagnostics_consent|lamusica_mcpd_diagnostics_crash_smoke'
ctest --preset release -R 'lamusica_cli_(help|schema|prepare_arbitrary_id_project|query_arbitrary_id|arbitrary_id_edit|query_generic|generic_edit|generic_render)'
ctest --preset release -R first_track
ctest --preset release -R 'lamusica_cli_.*edit_first_track|lamusica_cli_verify_edit_first_track_package'
ctest --preset release -R lamusica_daw_app_session_preferences_first_track_smoke
```

## Determinism And Perf Gates

The determinism test compares independent renders, block-size variants, command-journal replay,
the committed float golden, the committed WAV golden, and the combined SHA-256 digest in
`tests/determinism/golden/render-golden.sha256`. A legitimate engine-output change must refresh all
three golden artifacts together:

```sh
build/unix-debug/tests/lamusica_render_determinism_tests \
  build/unix-debug/tests/determinism . --update-golden
```

Review the resulting diffs to `render-golden.float32`, `render-golden.wav`, and
`render-golden.sha256` in the same change as the engine update. Do not regenerate goldens from CI or
as an unrelated cleanup.

The realtime deadline gate writes `tests/perf/rt-history.jsonl` in the build tree and fails when any
tracked scale point reports xruns or p99 block time at or above the 1024-frame buffer period. Shared
CI runner jitter is handled by the buffer size; threshold changes require an explicit update to
[tools/perf/README.md](../../tools/perf/README.md) and release-note context.

AddressSanitizer builds use:

```sh
cmake --preset asan -DLAMUSICA_JUCE_PATH=/path/to/JUCE-8.0.13
cmake --build --preset asan
ctest --preset asan
```

Profiling builds use:

```sh
cmake --preset profiling -DLAMUSICA_JUCE_PATH=/path/to/JUCE-8.0.13
cmake --build --preset profiling
```

## App Smoke

```sh
build/unix-debug/apps/daw/lamusica_daw_smoke --app-session-verify-first-track-project-smoke
build/unix-debug/apps/daw/lamusica_daw_smoke --app-session-preferences-first-track-smoke
```

## Formatting

```sh
find apps libs tools tests \( -name '*.cpp' -o -name '*.hpp' -o -name '*.mm' \) -exec xcrun clang-format --dry-run --Werror {} +
cmake -P cmake/CheckMarkdown.cmake
cmake -P cmake/CheckDependencyLock.cmake
scripts/verify-ci-workflow.sh --self-test
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
- Localization: `docs/developer/localization.md`
- Project format: `docs/developer/project-format.md`
- Project schema: `docs/schemas/project-v3.schema.json`
- Command API: `docs/developer/command-api.md`
- MCP tools: `docs/developer/mcp-tools.md`
- Performance policy: `docs/performance/realtime-policy.md`
