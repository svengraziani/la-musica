# 003 Application Shell

## Objective

Create a native macOS DAW application shell with stable windowing, menu commands, preferences, document lifecycle, and crash-safe startup.

## Dependencies

- 002 Build, Tooling, And CI.

## Deliverables

- `LaMusica.app` target.
- Main window with timeline, inspector, browser, mixer, and transport regions.
- macOS menu bar commands for project, edit, transport, view, audio, MIDI, tools, and help.
- Preferences shell for audio device, MIDI devices, plugins, MCP, keyboard shortcuts, and privacy.
- App lifecycle tests for create/open/save/close project stubs.

## Acceptance Gates

- App launches on supported macOS versions.
- Empty project can be created, saved, reopened, and closed without leaks or crashes.
- Keyboard focus and menu command routing work across primary panels.

## Notes For Agents

The shell must be built as the real product surface, not a temporary demo window.
