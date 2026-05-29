# Production Roadmap

> **Target:** a full professional macOS DAW with a system-level MCP daemon — **not** an MVP.
> **Current state:** `scaffold` (see [Production Readiness Assessment](production-readiness-assessment.md)).
> **Tasks:** concrete, code-grounded task files live in [`tasks/`](tasks/) (P01–P34).

This roadmap turns the audited gaps into 34 dependency-ordered tasks across 8 phases. It is the
active execution plan. It **supersedes the ordering** of `docs/tasks/026-036` (which is UI-first and
assumes a working engine); those files remain as background acceptance criteria, and each task here
records its mapping back to them. Task-level tradeoffs are tracked in the
[Decision And Compromise Register](decision-compromise-register.md).

## Legend

- **Severity** — `blocker` (cannot ship a usable pro DAW without it) · `high` · `medium` · `low`.
- **Effort** — `S` (< 1 day) · `M` (a few days) · `L` (~1–2 weeks) · `XL` (multiple weeks).

## The Critical Path

The foundation is strictly serial; almost nothing downstream is real until it lands:

```
P01 format integrity ─┐
                      ├─► P03 unified commands ─► P07 generic authoring ─► P09 faithful render ─┐
P04 live CoreAudio ──►P05 RT snapshot/queue ───────────────────────────────────────────────────┤
                                                                                                 ├─► P11/P12 plugin host
                                                                                                 │      └─► P15/P16/P17/P18 mixer·MIDI·record·automation
                                                                                                 │            └─► P19→P20→P21/P22/P23 interactive UI
                                                                                                 │                  └─► P24→P25 MCP-over-the-wire
                                                                                                 └────────────────────────► P26 verification ─► P27 release
```

**Start immediately and in parallel:** `P01`, `P04` (no dependencies), then `P03`/`P05`. These four
are the gate that unblocks the entire product.

## Phases

### Phase 0 — Foundation Hygiene
Fix the data-integrity, determinism, and command defects so everything else builds on trustworthy,
schema-conformant, durable ground.

| Task | Sev/Effort | Depends on | Blocks |
| --- | --- | --- | --- |
| [P01 Fix on-disk format integrity (schema, lossless/locale floats, escaping, durable atomic save)](tasks/P01-format-integrity-durable-save.md) | blocker/M | — | P02, P03, P07, P08, P29 |
| [P02 Real test framework with per-case isolation, JUnit, and a corrupt-project fuzz harness](tasks/P02-test-framework-and-fuzz-harness.md) | high/M | P01 | P26 |
| [P03 Unified polymorphic command history (serialization, coalescing, bounded depth, error handling)](tasks/P03-unified-command-history.md) | blocker/XL | P01 | P07, P24 |

### Phase 1 — Realtime Audio Spine
Make sound happen: live CoreAudio device I/O on a realtime thread, a validated lock-free
snapshot/command spine, a `noexcept` render path, and a *real* RT-safety harness.

| Task | Sev/Effort | Depends on | Blocks |
| --- | --- | --- | --- |
| [P04 Live CoreAudio device I/O on a realtime thread driving the engine callback](tasks/P04-live-coreaudio-realtime-thread.md) | blocker/L | — | P05, P10, P11, P16, P17, P20, P29 |
| [P05 Validated audio-graph snapshot, precompiled noexcept render path, true lock-free SPSC queue](tasks/P05-audio-graph-snapshot-spsc-queue.md) | blocker/M | P04 | P06, P09, P11, P15, P18, P20, P25, P28 |
| [P06 Real realtime-safety enforcement harness + TSan CI job (replace the tautological audit)](tasks/P06-realtime-safety-harness-tsan.md) | blocker/L | P05 | P26 |

### Phase 2 — Generic Authoring + Faithful Render
Replace the canned starter with a command-authored generic multitrack model, render real user
content, persist it, and make projects sample-rate-portable.

| Task | Sev/Effort | Depends on | Blocks |
| --- | --- | --- | --- |
| [P07 Generic command-authored multitrack session API (tracks/clips/sends/automation/markers, Save-As)](tasks/P07-generic-multitrack-authoring-api.md) | blocker/XL | P01, P03 | P08, P09, P19, P24 |
| [P08 Versioned binary sidecars (MIDI/automation/plugin-state/analysis) + migration + autosave/recovery](tasks/P08-binary-sidecar-format-autosave.md) | blocker/L | P01, P07 | P09, P13, P23, P29 |
| [P09 Faithful GraphCompiler: render real user audio/MIDI + instrument/synth graph node](tasks/P09-faithful-graphcompiler-render.md) | blocker/L | P05, P07, P08 | P10, P12, P15, P21, P24, P28, P33 |
| [P10 Streaming WAV read/write, incremental recording-to-disk, accurate sample-rate headers](tasks/P10-streaming-wav-and-recording-io.md) | medium/M | P04, P09 | P17, P28 |
| [P28 Time-stretch, pitch-shift, and warp-marker DSP in the render and bounce path](tasks/P28-warp-stretch-pitch-dsp.md) | blocker/L | P05, P09, P10 | P26 |
| [P29 Project sample-rate field, asset resampling on import, SR-mismatch reconciliation](tasks/P29-project-sample-rate-independence.md) | blocker/M | P01, P04, P08 | P28 |

