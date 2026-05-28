# Dependency Lock Strategy

LaMusica does not download third-party source during configure or build. All external inputs are
either platform toolchain components or explicitly supplied source checkouts.

## Locked Inputs

| Dependency | Required version | Source | Verification |
| --- | --- | --- | --- |
| Xcode command line tools | Current macOS CI image toolchain | Apple system install | CI configures and builds all presets on macOS. |
| CMake | 3.25 or newer | Developer or CI install | `cmake_minimum_required(VERSION 3.25)` rejects older versions. |
| JUCE | Major version 8 | Maintainer-provided checkout via `LAMUSICA_JUCE_PATH` | Configure fails when JUCE integration is enabled without a path; version is checked when JUCE exposes `JUCE_VERSION`. |
| Cocoa/AppKit | macOS SDK from Xcode | Apple system SDK | App bundle target links `-framework Cocoa` only on Apple platforms. |

## Policy

- Do not add package-manager downloads, `FetchContent`, CPM, vcpkg, Conan, or `ExternalProject_Add`
  without updating this lock strategy.
- New third-party libraries must have a pinned version or commit, license review, and a documented
  update path before they are used by app, daemon, CLI, tests, or packaging.
- Generated IDE files and machine-local dependency caches are not source-of-truth dependencies.
- Optional SDKs must be disabled by default and gated behind explicit CMake options.

Run `cmake -P cmake/CheckDependencyLock.cmake` before submitting build-system changes.
