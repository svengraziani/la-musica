# Release Checklist

## Build

- Configure: `cmake --preset release`
- Build: `cmake --build --preset release`
- Test: `ctest --preset debug`
- Formatting: `find apps libs tools tests \( -name '*.cpp' -o -name '*.hpp' -o -name '*.mm' \) -exec xcrun clang-format --dry-run --Werror {} +`
- Package: `cpack --config build/unix-release/CPackConfig.cmake`
- Headless package verification: `cpack -G TGZ --config build/unix-release/CPackConfig.cmake`

## macOS Packaging

- Produce `LaMusica.app` from the release build.
- Include `lamusica_mcpd`, `lamusica_cli`, docs, and example projects.
- Sign app bundle with the maintainer Developer ID.
- Sign daemon and CLI binaries with the same Developer ID.
- Notarize app bundle or disk image.
- Staple notarization ticket.
- Verify signatures: `codesign --verify --strict --deep --verbose=2`.
- Verify Gatekeeper assessment: `spctl --assess --type execute --verbose`.
- Verify launch on a clean macOS user account.
- Follow [macOS Distribution](macos-distribution.md).

## Assets And Examples

- Include only redistributable assets.
- Verify example projects load without missing assets.
- Keep generated caches out of source distributions.

## MCP

- Verify `lamusica_mcpd` reports health.
- Verify read-only tools do not mutate session state.
- Verify edit and render tools require capabilities.

## Documentation

- Update user manual.
- Update developer docs.
- Update changelog.
- Confirm security contact is current.
- Confirm versioning and schema migration notes are present.
