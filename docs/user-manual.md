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
Input latency compensation is based on measured impulse alignment and is stored with recording
plans and committed takes so timeline placement is derived from calibration data rather than a
guess.

## Application Preferences

The native shell keeps preferences for the selected audio device, enabled MIDI inputs, plugin
search paths, MCP availability and mutation scope, keyboard shortcuts, user-folder scanning privacy,
and diagnostics sharing. MCP project mutation cannot be enabled unless MCP itself is enabled.
Menu commands route through the focused primary panel: Browser, Timeline, Inspector, Mixer, or
Transport. View commands move focus to their target panel, project commands require an open project,
and transport commands focus the transport area before toggling playback state.

## Editing

Arrangement edits are nondestructive. Clip move, trim, split, slip, duplicate, fade, gain, mute,
reverse, label, and delete operations are undoable. Clip fades, source offset, reverse playback,
gain, clip gain envelopes, and crossfades are rendered from metadata so source media is not
rewritten unless the user explicitly bounces to a new asset.
Take lanes store alternate clip performances and comp segments select ranges from those takes
without copying or rewriting source audio.
Timeline organization includes color labels, track folders, and arranger sections. Snapping can
target sample or beat grids, clip edges, markers, and detected transient positions when analysis
data is available.
Timeline layout uses stable track-header, lane, clip, ruler, marker, loop-region, and playhead
geometry derived from the visible sample range, so zooming and scrolling do not resize lanes or
shift labels unpredictably.
Warped audio can be retargeted to new tempos while preserving editable markers, extracted groove
offsets, pitch settings, quality mode, and render-cache identity. Preview and offline warp rendering
use the same mapping and interpolation path so timing and samples can be compared within a
documented tolerance.
Tempo conforming can seed editable warp markers from detected transients plus clip boundaries, so
later marker edits remain nondestructive and naturally invalidate stale processed renders.

## MIDI, Drums, And Sequencing

MIDI clips support note editing, split, quantize, transpose, velocity changes, lengths, and legato.
Drum clips also carry CC, pitch bend, aftertouch, program changes, and metadata so piano-roll
controller lanes can be edited independently from notes through undoable commands. MIDI note timing
can be represented in samples or PPQ against an explicit sample-rate and tempo context, so musical
edits and transport playback can round-trip predictably. MIDI recording
supports overdub and replace commits, with optional quantization while preserving recorded note lengths. MIDI device setup tracks
discovered devices, enabled inputs, per-track output routes, and internal/send/receive clock modes.
Piano-roll views support fold-to-used-pitches, scale highlighting, chord labels, ghost notes,
event-list precision editing, note audition events, and automation-linked controller lanes that
render automation points without changing note selection. The piano-roll surface uses deterministic
keyboard rows, sample-grid lines, note rectangles, and controller-lane bounds derived from the
visible range, so dense clips can be culled to visible notes while pointer note drawing maps back to
sample and pitch values.
Dragging a project audio asset onto a drum pad assigns it as a portable velocity layer on that pad;
matching velocity ranges are replaced, while new ranges are added.
Drum patterns support pads, choke groups, velocity layers, sample start/end, reverse playback,
pitch, gain, envelopes, simple filtering, swing, ratchets, probability, independent cycling
per-lane lengths, tied-step sustain, arrangement placement, and conversion to and from MIDI.
Placed pattern clips can emit buffer-relative MIDI playback events for a requested transport sample
range, keeping online playback and offline render sample-aligned.
Step-grid edits are command-backed so pattern changes can be undone and later exposed through MCP
orchestration.
Drum-machine presets save pad colors, routing, choke groups, playback controls, velocity-layer
asset references, and license metadata so starter kits can be portable without bundling unclear
sample assets. Preset collection enumerates every referenced sample by pad and rewrites velocity
layers to collected relative asset IDs, rejecting incomplete mappings instead of producing a preset
that only works on the original machine.

## Mixing, Plugins, And Automation

Mixer channels support routing, sends, volume, pan, mute, solo, record arm, input monitoring, phase,
meters, and fader groups. Plugin references and automation lanes are stored in the project and can
be queried by MCP tools. Plugin insert chains and presets save parameter values with stable
automation addresses so sessions can reload consistently. Plugin parameter discovery exposes those
addresses from validated plugin metadata before automation lanes bind to them. Instrument slots
accept only discovered instrument plugins, and plugin editor windows save open/closed state plus
geometry for reload. Plugin format availability is explicit: built-in devices are always available,
Audio Units require macOS runtime support, and VST3 is gated until SDK availability and license
acceptance are recorded. Automation playback supports read, write, touch, latch, trim, and off modes.
Read-like modes drive mixer and plugin parameters directly,
trim offsets the current playback value, and off leaves the live parameter unchanged. Clip
automation can drive gain, mute, reverse, fade, and source-offset metadata. Project files persist
automation target kinds, modes, defaults, regions, points, and curve types for plugin parameter
recall.
Automation parameter pickers are built from mixer channels, plugin insert metadata, instrument
slots, and clip render metadata so lanes can bind to current session targets without requiring a UI
panel to stay open.
Automation evaluation is sample-based, so chunked playback buffers produce the same parameter values
as one continuous offline block.
Timeline automation views can request only the points inside the visible sample range, so drawing
and edits do not depend on hidden UI panels or off-screen lane data.
Automation point edits reject negative sample positions so playback data stays on the project
timeline.
Mixer routing, sidechain, and send edits are validated and compiled into replacement graph update
plans before they are published to playback, so unsafe feedback is rejected off the realtime path.
The routing matrix can preview route, send, and sidechain availability for every channel pair,
including feedback warnings, before mutating mixer state.

## Rendering

Render tools can export mixes, batch mix variants, stems, selected ranges, normalized or reversed
WAV files, bounce-in-place assets, and frozen track renders. Export options include sample rate,
channel count, PCM16 or PCM24 bit depth, peak normalization, and deterministic triangular
dithering.
Destructive overwrites require confirmation tokens.

## Browser And Media Analysis

Project assets track tags, favorites, missing-file state, analysis metadata, and waveform overview
caches. Relinking an asset invalidates stale analysis so previews and agents do not rely on data
from an old file path. Media analysis jobs are scheduled separately from playback and completed
back into the asset catalog when results are ready.
The current built-in importer decodes PCM16 WAV files, copies collected media into the project
`Assets` folder, and stores waveform and analysis metadata during registration.
User folders must be explicitly granted before the browser treats their files as available.
Folder scans are planned from those grants rather than arbitrary paths, and collected imports choose
non-conflicting names under the project `Assets` folder so portable projects do not overwrite
existing media.
Recent browser items are tracked by type, and drag/drop plans validate whether an asset can target
the timeline, drum pads, sampler slots, or plugin areas before mutating the project.
The browser view groups catalog data into project media, granted user folders, plugin presets, drum
kits, templates, and recent files so project-local and explicitly granted sources stay distinct.

## Current Command-Line Workflows

- Validate a project: `lamusica_cli validate fixtures/empty.Project.lamusica`
- Render a test tone: `lamusica_cli render-test-tone /tmp/lamusica-test-tone.wav`
- Check daemon health: `lamusica_mcpd`

## MCP

Attach MCP clients with the narrowest capability set needed. Read-only tools cannot mutate
projects. Edit tools return previews and validation results before applying. Render and
import/export tools are required for generated media.
