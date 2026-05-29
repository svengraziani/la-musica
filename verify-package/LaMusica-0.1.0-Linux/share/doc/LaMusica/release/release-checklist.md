# Release Checklist

## Build

- Configure: `cmake --preset release-universal -DLAMUSICA_JUCE_PATH=/path/to/JUCE-8.0.13`
- Build: `cmake --build --preset release-universal`
- Confirm the release workflow uses the pinned `macos-14` runner and Xcode `15.4`.
- Generate dSYMs: `scripts/archive-dsyms.sh`
- Confirm every generated `.dSYM` contains a non-empty `Contents/Resources/DWARF/<binary-basename>`
  payload before archiving.
- Optional local dSYM validator smoke: `scripts/archive-dsyms.sh --self-test`
- Verify dSYM symbolication:
  `scripts/verify-symbolication.sh --binary <app-or-helper-binary> --dsym <app-or-helper-binary>.dSYM --symbol main --expect main --arch arm64`
  and repeat for `x86_64` across the app, plugin scan worker, MCP daemon, and CLI.
- Optional local symbolication validator smoke: `scripts/verify-symbolication.sh --self-test`
- Optional local entitlement validator smoke: `scripts/sign-macos.sh --self-test`
- Optional local notarization argument validator smoke: `scripts/notarize-macos.sh --self-test`
- Optional local signature verifier smoke: `scripts/verify-signature.sh --self-test`
- Optional local provenance validator smoke: `scripts/verify-provenance.sh --self-test`
- Verify binary provenance:
  `scripts/verify-provenance.sh --app build/macos-release-universal/apps/daw/lamusica_daw_artefacts/LaMusica.app/Contents/MacOS/LaMusica --mcpd build/macos-release-universal/apps/mcpd/lamusica_mcpd --cli build/macos-release-universal/tools/cli/lamusica_cli`.
- Confirm provenance reports the source checkout's `HEAD` commit and `dirty=false`; do not use
  `--allow-dirty` for signed or published release artifacts.
- Test debug: `ctest --preset debug`
- Test universal release: `ctest --preset release-universal`
- Formatting: `find apps libs tools tests \( -name '*.cpp' -o -name '*.hpp' -o -name '*.mm' \) -exec xcrun clang-format --dry-run --Werror {} +`
- Package: `cpack -G DragNDrop --config build/macos-release-universal/CPackConfig.cmake`
- Headless package verification: `cpack -G TGZ --config build/macos-release-universal/CPackConfig.cmake`
- Package contents: `cmake -DPACKAGE=LaMusica-0.1.0-Darwin.tar.gz -P cmake/VerifyPackage.cmake`
- Optional local package verifier smoke:
  `cmake -DLAMUSICA_VERIFY_PACKAGE_SELF_TEST=ON -P cmake/VerifyPackage.cmake`
- Example verification:
  `build/macos-release-universal/tools/cli/lamusica_cli verify-examples fixtures/examples`
- Tutorial verification:
  `build/macos-release-universal/tools/cli/lamusica_cli verify-examples fixtures/tutorials`
- Full CTest gate: confirm debug and release-universal both pass every configured test.
- P26 verification gates:
  `ctest --preset release-universal -L 'determinism|audio-correctness|plugin-hosting|perf'`
- Generic CLI production gates:
  `ctest --preset release-universal -R 'lamusica_cli_(first_track_lifecycle_clean|help|schema|prepare_legacy_migrate_project|migrate_legacy_project_json|create_generic_project|prepare_arbitrary_id_project|query_arbitrary_id|arbitrary_id_edit|query_generic|query_json_escapes_control_bytes|generic_edit|generic_render)'`
- DAW shell/onboarding/privacy gates:
  `ctest --preset release-universal -R 'lamusica_daw_(accessibility_audit|diagnostics_consent|onboarding_templates|headless_binding)'`
- MCP daemon diagnostics/privacy gate:
  `ctest --preset release-universal -R lamusica_mcpd_diagnostics_crash_smoke`
- Legacy first-track compatibility gates:
  `ctest --preset release-universal -R 'first_track|lamusica_cli_.*edit_first_track|lamusica_cli_verify_edit_first_track_package'`

## macOS Packaging

