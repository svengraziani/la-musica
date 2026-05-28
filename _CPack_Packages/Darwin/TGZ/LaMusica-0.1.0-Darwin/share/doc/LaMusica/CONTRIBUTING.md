# Contributing

LaMusica is planned as a full open-source macOS Digital Audio Workstation with a local MCP daemon for safe AI-assisted production workflows.

## Current Status

The repository is in bootstrap and architecture execution. Follow the numbered tasks in [docs/tasks](docs/tasks/README.md) in order.

## Development Rules

- Preserve the full DAW scope; do not reduce work to an MVP.
- Keep user-facing edits, CLI edits, and MCP edits routed through the command layer.
- Keep realtime audio code free of allocation, locks, file I/O, logging, JSON parsing, and daemon work.
- Add tests or verification fixtures for each behavioral change.
- Document architecture decisions and compromises when they affect later tasks.

## Language Baseline

Use C++23 for LaMusica-owned application and library code when supported by the pinned macOS toolchain. Keep JUCE, plugin SDK, and third-party boundaries compatible with their verified requirements in CI.

Rust may be introduced only for isolated non-realtime services when the FFI and build-system cost is justified.

## Pull Request Expectations

- Reference the numbered task being advanced.
- List acceptance gates run locally.
- Include screenshots or recordings for UI changes when relevant.
- Include fixture updates for project format, audio, MIDI, MCP, and rendering behavior.
