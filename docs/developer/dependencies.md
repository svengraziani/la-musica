# Dependency Lock Strategy

LaMusica does not download third-party source during configure or build. All external inputs are
either platform toolchain components or explicitly supplied source checkouts.

## Locked Inputs

| Dependency | Required version | Source | Verification |
| --- | --- | --- | --- |
| Xcode command line tools | Current macOS CI image toolchain | Apple system install | CI configures and builds all presets on macOS. |
| CMake | 3.25 or newer | Developer or CI install | `cmake_minimum_required(VERSION 3.25)` rejects older versions. |
| JUCE | 8.0.13, commit `7c9d3783b127263d72bb65fe0a7e2dc8a02a7ac2` | Maintainer-provided checkout via `LAMUSICA_JUCE_PATH` | Configure fails without a path; version is checked when JUCE exposes `JUCE_VERSION`, and Git checkouts are checked against the pinned commit. |

## Policy

- Do not add package-manager downloads, `FetchContent`, CPM, vcpkg, Conan, or `ExternalProject_Add`
  without updating this lock strategy.
- New third-party libraries must have a pinned version or commit, license review, and a documented
  update path before they are used by app, daemon, CLI, tests, or packaging.
- Generated IDE files and machine-local dependency caches are not source-of-truth dependencies.
- JUCE is required for the product app. Smoke-test helpers may remain non-JUCE only when they do not
  ship as the user-facing application.

## JUCE Checkout

Use the pinned JUCE tag:

```sh
git clone --branch 8.0.13 --depth 1 https://github.com/juce-framework/JUCE ../JUCE-8.0.13
git -C ../JUCE-8.0.13 rev-parse HEAD
cmake --preset debug -DLAMUSICA_JUCE_PATH="$(pwd)/../JUCE-8.0.13"
```

The checkout is not vendored and CMake must not download it during configure.

Run `cmake -P cmake/CheckDependencyLock.cmake` before submitting build-system changes.
