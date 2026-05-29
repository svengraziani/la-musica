# MCP Tools

LaMusica MCP tools are capability-scoped and project-scoped.

## Capabilities

- `read_only`: query project, transport, selection, mixer, assets, plugins, automation, and render capabilities.
- `edit`: preview, apply, undo, and redo command-backed edits.
- `render`: run bounded analysis, bounce, transform, mix export, batch export, stem export, freeze, and render jobs.
- `import_export`: register generated files as project assets, including bounce-in-place and track freeze renders.
- `plugin_control`: reserved for plugin-specific control surfaces.
- `orchestration`: create advisory workflow plans.

Workflow plan creation through MCP requires an attached project scoped with `orchestration`.
Generated plans remain advisory: every step still resolves to command previews and must pass the
DAW command validator before application.
Built-in workflows cover section marker arrangement, MIDI harmony, deterministic drum variation,
song-structure labels, and mix-preparation passes.
Workflow templates can be serialized as schema-versioned project or user-library files and reloaded
before resolving them into command-preview plans.

## Protocol Smoke

```sh
lamusica_mcpd --stdio
health
attach fixtures/empty.Project.lamusica read_only
auth <token-from-attach> query project_summary
auth <current-token> detach
```

`health` is intentionally available before project launch or attachment so supervisors can check the
daemon while the DAW is closed. Project-scoped requests use `auth <token> ...`; the daemon issues
the token on `attach`, validates it exactly for queries and session lifecycle commands, and requires
the interrupted-session token for `recover`.

The daemon lifecycle surface is modeled with `install [label]`, `launch`, `stop`, `health`, and
`logs` protocol commands plus matching `lamusica_mcpd --install`, `--launch`, `--stop`,
`--health`, and `--logs` process entry points. Lifecycle events are kept in the daemon log alongside
denied protocol requests.

The daemon explicitly denies shell, process, and arbitrary filesystem command surfaces with stable
`forbidden_*` protocol errors and an in-memory denied-request log. Asset queries expose project
catalog summaries only; they are not a general file browser.

## Asset Queries

Asset catalog query responses include file metadata plus optional analysis and waveform summaries:
duration, channels, sample rate, peak, RMS, LUFS estimate, tempo, key, transient count, waveform
validity, bucket size, and bucket count. MCP tools receive summaries only; they do not get
arbitrary file read access through the asset query surface.

## Large Session Queries

Timeline clip queries and automation point queries support bounded sample ranges plus pagination.
Use `query clips_range <startSample> <endSample> [offset] [limit]` and
`query automation_range <startSample> <endSample> [offset] [limit]` when an agent needs visible or
selected-region data instead of asking for every clip or automation lane in the project.

## Routing Queries

Routing queries expose manifest track route endpoints and mixer routing graph summaries. Mixer
responses include explicit routes, sends, pre/post-fader send state, and sidechain insert targets so
agents can inspect routing decisions without using edit tools or broad state dumps.

## Edit Coverage

MCP edit tools use command-backed validate, preview, apply, undo, and redo flows for timeline,
MIDI, automation, mixer, routing, and plugin insert-chain edits. Timeline cut/remove, trim, split,
move, and duplicate operations use the shared project command history. Plugin insert
add/remove/reorder and preset application use the same result envelope as other edit tools so
undo/redo state is visible to clients. Destructive remove operations, including clip removal, route
removal, and plugin insert removal, require confirmation tokens before mutation. Label edits such as
track rename use the same project command history.

## Render Coverage

MCP render tools expose bounded WAV analysis with loudness and waveform summaries, selected-region
graph bounce, project mix export, batch mix export, stem export, normalize, reverse,
bounce-in-place, and freeze. Generated explicit exports are marked in result manifests; generated
project media from bounce-in-place and freeze requires `import_export` and is registered as project
assets. Existing outputs and source-media overwrites require matching confirmation tokens, and
running render jobs expose progress so they can be cancelled before completion.

## Schemas

- Query tools: `docs/schemas/mcp-query-tools-v1.md`
- Edit tools: `docs/schemas/mcp-edit-tools-v1.md`
- Render tools: `docs/schemas/mcp-render-tools-v1.md`
- Workflow plans: `docs/schemas/mcp-workflow-plans-v1.md`

Any incompatible schema change requires a new schema version and release-note entry.
