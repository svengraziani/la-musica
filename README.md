# LaMusica

LaMusica is planned as an open-source macOS Digital Audio Workstation with a system-level MCP server so AI agents can assist with orchestration, editing, sequencing, mixing, and project operations.

This repository contains the current LaMusica source, fixtures, documentation, and release
scaffolding. Agents should start with:

1. [Mission](docs/mission/product-mission.md)
2. [Architecture Baseline](docs/architecture/architecture-baseline.md)
3. [Agent Execution Rules](docs/process/agent-execution-rules.md)
4. [Task Index](docs/tasks/README.md)

The product target is a full DAW, not an MVP.

## Project Status

Bootstrap implementation is complete through the packaging and release scaffolding. The active
remaining task plan is tracked under [docs/tasks](docs/tasks/README.md), with the production JUCE
app shell and full DAW UI called out as the next major gaps. Current user, developer, performance,
MCP, and release documentation is installed into packages.

## Build Prerequisites

- macOS with a modern Xcode toolchain.
- C++23 for LaMusica-owned code, with JUCE/plugin boundary compatibility verified by CI.
- CMake.
- Optional JUCE 8 checkout when enabling JUCE-backed targets.

Third-party dependency policy is documented in
[docs/developer/dependencies.md](docs/developer/dependencies.md). The build does not download
external source during configure; optional JUCE integration uses an explicit `LAMUSICA_JUCE_PATH`.

## License

LaMusica is initially licensed as `AGPL-3.0-or-later`. This aligns the open-source path with JUCE 8's AGPL/commercial licensing model. Maintainers should review licensing before distributing public binaries, bundled assets, plugin SDK integrations, or commercial variants.
