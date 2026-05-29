# macOS Distribution

LaMusica release artifacts are produced from the `release-universal` preset on macOS. The release
workflow is pinned to the `macos-14` runner and Xcode `15.4`; changing either requires rerunning the
full release verification gate and updating this document.

## Package

```sh
cmake --preset release-universal -DLAMUSICA_JUCE_PATH=/path/to/JUCE-8.0.13
cmake --build --preset release-universal
scripts/archive-dsyms.sh
ctest --preset release-universal
ctest --preset release-universal -L 'determinism|audio-correctness|plugin-hosting|perf'
ctest --preset release-universal -R 'lamusica_cli_(first_track_lifecycle_clean|help|schema|prepare_legacy_migrate_project|migrate_legacy_project_json|create_generic_project|prepare_arbitrary_id_project|query_arbitrary_id|arbitrary_id_edit|query_generic|query_json_escapes_control_bytes|generic_edit|generic_render)'
ctest --preset release-universal -R 'lamusica_daw_(accessibility_audit|diagnostics_consent|onboarding_templates|headless_binding)'
ctest --preset release-universal -R lamusica_mcpd_diagnostics_crash_smoke
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
cpack -G TGZ --config build/macos-release-universal/CPackConfig.cmake
cmake -DPACKAGE=LaMusica-0.1.0-Darwin.tar.gz -P cmake/VerifyPackage.cmake
```

## Install And Launch

For a signed disk image, a fresh user should be able to:

1. Open `LaMusica-<version>-Darwin.dmg`.
2. Drag `LaMusica.app` to `/Applications`.
3. Launch LaMusica from Finder without installing developer tools.
4. Grant microphone access only when recording is used.
5. Run bundled command-line tools from the package `bin` directory when needed for project
   validation or MCP daemon checks.

Before publishing, verify launch from a clean macOS user account:

```sh
open /Applications/LaMusica.app
```

Record the signing, notarization, Gatekeeper, offline-launch, microphone TCC, dSYM, and published asset evidence with [macOS Release Evidence](macos-release-evidence-template.md), then attach the
completed evidence to the release notes.

Verify redistributable examples before publishing a package:

```sh
build/macos-release-universal/tools/cli/lamusica_cli verify-examples fixtures/examples
build/macos-release-universal/tools/cli/lamusica_cli verify-examples fixtures/tutorials
ctest --preset release-universal -R 'lamusica_cli_(first_track_lifecycle_clean|schema|prepare_legacy_migrate_project|migrate_legacy_project_json|create_generic_project|prepare_arbitrary_id_project|query_arbitrary_id|arbitrary_id_edit|query_generic|query_json_escapes_control_bytes|generic_edit|generic_render)'
```

Legacy first-track fixtures are retained as compatibility coverage and run as part of the full
release preset, but they are not the primary release-readiness proof. The P26 generic CLI gates above
prove ID-agnostic query/edit/render/schema/migrate behavior against arbitrary projects.

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

The signing script signs and verifies the app bundle, the plugin scan worker, the MCP daemon, and
the CLI. It validates the source entitlement file before signing and extracts the signed app
entitlements afterward to require microphone and Apple Events entitlements. The identity must be a
Developer ID Application identity in the form `Developer ID Application: ... (<TEAMID>)` with a 10-character Team ID; ad-hoc or malformed identity strings are rejected. Missing release binaries are treated as a release failure, including bundles missing `LaMusica.app/Contents/MacOS/LaMusica`. Local
entitlement validation can be run with `scripts/sign-macos.sh --self-test`.
GitHub releases require `LAMUSICA_CODESIGN_CERT_P12_BASE64`,
`LAMUSICA_CODESIGN_CERT_PASSWORD`, and `LAMUSICA_CODESIGN_IDENTITY` secrets.

Before signing, confirm `LaMusica.app/Contents/Resources/en.lproj/InfoPlist.strings`,
`es.lproj/InfoPlist.strings`, and `fr.lproj/InfoPlist.strings` are present so the signed bundle
matches `CFBundleLocalizations` and ships localized microphone and Apple Events usage strings.

Before signing, verify that all user-facing binaries carry matching version provenance:

