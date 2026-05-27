# 015 Mixer And Routing

## Objective

Implement a complete mixer with flexible routing and professional channel workflows.

## Dependencies

- 004 Audio Engine Core.
- 014 Plugin Hosting.

## Deliverables

- Channel strips for audio, MIDI, instrument, group, return, master, and hardware I/O tracks.
- Volume, pan, mute, solo, record arm, input monitor, phase invert, inserts, sends, meters, and fader groups.
- Routing matrix for track outputs, sidechains, buses, sends, and hardware outputs.
- Metering: peak, RMS/LUFS where practical, clipping indicators, hold, and reset.
- Mixer UI tests and audio graph routing tests.

## Acceptance Gates

- Routing graph prevents feedback loops unless explicitly supported and safe.
- Meters match fixture signal levels within tolerance.
- Mixer state saves, reloads, and renders consistently offline.

## Notes For Agents

Mixer changes must update the audio graph without interrupting realtime playback.
