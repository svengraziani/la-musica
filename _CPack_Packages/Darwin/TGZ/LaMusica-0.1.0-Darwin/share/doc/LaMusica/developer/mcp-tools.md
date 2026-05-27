# MCP Tools

LaMusica MCP tools are capability-scoped and project-scoped.

## Capabilities

- `read_only`: query project, transport, selection, mixer, assets, plugins, automation, and render capabilities.
- `edit`: preview, apply, undo, and redo command-backed edits.
- `render`: run bounded analysis, bounce, transform, mix export, stem export, and render jobs.
- `import_export`: register generated files as project assets, including bounce-in-place.
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

The daemon must reject unknown shell-like commands and must not expose arbitrary filesystem or
process execution.

## Schemas

- Query tools: `docs/schemas/mcp-query-tools-v1.md`
- Edit tools: `docs/schemas/mcp-edit-tools-v1.md`
- Render tools: `docs/schemas/mcp-render-tools-v1.md`
- Workflow plans: `docs/schemas/mcp-workflow-plans-v1.md`

Any incompatible schema change requires a new schema version and release-note entry.
