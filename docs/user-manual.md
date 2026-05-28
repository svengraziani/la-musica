# LaMusica User Manual

This manual is a living outline until the JUCE application UI is complete.

## Core Workflows

- Create, save, open, and validate projects.
- Import or record audio and MIDI.
- Edit clips, notes, patterns, automation, and mixer state.
- Render exports.
- Use MCP agents through scoped capabilities.

## Installation And First Launch

Install signed public releases from the LaMusica disk image by dragging `LaMusica.app` to
`/Applications`. The app should launch from Finder without Xcode, CMake, or other developer tools.
Unsigned nightly archives are contributor artifacts and may require local security approval.

Packaged command-line tools live beside the app distribution and can validate projects, verify
first-track readiness, or check MCP daemon health. Example and tutorial projects shipped with the
package use generated or empty media references so they can open without external sample packs.

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
and diagnostics sharing. The Preferences window reflects the current session preference state across
Audio, MIDI, Plugins, MCP, Shortcuts, and Privacy tabs. MCP project mutation cannot be enabled
unless MCP itself is enabled.
Menu commands route through the focused primary panel: Browser, Timeline, Inspector, Mixer, or
Transport. View commands move focus to their target panel, project commands require an open project,
transport commands focus the transport area before toggling playback state, and first-track export,
recording, import, transpose, and starter-mix commands are only enabled once the open project passes
the first-track readiness checks. Focused first-track take, import, and collected-track mixer
commands are also routed through the session layer so shortcuts and menu validation share the same
enabled-state rules as the native menus.
New Project creates a reusable first-track starter session with generated drums, generated bass, a
master track, section markers, clips, routing, built-in starter devices, MIDI clip references, and
automation lanes, so the first saved project can be rendered without external assets.
The starter MIDI reference resolves to an eight-note C-minor bass phrase for the generated bass
track, giving the first project concrete editable musical material rather than an empty MIDI slot.
Readiness reports include tempo, meter, section names, section positions, and track-role counts, so
smoke checks verify the starter session is musically shaped, not just structurally present.
The bundled first-song tutorial project uses the same starter devices and automation lanes as New
Project, so examples and newly created sessions exercise the same first-track path.
The native shell surfaces that readiness in its transport, browser, timeline, inspector, and mixer
panels after creating or opening a project, and the Project menu can export the current open mix to
a WAV file from the same session layer.
The Project menu also includes first-track bass transpose commands, which save a persisted MIDI
reference transform and update the generated bass render without changing the clip timing.
Starter mix commands adjust the generated drum and bass clip gains in 3 dB steps. The Project menu
also exposes first-track track-mix commands for generated drums, generated bass, recorded takes, and
imported audio track volume, pan, mute, and solo state. First-track track mix edits persist for
generated and collected tracks, and both edit types feed the same compiled graph used by export.
Undo and Redo are available for first-track session edits such as gain changes, bass transpose, loop
changes, verse extension, recorded-take placement, and imported-audio placement.
The starter project also saves an enabled loop over the first-track arrangement, with menu commands
to restore the Intro loop and export only the current loop region.
Transport play, stop, seek, playhead, and loop-wrap state are owned by the session layer, so the
first-track starter can be auditioned through the same compiled mix graph used for export.
The first-track arrangement can also extend the Intro material into a Verse section, repeating the
starter bass phrase across the longer render while keeping the intro loop available for focused
practice and loop export.
First-track recording creates a project-local WAV asset, adds a Recorded Takes audio track when
needed, places the take as an audio clip, and saves the project so the first session can move from
generated starter material to captured audio.
Reopened projects restore the latest recorded take and imported clip targets, so focused menu edits
remain available after closing and reopening a first-track session.
Before committing a take, LaMusica can prepare a first-track recording plan with the target timeline
start, frame count, count-in length, preroll start, and punch range metadata. The native Record command
uses the loop start when a loop is enabled and otherwise records at the playhead. The command-line
recording command can commit the same planned start, count-in, and punch in/out range that the
planning command previews.
First-track clips can be listed with their clip id, track, type, timeline position, fade, gain,
mute, reverse, source offset, asset id, media path, and media availability. Recorded first-track
takes have a focused listing with their clip id, asset id, timeline start, frame count, mute state,
media path, and media availability. Muting a take is nondestructive, participates in Undo/Redo, and
removes that take from the render graph without deleting the underlying WAV.
Any first-track clip can also be muted nondestructively, including starter clips and imported audio,
so arrangement ideas can be A/B checked without deleting material.
Clip fades are also nondestructive first-track edits, so recorded and imported clips can have their
fade-in and fade-out sample lengths adjusted to smooth take boundaries before export.
Clip reverse is a nondestructive first-track edit as well, allowing recorded or imported clips to
play backward for transitions and texture while keeping the original media unchanged.
Clip timing edits are nondestructive too: recorded and imported clips can be moved on the timeline,
trimmed to a shorter visible length, or given a source offset while the project keeps the original
WAV asset intact.
Clip duplication is also available for first-track arranging, including duplicated starter MIDI
references, recorded takes, and imported audio clips, so repeated hooks or phrases can be built
without re-recording or copying media files.
Unwanted first-track clips can be removed nondestructively as timeline edits. Removing a MIDI clip
also removes its MIDI data reference, while referenced WAV assets stay in project media for undo or
later reuse.
First-track audio import accepts PCM16 WAV files, copies the source into the project `Assets`
folder, adds an Imported Audio track when needed, places the imported clip on the timeline, and
renders that clip through mix, stem, and package exports. The Project menu exposes Import Audio so
the selected WAV lands at the current playhead in the first-track session. It also includes focused
last-import actions for fades, mute toggle, reverse toggle, trim-to-loop,
duplicate-at-playhead, and remove.
First-track stem export writes separate generated drum and bass WAV files for handoff, remixing, or
checking the mix outside the app.
First-track package export writes the full mix, the intro loop, all starter stems, and a portable
project snapshot with copied project-local assets into one handoff folder, along with a package
manifest that records project metadata, render lengths, loop range, stems, imported audio, and
recorded takes so the first session can be checked or shared without repeating separate exports.
Package manifests use paths relative to the handoff folder, so a verified folder can be moved as a
unit. The command-line tools can export and verify that handoff folder with
`lamusica_cli export-first-track-package` and `lamusica_cli verify-first-track-package`; package
verification reports the embedded project snapshot, copied project asset count, recorded take count,
and imported audio count after checking them against the snapshot, including checksums for copied
project-local assets.
The Project menu can also verify that the open project is first-track ready, verify a first-track
package folder, and surface the verified package state in the mixer panel after checking the
manifest, its render lengths and stem counts, and the referenced WAV files, including per-file
checksums when present.
Verification distinguishes editable starter sessions from package-ready sessions: if the intro loop
is cleared, recording, import, arrangement, transpose, and starter-mix actions remain available, and
restoring the Intro loop makes loop and package export ready again.
Verification also checks project-local audio referenced by recorded or imported clips; missing WAV
files mark the session as not media-ready and disable first-track record/package actions until the
media is restored. A missing first-track WAV can be relinked from a valid PCM16 WAV source, which
copies the replacement back into the project bundle and restores readiness when all referenced media
is present again.
The noninteractive first-track workflows used by release checks are available through
`lamusica_daw_smoke --create-first-track MyFirstTrack.Project.lamusica "My First Track"` followed
by `lamusica_daw_smoke --inspect-project MyFirstTrack.Project.lamusica` and
`lamusica_daw_smoke --render-project MyFirstTrack.Project.lamusica first-track.wav`.

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
- Verify first-track readiness: `lamusica_cli verify-first-track-project MyFirstTrack.Project.lamusica`
  prints the starter structure, render, loop, and missing-requirement counts. When verification
  fails, it also lists the missing requirement ids to repair before recording or packaging.
