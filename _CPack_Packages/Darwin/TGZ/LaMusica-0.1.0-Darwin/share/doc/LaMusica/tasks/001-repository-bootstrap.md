# 001 Repository Bootstrap

## Objective

Create the repository foundation for a full open-source macOS DAW, including licensing, contribution policy, source layout, and committed planning documents.

## Dependencies

- None.

## Deliverables

- `LICENSE` with an open-source license selected by maintainers.
- `CONTRIBUTING.md`, `CODE_OF_CONDUCT.md`, and `SECURITY.md`.
- Source directories: `apps/daw`, `apps/mcpd`, `tools/cli`, `libs/audio`, `libs/session`, `libs/commands`, `libs/mcp_bridge`, `tests`, `fixtures`.
- Top-level `README.md` updated with build prerequisites and project status.
- Architecture docs retained under `docs/`.

## Acceptance Gates

- `git status --short` shows only intentional new files.
- `find . -maxdepth 3 -type d` includes all planned source directories.
- Documentation states that the product target is a complete DAW, not an MVP.

## Notes For Agents

Do not choose a license casually if distribution of plugin SDKs or bundled DSP libraries is affected. Record the license rationale in the README or a dedicated decision note.
