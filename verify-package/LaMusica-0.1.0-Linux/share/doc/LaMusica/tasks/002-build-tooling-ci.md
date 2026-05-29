# 002 Build, Tooling, And CI

## Objective

Add reproducible local and CI builds for the macOS app, daemon, CLI, tests, and documentation checks.

## Dependencies

- 001 Repository Bootstrap.

## Deliverables

- CMake or equivalent build system with JUCE 8 integration.
- Dependency lock strategy for all third-party libraries.
- Debug, release, sanitizers, and profiling build presets.
- GitHub Actions or equivalent CI for formatting, build, unit tests, and packaging smoke checks.
- Formatting and lint configuration for C++, optional Rust, Markdown, and scripts.

## Acceptance Gates

- Clean checkout can configure and build without manual IDE steps.
- CI runs on macOS and fails on formatting, compile, or test errors.
- Build artifacts separate app, daemon, CLI, tests, and fixtures.

## Notes For Agents

Prefer generated IDE projects from committed build definitions. Avoid committing machine-local Xcode state.