- Create a starter track: `lamusica_cli create-first-track MyFirstTrack.Project.lamusica "My First Track"`
- List editable first-track clips: `lamusica_cli list-first-track-clips MyFirstTrack.Project.lamusica`
- Edit the starter: `lamusica_cli transpose-first-track-bass MyFirstTrack.Project.lamusica 12`,
  `lamusica_cli set-first-track-clip-gain MyFirstTrack.Project.lamusica drum-loop -18`,
  `lamusica_cli set-first-track-track-mix MyFirstTrack.Project.lamusica drums -12 0.25 false false`,
  `lamusica_cli set-first-track-loop-intro MyFirstTrack.Project.lamusica`, and
  `lamusica_cli extend-first-track-verse MyFirstTrack.Project.lamusica`
- List first-track track mix: `lamusica_cli list-first-track-track-mix MyFirstTrack.Project.lamusica`
- Capture or collect audio:
  `lamusica_cli record-first-track-take MyFirstTrack.Project.lamusica 48000` and
  `lamusica_cli import-first-track-audio MyFirstTrack.Project.lamusica vocal.wav 48000`
- Punch-record a section:
  `lamusica_cli record-first-track-take MyFirstTrack.Project.lamusica 48000 0 2 48000 96000`
- Repair missing first-track media:
  `lamusica_cli relink-first-track-audio MyFirstTrack.Project.lamusica imported-audio-1 vocal.wav`
- Render an arrangement: `lamusica_cli render-project fixtures/tutorials/first-song.Project.lamusica first-song.wav`
- Export a first-track handoff folder: `lamusica_cli export-first-track-package MyFirstTrack.Project.lamusica first-track-package`
- Verify a first-track handoff folder: `lamusica_cli verify-first-track-package first-track-package`
- Render a test tone: `lamusica_cli render-test-tone /tmp/lamusica-test-tone.wav`
- Check daemon health: `lamusica_mcpd`

## MCP

Attach MCP clients with the narrowest capability set needed. Read-only tools cannot mutate
projects. Edit tools return previews and validation results before applying. Render and
import/export tools are required for generated media.
