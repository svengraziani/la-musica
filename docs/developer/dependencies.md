# Dependency Lock Strategy

LaMusica does not download third-party source during configure or build. All external inputs are
either platform toolchain components or explicitly supplied source checkouts.

## Locked Inputs

| Dependency | Required version | Source | Verification |
| --- | --- | --- | --- |
| Xcode command line tools | Current macOS CI image toolchain | Apple system install | CI configures and builds all presets on macOS. |
| CMake | 3.25 or newer | Developer or CI install | `cmake_minimum_required(VERSION 3.25)` rejects older versions. |
| JUCE | 8.0.13, commit `7c9d3783b127263d72bb65fe0a7e2dc8a02a7ac2`, content manifest checksum `e2ee824cf139a72e3720e996c1cdc70e9ff9dac9653c7f74ccf7d40cf1e3d1c4` | Maintainer-provided checkout via `LAMUSICA_JUCE_PATH` | Configure fails without a path; version is checked when JUCE exposes `JUCE_VERSION`, Git checkouts are checked against the pinned commit, and dependency verification compares a recursive content manifest checksum. |
| Linux GUI headers | System packages providing `pkg-config`, X11, OpenGL, FreeType, Fontconfig, ALSA, GTK/WebKit, and libcurl development files | Developer or CI install | Required only when configuring the JUCE GUI app on Linux; macOS release CI uses Xcode/system frameworks instead. Missing `pkg-config` or `X11/Xlib.h` fails before LaMusica targets configure. |

## Policy

- Do not add package-manager downloads, `FetchContent`, CPM, vcpkg, Conan, or `ExternalProject_Add`
  without updating this lock strategy.
- New third-party libraries must have a pinned version or commit, license review, and a documented
  update path before they are used by app, daemon, CLI, tests, or packaging.
- Generated IDE files and machine-local dependency caches are not source-of-truth dependencies.
- JUCE is required for the product app. Smoke-test helpers may remain non-JUCE only when they do not
  ship as the user-facing application.
- Supply-chain checks recurse over every source `CMakeLists.txt` and `*.cmake` file outside
  generated build/package directories and reject unreviewed downloader integrations.

## JUCE Checkout

Use the pinned JUCE tag:

```sh
git clone --branch 8.0.13 --depth 1 https://github.com/juce-framework/JUCE ../JUCE-8.0.13
git -C ../JUCE-8.0.13 rev-parse HEAD
cmake --preset debug -DLAMUSICA_JUCE_PATH="$(pwd)/../JUCE-8.0.13"
```

The checkout is not vendored and CMake must not download it during configure.
Local checkouts under `external/` are ignored by Git; pass them via `LAMUSICA_JUCE_PATH`.

To record or verify the stronger JUCE tree lock, run:

```sh
cmake -DLAMUSICA_JUCE_PATH=/path/to/JUCE-8.0.13 -P cmake/CheckDependencyLock.cmake
```

The script prints and verifies the pinned JUCE content manifest checksum. Release automation may
pass `-DLAMUSICA_JUCE_CONTENT_SHA256=<sha256>` only when intentionally testing a mismatch or
reviewing a dependency update; a changed checkout fails before configure.

Run `cmake -P cmake/CheckDependencyLock.cmake` before submitting build-system changes.
