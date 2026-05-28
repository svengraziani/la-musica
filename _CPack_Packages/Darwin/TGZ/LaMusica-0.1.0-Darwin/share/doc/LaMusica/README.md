# LaMusica

LaMusica is planned as an open-source macOS Digital Audio Workstation with a system-level MCP server so AI agents can assist with orchestration, editing, sequencing, mixing, and project operations.

This repository currently contains the deterministic build plan. Agents should start with:

1. [Mission](docs/mission/product-mission.md)
2. [Architecture Baseline](docs/architecture/architecture-baseline.md)
3. [Agent Execution Rules](docs/process/agent-execution-rules.md)
4. [Task Index](docs/tasks/README.md)

The product target is a full DAW, not an MVP.

## Project Status

Bootstrap in progress. The repository is executing the numbered task plan under [docs/tasks](docs/tasks/README.md).

## Planned Build Prerequisites

- macOS with a modern Xcode toolchain.
- C++23 for LaMusica-owned code, with JUCE/plugin boundary compatibility verified by CI.
- JUCE 8.
- CMake or the selected committed generator once task 002 is implemented.

Third-party dependency policy is documented in
[docs/developer/dependencies.md](docs/developer/dependencies.md). The build does not download
external source during configure; optional JUCE integration uses an explicit `LAMUSICA_JUCE_PATH`.

## License

LaMusica is initially licensed as `AGPL-3.0-or-later`. This aligns the open-source path with JUCE 8's AGPL/commercial licensing model. Maintainers should review licensing before distributing public binaries, bundled assets, plugin SDK integrations, or commercial variants.
