# 018 Browser, Assets, And Media Analysis

## Objective

Build the project and media browser with asset management and analysis.

## Dependencies

- 005 Session Model And Project Format.
- 009 Audio Import, Recording, And Export.

## Deliverables

- Browser for project media, user folders, plugin presets, drum kits, templates, and recent files.
- Asset collect, relink, missing-file repair, preview, tags, favorites, and search.
- Background analysis for waveform, loudness, tempo, key, transients, and duration.
- Drag/drop into timeline, drum pads, sampler slots, and plugin areas.
- Tests for asset database and relink behavior.

## Acceptance Gates

- Missing assets are detected and recoverable.
- Background analysis cannot block realtime playback.
- Asset operations preserve project portability.

## Notes For Agents

Do not scan arbitrary user folders without explicit user selection and clear privacy behavior.