- Produce `LaMusica.app` from the release build.
- Include `lamusica_mcpd`, `lamusica_cli`, docs, example projects, and tutorial projects.
- Include release docs: changelog, security disclosure process, contributing guide, and user manual.
- Confirm package verification includes architecture, build/test, project format, command API, MCP,
  release, changelog, security, examples, and tutorials.
- Sign app bundle with the maintainer Developer ID.
- Sign daemon and CLI binaries with the same Developer ID.
- Sign the plugin scan worker before the app bundle.
- Confirm signing secrets are configured: `LAMUSICA_CODESIGN_CERT_P12_BASE64`,
  `LAMUSICA_CODESIGN_CERT_PASSWORD`, and `LAMUSICA_CODESIGN_IDENTITY`.
- Scripted signing: `scripts/sign-macos.sh --identity "Developer ID Application: <Team Name> (<TEAMID>)"`.
- Confirm the signing identity is a Developer ID Application identity with a 10-character Team ID;
  ad-hoc or malformed identity strings must be rejected.
- Confirm the signing helper rejects bundles missing the app executable at
  `LaMusica.app/Contents/MacOS/LaMusica`.
- Confirm universal architectures: `lipo -archs` reports `arm64 x86_64` for the app, plugin scan
  worker, MCP daemon, and CLI.
- Notarize app bundle or disk image.
- Staple notarization ticket.
- Validate the stapled artifact with `xcrun stapler validate LaMusica-<version>-Darwin.dmg`.
- Confirm release secrets are configured: `LAMUSICA_NOTARY_KEY_P8_BASE64`,
  `LAMUSICA_NOTARY_KEY_ID`, and `LAMUSICA_NOTARY_ISSUER_ID`.
- Scripted notarization:
  `scripts/notarize-macos.sh --artifact LaMusica-<version>-Darwin.dmg --key AuthKey.p8 --key-id <KEYID> --issuer <ISSUER_UUID>`.
- Confirm the notarization helper is called only with .dmg or .zip artifacts and rejects any release
  DMG not named `LaMusica-<version>-Darwin.dmg`.
- Confirm the App Store Connect key id is 10 uppercase alphanumeric characters and the issuer is a UUID; malformed credential identifiers must be rejected before `notarytool` runs.
- Optional local notarization argument validator smoke: `scripts/notarize-macos.sh --self-test`.
- Verify signatures: `codesign --verify --strict --deep --verbose=2`.
- Verify signed app entitlements include microphone and Apple Events permissions.
- Verify a known crash/symbol address resolves against the archived dSYMs with
  `scripts/verify-symbolication.sh`; the output must name the expected function, not `??`.
- Confirm `LaMusica-dSYMs.tar.gz` includes dSYM bundles for the app, plugin scan worker, MCP daemon, and CLI.
- Verify Gatekeeper assessment: `spctl --assess --type execute --verbose`.
- Verify stapling/signature gate:
  `scripts/verify-signature.sh --app LaMusica.app --binary lamusica_plugin_scan_worker --binary lamusica_mcpd --binary lamusica_cli --artifact LaMusica-<version>-Darwin.dmg`.
  The release signature gate must use the stapled `LaMusica-<version>-Darwin.dmg` and requires --binary checks for the
  plugin scan worker, MCP daemon, and CLI.
- Optional local signature verifier smoke: `scripts/verify-signature.sh --self-test`.
- Generate SBOM and checksums for every published binary artifact:
  `scripts/sbom.sh --artifact LaMusica-<version>-Darwin.dmg --artifact LaMusica-dSYMs.tar.gz --output build/release-metadata`.
  DMG release metadata must include `LaMusica-dSYMs.tar.gz`; the SBOM helper rejects a DMG-only checksum set or a non-`LaMusica-<version>-Darwin.dmg` release DMG name.
- Optional local SBOM/checksum validator smoke: `scripts/sbom.sh --self-test`.
- Sign and verify checksums:
  `scripts/sign-checksums.sh --identity "Developer ID Application: <Team Name> (<TEAMID>)" --checksums build/release-metadata/SHA256SUMS`.
- Confirm Checksum signing uses the same Developer ID Application identity shape with a
  10-character Team ID; ad-hoc or malformed identity strings must be rejected.
- Confirm Checksum signing rejects `SHA256SUMS` files with a release DMG row that is not named
  `LaMusica-<version>-Darwin.dmg` or that omits `LaMusica-dSYMs.tar.gz`.
