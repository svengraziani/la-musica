# 032 Mixer, Routing, And Automation UI

## Objective

Build the production mixer, routing, metering, and automation surfaces.

## Dependencies

- 031 Plugin Hosting And Plugin UI.

## Deliverables

- Channel strips for audio, MIDI, instrument, group, return, master, and hardware I/O tracks.
- Volume, pan, mute, solo, record arm, input monitor, phase invert, inserts, sends, meters, and
  fader groups.
- Routing matrix for outputs, sidechains, buses, sends, and hardware outputs.
- Peak and RMS/LUFS metering where practical, clipping indicators, hold, and reset.
- Automation lanes, parameter selection, read/write/touch/latch/trim/off modes, and curve editing.

## Acceptance Gates

- Routing graph prevents unsafe feedback loops.
- Meters match fixture signal levels within tolerance.
- Automation playback is deterministic across buffer sizes.
- Writing automation during playback creates undoable command batches.

## Notes For Agents

Mixer changes must update the audio graph without interrupting realtime playback.