```sh
scripts/verify-provenance.sh \
  --app build/macos-release-universal/apps/daw/lamusica_daw_artefacts/LaMusica.app/Contents/MacOS/LaMusica \
  --mcpd build/macos-release-universal/apps/mcpd/lamusica_mcpd \
  --cli build/macos-release-universal/tools/cli/lamusica_cli
```

The release gate requires `dirty=false` and a commit matching the source checkout's `HEAD`.
Use `--allow-dirty` only for local diagnostic builds that are never signed or published.

Verify signatures before notarization:

```sh
codesign --verify --strict --deep --verbose=2 \
  build/macos-release-universal/apps/daw/lamusica_daw_artefacts/LaMusica.app
codesign -d --entitlements :- \
  build/macos-release-universal/apps/daw/lamusica_daw_artefacts/LaMusica.app
spctl --assess --type execute --verbose \
  build/macos-release-universal/apps/daw/lamusica_daw_artefacts/LaMusica.app
```

## Notarize

Submit the disk image or zip artifact with `notarytool`.

```sh
xcrun notarytool submit LaMusica-<version>-Darwin.dmg \
  --key /path/to/AuthKey_<KEYID>.p8 \
  --key-id <KEYID> \
  --issuer <ISSUER_UUID> \
  --wait
xcrun stapler staple LaMusica-<version>-Darwin.dmg
xcrun stapler validate LaMusica-<version>-Darwin.dmg
```

The release workflow calls:

```sh
scripts/notarize-macos.sh \
  --artifact LaMusica-<version>-Darwin.dmg \
  --key /path/to/AuthKey_<KEYID>.p8 \
  --key-id <KEYID> \
  --issuer <ISSUER_UUID>
scripts/verify-signature.sh \
  --app build/macos-release-universal/apps/daw/lamusica_daw_artefacts/LaMusica.app \
  --binary build/macos-release-universal/apps/plugin_scan_worker/lamusica_plugin_scan_worker \
  --binary build/macos-release-universal/apps/mcpd/lamusica_mcpd \
  --binary build/macos-release-universal/tools/cli/lamusica_cli \
  --artifact LaMusica-<version>-Darwin.dmg
```

The signature verifier treats `--artifact` as the stapled release .dmg gate: the artifact must be a
`LaMusica-<version>-Darwin.dmg`, and the command must include helper --binary checks for `lamusica_plugin_scan_worker`,
`lamusica_mcpd`, and `lamusica_cli`.

For local releases, `scripts/notarize-macos.sh --keychain-profile <profile>` is also supported when
the profile was created ahead of time with `xcrun notarytool store-credentials`. The scripted helper
only accepts .dmg or .zip notarization artifacts. GitHub releases use
`LAMUSICA_NOTARY_KEY_P8_BASE64`, `LAMUSICA_NOTARY_KEY_ID`, and `LAMUSICA_NOTARY_ISSUER_ID` secrets.
The key id must be a 10-character App Store Connect key id, and the issuer must be an App Store
Connect issuer UUID.
Run `scripts/notarize-macos.sh --self-test` and `scripts/verify-signature.sh --self-test` as cheap
local preflights for credential-mode validation and entitlement-gate validation.

## Provenance And Symbols

