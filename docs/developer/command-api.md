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
Use `registeredProjectCommandNames`, `commandFromSerialized`, and `replaySerializedCommands` for
project-manifest command journaling. Replay stops at the first validation failure and leaves later
commands unapplied.

Audio clip editing commands update manifest metadata. Rendering helpers in
`lamusica/session/AudioClipEditing.hpp` consume that metadata for source offset, reverse playback,
gain, fades, and crossfades without modifying source audio buffers.
Routing commands cover adding and removing manifest routing connections. Both directions validate
track references and preserve undo state so MCP route edits stay in the shared project command
history.

## Store-Backed Commands

MIDI, automation, mixer, plugin insert-chain, and warp-marker edits use store-specific command
types and MCP history wrappers where exposed. They must still return the same validation and
preview information before mutation.

Pattern edits use store-backed commands for adding pattern clips and duplicating deterministic
variations. This keeps pattern generation undoable before those operations are surfaced through
MCP orchestration.

Automation lanes can be selected by target id and parameter id independently of any visible UI.
Playback helpers apply read/write/touch/latch values directly, apply trim as an offset against the
current parameter value, and leave parameters unchanged while automation is off. Mixer, plugin or
instrument insert chains, and clip metadata each have explicit application paths.

Plugin insert commands cover add, remove, reorder, and preset application. They keep the previous
insert or chain order for undo so saved plugin state can participate in the same edit model as
timeline and mixer changes.

Warp marker commands cover add, move, remove, and quantize operations. They mutate only
`WarpState` metadata and keep previous marker or warp-state snapshots for undo.

## Rules

- Invalid commands must leave state unchanged.
- Destructive MCP flows require confirmation tokens.
- Do not add MCP-only mutation paths.
- Command previews are advisory; validators are authoritative.
