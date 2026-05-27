# 010 MIDI Core

## Objective

Implement MIDI data, routing, devices, recording, playback, quantization primitives, and event processing.

## Dependencies

- 006 Undoable Command Layer.

## Deliverables

- MIDI device discovery, input enablement, output routing, and clock options.
- MIDI clips with notes, CC, pitch bend, aftertouch, program changes, and metadata.
- Quantize, transpose, velocity transform, humanize, legato, and note length operations.
- MIDI recording with overdub and replace modes.
- Deterministic MIDI fixture tests.

## Acceptance Gates

- MIDI playback is sample-aligned with transport.
- Recorded MIDI preserves timing within configured quantization state.
- Transform commands are undoable and deterministic.

## Notes For Agents

Represent MIDI times in musical and sample domains where needed, with explicit conversions.
