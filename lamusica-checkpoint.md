# LaMusica Checkpoint

Goal: implement till all tasks are complete.

Workspace: `/Users/svengraziani/Development/nursvendsp/lamusica`

Status as of 2026-05-28:

- Tasks 011 through 018 have received recent implementation increments with focused tests/docs.
- Latest completed increment: task 025 Packaging, Signing, Docs, And Release.
- Task 018 added browser section aggregation:
  - `BrowserSectionKind`
  - `BrowserSectionItem`
  - `BrowserSection`
  - `buildBrowserSections(const AssetCatalog&, std::size_t recentLimit = 16)`
- Files touched for task 018:
  - `libs/session/include/lamusica/session/Assets.hpp`
  - `libs/session/src/Assets.cpp`
  - `tests/unit/bootstrap_tests.cpp`
  - `docs/user-manual.md`
- Task 018 tests now cover browser sections for project media, granted user folders, plugin presets,
  drum kits, templates, and recent files with resolved asset metadata.

Verification after task 018:

- `cmake -P cmake/CheckDependencyLock.cmake` passed.
- `cmake -P cmake/CheckMarkdown.cmake` passed.
- `git diff --check` passed.
- `find apps libs tools tests \( -name '*.cpp' -o -name '*.hpp' -o -name '*.mm' \) -exec xcrun clang-format --dry-run --Werror {} +` passed.
- `cmake --build --preset debug` passed.
- `ctest --preset debug` passed: 8/8 tests.
- `cmake --build --preset asan` passed.
- `cmake --build --preset release` passed.
- `ctest --preset asan` was not run because this repo has a known history of that preset hanging;
  ASan build was used as the sanitizer gate.

Task 019 MCP Daemon Foundation completed in this increment:

- Added daemon lifecycle state and logging:
  - `DaemonLifecycleStatus`
  - `DaemonLogEntry`
  - `DaemonSession::install`, `launch`, `stop`, `daemonLog`, `launchLabel`
- Added `lamusica_mcpd` process flags:
  - `--install`
  - `--launch`
  - `--stop`
  - `--health`
  - `--logs`
- Strengthened local auth:
  - attach issues randomized `lmcp_` tokens.
  - project-scoped protocol requests use `auth <token> ...`.
  - invalid active and recovery tokens are rejected.
  - recovery rotates the active token.
  - replacing an attached project scope requires valid current auth.
- Tightened scope validation:
  - unknown attach capabilities are rejected.
  - unauthenticated project queries are rejected.
  - shell/process/filesystem-like protocol surfaces remain denied and logged.
- Added an interactive fixture client:
  - `tests/fixtures/mcp_fixture_client.cpp`
  - CTest target `lamusica_mcpd_fixture_client`
  - Covers health without project, attach token issuance, authenticated query, scoped mutation
    denial, shell denial, interrupted connection recovery, token rotation, and authenticated detach.
- Updated `docs/developer/mcp-tools.md` with authenticated protocol and lifecycle commands.

Verification after task 019:

- Local toolchain was extracted under `/tmp/lamusica-toolchain` because the container lacked system
  CMake/compiler/clang-format and passwordless sudo.
- `cmake -P cmake/CheckDependencyLock.cmake` passed.
- `cmake -P cmake/CheckMarkdown.cmake` passed.
- `git diff --check` passed.
- `find apps libs tools tests \( -name '*.cpp' -o -name '*.hpp' -o -name '*.mm' \) -exec clang-format --dry-run --Werror {} +` passed.
- `cmake --preset debug -DCMAKE_CXX_COMPILER=/tmp/lamusica-toolchain/usr/bin/g++` passed.
- `cmake --build --preset debug` passed.
- `ctest --preset debug` passed: 10/10 tests.
- `cmake --preset asan -DCMAKE_CXX_COMPILER=/tmp/lamusica-toolchain/usr/bin/g++` passed.
- `cmake --build --preset asan` passed.
- `ctest --preset asan` passed: 10/10 tests.
- `cmake --preset release -DCMAKE_CXX_COMPILER=/tmp/lamusica-toolchain/usr/bin/g++` passed.
- `cmake --build --preset release` passed.
- `build/unix-release/apps/mcpd/lamusica_mcpd --health` passed.
- Release fixture validation passed:
  - `build/unix-release/tools/cli/lamusica_cli validate fixtures/empty.Project.lamusica`
  - `build/unix-release/tools/cli/lamusica_cli validate fixtures/examples/generated-tone.Project.lamusica`
  - `build/unix-release/tools/cli/lamusica_cli validate fixtures/tutorials/first-song.Project.lamusica`
  - `build/unix-release/tools/cli/lamusica_cli verify-examples fixtures/tutorials`

