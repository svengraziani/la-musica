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
/Applications/LaMusica.app/Contents/MacOS/LaMusica --smoke
```

Verify redistributable examples before publishing a package:

```sh
build/unix-release/tools/cli/lamusica_cli verify-examples fixtures/examples
build/unix-release/tools/cli/lamusica_cli verify-examples fixtures/tutorials
```

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

Unsigned nightly archives must be labeled as contributor artifacts. Beta and stable artifacts must
pass `codesign`, `spctl`, `notarytool`, and `stapler` checks before publication.

## Release Channels

- `nightly`: unsigned or ad-hoc signed CI artifacts for contributors.
- `beta`: signed and notarized artifacts with explicit pre-release notes.
- `stable`: signed and notarized artifacts linked from the public release page.

## Update Strategy

Until an updater is implemented, users download full signed disk images. Release notes must state
whether project, command, MCP, plugin, or preset schema versions changed.
