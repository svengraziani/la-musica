# 014 Plugin Hosting

## Objective

Host third-party instruments and effects safely and professionally.

## Dependencies

- 004 Audio Engine Core.
- 006 Undoable Command Layer.

## Deliverables

- Plugin scanning, cache, validation, blacklist, and rescan UI.
- Audio Unit and VST3 hosting where supported by licensing and platform constraints.
- Plugin insert chain, instrument slots, preset handling, parameter discovery, and plugin editor windows.
- Crash containment strategy for scanning and optional out-of-process hosting research.
- Tests with known test plugins or mocks.

## Acceptance Gates

- Bad plugins cannot prevent normal app launch.
- Plugin state saves, reloads, and participates in undoable insert/remove/reorder commands.
- Parameter automation IDs remain stable across reloads.

## Notes For Agents

Record any SDK licensing constraints before enabling plugin formats.
