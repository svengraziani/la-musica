# MCP Render Tools Schema V1

Render jobs return:

- `schemaVersion`
- `jobId`
- `status`
- `progress`
- `outputPath`
- `message`
- `confirmationToken`
- `resultManifest`

Render tools require an attached project with the `render` capability. Generated files must be explicit exports or registered project assets.
Job ids are stable queue identifiers. A request with an id already present in the queue fails
without replacing the existing job. Cancellation only applies to queued or running jobs; completed
and failed jobs remain immutable status records.

Analysis jobs return a `wav_analysis` result manifest with frame count, channels, sample rate,
bit depth, peak, RMS, LUFS estimate, transient count, tempo/key estimates, waveform validity,
waveform bucket size, waveform bucket count, and `explicitExport`. Analysis of an existing WAV
reports `explicitExport: false`; generated test-tone WAVs report `explicitExport: true`.
Bounce jobs return a `graph_bounce` result manifest with range, format, normalization peaks, and
`explicitExport: true`.

Normalize and reverse jobs return an `audio_transform` result manifest with input/output paths,
operation name, peak values, and `explicitExport: true`.

Project mix, batch mix, and stem exports return `project_mix_export`,
`batch_project_mix_export`, and `stem_export` manifests with `explicitExport: true`. Batch mix
manifests include one item per requested export. Stem manifests include one item per rendered track.

Bounce-in-place jobs require `render` and `import_export` capabilities. They return a
`bounce_in_place` manifest with `assetRegistered: true`, add the generated WAV to the project asset
list, and place a new audio clip that references that asset.

Freeze jobs require `render` and `import_export` capabilities. They return a `freeze_track`
manifest with `assetRegistered: true`, add the frozen WAV to the project asset list, place a frozen
audio clip on the source track, and list the source clips muted by the freeze operation.

Jobs that would overwrite an existing output fail without writing and return a confirmation token.
Callers must repeat the request with overwrite enabled and the matching token. Source-media
transforms follow the same rule when input and output paths are identical.

Long-running render records may remain in `running` state with bounded `progress` below 1.0 and can
be cancelled cleanly before completion.
