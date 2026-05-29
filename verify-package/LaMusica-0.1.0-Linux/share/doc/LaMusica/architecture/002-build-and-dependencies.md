# ADR 002: Build And Dependency Baseline

## Status

Accepted for initial implementation.

## Decision

- Use CMake presets as the authoritative local and CI build entry point.
- Use C++23 for LaMusica-owned targets.
- Require a pinned JUCE 8.0.13 checkout for the product app.
- Build separate targets for the JUCE app shell, smoke-test harness, MCP daemon, CLI, core
  libraries, and tests.

## Dependency Lock Strategy

The dependency lock strategy is source-pinned third-party dependencies:

- JUCE is pinned to tag `8.0.13`, commit `7c9d3783b127263d72bb65fe0a7e2dc8a02a7ac2`,
  and supplied through `LAMUSICA_JUCE_PATH`.
- Plugin SDKs will be pinned separately and reviewed for redistribution constraints.
- Generated IDE projects are not authoritative; CMake inputs are authoritative.

No third-party source is vendored until license compatibility and update policy are recorded.

## Compromises

The build now requires a local JUCE checkout, so a clean checkout no longer configures until
`LAMUSICA_JUCE_PATH` is set. The benefit is that `LaMusica.app` is the JUCE product shell instead
of a Cocoa placeholder. The repository still avoids configure-time dependency downloads.
