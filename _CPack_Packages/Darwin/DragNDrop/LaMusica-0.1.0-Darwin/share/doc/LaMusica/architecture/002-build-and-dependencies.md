# ADR 002: Build And Dependency Baseline

## Status

Accepted for initial implementation.

## Decision

- Use CMake presets as the authoritative local and CI build entry point.
- Use C++23 for LaMusica-owned targets.
- Keep JUCE 8 integration optional until the dependency is pinned and fetched by a committed dependency workflow.
- Build separate targets for the app placeholder, MCP daemon, CLI, core libraries, and tests.

## Dependency Lock Strategy

The initial dependency lock strategy is source-pinned third-party dependencies:

- JUCE 8 will be referenced by a specific tag or commit.
- Plugin SDKs will be pinned separately and reviewed for redistribution constraints.
- Generated IDE projects are not authoritative; CMake inputs are authoritative.

No third-party source is vendored until license compatibility and update policy are recorded.

## Compromises

This allows a clean checkout to configure and build the current skeleton without downloading JUCE. The compromise is that JUCE-backed UI and audio integration remains gated behind `LAMUSICA_ENABLE_JUCE` until the pinned dependency is added.
