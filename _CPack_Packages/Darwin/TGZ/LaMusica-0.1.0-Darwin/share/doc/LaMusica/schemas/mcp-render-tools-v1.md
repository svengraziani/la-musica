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

Analysis jobs return a `wav_analysis` result manifest with frame count, channels, sample rate,
bit depth, peak, RMS, and LUFS estimate. Bounce jobs return a `graph_bounce` result manifest with
range, format, and normalization peaks.

Normalize and reverse jobs return an `audio_transform` result manifest with input/output paths,
operation name, peak values, and `explicitExport: true`.

Project mix and stem exports return `project_mix_export` and `stem_export` manifests with
`explicitExport: true`. Stem manifests include one item per rendered track.

Bounce-in-place jobs require `render` and `import_export` capabilities. They return a
`bounce_in_place` manifest with `assetRegistered: true`, add the generated WAV to the project asset
list, and place a new audio clip that references that asset.

Jobs that would overwrite an existing output fail without writing and return a confirmation token.
Callers must repeat the request with overwrite enabled and the matching token.
