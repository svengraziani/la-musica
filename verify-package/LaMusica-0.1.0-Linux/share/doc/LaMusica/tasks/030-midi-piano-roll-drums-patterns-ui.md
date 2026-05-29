# 030 MIDI, Piano Roll, Drum Machine, And Patterns UI

## Objective

Complete the musical MIDI editing surfaces needed for composition.

## Dependencies

- 027 Replace Bootstrap Cocoa Shell With DAW Shell.
- 028 Arrangement Timeline UI.

## Deliverables

- Piano roll with note draw, select, move, resize, split, mute, velocity, controller lanes, audition,
  fold mode, scale highlighting, ghost notes, and event list editing.
- MIDI device UI for inputs, outputs, monitoring, recording, overdub, and replace.
- Drum rack with pads, sample assignment, choke groups, mute/solo, pad colors, velocity layers, and
  per-pad routing.
- Step sequencer with variable length, resolution, swing, probability, velocity, ratchets, ties,
  slides, accents, and per-lane length.
- Pattern clips and conversion between pattern clips and MIDI clips.

## Acceptance Gates

- MIDI edits and transforms are command-backed and deterministic.
- Dense MIDI clips remain responsive.
- Pattern playback is sample-aligned and deterministic under fixed seed.
- Drum presets remain portable when assets are collected.

## Notes For Agents

Bundled sounds and MIDI examples must have clear redistribution rights.
