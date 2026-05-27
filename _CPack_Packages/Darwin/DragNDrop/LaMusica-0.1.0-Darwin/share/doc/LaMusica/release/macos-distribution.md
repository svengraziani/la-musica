# macOS Distribution

LaMusica release artifacts are produced from the `release` preset.

## Package

```sh
cmake --preset release
cmake --build --preset release
cpack --config build/unix-release/CPackConfig.cmake
```

The package must include:

- `LaMusica.app`
- `lamusica_mcpd`
- `lamusica_cli`
- installed documentation
- redistributable example projects

## Sign

Set the Developer ID identity explicitly in the release environment.

```sh
codesign --force --options runtime --timestamp \
  --sign "Developer ID Application: <Team Name> (<TEAMID>)" \
  build/unix-release/apps/daw/LaMusica.app

codesign --force --options runtime --timestamp \
  --sign "Developer ID Application: <Team Name> (<TEAMID>)" \
  build/unix-release/apps/mcpd/lamusica_mcpd

codesign --force --options runtime --timestamp \
  --sign "Developer ID Application: <Team Name> (<TEAMID>)" \
  build/unix-release/tools/cli/lamusica_cli
```

Verify signatures before notarization:

```sh
codesign --verify --strict --deep --verbose=2 build/unix-release/apps/daw/LaMusica.app
spctl --assess --type execute --verbose build/unix-release/apps/daw/LaMusica.app
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

## Release Channels

- `nightly`: unsigned or ad-hoc signed CI artifacts for contributors.
- `beta`: signed and notarized artifacts with explicit pre-release notes.
- `stable`: signed and notarized artifacts linked from the public release page.

## Update Strategy

Until an updater is implemented, users download full signed disk images. Release notes must state
whether project, command, MCP, plugin, or preset schema versions changed.
