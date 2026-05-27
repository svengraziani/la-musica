# 004 Audio Engine Core

## Objective

Implement the realtime audio engine foundation: device management, transport, graph scheduling, buffers, clocks, and safe cross-thread messaging.

## Dependencies

- 003 Application Shell.

## Deliverables

- Audio device selection and persistence.
- Realtime-safe graph execution model.
- Transport state: play, stop, record, seek, loop, tempo, time signature.
- Sample-accurate timeline clock with PPQ and sample position conversions.
- Engine test harness with fixed sample-rate and buffer-size fixtures.

## Acceptance Gates

- Playback callback runs without allocation, locks, file I/O, logging, or JSON parsing.
- Transport position remains stable across buffer sizes and sample rates.
- Engine can render silence, metronome, and test oscillator fixtures offline and live.
- Realtime safety checks are documented and enforced by tests where possible.

## Notes For Agents

Any communication into the audio callback must use bounded realtime-safe mechanisms.
