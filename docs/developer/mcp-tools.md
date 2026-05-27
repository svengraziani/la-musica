# MCP Tools

LaMusica MCP tools are capability-scoped and project-scoped.

## Capabilities

- `read_only`: query project, transport, selection, mixer, assets, plugins, automation, and render capabilities.
- `edit`: preview, apply, undo, and redo command-backed edits.
- `render`: run bounded analysis, bounce, transform, mix export, batch export, stem export, freeze, and render jobs.
- `import_export`: register generated files as project assets, including bounce-in-place and track freeze renders.
- `plugin_control`: reserved for plugin-specific control surfaces.
- `orchestration`: create advisory workflow plans.

## Protocol Smoke

```sh
lamusica_mcpd --stdio
health
attach fixtures/empty.Project.lamusica read_only
query project_summary
detach
```

The daemon explicitly denies shell, process, and arbitrary filesystem command surfaces with stable
`forbidden_*` protocol errors and an in-memory denied-request log. Asset queries expose project
catalog summaries only; they are not a general file browser.

## Asset Queries

Asset catalog query responses include file metadata plus optional analysis and waveform summaries:
duration, channels, sample rate, peak, RMS, LUFS estimate, transient count, waveform validity,
bucket size, and bucket count. MCP tools receive summaries only; they do not get arbitrary file
read access through the asset query surface.

## Large Session Queries

Timeline clip queries and automation point queries support bounded sample ranges plus pagination.
Use `query automation_range <startSample> <endSample> [offset] [limit]` when an agent needs point
data for a visible or selected region instead of asking for every automation lane in the project.

## Edit Coverage

MCP edit tools use command-backed histories for timeline, MIDI, automation, mixer, routing, and
plugin insert-chain edits. Plugin insert add/remove/reorder and preset application use the same
result envelope as other edit tools so undo/redo state is visible to clients.

## Schemas

- Query tools: `docs/schemas/mcp-query-tools-v1.md`
- Edit tools: `docs/schemas/mcp-edit-tools-v1.md`
- Render tools: `docs/schemas/mcp-render-tools-v1.md`
- Workflow plans: `docs/schemas/mcp-workflow-plans-v1.md`

Any incompatible schema change requires a new schema version and release-note entry.
