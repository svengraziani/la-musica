# Production Readiness Assessment

> **Date:** 2026-05-28 · **Overall readiness:** `scaffold` · **Target:** full professional macOS DAW (not MVP)
>
> This assessment was produced from a deep, multi-agent audit of the actual source tree
> (every subsystem read, claims verified against the code). It is the basis for the
> [Production Roadmap](production-roadmap.md) and its concrete tasks under
> [`tasks/`](tasks/). Read it alongside the
> [Decision And Compromise Register](decision-compromise-register.md).

## Status Update — 2026-05-29

The verdict below is the original 2026-05-28 baseline snapshot. The current tree has since closed
substantial P26-P34 implementation gaps:

- P26 now has CI-labeled determinism, plugin-hosting, audio-correctness, behavior, CLI, perf, and
  headless GUI binding tests, plus artifact-based assertions replacing brittle stdout-only checks.
- P27 now has scripted signing, notarization/stapling, signature verification, dSYM archival,
  symbolication verification, provenance checks, SBOM/checksum generation, dependency-lock checks,
  package verification, release workflow gates, and release docs. The remaining release gates still
  require a real macOS release environment: Developer ID signing/notarization/stapling,
  Gatekeeper launch from a clean account, and symbolication against archived dSYMs on macOS.
- P28/P29/P33 now cover warp DSP, project sample-rate persistence/reconciliation, mixed-rate audio
  import, sample-rate-tagged analysis caches, and comp-segment graph/export rendering.
- P30/P31/P34 now cover automated accessibility audits, localization table coverage, Spanish
  strings, a stub third-locale add test, onboarding templates, localized welcome accessibility
  metadata, and bundled manual lookup. The qualitative VoiceOver checklist remains a manual macOS
  release gate.
- P32 now has explicit diagnostics consent defaults, endpoint validation/override docs, scrubber
  tests, privacy documentation, and package-verifier coverage.

Use the task files, tests, release scripts, and workflows as the authoritative current evidence.
This assessment remains useful as the historical audit that motivated the roadmap, not as a fresh
description of today's implementation state.

## Verdict

LaMusica today is **honest, well-tested scaffolding — not a working DAW.** The model and
serialization layer (session model, command logic, MCP tool *logic*) is real, in-memory, and
covered by tests. But the three load-bearing runtime spines of a DAW are entirely absent:

1. **No live audio.** Nothing links `juce_audio_devices` / `juce_audio_processors`. There is no
   `AudioDeviceManager`, no realtime audio thread, and `AudioEngine::renderGraphBlock` is only
   ever called from offline render wrappers. The app has never driven a speaker or captured a
   microphone.
2. **No generic authoring.** The entire user-facing surface is hardwired to a single canned
   drums + bass + master "starter" project. There is no generic add-track / add-clip / add-plugin
   API, and `GraphCompiler` only synthesizes MIDI for `dataId == "starter-bass-midi"` and renders
   every other clip as an FNV-hash-derived sine tone.
3. **No real UI or MCP transport.** The shipped GUI (`apps/daw/src/main_juce.cpp`) is five static,
   read-only text labels frozen after launch. The MCP daemon is a bespoke whitespace line-grammar
   stub whose edit/render/orchestration tools exist only in unit tests and never reach a running
   session.

Plugin hosting, plugin delay compensation, metering, freeze/bounce, MIDI I/O, mixer buses, and
automation application **do not exist on the audio path**. On top of that, the on-disk format
silently violates its own published JSON schema, float serialization is lossy and
locale-dependent, saves are not durable (no `fsync`), and there is no signing/notarization or
crash-reporting pipeline. The realtime-safety guarantee — the project's central architectural
promise — is currently *fabricated* by a tautological audit harness, so it is unverified.

**Almost everything currently labeled "done" is in-process model logic.** The path to production
is essentially the entire real-time, interactive, and hosting layer of a DAW, built on top of
(and correcting) the existing model.

## Readiness By Area

| Area | Readiness | Headline |
| --- | --- | --- |
| Audio engine / realtime spine | `scaffold` | No live device I/O, no realtime thread; render path throws and re-sorts per block; engine exercised only offline. Foundational gap blocking the whole product. |
| Session model / project format | `partial` | Solid serialization/undo plumbing, but hardwired to one canned starter; no generic authoring API; MIDI/plugin/mixer state not persisted; output violates its own v1 schema; saves not durable. |
| Command layer | `partial` | 18 `ProjectManifest` commands are real and undoable, but ~28 edit-domain commands sit outside the unified history, most aren't serializable/replayable, strings aren't escaped, and no UI routes through commands. |
| Plugin hosting | `scaffold` | All metadata, zero runtime: no `juce_audio_processors` linked, no scanning, no plugin graph node, no state blob, no PDC, no editor windows. The "use instruments/effects" requirement is unmet. |
| MCP daemon | `scaffold` | Bespoke line grammar instead of JSON-RPC/MCP; edit/render/orchestration tools unreachable over the wire; no persistent audit journal; weak auth; lifecycle and sandbox are in-memory fiction. |
| DAW UI (shipped app) | `scaffold` | ~194-line app of static read-only labels: no menus, no transport, no timeline/mixer/piano-roll, no document lifecycle, no command manager; frozen after launch. |
| CLI / offline render | `scaffold` | `render-project` bounces ID-hashed sines, not real music; edits gated to the starter template; no JSON output, no path sandbox, no format/range control. |
| Tests / QA | `partial` | ~899 hand-rolled asserts in one monolithic `main()` that exits on first failure; no byte-exact determinism, no TSan/RT-safety enforcement, no MCP-over-wire e2e; GUI untested; realtime audit is tautological. |
| Build / CI / packaging | `partial` | Builds and pins JUCE, but no codesigning/notarization/stapling, no hardened runtime or committed entitlements, arm64-only, `Info.plist` not wired (bundle-id mismatch), no dSYM/provenance/SBOM/THIRD_PARTY. |
| Realtime safety / perf / security | `scaffold` | RT-safety harness is tautological (false CI assurance); command queue not truly atomic; no crash reporting; sandbox is a bypassable denylist; no real profiling; security disclosure is a placeholder. |

