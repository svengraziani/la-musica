# 005 Session Model And Project Format

## Objective

Define and implement the durable project/session model for tracks, clips, media, routing, tempo, markers, plugins, automation, and metadata.

## Dependencies

- 004 Audio Engine Core.

## Deliverables

- Versioned `Project.lamusica/` directory format.
- Schemas for project manifest, tracks, clips, tempo map, routing, plugins, automation, assets, and MCP audit log.
- Load/save/migration library.
- Golden fixture projects.
- Validation CLI command.

## Acceptance Gates

- Save/load round trips preserve semantic project state.
- Invalid project files produce actionable validation errors.
- Schema version migrations are tested.
- Project files are stable enough for source control diffs.

## Notes For Agents

Use structured parsers and schema validation. Do not invent ad hoc string parsing.
