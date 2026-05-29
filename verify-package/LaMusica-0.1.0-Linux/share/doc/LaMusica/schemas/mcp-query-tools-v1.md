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
`offset`, `limit`, `total`, and `items` fields. The daemon clamps `limit` to 500 items per response
so large projects return bounded payloads. Clip range queries use
`query clips_range <startSample> <endSample> [offset] [limit]` and return clips overlapping the
half-open sample range. Automation point queries use
`query automation_range <startSample> <endSample> [offset] [limit]` and return lane items with only
points inside the half-open sample range.

## Tool Contracts

- `project_summary`: `name`, `tracks`, `clips`, `markers`.
- `tracks`: paged `items` with `id`, `name`, `type`.
- `clips`: paged `items` with `id`, `trackId`, `type`, `startSample`, `lengthSamples`.
- `clips_range`: `range` plus paged clip `items`.
- `selection`: `trackIds`, `clipIds`, nullable `range`.
- `transport`: `playing`, `recording`, `loopEnabled`, sample positions, tempo, time signature.
- `tempo`: `items` with `samplePosition` and `bpm`.
- `markers`: paged `items` with `id`, `name`, `samplePosition`.
- `routing`: route endpoints, with mixer responses also including `channels`, `sends`, and
  `sidechains`.
- `plugins`: project plugin references or scan-cache plugin summaries.
- `automation`: paged lane summaries with target and parameter identifiers.
- `automation_range`: `range` plus paged lane `items` containing only in-range `points`.
- `assets`: project asset references or asset-catalog records with analysis, waveform, tags, and
  missing/favorite state.
- `render_capabilities`: boolean support flags for render-related query decisions.

The `routing` tool returns manifest track route endpoints for project manifests. Mixer routing
queries expose channel count plus explicit `routes`, `sends`, and `sidechains` arrays so clients can
inspect the routable mixer graph, send destinations, pre/post-fader send state, and sidechain insert
targets without mutating state.

Successful protocol queries append an in-memory read audit entry with an id, tool name, and
capability scope. Query tools must not mutate project state or the project manifest audit log.
