# 008 Audio Clip Editing

## Objective

Implement nondestructive audio clip editing for professional arrangement workflows.

## Dependencies

- 007 Arrangement Timeline.

## Deliverables

- Split, trim, crop, duplicate, mute, gain, normalize, reverse, fade in/out, crossfade, and clip envelopes.
- Waveform overview generation and cache invalidation.
- Clip take lanes and comping foundation.
- Sample-accurate edit boundaries.
- Tests using fixed fixture audio.

## Acceptance Gates

- Edits never modify source media unless explicitly bounced to a new asset.
- Undo/redo restores clip state and cache references.
- Crossfades and fades render without clicks in fixture tests.

## Notes For Agents

Prefer nondestructive metadata until a task explicitly requires destructive processing or bounce.