Task 020 MCP DAW Query Tools completed in this increment:

- Audited existing read-only query coverage for:
  - project summary
  - tracks
  - clips and clip ranges
  - selection
  - transport
  - tempo
  - markers
  - routing
  - plugins
  - automation and automation ranges
  - assets
  - render capabilities
- Added `maxQueryPageLimit` with a 500-item response cap for paged query payloads.
- Strengthened schema-contract tests so every query response is checked for `schemaVersion: 1` and
  a stable `tool` name.
- Extended `docs/schemas/mcp-query-tools-v1.md` with per-tool response contracts and documented
  the page-limit clamp.
- Existing read audit behavior remains covered through successful protocol query audit entries, and
  read-only manifest non-mutation remains tested by serializing the manifest before and after query
  calls.

Verification after task 020:

- `cmake -P cmake/CheckDependencyLock.cmake` passed.
- `cmake -P cmake/CheckMarkdown.cmake` passed.
- `git diff --check` passed.
- `find apps libs tools tests \( -name '*.cpp' -o -name '*.hpp' -o -name '*.mm' \) -exec clang-format --dry-run --Werror {} +` passed.
- `cmake --build --preset debug` passed.
- `ctest --preset debug` passed: 10/10 tests.
- `cmake --build --preset asan` passed.
- `ctest --preset asan` passed: 10/10 tests.
- `cmake --build --preset release` passed.
- Release fixture validation passed:
  - `build/unix-release/tools/cli/lamusica_cli validate fixtures/empty.Project.lamusica`
  - `build/unix-release/tools/cli/lamusica_cli validate fixtures/examples/generated-tone.Project.lamusica`
  - `build/unix-release/tools/cli/lamusica_cli validate fixtures/tutorials/first-song.Project.lamusica`
  - `build/unix-release/tools/cli/lamusica_cli verify-examples fixtures/tutorials`

Task 021 MCP Editing Tools completed in this increment:

- Added explicit MCP edit validation entry points:
  - `validateCommand`
  - `validateMidiCommand`
  - `validateAutomationCommand`
  - `validateMixerCommand`
  - `validatePluginCommand`
- Kept mutation paths command-backed:
  - project-manifest edit tools still apply through `commands::CommandHistory`
  - MIDI, automation, mixer, and plugin edits still apply through their store-backed history stacks
- Expanded MCP edit coverage for timeline operations:
  - cut/remove with confirmation token
  - trim
  - split as a transaction
  - move
  - duplicate
- Strengthened invalid-edit behavior coverage:
  - invalid timeline edits return failed validation
  - invalid edits leave serialized project state unchanged
- Existing MCP edit coverage still includes:
  - labels through track rename
  - routing add/remove with destructive confirmation
  - MIDI add note, transpose, undo, redo
  - automation point write, undo, redo
  - mixer volume/pan/mute, undo, redo
  - plugin insert add/remove and preset apply, undo, redo
- Updated MCP edit docs to document validate, preview, apply, undo, redo flows and timeline
  cut/remove, trim, split, move, duplicate coverage.

Verification after task 021:

- `cmake -P cmake/CheckDependencyLock.cmake` passed.
- `cmake -P cmake/CheckMarkdown.cmake` passed.
- `git diff --check` passed.
- `find apps libs tools tests \( -name '*.cpp' -o -name '*.hpp' -o -name '*.mm' \) -exec clang-format --dry-run --Werror {} +` passed.
- `cmake --build --preset debug` passed.
- `ctest --preset debug` passed: 10/10 tests.
- `cmake --build --preset asan` passed.
- `ctest --preset asan` passed: 10/10 tests.
- `cmake --build --preset release` passed.
- Release fixture validation passed:
  - `build/unix-release/tools/cli/lamusica_cli validate fixtures/empty.Project.lamusica`
  - `build/unix-release/tools/cli/lamusica_cli validate fixtures/examples/generated-tone.Project.lamusica`
  - `build/unix-release/tools/cli/lamusica_cli validate fixtures/tutorials/first-song.Project.lamusica`
  - `build/unix-release/tools/cli/lamusica_cli verify-examples fixtures/tutorials`

Current resume point:

Task 022 MCP Audio And Render Tools completed in this increment:

- Audited existing render-tool coverage for:
  - WAV analysis
  - test-tone render
  - selected-region graph bounce
  - project mix export
  - batch mix export
  - stem export
  - normalize and reverse transforms
  - bounce-in-place
  - freeze track
  - overwrite confirmation tokens
  - import/export capability checks
- Added waveform and richer analysis metadata to `wav_analysis` result manifests:
  - LUFS estimate remains present
  - tempo/key estimates
  - transient count
  - waveform validity
  - waveform bucket size
  - waveform bucket count
