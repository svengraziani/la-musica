# 013 Step Sequencer And Patterns

## Objective

Build pattern-based sequencing for drums, basslines, and melodic ideas.

## Dependencies

- 012 Drum Machine.

## Deliverables

- Step grid with variable length, resolution, swing, probability, velocity, ratchets, ties, slides, accents, and per-lane length.
- Pattern clips on the arrangement timeline.
- Conversion between pattern clips and MIDI clips.
- Pattern chaining and duplicate/variation tools.
- Tests for deterministic pattern playback.

## Acceptance Gates

- Pattern playback is sample-aligned to transport.
- Pattern-to-MIDI conversion round trips expected musical data.
- Probability can be deterministic under fixed seed for tests and renders.

## Notes For Agents

Expose pattern operations through the command layer so MCP can later orchestrate them.
