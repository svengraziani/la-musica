# MCP Edit Tools Schema V1

Edit tools wrap command-layer operations. They must not mutate session state except through `lamusica::commands`.

Every edit result includes:

- `schemaVersion`
- `commandId`
- `auditId`
- `validationOk`
- `applied`
- `undoAvailable`
- `redoAvailable`
- `confirmationRequired`
- `confirmationToken`
- `message`
- `preview`

Mutation requires an attached project with the `edit` capability.

Destructive tools such as `remove_clip` return `confirmationRequired: true` from preview/apply
attempts and do not mutate until the caller repeats the apply request with the matching
`confirmationToken`.

Undo and redo flows return the same result envelope so MCP-applied edits remain in the shared UI
command history.

MIDI and automation tools use the same result envelope with store-backed histories for operations
such as add note, quantize, transpose, edit note, add automation point, and captured automation
writes.

Mixer tools use the same result envelope with store-backed histories for channel mix changes such
as volume, pan, mute, and solo.
