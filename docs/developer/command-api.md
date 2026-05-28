# Command API

All user, UI, and MCP mutations must go through command-backed APIs.

## Session Commands

Project-manifest commands implement:

- metadata with `commandId`, `auditId`, and command name
- validation
- preview
- apply
- undo
- redo
- serialization

Use `commands::CommandHistory` for apply, undo, and redo so UI and MCP history stay aligned.
Use `registeredProjectCommandNames`, `commandFromSerialized`, and `replaySerializedCommands` for
project-manifest command journaling. Replay stops at the first validation failure and leaves later
commands unapplied.
`TransactionCommand` groups multi-step project edits as a single history entry. Its validation
simulates each subcommand against a manifest snapshot, so later steps can depend on earlier steps
without mutating real session state during preview.
Serialized transactions include their child command payloads and can be reconstructed through
`commandFromSerialized`, keeping split clips and other compound edits replayable from journals.
Serialized clip-add commands include full render metadata such as type, source offset, fades, gain,
mute, reverse, and asset references so journal replay preserves nondestructive edit state.

Audio clip editing commands update manifest metadata. Rendering helpers in
`lamusica/session/AudioClipEditing.hpp` consume that metadata for source offset, reverse playback,
gain, clip gain envelopes, fades, and crossfades without modifying source audio buffers.
Slip commands change only the clip source offset so the timeline placement and clip length stay
stable while the visible media content moves under the clip.
Clip render-property commands keep gain, mute, and reverse edits in project command history and
serialized journals.
Routing commands cover adding and removing manifest routing connections. Both directions validate
track references and preserve undo state so MCP route edits stay in the shared project command
history.
Project metadata commands cover project renames, timeline markers, and tempo-map event insertion,
so global project edits use the same validation, undo/redo, serialization, and replay path as
track and clip edits.

## Store-Backed Commands

MIDI, automation, mixer, plugin insert-chain, and warp-marker edits use store-specific command
types and MCP history wrappers where exposed. They must still return the same validation and
preview information before mutation.
MIDI transform commands cover quantize, transpose, velocity scaling, humanize, note length, and
legato edits, preserving the previous clip snapshot for undo.
MIDI event commands cover notes, split notes, CC, pitch bend, aftertouch, and program changes so
piano-roll controller lanes and event-list edits can mutate independently from note data.

Pattern edits use store-backed commands for adding pattern clips and duplicating deterministic
variations. This keeps pattern generation undoable before those operations are surfaced through
MCP orchestration.

Automation lanes can be selected by target id and parameter id independently of any visible UI.
Playback helpers apply read/write/touch/latch values directly, apply trim as an offset against the
current parameter value, and leave parameters unchanged while automation is off. Mixer, plugin or
instrument insert chains, and clip metadata each have explicit application paths.
Automation binding helpers enumerate mixer, plugin, instrument, and clip parameters with ranges and
current defaults so UI and MCP selectors do not need hard-coded parameter lists.
Automation write commands expose the captured command batch after apply, and timeline queries can
retrieve lane points in a bounded sample range before drawing or editing visible automation.
Mixer sidechain routes are stored as routing-matrix metadata with source, destination, and target
insert ids. They validate channel references and duplicate ids without being treated as audio edges
for feedback-cycle detection.

Plugin insert commands cover add, remove, reorder, and preset application. They keep the previous
insert or chain order for undo so saved plugin state can participate in the same edit model as
timeline and mixer changes.
Plugin instrument slots are saved separately from effect insert chains and validate against the
plugin scan cache so effect plugins cannot occupy instrument slots. Plugin editor windows persist
open state, pinning, and geometry as reloadable UI state.

Warp marker commands cover add, move, remove, quantize, and groove-apply operations. They mutate
only `WarpState` metadata and keep previous marker or warp-state snapshots for undo.

## Rules

- Invalid commands must leave state unchanged.
- Destructive MCP flows require confirmation tokens.
- Do not add MCP-only mutation paths.
- Command previews are advisory; validators are authoritative.
