# 033 Browser, Assets, Import, Recording, And Export UX

## Objective

Complete user-facing asset, import, recording, and export workflows.

## Dependencies

- 027 Replace Bootstrap Cocoa Shell With DAW Shell.
- 029 Audio Clip Editing UI And Waveforms.

## Deliverables

- Browser for project media, user folders, plugin presets, drum kits, templates, and recent files.
- Asset collect, relink, missing-file repair, preview, tags, favorites, and search.
- Background analysis for waveform, loudness, tempo, key, transients, and duration.
- Recording UI for input selection, monitoring, pre-roll, count-in, punch in/out, and take handling.
- Export UI for mix, loop, selected range, selected tracks, stems, and package export.

## Acceptance Gates

- Missing assets are detected and recoverable.
- Background analysis cannot block realtime playback.
- Recorded files align with the project timeline within documented latency tolerance.
- Exported mix and stems match the offline render path.

## Notes For Agents

Do not scan arbitrary user folders without explicit user selection and clear privacy behavior.
