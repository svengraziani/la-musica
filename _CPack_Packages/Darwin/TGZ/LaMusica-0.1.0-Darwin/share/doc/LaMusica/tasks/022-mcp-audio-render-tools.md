# 022 MCP Audio And Render Tools

## Objective

Allow AI agents to perform bounded audio operations and renders through MCP.

## Dependencies

- 021 MCP Editing Tools.
- 009 Audio Import, Recording, And Export.

## Deliverables

- Tools for waveform analysis, loudness analysis, stem render, selected-region render, bounce-in-place, freeze, normalize, reverse, and batch export.
- Render job queue with progress, cancellation, and result manifests.
- Capability checks for asset creation and export paths.
- Fixture tests for render tool outputs.

## Acceptance Gates

- Render tools cannot overwrite source media without explicit confirmation token.
- Offline render results match app render path.
- Long renders report progress and can be cancelled cleanly.

## Notes For Agents

All generated files must be registered as project assets or explicit exports.
