# macOS Distribution

LaMusica release artifacts are produced from the `release-universal` preset on macOS.

## Package

```sh
cmake --preset release-universal -DLAMUSICA_JUCE_PATH=/path/to/JUCE-8.0.13
cmake --build --preset release-universal
ctest --preset release-universal
ctest --preset release -R first_track
ctest --preset release -R lamusica_daw_app_session_preferences_first_track_smoke
cpack -G DragNDrop --config build/macos-release-universal/CPackConfig.cmake
```

The package must include:

- `LaMusica.app`
- `lamusica_mcpd`
- `lamusica_cli`
- installed documentation
- redistributable example projects
- redistributable tutorial projects

Headless CI can verify package contents with the TGZ generator when DMG creation is unavailable:

```sh
cpack -G TGZ --config build/unix-release/CPackConfig.cmake
cmake -DPACKAGE=LaMusica-0.1.0-Darwin.tar.gz -P cmake/VerifyPackage.cmake
```

## Install And Launch

For a signed disk image, a fresh user should be able to:

1. Open `LaMusica-<version>.dmg`.
2. Drag `LaMusica.app` to `/Applications`.
3. Launch LaMusica from Finder without installing developer tools.
4. Grant microphone access only when recording is used.
5. Run bundled command-line tools from the package `bin` directory when needed for project
   validation or MCP daemon checks.

Before publishing, verify launch from a clean macOS user account:

```sh
open /Applications/LaMusica.app
```

Verify redistributable examples before publishing a package:

```sh
build/unix-release/tools/cli/lamusica_cli verify-examples fixtures/examples
build/unix-release/tools/cli/lamusica_cli verify-examples fixtures/tutorials
build/unix-release/tools/cli/lamusica_cli verify-first-track-project fixtures/tutorials/first-song.Project.lamusica
```

## Sign

Set the Developer ID identity explicitly in the release environment.

```sh
codesign --force --options runtime --timestamp \
  --entitlements apps/daw/lamusica.entitlements \
  --sign "Developer ID Application: <Team Name> (<TEAMID>)" \
  build/macos-release-universal/apps/daw/lamusica_daw_artefacts/LaMusica.app

codesign --force --options runtime --timestamp \
  --sign "Developer ID Application: <Team Name> (<TEAMID>)" \
  build/macos-release-universal/apps/mcpd/lamusica_mcpd

codesign --force --options runtime --timestamp \
  --sign "Developer ID Application: <Team Name> (<TEAMID>)" \
  build/macos-release-universal/tools/cli/lamusica_cli
```

The release workflow calls the scripted version:

```sh
scripts/sign-macos.sh --identity "Developer ID Application: <Team Name> (<TEAMID>)"
```

Verify signatures before notarization:

```sh
codesign --verify --strict --deep --verbose=2 \
  build/unix-release/apps/daw/lamusica_daw_artefacts/LaMusica.app
spctl --assess --type execute --verbose \
  build/unix-release/apps/daw/lamusica_daw_artefacts/LaMusica.app
```

## Notarize

Submit the disk image or zip artifact with `notarytool`.

```sh
xcrun notarytool submit LaMusica-<version>.dmg \
  --keychain-profile lamusica-notary \
  --wait
xcrun stapler staple LaMusica-<version>.dmg
xcrun stapler validate LaMusica-<version>.dmg
```

The release workflow calls:

```sh
scripts/notarize-macos.sh --artifact LaMusica-<version>.dmg --keychain-profile lamusica-notary
scripts/verify-signature.sh \
  --app build/macos-release-universal/apps/daw/lamusica_daw_artefacts/LaMusica.app \
  --artifact LaMusica-<version>.dmg
```

## Provenance And Symbols

Every binary prints `version`, `commit`, `dirty`, and `buildDate` through `--version`. Release builds
archive dSYM bundles, emit an SPDX SBOM plus `SHA256SUMS`, and sign the checksum file. Crash reports
are captured locally into the user's temporary `LaMusica Crash Reports` directory. Network upload is
disabled unless diagnostics consent is granted and diagnostics sharing is enabled.

Diagnostics upload endpoints are user-overridable for self-hosted deployments. Use an HTTPS
`ApplicationPreferences::diagnosticsEndpoint` value, or set `LAMUSICA_DIAGNOSTICS_ENDPOINT` in the
release environment. Non-HTTPS endpoints are rejected by preference validation.

Unsigned nightly archives must be labeled as contributor artifacts. Beta and stable artifacts must
pass `codesign`, `spctl`, `notarytool`, and `stapler` checks before publication.

## Release Channels

- `nightly`: unsigned or ad-hoc signed CI artifacts for contributors.
- `beta`: signed and notarized artifacts with explicit pre-release notes.
- `stable`: signed and notarized artifacts linked from the public release page.

## Update Strategy

Until an updater is implemented, users download full signed disk images. Release notes must state
whether project, command, MCP, plugin, or preset schema versions changed.
