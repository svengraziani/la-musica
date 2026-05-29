# ADR 001: License And Language Baseline

## Status

Accepted for initial implementation planning. Maintainers should review before the first public binary release.

## Decision

- License LaMusica as `AGPL-3.0-or-later`.
- Prefer C++23 for LaMusica-owned code.
- Keep JUCE, plugin SDK, and third-party integration boundaries compatible with the versions verified by CI.
- Allow Rust only for isolated non-realtime services when it reduces risk enough to justify mixed-language build and FFI complexity.

## Context

LaMusica is intended to use JUCE 8. JUCE 8 modules are available under AGPLv3 or a commercial JUCE license, so AGPL keeps the open-source distribution path coherent while avoiding a commercial dependency requirement for contributors.

C++23 gives the project a modern baseline for application code, but audio and plugin ecosystems often lag behind the latest language mode. Compatibility must be proven by the pinned macOS toolchain and CI instead of assumed.

## Consequences

- Contributors can build and distribute open-source LaMusica under AGPL terms.
- Commercial redistribution or incompatible licensing may require a separate licensing strategy.
- Newer C++23 features should be used where they improve clarity without creating toolchain or SDK friction.
- Realtime audio code still follows strict deterministic and allocation-free rules regardless of language standard.
