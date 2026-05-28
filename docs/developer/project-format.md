# Project Format

LaMusica projects are directory bundles with the `.Project.lamusica` suffix. The bundle keeps
portable session metadata in `project.json` and stores collected media under project-relative
asset paths.

## Bundle Layout

Required file:

- `project.json`: UTF-8 JSON manifest using schema version `1`.

Recommended directories:

- `Audio/`: collected audio media owned by the project.
- `MIDI/`: collected or exported MIDI data.
- `Renders/`: user-created bounces and exports.
- `Analysis/`: rebuildable waveform and media-analysis caches.

Generated caches must be safe to delete. Source media referenced by committed clips must remain
under explicit project-relative paths or be represented by an empty `assetId` for generated test
fixtures.

## Manifest Contract

The authoritative v1 schema is `docs/schemas/project-v1.schema.json`. A v1 manifest includes:

- `schemaVersion`
- `name`
- `tempoMap`
- `timeSignatures`
- `markers`
- `assets`
- `tracks`
- `clips`
- `midiClips`
- `routing`
- `plugins`
- `automation`
- `mcpAuditLog`

Missing required arrays are validation errors. Loaders must not silently synthesize absent v1
fields except through an explicit migration path.

## Assets

Asset paths must be relative to the project bundle, must not be empty for real imported media, and
must not contain parent-directory traversal. Example and tutorial fixtures may use generated clips
with an empty `assetId` when no external media is required.

Only redistributable assets may be committed to fixtures or shipped in packages. Placeholder drum
kits must record that bundled sample assets are not included.

## Migrations

`migrateProjectManifest` is the only place that should upgrade older manifests. Current behavior:

- schema `0` bootstrap manifests are upgraded to schema `1`;
- `projectName` or `name` is preserved as the v1 project name;
- default tempo and time-signature metadata is added only during that migration.

Future schema changes must add deterministic migration code, unit coverage, schema documentation,
and release notes.

## Validation

Use the CLI before shipping fixtures or packages:

```sh
build/unix-release/tools/cli/lamusica_cli validate fixtures/empty.Project.lamusica
build/unix-release/tools/cli/lamusica_cli verify-examples fixtures/examples
build/unix-release/tools/cli/lamusica_cli verify-examples fixtures/tutorials
```
