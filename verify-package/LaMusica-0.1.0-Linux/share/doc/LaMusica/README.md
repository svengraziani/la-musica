# LaMusica

LaMusica is planned as an open-source macOS Digital Audio Workstation with a system-level MCP server so AI agents can assist with orchestration, editing, sequencing, mixing, and project operations.

This repository contains the current LaMusica source, fixtures, documentation, and release
scaffolding. Agents should start with:

1. [Mission](docs/mission/product-mission.md)
2. [Architecture Baseline](docs/architecture/architecture-baseline.md)
3. [Agent Execution Rules](docs/process/agent-execution-rules.md)
4. [Production Task Index](docs/planning/tasks/README.md)
5. [Legacy Task Index](docs/tasks/README.md)
6. [Localization Guide](docs/developer/localization.md)
7. [Privacy And Diagnostics](docs/release/privacy-and-diagnostics.md)

The product target is a full DAW, not an MVP.

## Project Status

Bootstrap implementation is complete through the packaging and release scaffolding. The active
production-readiness task plan is tracked under
[docs/planning/tasks](docs/planning/tasks/README.md), with P26-P34 covering determinism,
release hardening, warp/sample-rate correctness, accessibility, localization, privacy, comp
rendering, and onboarding. Current user, developer, performance, MCP, and release documentation is
installed into packages.

## Build Prerequisites

- macOS with a modern Xcode toolchain.
- C++23 for LaMusica-owned code, with JUCE/plugin boundary compatibility verified by CI.
- CMake.
- JUCE 8.0.13 checkout supplied through `LAMUSICA_JUCE_PATH`.

Third-party dependency policy is documented in
[docs/developer/dependencies.md](docs/developer/dependencies.md). The build does not download
external source during configure; JUCE is an explicit pinned checkout.

## License

LaMusica is initially licensed as `AGPL-3.0-or-later`. This aligns the open-source path with JUCE 8's AGPL/commercial licensing model. Maintainers should review licensing before distributing public binaries, bundled assets, plugin SDK integrations, or commercial variants.
