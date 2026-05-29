# Project Format

LaMusica projects are directory bundles with the `.Project.lamusica` suffix. The bundle keeps
portable session metadata in `project.json` and stores collected media under project-relative
asset paths.

## Bundle Layout

Required file:

- `project.json`: UTF-8 JSON manifest using schema version `3`.

Recommended directories:

- `Audio/`: collected audio media owned by the project.
- `MIDI/`: collected or exported MIDI data.
- `Renders/`: user-created bounces and exports.
- `Analysis/`: rebuildable waveform and media-analysis caches.

Generated caches must be safe to delete. Source media referenced by committed clips must remain
under explicit project-relative paths or be represented by an empty `assetId` for generated test
fixtures.

## Manifest Contract

The authoritative v3 schema is `docs/schemas/project-v3.schema.json`. The historical
`project-v1.schema.json` path remains as a compatibility copy for older references, but new tooling
and release checks should consume the versioned v3 filename. A v3 manifest includes:

- `schemaVersion`
- `name`
- `projectSampleRate`
- `tempoMap`
- `timeSignatures`
- `markers`
- `assets`
- `tracks`
- `clips`
- `midiClips`
- `takeLanes`
- `comps`
- `routing`
- `trackMix` (optional on load for compatibility with early schema-1 manifests)
- `plugins`
- `automation`
- `mcpAuditLog`

`projectSampleRate` is persisted in Hz and defaults to `48000.0` for migrated legacy projects.
Missing required arrays are validation errors. Loaders must not silently synthesize absent v3
fields except through an explicit migration path.
`trackMix` entries are keyed by `trackId` and persist track-level volume, pan, mute, and solo state
for graph compilation.
`takeLanes` persist nondestructive take metadata by clip, and `comps` persist the selected take
segments rendered by the graph compiler. Take media is referenced through project `assets`; editing
a comp must not rewrite the source take asset bytes.

## Assets

Asset paths must be relative to the project bundle, must not be empty for real imported media, and
must not contain parent-directory traversal. Example and tutorial fixtures may use generated clips
with an empty `assetId` when no external media is required.

Audio asset catalog records keep the native `sourceSampleRate` and waveform caches record the rate
they were analyzed at. The current import policy preserves source media and converts mismatched-rate
audio during render through the sample-node resampler; offline resample-on-import is reserved for a
future cache-backed conversion path.

Only redistributable assets may be committed to fixtures or shipped in packages. Placeholder drum
kits must record that bundled sample assets are not included.

## Migrations

`migrateProjectManifest` is the only place that should upgrade older manifests. Current behavior:

- schema `0` bootstrap manifests are first normalized to schema `1`;
- `projectName` or `name` is preserved as the v1 project name;
- default tempo and time-signature metadata is added only during that migration;
- schema `1` manifests are upgraded to schema `2` with `projectSampleRate = 48000.0`;
- schema `2` manifests are upgraded to schema `3` with empty `takeLanes` and `comps`.

Future schema changes must add deterministic migration code, unit coverage, schema documentation,
and release notes.

## Validation

Use the CLI before shipping fixtures or packages:

```sh
build/unix-release/tools/cli/lamusica_cli validate fixtures/empty.Project.lamusica
build/unix-release/tools/cli/lamusica_cli verify-examples fixtures/examples
build/unix-release/tools/cli/lamusica_cli verify-examples fixtures/tutorials
```
