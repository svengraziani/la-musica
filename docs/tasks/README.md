# Remaining Task Index

This index tracks the remaining work needed to turn the current bootstrap repository into the
intended full macOS DAW. The previous 001-025 bootstrap plan is no longer the active execution
order: much of it has been implemented as first-track/session scaffolding, and some items were
intentionally deferred until JUCE and production UI dependencies are pinned.

> **⚠️ Superseded ordering — read the production roadmap first.** A deep code audit (2026-05-28)
> found the codebase is a `scaffold`: a well-tested headless offline session/render/MCP engine with
> no live audio, no real plugin hosting, no interactive UI, and a stub MCP transport. The 026–036
> epics below are **UI-first and assume a working engine that does not exist**. The authoritative,
> dependency-ordered, code-grounded production plan now lives under
> [`docs/planning/`](../planning/production-roadmap.md): see the
> [Production Roadmap](../planning/production-roadmap.md) (tasks P01–P34) and the
> [Production Readiness Assessment](../planning/production-readiness-assessment.md). Treat 026–036
> as high-level background acceptance criteria; each P-task records its mapping back to them.

For task-level choices and tradeoffs, use the
[Decision And Compromise Register](../planning/decision-compromise-register.md).

## Current Baseline

- The repository, CMake presets, app bundle target, MCP daemon target, CLI target, libraries, tests,
  fixtures, packaging scaffolding, and developer docs exist.
- `LaMusica.app` is now a JUCE 8.0.13 app shell supplied through `LAMUSICA_JUCE_PATH`.
- The previous Cocoa bootstrap path has been demoted in favor of the JUCE product shell.
- `lamusica_daw_smoke` keeps the noninteractive app-session smoke workflows out of the product app.
- Session, command, audio render, project format, first-track, MCP bridge, and packaging work exists
  as scaffolding, but the production DAW UI and several professional workflows are still missing.

## Active Remaining Tasks

| Order | Task | Depends On |
| --- | --- | --- |
| 027 | [Replace Bootstrap Cocoa Shell With DAW Shell](027-production-daw-shell.md) | Current baseline |
| 028 | [Arrangement Timeline UI](028-arrangement-timeline-ui.md) | 027 |
| 029 | [Audio Clip Editing UI And Waveforms](029-audio-clip-editing-ui-waveforms.md) | 028 |
| 030 | [MIDI, Piano Roll, Drum Machine, And Patterns UI](030-midi-piano-roll-drums-patterns-ui.md) | 027, 028 |
| 031 | [Plugin Hosting And Plugin UI](031-plugin-hosting-ui.md) | 026, 027 |
| 032 | [Mixer, Routing, And Automation UI](032-mixer-routing-automation-ui.md) | 031 |
| 033 | [Browser, Assets, Import, Recording, And Export UX](033-browser-assets-import-record-export-ux.md) | 027, 029 |
| 034 | [Production MCP App Integration](034-production-mcp-app-integration.md) | 027, 028, 032 |
| 035 | [Performance, Stress, And Realtime Verification](035-performance-stress-realtime-verification.md) | 028-034 |
| 036 | [Release Hardening, Signing, Docs, And Examples](036-release-hardening-signing-docs-examples.md) | 035 |

## Retired From Active Planning

The old 001-025 task files are retained as historical source material only. Do not treat them as the
current task order. When a remaining task overlaps an old task, follow the 026-036 task file and use
the older file only for background acceptance criteria.

Use [000-task-template.md](000-task-template.md) for any new active task added later.
