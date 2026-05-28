# 034 Production MCP App Integration

## Objective

Connect the MCP daemon and bridge to the production app with safe, user-visible workflows.

## Dependencies

- 027 Replace Bootstrap Cocoa Shell With DAW Shell.
- 028 Arrangement Timeline UI.
- 032 Mixer, Routing, And Automation UI.

## Deliverables

- Local authenticated app/daemon connection with project-scoped session attachment.
- Capability UI for read-only, edit, render, import/export, plugin-control, and orchestration scopes.
- MCP query, edit, render, and orchestration tools wired to production app state.
- Human approval UI for multi-command agent plans and destructive operations.
- Audit log views and privacy controls.

## Acceptance Gates

- MCP cannot mutate a project without an active scoped capability.
- UI undo history includes MCP-applied edits.
- Lost app/daemon connection recovers without corrupting session state.
- Large projects can be queried without blocking audio playback.

## Notes For Agents

Do not implement MCP-only mutation paths. The DAW command validator remains authoritative.
