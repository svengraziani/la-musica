# 019 MCP Daemon Foundation

## Objective

Create the local system-level MCP daemon that exposes the DAW through safe, scoped tools.

## Dependencies

- 006 Undoable Command Layer.

## Deliverables

- `lamusica-mcpd` process with install, launch, stop, health, and logging behavior.
- Local authenticated connection between app and daemon.
- Capability model: read-only, edit, render, import/export, plugin-control, orchestration.
- Project-scoped session attachment and detachment.
- MCP protocol tests with fixture clients.

## Acceptance Gates

- Daemon starts and reports health without launching a project.
- Daemon cannot mutate a project without an active scoped capability.
- Lost app/daemon connection recovers without corrupting session state.

## Notes For Agents

The daemon must not expose shell execution or unrestricted filesystem browsing.