- Added `RenderJobQueue::enqueueLongRender` to model a capability-checked running render with
  bounded progress below completion.
- Strengthened render tests for:
  - loudness and waveform analysis fields
  - confirmed source-media overwrite for transforms
  - running long-render progress
  - clean cancellation of a running render
- Updated MCP render docs/schema to document waveform analysis, source overwrite confirmation, and
  long-running progress/cancellation behavior.

Verification after task 022:

- `cmake -P cmake/CheckDependencyLock.cmake` passed.
- `cmake -P cmake/CheckMarkdown.cmake` passed.
- `git diff --check` passed.
- `find apps libs tools tests \( -name '*.cpp' -o -name '*.hpp' -o -name '*.mm' \) -exec clang-format --dry-run --Werror {} +` passed.
- `cmake --build --preset debug` passed.
- `ctest --preset debug` passed: 10/10 tests.
- `cmake --build --preset asan` passed.
- `ctest --preset asan` passed: 10/10 tests.
- `cmake --build --preset release` passed.
- Release fixture validation passed:
  - `build/unix-release/tools/cli/lamusica_cli validate fixtures/empty.Project.lamusica`
  - `build/unix-release/tools/cli/lamusica_cli validate fixtures/examples/generated-tone.Project.lamusica`
  - `build/unix-release/tools/cli/lamusica_cli validate fixtures/tutorials/first-song.Project.lamusica`
  - `build/unix-release/tools/cli/lamusica_cli verify-examples fixtures/tutorials`

Current resume point:

Task 023 AI Orchestration Workflows completed in this increment:

- Audited existing orchestration coverage for:
  - MIDI harmony plans
  - drum variation plans
  - song-structure label plans
  - mix preparation plans
  - capability-scoped plan creation
  - approve/reject/partial application review flows
  - template library serialization, parse, save, and load
- Added section arrangement planning:
  - `WorkflowSectionMarker`
  - `createArrangeSectionsPlan`
  - capability-scoped `createArrangeSectionsPlan` overload
  - section steps resolve through `AddMarkerCommand` previews and validation.
- Hardened deterministic workflow previews:
  - MIDI harmony now uses `AddMidiNoteCommand` validation for valid generated notes.
  - Drum variation now emits only a command-backed `duplicate_pattern_variation` step validated
    through `DuplicatePatternVariationCommand`.
- Strengthened tests for:
  - section arrangement marker validation
  - command-backed deterministic drum variation
  - reproducible workflow JSON for fixed inputs and seed
  - invalid approved steps never reaching the application callback
- Updated workflow docs/schema to document section arrangement, deterministic output, command
  previews, and validation-before-application behavior.

Verification after task 023:

- `cmake -P cmake/CheckDependencyLock.cmake` passed.
- `cmake -P cmake/CheckMarkdown.cmake` passed.
- `git diff --check` passed.
- `find apps libs tools tests \( -name '*.cpp' -o -name '*.hpp' -o -name '*.mm' \) -exec clang-format --dry-run --Werror {} +` passed.
- `cmake --build --preset debug` passed.
- `ctest --preset debug` passed: 10/10 tests.
- `cmake --build --preset asan` passed.
- `ctest --preset asan` passed: 10/10 tests.
- `cmake --build --preset release` passed.
- Release fixture validation passed:
  - `build/unix-release/tools/cli/lamusica_cli validate fixtures/empty.Project.lamusica`
  - `build/unix-release/tools/cli/lamusica_cli validate fixtures/examples/generated-tone.Project.lamusica`
  - `build/unix-release/tools/cli/lamusica_cli validate fixtures/tutorials/first-song.Project.lamusica`
  - `build/unix-release/tools/cli/lamusica_cli verify-examples fixtures/tutorials`

Current resume point:

Task 024 Performance, Stress, And Realtime Verification completed in this increment:

- Audited existing stress fixtures and benchmark coverage for tracks, clips, plugins, automation
  lanes, MIDI note data, assets, markers, MCP audit activity, machine context, CPU/query work,
  memory and disk estimates, save/load, render realtime factor, and CI benchmark smoke wiring.
- Added realtime graph callback instrumentation:
  - `RealtimeCallbackAudit`
  - `auditRealtimeGraphCallback`
  - callback completion, transport advancement, deadline, and forbidden-operation policy checks
- Extended benchmark thresholds and results with:
  - edit latency
  - MCP query latency
  - realtime callback latency
  - realtime callback safety status
- Expanded stress benchmark work to cover representative edit validation and MCP audit/query
  traversal in addition to startup, plugin scan, CPU/query, save/load, memory, disk, and render.
- Updated `lamusica_cli benchmark-smoke` output and thresholds to print and gate the new
  measurements.
