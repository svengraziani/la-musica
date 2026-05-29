# Agent Execution Rules

## Execution Order

Agents execute only the active remaining tasks listed in
[Task Index](../tasks/README.md). A task may start only when all listed dependencies are complete
and verified. Historical task files that are no longer listed in the active table are reference
material, not execution targets.

## Required Work Per Task

For each task:

1. Read the task file and all referenced architecture documents.
2. Create or modify only the files required by the task.
3. Add tests or verification tools matching the task risk.
4. Run the task's acceptance gates.
5. Update the task file with evidence only if the repository convention later adds status tracking.

## Completion Standard

A task is complete when every deliverable exists, every acceptance gate passes, and the produced behavior matches the user-facing requirement. Passing compilation alone is not enough for feature tasks.

## Determinism Rules

- Prefer generated project files only when the generator input is committed.
- Project/session tests must compare stable structured data.
- Audio tests must use fixed sample rates, seeds, buffers, and fixture media.
- AI/MCP tools must expose deterministic command previews and validation failures.

## Change Discipline

- Do not narrow product scope to an MVP.
- Do not bypass undo/redo for editor actions.
- Do not add MCP tools that mutate state outside the command layer.
- Do not introduce runtime dependencies that prevent open-source distribution.
