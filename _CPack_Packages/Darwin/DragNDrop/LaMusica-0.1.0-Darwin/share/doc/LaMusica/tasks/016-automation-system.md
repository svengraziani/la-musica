# 016 Automation System

## Objective

Add automation lanes and parameter modulation across mixer, plugins, instruments, and clip parameters.

## Dependencies

- 006 Undoable Command Layer.
- 015 Mixer And Routing.

## Deliverables

- Automation data model for points, curves, steps, ramps, and regions.
- Read, write, touch, latch, trim, and off modes.
- Automation lanes on timeline and mixer-linked parameter selection.
- Sample-accurate playback for automatable parameters.
- Tests for automation interpolation and command edits.

## Acceptance Gates

- Automation playback is deterministic across buffer sizes.
- Writing automation during playback creates undoable command batches.
- Plugin parameter automation survives save/load.

## Notes For Agents

Automation must not require UI panels to be open in order to play back correctly.
