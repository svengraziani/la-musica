# 021 MCP Editing Tools

## Objective

Expose safe MCP tools for timeline, MIDI, automation, routing, and mixer edits.

## Dependencies

- 020 MCP DAW Query Tools.
- 006 Undoable Command Layer.

## Deliverables

- MCP edit tools mapped one-to-one or transactionally to command-layer operations.
- Preview, validate, apply, undo, and redo tool flows.
- Confirmation-token flow for destructive operations.
- Tool coverage for cut, trim, split, move, duplicate, quantize, transpose, automate, route, mix, and label operations.
- Contract and replay tests.

## Acceptance Gates

- Every mutation returns command id, audit id, validation result, and undo availability.
- Invalid MCP edits leave session state unchanged.
- UI undo history correctly includes MCP-applied edits.

## Notes For Agents

Do not implement MCP-only mutation paths.
