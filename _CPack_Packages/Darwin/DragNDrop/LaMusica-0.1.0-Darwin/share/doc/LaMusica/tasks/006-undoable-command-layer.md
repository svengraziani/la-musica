# 006 Undoable Command Layer

## Objective

Create the deterministic command layer used by UI, CLI, and MCP for all session mutations.

## Dependencies

- 005 Session Model And Project Format.

## Deliverables

- Command interface with validate, preview, apply, undo, redo, serialize, and audit metadata.
- Command registry for track, clip, MIDI, automation, plugin, routing, and project operations.
- Transaction support for multi-step edits.
- Undo history persistence strategy.
- Tests for command determinism and rollback.

## Acceptance Gates

- Every implemented edit command supports undo and redo.
- Failed validation leaves session state unchanged.
- Serialized commands replay deterministically on fixture projects.
- MCP and UI can share the same command APIs.

## Notes For Agents

Do not let UI code mutate session state directly.
