# Versioning Policy

LaMusica uses semantic versioning after the first public release.

- `MAJOR`: incompatible project format, plugin state, command, or MCP schema changes.
- `MINOR`: backward-compatible features.
- `PATCH`: bug fixes and documentation updates.

Project format and MCP schemas must carry their own schema versions and migration notes.

## Project Format Migrations

- Current project manifest schema: `3`.
- Schema `0` is treated as a legacy bootstrap format. The loader migrates `projectName` or
  `name` into the v1 `name` field, upgrades `schemaVersion` to `1`, and creates default tempo
  and time-signature metadata when those fields were absent.
- Schema `1` manifests are upgraded to schema `2` with `projectSampleRate = 48000.0`.
- Schema `2` manifests are upgraded to schema `3` with empty comp `takeLanes` and `comps`.
- Schema `3` manifests must include all required top-level arrays from
  `docs/schemas/project-v3.schema.json`; missing arrays are validation errors, not implicit
  defaults.
- Future schema changes must add a deterministic migration in `migrateProjectManifest` and unit coverage for migrated state.
