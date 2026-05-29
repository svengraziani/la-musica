# 020 MCP DAW Query Tools

## Objective

Expose read-only MCP tools for agents to understand DAW state.

## Dependencies

- 019 MCP Daemon Foundation.

## Deliverables

- Tools for project summary, tracks, clips, selection, transport, tempo, markers, routing, plugins, automation, assets, and render capabilities.
- Stable JSON schemas for every tool response.
- Pagination or region filters for large sessions.
- Audit logging for MCP reads where privacy settings require it.
- Contract tests for tool schemas.

## Acceptance Gates

- Query tools cannot mutate session state.
- Large projects can be queried without blocking audio playback.
- Schema changes are versioned and tested.

## Notes For Agents

Prefer explicit tool names and narrow responses over a single broad state dump.
