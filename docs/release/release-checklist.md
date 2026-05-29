# Release Checklist

## Build

- Configure: `cmake --preset release-universal -DLAMUSICA_JUCE_PATH=/path/to/JUCE-8.0.13`
- Build: `cmake --build --preset release-universal`
- Test debug: `ctest --preset debug`
- Test release: `ctest --preset release`
- Formatting: `find apps libs tools tests \( -name '*.cpp' -o -name '*.hpp' -o -name '*.mm' \) -exec xcrun clang-format --dry-run --Werror {} +`
- Package: `cpack --config build/unix-release/CPackConfig.cmake`
- Headless package verification: `cpack -G TGZ --config build/unix-release/CPackConfig.cmake`
- Package contents: `cmake -DPACKAGE=LaMusica-0.1.0-Darwin.tar.gz -P cmake/VerifyPackage.cmake`
- Example verification: `build/unix-release/tools/cli/lamusica_cli verify-examples fixtures/examples`
- Tutorial verification: `build/unix-release/tools/cli/lamusica_cli verify-examples fixtures/tutorials`
- Full CTest gate: confirm debug and release each report 82/82 tests passing.
- First-track readiness: `ctest --preset release -R first_track`
- First-track preferences gate:
  `ctest --preset release -R lamusica_daw_app_session_preferences_first_track_smoke`
- First-track CLI workflow:
  `ctest --preset release -R 'lamusica_cli_.*edit_first_track|lamusica_cli_verify_edit_first_track_package'`

## macOS Packaging

- Produce `LaMusica.app` from the release build.
- Include `lamusica_mcpd`, `lamusica_cli`, docs, example projects, and tutorial projects.
- Include release docs: changelog, security disclosure process, contributing guide, and user manual.
- Confirm package verification includes architecture, build/test, project format, command API, MCP,
  release, changelog, security, examples, and tutorials.
- Sign app bundle with the maintainer Developer ID.
- Sign daemon and CLI binaries with the same Developer ID.
- Sign the plugin scan worker before the app bundle.
- Scripted signing: `scripts/sign-macos.sh --identity "Developer ID Application: <Team Name> (<TEAMID>)"`.
- Notarize app bundle or disk image.
- Staple notarization ticket.
- Scripted notarization: `scripts/notarize-macos.sh --artifact LaMusica-<version>.dmg --keychain-profile lamusica-notary`.
- Verify signatures: `codesign --verify --strict --deep --verbose=2`.
- Verify Gatekeeper assessment: `spctl --assess --type execute --verbose`.
- Verify stapling/signature gate: `scripts/verify-signature.sh --app LaMusica.app --artifact LaMusica-<version>.dmg`.
- Generate SBOM and checksums: `scripts/sbom.sh --artifact LaMusica-<version>.dmg --output build/release-metadata`.
- Verify launch on a clean macOS user account.
- Follow [macOS Distribution](macos-distribution.md).

## Accessibility

- Run the automated accessibility gate:
  `ctest --preset release -R lamusica_daw_accessibility_audit --output-on-failure`.
- Enable VoiceOver and complete a manual pass for the primary DAW surfaces:
  transport controls, a mixer strip, a timeline clip, a piano-roll note, and a plugin control.
- Confirm VoiceOver announces each checked control with the expected role, name, value text, and
  action; no interactive control should be announced as an unlabeled generic group.
- Complete the release workflows without a mouse while VoiceOver is on: start/stop transport,
  select and edit a clip, change a fader value, insert or inspect a plugin control, and open the
  export path.
- Enable macOS Reduce Motion and confirm playhead/meter animation uses the reduced cadence while
  value text still updates.
- Enable macOS Increase Contrast and confirm the high-contrast palette is active and focus rings are
  visible.

## Assets And Examples

- Include only redistributable assets.
- Verify example projects load without missing assets: `lamusica_cli verify-examples fixtures/examples`.
- Verify tutorial projects load without missing assets: `lamusica_cli verify-examples fixtures/tutorials`.
- Verify the first-track tutorial is ready: `lamusica_cli verify-first-track-project fixtures/tutorials/first-song.Project.lamusica`.
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
- Confirm `docs/release/security-disclosure.md` matches `SECURITY.md`.
- Confirm versioning and schema migration notes are present.
