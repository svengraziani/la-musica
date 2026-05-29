# 028 Arrangement Timeline UI

## Objective

Build the production arrangement timeline for multitrack editing.

## Dependencies

- 027 Replace Bootstrap Cocoa Shell With DAW Shell.

## Deliverables

- Track headers, lanes, clips, rulers, bars/beats grid, markers, loop region, playhead, and selection.
- Pointer tools for select, range, draw, split, trim, slip, fade, mute, and zoom.
- Snapping to bars, beats, samples, clips, markers, and automation points.
- Scroll, zoom, minimap or overview, and large-session virtualization.
- Command-backed UI tests for core editing gestures.

## Acceptance Gates

- Timeline edits are undoable and redoable through the shared command layer.
- Large fixture sessions remain responsive during scroll and zoom.
- Selection, snapping, and playhead behavior are consistent across audio, MIDI, and automation lanes.

## Notes For Agents

The timeline must render real project data, not static placeholders.