### Phase 3 — Plugin Hosting
Host real AU/VST3 plugins end-to-end: crash-isolated scanning, graph nodes, RT-safe parameters,
opaque state persistence, editor windows, PDC, and freeze/bounce.

| Task | Sev/Effort | Depends on | Blocks |
| --- | --- | --- | --- |
| [P11 JUCE plugin host module + crash-isolated out-of-process scanner with persistent cache](tasks/P11-plugin-host-and-scanner.md) | blocker/XL | P04, P05 | P12, P22, P27 |
| [P12 Plugin graph node + RT-safe parameter/bypass delivery in the render path](tasks/P12-plugin-graph-node-rt-params.md) | blocker/XL | P09, P11 | P13, P14, P15, P16, P18, P26 |
| [P13 Plugin opaque state persistence, editor windows, graceful plugin error model](tasks/P13-plugin-state-and-editor-windows.md) | blocker/L | P08, P12 | P14, P22 |
| [P14 Engine-wide plugin delay compensation (PDC) + track freeze/bounce-to-cache](tasks/P14-plugin-delay-compensation-freeze.md) | high/L | P12, P13 | P17, P26 |

### Phase 4 — Mixer, MIDI I/O, Recording, Automation (on the audio path)
Complete the realtime feature set the arrangement and mixer depend on.

| Task | Sev/Effort | Depends on | Blocks |
| --- | --- | --- | --- |
| [P15 Real bus/send/channel-strip processing and live metering taps](tasks/P15-buses-sends-strips-metering.md) | high/L | P05, P09, P12 | P22 |
| [P16 Live MIDI input/output ingestion and routing to instrument nodes](tasks/P16-live-midi-io-routing.md) | high/L | P04, P12 | P22, P23 |
| [P17 Live latency-compensated recording with monitoring, punch/count-in, take handling](tasks/P17-live-recording-monitoring-punch.md) | blocker/L | P04, P10, P14¹ | P22, P26, P33 |
| [P18 Sample-accurate automation application and ramping in the render path](tasks/P18-sample-accurate-automation.md) | medium/M | P05, P12 | P26 |
| [P33 Comp-segment and take selection rendering in the GraphCompiler/render path](tasks/P33-comp-segment-rendering.md) | medium/M | P09, P17 | — |

¹ Per the audit correction, P17 should depend only on **engine-latency reporting** split out of P14,
not on the freeze/bounce half — basic recording must not be serialized behind freeze. See the P14/P17
task files.

### Phase 5 — Interactive DAW UI
Turn the static shell into a real application, then build the editors.

| Task | Sev/Effort | Depends on | Blocks |
| --- | --- | --- | --- |
| [P19 Interactive DAW shell: command manager, native menus, document lifecycle, live data binding](tasks/P19-interactive-daw-shell.md) | blocker/L | P07 | P20, P21, P25, P30, P31, P34 |
| [P20 Functional transport bar and UI-to-engine playback/monitoring bridge](tasks/P20-transport-bar-engine-bridge.md) | blocker/L | P04, P05, P19 | P21, P22 |
| [P21 Arrangement timeline with clip/automation editing + high-level audio clip editing UI](tasks/P21-arrangement-timeline-clip-editing.md) | blocker/XL | P09, P19, P20 | P23, P26, P30, P34 |
| [P22 Mixer, plugin-management, and recording UIs](tasks/P22-mixer-plugin-recording-ui.md) | blocker/XL | P11, P13, P15, P16, P17, P20 | P23, P30, P34 |
| [P23 Drum machine, step sequencer, clip-launcher, browser/inspector, and export UIs](tasks/P23-drums-patterns-browser-export-ui.md) | high/XL | P08, P16, P21, P22 | P30, P34 |
| [P30 Accessibility: VoiceOver/AX wiring, keyboard operability, contrast, reduced-motion](tasks/P30-accessibility-voiceover-keyboard.md) | high/L | P19, P21, P22, P23 | — |
| [P31 Localization/i18n scaffold + a Spanish (es) localization](tasks/P31-localization-i18n-spanish.md) | medium/M | P19 | — |
| [P34 In-app onboarding, project templates/chooser, and contextual help](tasks/P34-onboarding-templates-help.md) | low/M | P19, P21, P22, P23 | — |

