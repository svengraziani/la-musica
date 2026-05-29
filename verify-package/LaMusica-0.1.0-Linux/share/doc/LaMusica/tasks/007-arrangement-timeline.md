# 007 Arrangement Timeline

## Objective

Build the full arrangement timeline for multitrack composition and editing.

## Dependencies

- 006 Undoable Command Layer.

## Deliverables

- Track headers, lanes, clips, grid, rulers, markers, loop region, playhead, and selection model.
- Tools for select, range, cut, draw, trim, split, slip, fade, mute, and zoom.
- Snapping to bars, beats, frames, samples, clips, markers, and transients when available.
- Grouping, track folders, color labels, markers, and arranger sections.
- UI tests for core editing gestures.

## Acceptance Gates

- Timeline edits are command-backed and undoable.
- Large sessions remain navigable and responsive.
- Selection, snapping, and zoom behave consistently across audio, MIDI, and automation lanes.

## Notes For Agents

Keep layout dimensions stable so labels, clips, and handles do not shift unpredictably during interaction.