- Optional local checksum validator smoke: `scripts/sign-checksums.sh --self-test`.
- On tag builds, confirm the GitHub Release includes the DMG, dSYMs archive, SBOM, `SHA256SUMS`,
  and `SHA256SUMS.sig`, and that `SHA256SUMS` contains one row for the DMG and one row for the
  dSYMs archive.
- Confirm the GitHub Release also includes the blank evidence templates:
  `macos-release-evidence-template.md` and `accessibility-voiceover-evidence-template.md`.
- Confirm the GitHub Release includes the validated completed evidence files:
  `completed-macos-release-evidence.md` and `completed-accessibility-voiceover-evidence.md`.
- Confirm the `softprops/action-gh-release` publish step sets `fail_on_unmatched_files: true` so
  every release asset glob must resolve before publication succeeds.
- Confirm manual `workflow_dispatch` releases check out the requested `release_tag`; the workflow
  must build, sign, verify, and publish the same release ref.
- Confirm the `Vulnerability scan` step has `fail-build: true` and `severity-cutoff: critical`, and
  that it runs after completed evidence validation but before `Publish GitHub release`.
- Confirm both release artifact upload steps use `if-no-files-found: error`; missing DMG, dSYMs,
  SBOM, checksums, or evidence files must fail the workflow instead of publishing a partial release.
- For the manual evidence pass, download the `LaMusica-release-candidate` workflow artifact and use
  its DMG and dSYMs when completing Gatekeeper, TCC, symbolication, and VoiceOver evidence.
- Record the signing, notarization, Gatekeeper, symbolication, and clean-account launch results with
  [macOS Release Evidence](macos-release-evidence-template.md). Attach the completed evidence to
  the release notes.
- Validate completed macOS and VoiceOver evidence before approving the release:
  `scripts/verify-release-evidence.sh --macos completed-macos-release-evidence.md --voiceover completed-accessibility-voiceover-evidence.md`.
  For manual `workflow_dispatch` releases, provide the semantic `v*` `release_tag` to publish and
  completed-evidence input paths that already exist in the checked-out release branch or tag.
  The validator rejects blank fields, pass/fail or yes/no placeholders, unresolved TBD/TODO
  markers, pending-evidence notes, not-run or skipped markers, and explicit fail/failed/error/denied/rejected/blocked results; every row must contain concrete release evidence.
  It also rejects malformed release versions, malformed Git commits, malformed Developer ID
  identities, and malformed notarization request ids.
  Completed VoiceOver evidence must include the artifact name, signing identity, notarization request id, and stapled artifact validation for the same release artifact.
  Completed macOS and VoiceOver evidence must agree on release version, Git commit, signing identity,
  notarization request id, and the `LaMusica-<version>-Darwin.dmg` artifact name.
  The artifact name must match the declared release version.
  Completed evidence must include concrete CI/log references as an `http(s)` URL or log path, Xcode version, tester, date,
  macOS version, hardware, clean-account name where applicable, and release approver fields.
  Valid evidence must contain semantic release versions, 12-40 hex Git commits, a Developer ID
  Application signing identity with a 10-character Team ID, and a concrete notarization request id.
- Optional local evidence validator smoke: `scripts/verify-release-evidence.sh --self-test`.
- Optional local release workflow validator smoke: `scripts/verify-release-workflow.sh --self-test`.
- Optional local CI workflow validator smoke: `scripts/verify-ci-workflow.sh --self-test`.
- Verify launch on a clean macOS user account, online and offline, with no Gatekeeper override.
- Follow [macOS Distribution](macos-distribution.md).

## Accessibility

- Run the automated accessibility gate:
  `ctest --preset release-universal -R lamusica_daw_accessibility_audit --output-on-failure`.
- Use the manual checklist in [Accessibility VoiceOver Checklist](accessibility-voiceover-checklist.md)
  and record the results with
  [Accessibility VoiceOver Evidence](accessibility-voiceover-evidence-template.md). Attach the
  completed evidence to the release notes.
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
- Verify generic CLI query/edit/render/schema/migrate tests pass against non-hardwired project ids.
- Verify the first-track tutorial remains ready as a legacy compatibility fixture:
  `lamusica_cli verify-first-track-project fixtures/tutorials/first-song.Project.lamusica`.
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
