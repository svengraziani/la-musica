# MCP Query Tools Schema V1

All read-only query tools return compact JSON objects with:

- `schemaVersion`: currently `1`.
- `tool`: stable tool name.
- Tool-specific fields.

Tools:

- `project_summary`
- `tracks`
- `clips`
- `selection`
- `transport`
- `tempo`
- `markers`
- `routing`
- `plugins`
- `automation`
- `assets`
- `render_capabilities`

Protocol clients invoke these as `query <tool> [offset] [limit]`. Paged tools include `offset`,
`limit`, `total`, and `items` fields.

Successful protocol queries append an in-memory read audit entry with an id, tool name, and
capability scope. Query tools must not mutate project state or the project manifest audit log.
