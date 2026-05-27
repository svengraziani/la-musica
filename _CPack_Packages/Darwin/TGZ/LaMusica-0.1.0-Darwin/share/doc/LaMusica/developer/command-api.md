# Command API

All user, UI, and MCP mutations must go through command-backed APIs.

## Session Commands

Project-manifest commands implement:

- metadata with `commandId`, `auditId`, and command name
- validation
- preview
- apply
- undo
- serialization

Use `commands::CommandHistory` for apply, undo, and redo so UI and MCP history stay aligned.

## Store-Backed Commands

MIDI, automation, and mixer edits use store-specific command types and MCP history wrappers. They
must still return the same validation and preview information before mutation.

## Rules

- Invalid commands must leave state unchanged.
- Destructive MCP flows require confirmation tokens.
- Do not add MCP-only mutation paths.
- Command previews are advisory; validators are authoritative.