## Verified Critical Findings

Each finding below was confirmed by reading the cited code during the audit.

- **Render path is not realtime-safe and not live.** `libs/audio/src/AudioGraph.cpp::renderGraph`
  throws, allocates a `std::map`/`std::vector` per block, and re-runs a string-keyed Kahn topological
  sort every call. The faster `AudioEngine::renderGraphBlock` uses preallocated buffers and integer
  schedules but still `throw`s `std::runtime_error` and is only invoked from offline wrappers. No
  device thread drives either path.
- **The lock-free queue is not lock-free-correct.** `RealtimeCommandQueue` (in
  `libs/audio/include/lamusica/audio/AudioEngine.hpp`) tracks `size_` with a non-atomic
  `std::size_t` mutated from both `push` (`++`) and `pop` (`--`), so it is not SPSC-safe.
- **The realtime-safety audit is tautological.** `auditRealtimeGraphCallback` hardcodes its own
  operations list and then asserts that list is clean — it proves nothing about the real callback.
- **Rendering is fake.** `libs/session/src/GraphCompiler.cpp` renders every non-asset clip as an
  FNV-hash sine (`clipFixtureFrequency`) and only synthesizes MIDI for `dataId == "starter-bass-midi"`.
- **The MCP transport is a stub.** The daemon uses a whitespace line grammar (`splitWords`) that
  cannot carry arguments containing spaces; `DaemonSession` install/launch/stop merely flip an enum
  with no OS process; the audit log is an in-memory `std::vector` with no file persistence.
- **The on-disk format violates its own schema.** `libs/session/src/ProjectManifest.cpp` emits
  `loopEnabled`/`loopStartSample`/`loopEndSample` (lines ~656–658) and `midiClip.transposeSemitones`
  (line ~719), all forbidden by `docs/schemas/project-v1.schema.json` (`additionalProperties: false`).
  Floats (`bpm`/`gainDb`/`volumeDb`/`pan`) are written via locale-dependent `ostringstream`. Saves
  are not durable (no `fsync`/`F_FULLFSYNC`). No project sample rate is stored anywhere.
- **The shipped GUI is static.** `apps/daw/src/main_juce.cpp` is five `StatusPanel` labels with
  `"Stopped"` hardcoded; no menus, transport, timeline, mixer, piano roll, document lifecycle, or
  command manager.
- **Release/licensing gaps.** Bundle-id mismatch (`Info.plist` `dev.lamusica.daw` vs CMake
  `dev.lamusica.app`); no `THIRD_PARTY`/`NOTICE` under an `AGPL-3.0` license that bundles AGPL JUCE.

## Coverage Gaps Found By Adversarial Review

These were missing from the first-pass plan and are now owned by dedicated tasks (P28–P34):

- **Time-stretch / pitch-shift / warp DSP is non-negotiable scope but unbuilt.** Only planning math
  exists (`libs/session/include/lamusica/session/Warp.hpp`); the render path does zero
  resampling/stretching. → **P28**.
- **Sample-rate independence is uncovered.** The format stores no project sample rate and assets are
  read 1:1, so a project authored at 48 kHz opened on a 44.1 kHz device plays at the wrong time and
  pitch, and imported assets are never resampled. → **P29**.
- **Accessibility (VoiceOver / macOS AX) is entirely absent** from the codebase and from every UI
  task. → **P30**.
- **Localization / i18n is absent** despite the Spanish product name "LaMusica." → **P31**.
- **Crash-reporting consent / privacy / telemetry policy** is left as plumbing-only; the decision
  register flags it open. → **P32**.
- **Comp-segment *rendering* is unwired** — the comping model exists but `GraphCompiler` ignores it,
  so a chosen comp would not actually render. → **P33**.
- **In-app onboarding / templates / contextual help** has no owner. → **P34**.

## How This Reframes The Existing Plan

The previous task index (`docs/tasks/026-036`) is **UI-first** and silently assumes a working audio
engine, plugin host, generic session model, and MCP transport — none of which exist. This roadmap
**inverts that order**: it builds the realtime spine and the generic command-authored model *before*
the UI, and treats 026–036 as background acceptance criteria rather than the execution order. The
mapping from each new task back to 026–036 is recorded in each task file's "Notes For Agents."
