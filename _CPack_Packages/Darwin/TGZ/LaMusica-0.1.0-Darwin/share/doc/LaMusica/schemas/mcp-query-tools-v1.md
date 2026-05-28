# MCP Query Tools Schema V1

All read-only query tools return compact JSON objects with:

- `schemaVersion`: currently `1`.
- `tool`: stable tool name.
- Tool-specific fields.

Tools:

- `project_summary`
- `tracks`
- `clips`
- `clips_range`
- `selection`
- `transport`
- `tempo`
- `markers`
- `routing`
- `plugins`
- `automation`
- `automation_range`
- `assets`
- `render_capabilities`

Protocol clients invoke paged tools as `query <tool> [offset] [limit]`. Paged tools include
`offset`, `limit`, `total`, and `items` fields. Clip range queries use
`query clips_range <startSample> <endSample> [offset] [limit]` and return clips overlapping the
half-open sample range. Automation point queries use
`query automation_range <startSample> <endSample> [offset] [limit]` and return lane items with only
points inside the half-open sample range.

The `routing` tool returns manifest track route endpoints for project manifests. Mixer routing
queries expose channel count plus explicit `routes`, `sends`, and `sidechains` arrays so clients can
inspect the routable mixer graph, send destinations, pre/post-fader send state, and sidechain insert
targets without mutating state.

Successful protocol queries append an in-memory read audit entry with an id, tool name, and
capability scope. Query tools must not mutate project state or the project manifest audit log.
