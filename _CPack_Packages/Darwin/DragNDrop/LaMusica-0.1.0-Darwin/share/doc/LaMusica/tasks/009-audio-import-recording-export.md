# 009 Audio Import, Recording, And Export

## Objective

Support importing media, recording audio, and exporting finished mixes and stems.

## Dependencies

- 008 Audio Clip Editing.

## Deliverables

- Import for common audio formats supported by the chosen framework.
- Recording pipeline with input monitoring, pre-roll, count-in, punch in/out, and take management.
- Offline bounce for mix, selected range, selected tracks, stems, and loop region.
- Export options for sample rate, bit depth, channels, normalization, and dithering where applicable.
- Recording and render tests.

## Acceptance Gates

- Recorded files align with project timeline within defined latency tolerance.
- Exported mix matches offline render fixtures.
- Interrupted recordings recover or discard safely with clear user choice.

## Notes For Agents

Latency compensation must be measured and stored, not guessed.
