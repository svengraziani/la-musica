# Architecture Baseline

## Architectural Decision

Use a C++/JUCE 8 macOS desktop application as the primary product shell, audio engine, UI framework, plugin host, and distribution target. Add a local system-level MCP daemon as a separate process communicating with the app through a narrow command/event protocol.

## Process Model

- `LaMusica.app`: user-facing DAW, real-time audio engine, editor UI, project session owner.
- `lamusica-mcpd`: local MCP server exposing safe tools to AI agents.
- `lamusica-cli`: command-line utility for validation, rendering, project inspection, and CI automation.

## Core Runtime Modules

- Audio engine: device setup, graph scheduling, transport, latency, buffers, realtime-safe messaging.
- Session model: tracks, clips, routing, plugins, automation, tempo map, markers, assets.
- Edit command layer: undoable deterministic commands for all user and MCP edits.
- UI shell: timeline, piano roll, mixer, browsers, inspectors, plugin windows, preferences.
- Plugin host: Audio Unit v3 and VST3 where legally and technically supported.
- Rendering pipeline: offline bounce, stem export, freeze, preview renders.
- MCP bridge: capability-scoped tools mapped to edit commands and query APIs.

## Realtime Rules

- No allocation, locks, file I/O, logging, plugin scanning, JSON parsing, or MCP work on the audio callback.
- All UI, MCP, disk, and plugin-management work must communicate with the audio thread through lock-free or bounded realtime-safe queues.
- Session mutations must be authored as commands, applied on the owning thread, and mirrored to the audio graph through validated snapshots.

## Project Format

- Directory-based session: `Project.lamusica/`.
- Human-readable manifest for tracks, clips, routing, tempo, automation, and plugin references.
- Assets stored in stable subdirectories.
- Render cache and analysis data separated from source media.
- Schema versioning and migration tests are required from the first saved project format.

## MCP Safety Model

- MCP tools are capability-scoped: read-only, edit, render, file import/export, plugin control, and experimental orchestration.
- Every edit-capable MCP tool returns a preview, validation result, and undo command id.
- Destructive actions require explicit project-local confirmation tokens.
- The daemon must never expose arbitrary filesystem or shell access.
- All MCP actions are journaled in the project audit log.
