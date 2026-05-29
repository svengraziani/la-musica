# 029 Audio Clip Editing UI And Waveforms

## Objective

Expose nondestructive audio editing through the timeline with waveform rendering and cache support.

## Dependencies

- 028 Arrangement Timeline UI.

## Deliverables

- Waveform overview generation, invalidation, caching, and progressive rendering.
- UI for split, trim, crop, duplicate, mute, gain, normalize, reverse, fade in/out, crossfade, and
  clip envelopes.
- Take lanes and comping foundation for recorded audio.
- Sample-accurate edit handles and inspector controls.
- Fixture tests for waveform cache, edits, fades, and offline render agreement.

## Acceptance Gates

- Source media is not modified by nondestructive edits.
- Undo/redo restores clip state and waveform/cache references.
- Fades and crossfades render without clicks in fixture tests.

## Notes For Agents

Use structured project state and command APIs. Do not encode clip edits in view-only state.
