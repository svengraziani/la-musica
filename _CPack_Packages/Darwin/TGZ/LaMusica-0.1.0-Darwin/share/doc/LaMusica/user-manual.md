# LaMusica User Manual

This manual is a living outline until the JUCE application UI is complete.

## Core Workflows

- Create, save, open, and validate projects.
- Import or record audio and MIDI.
- Edit clips, notes, patterns, automation, and mixer state.
- Render exports.
- Use MCP agents through scoped capabilities.

## Recording

Grant microphone permission when macOS asks for it. Recordings are written as project media and can
be committed or discarded.

## Editing

Arrangement edits are nondestructive. Clip move, trim, split, duplicate, fade, label, and delete
operations are undoable.

## MIDI, Drums, And Sequencing

MIDI clips support note editing, quantize, transpose, velocity changes, lengths, and legato. Drum
patterns support pads, choke groups, velocity layers, swing, ratchets, probability, and conversion
to MIDI.

## Mixing, Plugins, And Automation

Mixer channels support routing, sends, volume, pan, mute, solo, record arm, input monitoring, phase,
meters, and fader groups. Plugin references and automation lanes are stored in the project and can
be queried by MCP tools.

## Rendering

Render tools can export mixes, stems, selected ranges, normalized or reversed WAV files, and
bounce-in-place assets. Destructive overwrites require confirmation tokens.

## Current Command-Line Workflows

- Validate a project: `lamusica_cli validate fixtures/empty.Project.lamusica`
- Render a test tone: `lamusica_cli render-test-tone /tmp/lamusica-test-tone.wav`
- Check daemon health: `lamusica_mcpd`

## MCP

Attach MCP clients with the narrowest capability set needed. Read-only tools cannot mutate
projects. Edit tools return previews and validation results before applying. Render and
import/export tools are required for generated media.
