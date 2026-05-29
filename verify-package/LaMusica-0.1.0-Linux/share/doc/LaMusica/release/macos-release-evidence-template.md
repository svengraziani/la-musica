# macOS Release Evidence

Complete this file for each beta or stable macOS release and attach it to the release notes with
the signed artifacts.

## Build

- Release version:
- Git commit:
- CI run:
- Xcode version:
- Artifact names:
- Signing identity:
- Notarization request id:

## Universal Binary Gate

| Binary | `lipo -archs` output | Pass/fail |
| --- | --- | --- |
| LaMusica.app/Contents/MacOS/LaMusica |  |  |
| lamusica_plugin_scan_worker |  |  |
| lamusica_mcpd |  |  |
| lamusica_cli |  |  |

## Signing, Notarization, And Stapling

| Gate | Command or evidence | Pass/fail | Notes |
| --- | --- | --- | --- |
| App signature | `codesign --verify --strict --deep --verbose=2 LaMusica.app` |  |  |
| Entitlements | `codesign -d --entitlements :- LaMusica.app` |  |  |
| Gatekeeper | `spctl --assess --type execute --verbose LaMusica.app` |  |  |
| Notarization | `xcrun notarytool submit --wait` result |  |  |
| Stapling | `xcrun stapler validate LaMusica-<version>-Darwin.dmg` |  |  |
| Package verifier | `cmake -DPACKAGE=LaMusica-<version>-Darwin.tar.gz -P cmake/VerifyPackage.cmake` |  |  |

## Clean-Account Launch

- Tester:
- Date:
- macOS version:
- Hardware:
- Fresh user account name:
- Online launch from `/Applications`: pass/fail
- Offline launch from `/Applications`: pass/fail
- No Gatekeeper override required: yes/no
- First record attempt triggered microphone TCC prompt: yes/no
- Bundled CLI tools ran from package `bin`: yes/no

## Symbols And Crash Diagnostics

| Gate | Evidence | Pass/fail | Notes |
| --- | --- | --- | --- |
| dSYMs archived |  |  |  |
| `atos`/`llvm-symbolizer` resolved known address |  |  |  |
| DAW induced crash produced local report |  |  |  |
| lamusica_mcpd induced crash produced local report |  |  |  |
| Diagnostics upload stayed disabled without consent |  |  |  |

## Published Release Assets

| Asset | SHA-256 row present | Included in SBOM | Uploaded |
| --- | --- | --- | --- |
| DMG |  |  |  |
| dSYMs archive |  |  |  |
| SBOM |  |  |  |
| SHA256SUMS |  |  |  |
| SHA256SUMS.sig |  |  |  |

## Failures And Follow-Up

- Blocking failures:
- Non-blocking observations:
- Follow-up issue links:
- Release approved by:
