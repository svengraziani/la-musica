# MCP Workflow Plans Schema V1

AI orchestration workflows produce advisory plans. A plan step is not authoritative until it is approved by a human-facing flow and applied through command-backed MCP edit tools.

Plan fields:

- `schemaVersion`
- `workflowId`
- `name`
- `seed`
- `steps`

Step fields:

- `id`
- `status`
- `commandName`
- `description`
- `commandPreview`
- `validationOk`
- `validationMessage`

Workflow steps can be approved, rejected, and marked applied independently. Applied state is only
valid for approved steps whose validation passed.

Workflow templates are deterministic records with `id`, `name`, `description`, and `workflowType`
fields. Template ids must be unique within the library that stores them.