Every binary prints `version`, `commit`, `dirty`, and `buildDate` through `--version`.
`scripts/verify-provenance.sh` rejects malformed version, commit, dirty, and UTC build-date fields
and rejects dirty release provenance unless `--allow-dirty` is explicitly passed for a non-release
diagnostic build. It exposes `--self-test` for local validator checks. Release builds generate and archive dSYM
bundles with `scripts/archive-dsyms.sh`, which rejects bundles without a non-empty DWARF payload
matching the binary basename and
exposes `--self-test` for local validator checks. `scripts/verify-symbolication.sh` looks up a known
symbol in a dSYM with `dwarfdump`, resolves the address with `atos`, and rejects `??` or wrong-symbol
output for the app, plugin scan worker, MCP daemon, and CLI on both `arm64` and `x86_64`, so the
release job proves archived symbols can resolve crash addresses. Releases also emit an SPDX SBOM
plus `SHA256SUMS` for every published binary artifact with `scripts/sbom.sh`, which validates the
SPDX marker, LaMusica/JUCE package entries, checksum digest, required dSYM archive row, non-`LaMusica-<version>-Darwin.dmg` DMG names, and DMG-only checksum set mistakes before
publishing. The checksum file
is signed and verified with Checksum signing through
`scripts/sign-checksums.sh`, which rejects malformed checksum rows, ad-hoc or malformed Developer ID
identities, release DMG rows not named `LaMusica-<version>-Darwin.dmg`, release DMG checksum sets
without `LaMusica-dSYMs.tar.gz`, and empty detached signatures.
Package contents are verified with `cmake -DPACKAGE=... -P cmake/VerifyPackage.cmake`; its
`LAMUSICA_VERIFY_PACKAGE_SELF_TEST` mode proves the verifier accepts a complete package fixture and
rejects placeholder reserved-domain contacts before release jobs trust it.
Completed evidence is checked with `scripts/verify-release-evidence.sh`, which rejects malformed release versions, malformed Git commits, malformed Developer ID identities, and notarization request ids in addition to
blank, placeholder, not-run, skipped, or explicit failing evidence. Completed macOS and VoiceOver evidence must agree on release version,
Git commit, signing identity, notarization request id, and the
`LaMusica-<version>-Darwin.dmg` artifact name.
The artifact name must match the declared release version. Completed evidence must include concrete
CI/log references as an `http(s)` URL or log path, Xcode version, tester, date, macOS version, hardware, clean-account name where
applicable, and release approver fields.
Crash reports
are captured locally into the user's temporary `LaMusica Crash Reports` directory. Network upload is
disabled unless diagnostics consent is granted and diagnostics sharing is enabled.

Diagnostics upload endpoints are user-overridable for self-hosted deployments. Use an HTTPS
`ApplicationPreferences::diagnosticsEndpoint` value, or set `LAMUSICA_DIAGNOSTICS_ENDPOINT` in the
release environment. The environment override must resolve to a non-empty explicit HTTPS URL;
missing or non-HTTPS override values block upload. Non-HTTPS endpoints are rejected by preference
validation.

Unsigned nightly archives must be labeled as contributor artifacts. Beta and stable artifacts must
pass `codesign`, `spctl`, `notarytool`, and `stapler` checks before publication. Tag-triggered
release workflows validate completed macOS and VoiceOver evidence before publication, then publish
the DMG, dSYMs archive, SBOM, `SHA256SUMS`, detached checksum signature, blank evidence templates,
and completed evidence files to the GitHub Release after the vulnerability scan passes.
The GitHub Release publish step sets `fail_on_unmatched_files: true` so every release asset glob
must resolve before publication succeeds.
The `Vulnerability scan` step uses `fail-build: true` with `severity-cutoff: critical` and runs after
completed evidence validation but before `Publish GitHub release`.
Manual `workflow_dispatch` releases check out the requested `release_tag` so the build, signing,
verification, and publication steps all operate on the same release ref.
The release job uploads `LaMusica-release-candidate` before the completed-evidence gate so the
candidate DMG and dSYMs can be downloaded for clean-account, Gatekeeper, TCC, symbolication, and
VoiceOver evidence collection.
Both the candidate and final artifact uploads use `if-no-files-found: error`; a missing DMG, dSYMs
archive, SBOM, checksum, signature, template, or completed evidence file fails the workflow instead
of publishing a partial release.
For manual `workflow_dispatch` releases, set `release_tag` to the semantic `v*` tag being
published and set the completed-evidence input paths to files that already exist in the checked-out release branch or tag; the workflow fails before publication if either completed evidence file is
missing or fails `scripts/verify-release-evidence.sh`.

## Release Channels

- `nightly`: unsigned or ad-hoc signed CI artifacts for contributors.
- `beta`: signed and notarized artifacts with explicit pre-release notes.
- `stable`: signed and notarized artifacts linked from the public release page.

## Update Strategy

Until an updater is implemented, users download full signed disk images. Release notes must state
whether project, command, MCP, plugin, or preset schema versions changed.