### Phase 6 — MCP-Over-The-Wire Integration
Make AI delegation real: a standards-compliant server and a live bridge into the running DAW.

| Task | Sev/Effort | Depends on | Blocks |
| --- | --- | --- | --- |
| [P24 Standards-compliant MCP/JSON-RPC server with edit/render/orchestration tools and correct JSON](tasks/P24-mcp-json-rpc-server.md) | blocker/L | P03, P07, P09 | P25 |
| [P25 Live MCP-to-running-DAW bridge, OS daemon lifecycle, sandbox confinement, addressable undo registry](tasks/P25-live-mcp-daw-bridge-daemon.md) | blocker/XL | P05, P19, P24 | P27 |

### Phase 7 — Verification, Performance, and Release
Prove it, then ship it.

| Task | Sev/Effort | Depends on | Blocks |
| --- | --- | --- | --- |
| [P26 Determinism, plugin-hosting, audio-correctness, and realtime-deadline verification suite + CLI](tasks/P26-determinism-and-rt-verification.md) | blocker/L | P02, P06, P12, P14, P17, P18, P21, **P28**² | P27 |
| [P27 Release pipeline: signing, notarization/stapling, hardened runtime, Info.plist, provenance, crash reporting, SBOM, docs](tasks/P27-release-signing-notarization.md) | blocker/L | P11, P25, P26 | P32 |
| [P32 Crash-reporting consent, privacy policy, and telemetry opt-in surface](tasks/P32-crash-reporting-consent-privacy.md) | medium/S | P27 | — |

² Per the audit correction, P26's time-stretch/pitch/warp correctness tests depend on **P28** (the
DSP they assert); without P28 those tests would exercise nonexistent behavior.

## Full Task List (dependency order)

| # | Task | Phase | Sev/Effort |
| --- | --- | --- | --- |
| P01 | Format integrity + durable save | 0 | blocker/M |
| P02 | Test framework + fuzz harness | 0 | high/M |
| P03 | Unified command history | 0 | blocker/XL |
| P04 | Live CoreAudio realtime thread | 1 | blocker/L |
| P05 | Audio-graph snapshot + SPSC queue | 1 | blocker/M |
| P06 | RT-safety harness + TSan | 1 | blocker/L |
| P07 | Generic multitrack authoring API | 2 | blocker/XL |
| P08 | Binary sidecar format + autosave | 2 | blocker/L |
| P09 | Faithful GraphCompiler render | 2 | blocker/L |
| P10 | Streaming WAV + recording I/O | 2 | medium/M |
| P28 | Warp / time-stretch / pitch DSP | 2 | blocker/L |
| P29 | Project sample-rate independence | 2 | blocker/M |
| P11 | Plugin host + scanner | 3 | blocker/XL |
| P12 | Plugin graph node + RT params | 3 | blocker/XL |
| P13 | Plugin state + editor windows | 3 | blocker/L |
| P14 | Plugin delay compensation + freeze | 3 | high/L |
| P15 | Buses / sends / strips / metering | 4 | high/L |
| P16 | Live MIDI I/O routing | 4 | high/L |
| P17 | Live recording / monitoring / punch | 4 | blocker/L |
| P18 | Sample-accurate automation | 4 | medium/M |
| P33 | Comp-segment rendering | 4 | medium/M |
| P19 | Interactive DAW shell | 5 | blocker/L |
| P20 | Transport bar + engine bridge | 5 | blocker/L |
| P21 | Arrangement timeline + clip editing | 5 | blocker/XL |
| P22 | Mixer / plugin / recording UI | 5 | blocker/XL |
| P23 | Drums / patterns / browser / export UI | 5 | high/XL |
| P30 | Accessibility (VoiceOver / keyboard) | 5 | high/L |
| P31 | Localization / i18n (Spanish) | 5 | medium/M |
| P34 | Onboarding / templates / help | 5 | low/M |
| P24 | MCP / JSON-RPC server | 6 | blocker/L |
| P25 | Live MCP↔DAW bridge + daemon | 6 | blocker/XL |
| P26 | Determinism + RT verification suite | 7 | blocker/L |
| P27 | Release: signing / notarization | 7 | blocker/L |
| P32 | Crash-report consent + privacy | 7 | medium/S |

## How To Use This Plan

1. Pick a task whose dependencies are all complete and verified.
2. Read its task file in [`tasks/`](tasks/) **and** the architecture/schema docs it references.
3. Implement, add the tests/verification it specifies, and run its Acceptance Gates.
4. Honor the [Agent Execution Rules](../process/agent-execution-rules.md): do not narrow scope to an
   MVP, do not bypass undo/redo, do not mutate state outside the command layer, and keep the build
   open-source-distributable.
