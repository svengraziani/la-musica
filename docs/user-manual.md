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
be committed or discarded. If recording is interrupted while a temporary capture file exists,
LaMusica presents that file as an explicit recover-or-discard decision before it becomes project
media.
Recording plans reject negative punch ranges, and take lanes require a real file path, positive
frame count, and channel count before a take can be added.

## Editing

Arrangement edits are nondestructive. Clip move, trim, split, duplicate, fade, label, and delete
operations are undoable. Clip fades, reverse playback, gain, and crossfades are rendered from
metadata so source media is not rewritten unless the user explicitly bounces to a new asset.
Timeline organization includes color labels, track folders, and arranger sections. Snapping can
target sample or beat grids, clip edges, markers, and detected transient positions when analysis
data is available.
Warped audio can be retargeted to new tempos while preserving editable markers, pitch settings,
quality mode, and render-cache identity. Preview and offline warp render plans use the same mapping
so timing can be compared within a documented sample tolerance.

## MIDI, Drums, And Sequencing

MIDI clips support note editing, quantize, transpose, velocity changes, lengths, and legato. Drum
clips also carry CC, pitch bend, aftertouch, program changes, and metadata so piano-roll controller
lanes can be edited independently from notes. MIDI recording supports overdub and replace commits,
with optional quantization while preserving recorded note lengths. MIDI device setup tracks
discovered devices, enabled inputs, per-track output routes, and internal/send/receive clock modes.
Piano-roll views support fold-to-used-pitches, scale highlighting, chord labels, ghost notes,
event-list precision editing, and note audition events.
Drum patterns support pads, choke groups, velocity layers, sample start/end, reverse playback,
pitch, gain, envelopes, simple filtering, swing, ratchets, probability, independent cycling
per-lane lengths, arrangement placement, and conversion to and from MIDI.
Drum-machine presets save pad colors, routing, choke groups, playback controls, velocity-layer
asset references, and license metadata so starter kits can be portable without bundling unclear
sample assets.

## Mixing, Plugins, And Automation

Mixer channels support routing, sends, volume, pan, mute, solo, record arm, input monitoring, phase,
meters, and fader groups. Plugin references and automation lanes are stored in the project and can
be queried by MCP tools. Plugin insert chains and presets save parameter values with stable
automation addresses so sessions can reload consistently. Automation playback supports read, write,
touch, latch, trim, and off modes. Read-like modes drive mixer and plugin parameters directly,
trim offsets the current playback value, and off leaves the live parameter unchanged. Clip
automation can drive gain, mute, reverse, fade, and source-offset metadata. Project files persist
automation modes, defaults, regions, points, and curve types for plugin parameter recall.
Automation point edits reject negative sample positions so playback data stays on the project
timeline.
Mixer routing and send edits are validated and compiled into replacement graph update plans before
they are published to playback, so unsafe feedback is rejected off the realtime path.

## Rendering

Render tools can export mixes, batch mix variants, stems, selected ranges, normalized or reversed
WAV files, bounce-in-place assets, and frozen track renders. Export options include sample rate,
channel count, PCM16 bit depth, peak normalization, and deterministic triangular dithering.
Destructive overwrites require confirmation tokens.

## Browser And Media Analysis

Project assets track tags, favorites, missing-file state, analysis metadata, and waveform overview
caches. Relinking an asset invalidates stale analysis so previews and agents do not rely on data
from an old file path. Media analysis jobs are scheduled separately from playback and completed
back into the asset catalog when results are ready.
User folders must be explicitly granted before the browser treats their files as available.
Folder scans are planned from those grants rather than arbitrary paths, and collected imports choose
non-conflicting names under the project `Assets` folder so portable projects do not overwrite
existing media.
Recent browser items are tracked by type, and drag/drop plans validate whether an asset can target
the timeline, drum pads, sampler slots, or plugin areas before mutating the project.

## Current Command-Line Workflows

- Validate a project: `lamusica_cli validate fixtures/empty.Project.lamusica`
- Render a test tone: `lamusica_cli render-test-tone /tmp/lamusica-test-tone.wav`
- Check daemon health: `lamusica_mcpd`

## MCP

Attach MCP clients with the narrowest capability set needed. Read-only tools cannot mutate
projects. Edit tools return previews and validation results before applying. Render and
import/export tools are required for generated media.
