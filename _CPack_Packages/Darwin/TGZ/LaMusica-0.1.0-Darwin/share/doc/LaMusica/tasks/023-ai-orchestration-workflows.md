# 023 AI Orchestration Workflows

## Objective

Provide higher-level AI-assisted music production workflows built on safe MCP tools.

## Dependencies

- 021 MCP Editing Tools.
- 011 Piano Roll And MIDI Editing.
- 013 Step Sequencer And Patterns.

## Deliverables

- Orchestration helpers for arranging sections, harmonizing MIDI, generating drum variations, labeling song structure, and creating mix preparation passes.
- Promptable but deterministic workflow plans that resolve into command previews before applying.
- Human approval UI for multi-command agent plans.
- Workflow templates stored in project or user library.
- Tests that verify generated plans never bypass command validation.

## Acceptance Gates

- AI workflows can preview all intended edits before application.
- User can accept, reject, or partially apply plan steps.
- Workflow output is reproducible when given fixed inputs and deterministic model/tool settings.

## Notes For Agents

Do not make model output authoritative. The DAW command validator is authoritative.