- Updated `docs/performance/realtime-policy.md` with callback instrumentation, edit/MCP latency,
  and regression-gate expectations.
- Strengthened tests for realtime callback instrumentation and the expanded regression list.

Verification after task 024:

- `cmake -P cmake/CheckDependencyLock.cmake` passed.
- `cmake -P cmake/CheckMarkdown.cmake` passed.
- `git diff --check` passed.
- `find apps libs tools tests \( -name '*.cpp' -o -name '*.hpp' -o -name '*.mm' \) -exec clang-format --dry-run --Werror {} +` passed.
- `cmake --build --preset debug` passed.
- `ctest --preset debug` passed: 10/10 tests.
- `cmake --build --preset asan` passed.
- `ctest --preset asan` passed: 10/10 tests.
- `cmake --build --preset release` passed.
- Release fixture validation passed:
  - `build/unix-release/tools/cli/lamusica_cli validate fixtures/empty.Project.lamusica`
  - `build/unix-release/tools/cli/lamusica_cli validate fixtures/examples/generated-tone.Project.lamusica`
  - `build/unix-release/tools/cli/lamusica_cli validate fixtures/tutorials/first-song.Project.lamusica`
  - `build/unix-release/tools/cli/lamusica_cli verify-examples fixtures/tutorials`

Current resume point:

Task 025 Packaging, Signing, Docs, And Release completed in this increment:

- Audited existing release coverage for:
  - CPack TGZ/DragNDrop packaging
  - package-content verification
  - macOS signing and notarization docs
  - release checklist
  - versioning policy
  - user manual
  - developer build, command API, architecture, MCP, and dependency docs
  - redistributable generated/empty example and tutorial fixtures
- Added `docs/developer/project-format.md` covering project bundle layout, manifest contract,
  asset rules, migrations, and validation commands.
- Added `docs/release/security-disclosure.md` and linked the release checklist back to the root
  security process.
- Updated first-launch and installation guidance in the user manual and macOS distribution docs.
- Updated README status and build prerequisites to match the implemented repository state.
- Tightened `cmake/VerifyPackage.cmake` so packages must include the app executable, daemon, CLI,
  user manual, architecture docs, build/test docs, project-format docs, command API docs, MCP docs,
  release docs, changelog, security docs, examples, and tutorials.
- Made package verification support both macOS app bundles and non-macOS smoke binaries so the
  package-content check can be exercised in this container while macOS CI still verifies the app
  bundle archive.

Verification after task 025:

- `cmake -P cmake/CheckDependencyLock.cmake` passed.
- `cmake -P cmake/CheckMarkdown.cmake` passed.
- `git diff --check` passed.
- `find apps libs tools tests \( -name '*.cpp' -o -name '*.hpp' -o -name '*.mm' \) -exec clang-format --dry-run --Werror {} +` passed.
- `cmake --build --preset debug` passed.
- `ctest --preset debug` passed: 10/10 tests.
- `cmake --build --preset asan` passed.
- `ctest --preset asan` passed: 10/10 tests when run sequentially after debug tests.
- `cmake --build --preset release` passed.
- `cpack -G TGZ --config build/unix-release/CPackConfig.cmake` passed.
- `cmake -DPACKAGE=LaMusica-0.1.0-Linux.tar.gz -P cmake/VerifyPackage.cmake` passed.
- Release fixture validation passed:
  - `build/unix-release/tools/cli/lamusica_cli validate fixtures/empty.Project.lamusica`
  - `build/unix-release/tools/cli/lamusica_cli validate fixtures/examples/generated-tone.Project.lamusica`
  - `build/unix-release/tools/cli/lamusica_cli validate fixtures/tutorials/first-song.Project.lamusica`
  - `build/unix-release/tools/cli/lamusica_cli verify-examples fixtures/examples`
  - `build/unix-release/tools/cli/lamusica_cli verify-examples fixtures/tutorials`

Current resume point:

- All listed task documents through `docs/tasks/025-packaging-signing-docs-release.md` are complete.

Important repo/worktree note:

- The worktree is broadly dirty with many ongoing task changes and generated package artifacts.
- Do not revert unrelated files.
- Use current files as authoritative before continuing.

Standard gates to run after each implementation increment:

- `cmake -P cmake/CheckDependencyLock.cmake`
- `cmake -P cmake/CheckMarkdown.cmake`
- `git diff --check`
- `find apps libs tools tests \( -name '*.cpp' -o -name '*.hpp' -o -name '*.mm' \) -exec xcrun clang-format --dry-run --Werror {} +`
- `cmake --build --preset debug`
- `ctest --preset debug`
- `cmake --build --preset asan`
- `cmake --build --preset release`
