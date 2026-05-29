if(DEFINED LAMUSICA_VERIFY_PACKAGE_SELF_TEST AND LAMUSICA_VERIFY_PACKAGE_SELF_TEST)
  set(self_test_dir "${CMAKE_CURRENT_BINARY_DIR}/verify-package-self-test")
  set(self_test_root "${self_test_dir}/LaMusica-test")
  file(REMOVE_RECURSE "${self_test_dir}")
  file(MAKE_DIRECTORY "${self_test_root}/bin" "${self_test_root}/LaMusica.app/Contents/MacOS"
       "${self_test_root}/LaMusica.app/Contents/Resources/en.lproj"
       "${self_test_root}/LaMusica.app/Contents/Resources/es.lproj"
       "${self_test_root}/LaMusica.app/Contents/Resources/fr.lproj"
       "${self_test_root}/share/doc/LaMusica/developer" "${self_test_root}/share/doc/LaMusica/release"
       "${self_test_root}/share/doc/LaMusica/performance" "${self_test_root}/share/doc/LaMusica/schemas"
       "${self_test_root}/share/lamusica/examples/empty.Project.lamusica"
       "${self_test_root}/share/lamusica/examples/generated-tone.Project.lamusica"
       "${self_test_root}/share/lamusica/tutorials/first-song.Project.lamusica"
       "${self_test_root}/share/lamusica/i18n")

  file(WRITE "${self_test_root}/LaMusica.app/Contents/MacOS/LaMusica" "#!/bin/sh\n")
  file(WRITE "${self_test_root}/LaMusica.app/Contents/Info.plist"
       "<plist><dict>"
       "<key>CFBundleIdentifier</key><string>dev.lamusica.daw</string>"
       "<key>CFBundleLocalizations</key><array><string>en</string><string>es</string><string>fr</string></array>"
       "<key>NSMicrophoneUsageDescription</key><string>Microphone access records audio.</string>"
       "<key>NSAppleEventsUsageDescription</key><string>Apple Events support automation.</string>"
       "<key>LSApplicationCategoryType</key><string>public.app-category.music</string>"
       "<key>LSMinimumSystemVersion</key><string>14.0</string>"
       "</dict></plist>\n")
  file(WRITE "${self_test_root}/LaMusica.app/Contents/Resources/en.lproj/InfoPlist.strings"
       "\"CFBundleDisplayName\" = \"LaMusica\";\n"
       "\"NSMicrophoneUsageDescription\" = \"LaMusica needs microphone access when recording audio into a project.\";\n"
       "\"NSAppleEventsUsageDescription\" = \"LaMusica may use Apple Events only for user-approved local automation workflows.\";\n")
  file(WRITE "${self_test_root}/LaMusica.app/Contents/Resources/es.lproj/InfoPlist.strings"
       "\"CFBundleDisplayName\" = \"LaMusica\";\n"
       "\"NSMicrophoneUsageDescription\" = \"LaMusica necesita acceso al microfono para grabar audio en un proyecto.\";\n"
       "\"NSAppleEventsUsageDescription\" = \"LaMusica puede usar Apple Events solo para flujos de automatizacion local aprobados por el usuario.\";\n")
  file(WRITE "${self_test_root}/LaMusica.app/Contents/Resources/fr.lproj/InfoPlist.strings"
       "\"CFBundleDisplayName\" = \"LaMusica\";\n"
       "\"NSMicrophoneUsageDescription\" = \"LaMusica a besoin de l'acces au microphone pour enregistrer l'audio dans un projet.\";\n"
       "\"NSAppleEventsUsageDescription\" = \"LaMusica peut utiliser Apple Events uniquement pour les automatisations locales approuvees par l'utilisateur.\";\n")
  foreach(binary IN ITEMS lamusica_plugin_scan_worker lamusica_mcpd lamusica_cli)
    file(WRITE "${self_test_root}/bin/${binary}" "#!/bin/sh\n")
  endforeach()

  file(WRITE "${self_test_root}/share/doc/LaMusica/NOTICE"
       "LaMusica AGPL-3.0-or-later\nCorresponding source is available.\n")
  file(WRITE "${self_test_root}/share/doc/LaMusica/THIRD_PARTY-NOTICES.md"
       "# Third-Party Notices\n"
       "LaMusica currently links against JUCE when building the macOS application.\n"
       "## JUCE\n"
       "Version: 8.0.13\n"
       "Required commit: 7c9d3783b127263d72bb65fe0a7e2dc8a02a7ac2\n"
       "Source: https://github.com/juce-framework/JUCE\n"
       "License: GPL/commercial dual license. LaMusica uses the open-source GPL-compatible path and distributes LaMusica under AGPL-3.0-or-later.\n"
       "No third-party plugin binaries are bundled in release packages.\n"
       "Test plugin-hosting fixtures are in-repository mock components unless a future release note explicitly lists a redistributable plugin and its license.\n")
  file(WRITE "${self_test_root}/share/doc/LaMusica/privacy-and-diagnostics.md"
       "LaMusica does not send crash reports or telemetry unless the user grants diagnostics consent.\n"
       "Usage telemetry is a separate opt-in from crash reports, and LaMusica currently emits no usage telemetry events.\n"
       "The signal handler writes signal, pid, and a deferred backtrace marker using async-signal-safe operations.\n"
       "Scrubbing and any upload preparation happen later, outside the signal handler.\n"
       "Collected fields: application name, app version and Git commit, operating system version, crash signal, stack frames or backtrace.\n"
       "Payloads must not include project contents, audio, MIDI, usernames, home-directory paths, absolute file paths, or project bundle names.\n"
       "Scrubbed payloads replace sensitive values with <path> or <project> before upload.\n"
       "Endpoint: https://diagnostics.lamusica.dev/v1/crash. Override: LAMUSICA_DIAGNOSTICS_ENDPOINT.\n"
       "The environment override must resolve to a non-empty explicit HTTPS URL; missing or non-HTTPS override values block upload.\n"
       "Retention: within 30 days. Opt out by setting diagnostics consent to Declined and turning diagnostics sharing off in Preferences > Privacy.\n")
  file(WRITE "${self_test_root}/share/doc/LaMusica/README.md"
       "See privacy-and-diagnostics.md for diagnostics policy.\n")
  file(WRITE "${self_test_root}/share/doc/LaMusica/user-manual.md"
       "See privacy-and-diagnostics.md for diagnostics policy.\n"
       "On first launch, LaMusica opens a welcome surface.\n"
       "New Project opens the welcome/template flow with Empty, Basic Multitrack, Drum + Synth, and Podcast / Voice templates.\n")
  file(WRITE "${self_test_root}/share/doc/LaMusica/SECURITY.md"
       "Contact security@lamusica.dev. We acknowledge within 3 business days, prepare a fix and release plan, use PGP, and keep reports private.\n")
  file(WRITE "${self_test_root}/share/doc/LaMusica/release/security-disclosure.md"
       "Contact security@lamusica.dev. We acknowledge within 3 business days, prepare a fix and release plan, use PGP, and keep reports private.\n")
  foreach(doc IN ITEMS THIRD_PARTY-NOTICES.md architecture/architecture-baseline.md developer/build-and-test.md
                       developer/command-api.md developer/mcp-tools.md developer/project-format.md
                       release/release-checklist.md release/accessibility-voiceover-checklist.md
                       release/accessibility-voiceover-evidence-template.md
                       release/macos-release-evidence-template.md release/macos-distribution.md
                       release/versioning.md
                       CHANGELOG.md CONTRIBUTING.md developer/dependencies.md developer/localization.md
                       performance/realtime-policy.md schemas/cli-output-v1.schema.json
                       schemas/project-v1.schema.json schemas/project-v3.schema.json)
    file(WRITE "${self_test_root}/share/doc/LaMusica/${doc}" "packaged ${doc}\n")
  endforeach()
  file(WRITE "${self_test_root}/share/doc/LaMusica/THIRD_PARTY-NOTICES.md"
       "# Third-Party Notices\n"
       "LaMusica currently links against JUCE when building the macOS application.\n"
       "## JUCE\n"
       "Version: 8.0.13\n"
       "Required commit: 7c9d3783b127263d72bb65fe0a7e2dc8a02a7ac2\n"
       "Source: https://github.com/juce-framework/JUCE\n"
       "License: GPL/commercial dual license. LaMusica uses the open-source GPL-compatible path and distributes LaMusica under AGPL-3.0-or-later.\n"
       "No third-party plugin binaries are bundled in release packages.\n"
       "Test plugin-hosting fixtures are in-repository mock components unless a future release note explicitly lists a redistributable plugin and its license.\n")
  foreach(project_schema IN ITEMS schemas/project-v1.schema.json schemas/project-v3.schema.json)
    file(WRITE "${self_test_root}/share/doc/LaMusica/${project_schema}"
         "{\n"
         "  \"$id\": \"dev.lamusica.schemas.project-v3\",\n"
         "  \"additionalProperties\": false,\n"
         "  \"required\": [\"schemaVersion\", \"projectSampleRate\", \"takeLanes\", \"comps\"],\n"
         "  \"properties\": {\n"
         "    \"schemaVersion\": { \"const\": 3 },\n"
         "    \"projectSampleRate\": { \"type\": \"number\", \"exclusiveMinimum\": 0 },\n"
         "    \"sourceSampleRate\": { \"type\": \"number\", \"exclusiveMinimum\": 0 },\n"
         "    \"takeLanes\": { \"type\": \"array\" },\n"
         "    \"comps\": { \"type\": \"array\" }\n"
         "  }\n"
         "}\n")
  endforeach()
  file(WRITE "${self_test_root}/share/doc/LaMusica/schemas/cli-output-v1.schema.json"
       "{\n"
       "  \"$id\": \"https://lamusica.dev/schemas/cli-output-v1.schema.json\",\n"
       "  \"required\": [\"schemaVersion\"],\n"
       "  \"properties\": {\n"
       "    \"schemaVersion\": { \"const\": 1 },\n"
       "    \"project\": { \"required\": [\"name\", \"schemaVersion\", \"tracks\", \"clips\", \"plugins\", \"automation\"] },\n"
       "    \"preview\": { \"type\": \"boolean\" },\n"
       "    \"mutated\": { \"type\": \"boolean\" },\n"
       "    \"confirmationToken\": { \"type\": \"string\" },\n"
       "    \"render\": { \"required\": [\"project\", \"output\", \"format\", \"bitDepth\", \"startSample\", \"frames\", \"stems\", \"postDitherPeak\"] }\n"
       "  }\n"
       "}\n")
  file(WRITE "${self_test_root}/share/doc/LaMusica/release/accessibility-voiceover-evidence-template.md"
       "Artifact name Signing identity Notarization request id Stapled artifact validated\n"
       "VoiceOver enabled Full Keyboard Access enabled Reduce Motion tested Increase Contrast tested\n"
       "ctest --preset release-universal -R lamusica_daw_accessibility_audit --output-on-failure\n"
       "Result\n"
       "Transport play/stop Record/arm/monitor controls Mixer fader Pan control Meter Timeline clip Time ruler or playhead\n"
       "Piano-roll note Drum pad or step cell Browser tree Inspector fields Plugin chooser/control Export dialog Welcome/templates Guided tour\n"
       "Start and stop transport Arm a track and toggle monitoring Select and edit a timeline clip Change a mixer fader value\n"
       "Inspect a plugin control Cancel and confirm export Choose an onboarding template Restart and skip guided tour\n"
       "Completed without mouse VoiceOver evidence Blocking failures Non-blocking observations Follow-up issue links Release approved by\n")
  file(WRITE "${self_test_root}/share/doc/LaMusica/release/macos-release-evidence-template.md"
       "lipo -archs LaMusica.app/Contents/MacOS/LaMusica lamusica_plugin_scan_worker lamusica_mcpd lamusica_cli\n"
       "codesign --verify --strict --deep codesign -d --entitlements spctl --assess xcrun notarytool submit --wait xcrun stapler validate\n"
       "cmake -DPACKAGE=LaMusica-0.1.0-Darwin.tar.gz -P cmake/VerifyPackage.cmake\n"
       "Online launch from `/Applications` Offline launch from `/Applications` No Gatekeeper override required\n"
       "First record attempt triggered microphone TCC prompt Bundled CLI tools ran from package dSYMs archived atos llvm-symbolizer\n"
       "DAW induced crash produced local report lamusica_mcpd induced crash produced local report\n"
       "Diagnostics upload stayed disabled without consent DMG dSYMs archive SBOM SHA256SUMS SHA256SUMS.sig Blocking failures Non-blocking observations Follow-up issue links Release approved by\n")
  file(WRITE "${self_test_root}/share/doc/LaMusica/release/release-checklist.md"
       "macos-release-evidence-template.md accessibility-voiceover-evidence-template.md\n"
       "GitHub Release includes the blank evidence templates.\n"
       "GitHub Release includes the validated completed evidence files: completed-macos-release-evidence.md and completed-accessibility-voiceover-evidence.md.\n"
       "The GitHub Release publish step sets fail_on_unmatched_files: true so every release asset glob must resolve before publication succeeds.\n"
       "Release workflow uses pinned macos-14 runner and Xcode 15.4.\n"
       "Manual workflow_dispatch releases check out the requested release_tag so build, signing, verification, and publication use the same release ref.\n"
       "The Vulnerability scan step uses fail-build: true with severity-cutoff: critical and runs after completed evidence validation but before Publish GitHub release.\n"
       "Both release artifact upload steps use if-no-files-found: error so missing files fail instead of publishing a partial release.\n"
       "ctest --preset release-universal -R lamusica_mcpd_diagnostics_crash_smoke\n"
       "Manual evidence uses the LaMusica-release-candidate workflow artifact DMG and dSYMs for Gatekeeper, TCC, symbolication, and VoiceOver evidence.\n"
       "scripts/verify-release-evidence.sh validates completed evidence before approval.\n"
       "scripts/verify-release-workflow.sh --self-test validates the release workflow publication gate and rejects broken workflow fixtures.\n"
       "scripts/verify-ci-workflow.sh --self-test validates the CI workflow verification gates and rejects broken workflow fixtures.\n"
       "The validator rejects blank fields, pass/fail or yes/no placeholders, unresolved TBD/TODO markers, pending-evidence notes, not-run or skipped markers, and explicit fail/failed/error/denied/rejected/blocked results; every row must contain concrete release evidence.\n"
       "It also rejects malformed release versions, malformed Git commits, malformed Developer ID identities, and malformed notarization request ids.\n"
       "Completed VoiceOver evidence must include the artifact name, signing identity, notarization request id, and stapled artifact validation for the same release artifact.\n"
       "Completed macOS and VoiceOver evidence must agree on release version, Git commit, signing identity, notarization request id, and the LaMusica-<version>-Darwin.dmg artifact name; the artifact name must match the declared release version.\n"
       "Completed evidence must include concrete CI/log references, Xcode version, tester, date, macOS version, hardware, clean-account name where applicable, and release approver fields.\n"
       "For manual workflow_dispatch releases, provide the semantic v* release_tag to publish and completed-evidence input paths that already exist in the checked-out release branch or tag.\n"
       "Verify launch on a clean macOS user account, online and offline, with no Gatekeeper override.\n"
       "scripts/archive-dsyms.sh scripts/verify-symbolication.sh scripts/verify-provenance.sh\n"
       "Every generated .dSYM contains a non-empty Contents/Resources/DWARF/<binary-basename> payload.\n"
       "Verify provenance reports the source checkout HEAD and dirty=false; --allow-dirty is forbidden for signed release artifacts.\n"
       "verify-symbolication the app, plugin scan worker, MCP daemon, and CLI arm64 x86_64 wrong-symbol\n"
       "LaMusica-dSYMs.tar.gz includes dSYM bundles for the app, plugin scan worker, MCP daemon, and CLI\n"
       "scripts/sign-macos.sh scripts/notarize-macos.sh scripts/verify-signature.sh\n"
       "Developer ID Application identity 10-character Team ID ad-hoc or malformed identity strings are rejected\n"
       "signing helper rejects bundles missing LaMusica.app/Contents/MacOS/LaMusica\n"
       "notarization helper is called only with .dmg or .zip artifacts and rejects any release DMG not named LaMusica-<version>-Darwin.dmg\n"
       "xcrun stapler validate LaMusica-<version>-Darwin.dmg\n"
       "scripts/notarize-macos.sh --artifact LaMusica-<version>-Darwin.dmg --key AuthKey.p8 --key-id <KEYID> --issuer <ISSUER_UUID>\n"
       "App Store Connect key id is 10 uppercase alphanumeric characters and issuer is a UUID.\n"
       "scripts/verify-signature.sh --app LaMusica.app --binary lamusica_plugin_scan_worker --binary lamusica_mcpd --binary lamusica_cli --artifact LaMusica-<version>-Darwin.dmg\n"
       "The release signature gate uses the stapled LaMusica-<version>-Darwin.dmg and requires --binary checks for the plugin scan worker, MCP daemon, and CLI.\n"
       "scripts/sbom.sh scripts/sign-checksums.sh SHA256SUMS.sig\n"
       "scripts/sbom.sh --artifact LaMusica-<version>-Darwin.dmg --artifact LaMusica-dSYMs.tar.gz --output build/release-metadata\n"
       "DMG release metadata must include LaMusica-dSYMs.tar.gz and rejects a DMG-only checksum set or a non-LaMusica-<version>-Darwin.dmg release DMG name.\n"
       "Checksum signing uses a Developer ID Application identity with a 10-character Team ID; ad-hoc or malformed identity strings are rejected.\n"
       "Checksum signing rejects SHA256SUMS files with a release DMG row that is not named LaMusica-<version>-Darwin.dmg or that omits LaMusica-dSYMs.tar.gz.\n"
       "lipo -archs codesign --verify --strict --deep spctl --assess xcrun stapler validate\n"
       "VoiceOver checklist and completed evidence attached to the release notes.\n")
  file(WRITE "${self_test_root}/share/doc/LaMusica/release/accessibility-voiceover-checklist.md"
       "Accessibility VoiceOver Checklist\n"
       "ctest --preset release-universal -R lamusica_daw_accessibility_audit --output-on-failure\n"
       "Enable VoiceOver and Full Keyboard Access.\n"
       "Transport play/stop Record/arm/monitor controls Timeline clip Time ruler or playhead\n"
       "Mixer fader Pan control Meter Piano-roll note Drum pad or step cell Browser tree\n"
       "Inspector fields Plugin chooser/control Export dialog Welcome/templates Guided tour\n"
       "Start and stop transport Select a timeline clip Change a mixer fader value\n"
       "Open the plugin chooser Open the export path choose a template Restart and skip the guided tour\n"
       "Reduce Motion Increase Contrast Failure Policy keyboard with VoiceOver enabled\n")
  file(WRITE "${self_test_root}/share/doc/LaMusica/release/macos-distribution.md"
       "macos-release-evidence-template.md clean macOS user account\n"
       "Open `LaMusica-<version>-Darwin.dmg`\n"
       "online offline offline-launch Gatekeeper microphone TCC dSYM published asset evidence\n"
       "codesign --force --options runtime --timestamp --entitlements apps/daw/lamusica.entitlements\n"
       "scripts/sign-macos.sh codesign --verify --strict --deep spctl --assess\n"
       "Developer ID Application identity 10-character Team ID ad-hoc or malformed identity strings are rejected\n"
       "Missing release binaries are treated as a release failure, including bundles missing LaMusica.app/Contents/MacOS/LaMusica.\n"
       "xcrun notarytool submit LaMusica-<version>-Darwin.dmg xcrun stapler staple LaMusica-<version>-Darwin.dmg xcrun stapler validate LaMusica-<version>-Darwin.dmg\n"
       "scripts/notarize-macos.sh scripts/verify-signature.sh scripts/verify-provenance.sh\n"
       "Release provenance requires dirty=false; --allow-dirty is only for non-release diagnostic builds.\n"
       "ctest --preset release-universal -R lamusica_mcpd_diagnostics_crash_smoke\n"
       "Evidence validation rejects malformed release versions, malformed Git commits, malformed Developer ID identities, and malformed notarization request ids.\n"
       "scripted helper only accepts .dmg or .zip notarization artifacts\n"
       "notarization key id is a 10-character App Store Connect key id and issuer is an App Store Connect issuer UUID\n"
       "--artifact LaMusica-<version>-Darwin.dmg\n"
       "scripts/verify-signature.sh --app LaMusica.app --binary lamusica_plugin_scan_worker --binary lamusica_mcpd --binary lamusica_cli --artifact LaMusica-<version>-Darwin.dmg\n"
       "signature verifier artifact is the stapled release .dmg and requires helper --binary checks\n"
       "scripts/archive-dsyms.sh scripts/verify-symbolication.sh the app, plugin scan worker, MCP daemon, and CLI arm64 x86_64 wrong-symbol scripts/sbom.sh scripts/sign-checksums.sh\n"
       "dSYM DWARF payload matching the binary basename\n"
       "SBOM validates the required dSYM archive row, non-LaMusica-<version>-Darwin.dmg DMG names, and rejects a DMG-only checksum set.\n"
       "Checksum signing rejects ad-hoc or malformed Developer ID identities, release DMG rows not named LaMusica-<version>-Darwin.dmg, release DMG checksum sets without LaMusica-dSYMs.tar.gz, and empty detached signatures.\n"
       "Release workflows validate completed macOS and VoiceOver evidence before publication and publish completed evidence files.\n"
       "The GitHub Release publish step sets fail_on_unmatched_files: true so every release asset glob must resolve before publication succeeds.\n"
       "Release workflow uses pinned macos-14 runner and Xcode 15.4.\n"
       "Manual workflow_dispatch releases check out the requested release_tag so build, signing, verification, and publication use the same release ref.\n"
       "The Vulnerability scan step uses fail-build: true with severity-cutoff: critical and runs after completed evidence validation but before Publish GitHub release.\n"
       "Completed macOS and VoiceOver evidence must agree on release version, Git commit, signing identity, notarization request id, and the LaMusica-<version>-Darwin.dmg artifact name; the artifact name must match the declared release version.\n"
       "Completed evidence must include concrete CI/log references, Xcode version, tester, date, macOS version, hardware, clean-account name where applicable, and release approver fields.\n"
       "The release job uploads LaMusica-release-candidate before the completed-evidence gate for manual Gatekeeper, TCC, symbolication, and VoiceOver evidence collection.\n"
       "Both candidate and final artifact uploads use if-no-files-found: error so missing release files fail the workflow instead of publishing a partial release.\n"
       "Manual workflow_dispatch releases use a semantic v* release_tag and completed-evidence input paths that already exist in the checked-out release branch or tag.\n"
       "prepare_arbitrary_id_project query_arbitrary_id arbitrary_id_edit\n"
       "LaMusica.app/Contents/Resources/en.lproj/InfoPlist.strings es.lproj/InfoPlist.strings fr.lproj/InfoPlist.strings CFBundleLocalizations localized microphone and Apple Events usage strings.\n"
       "SHA256SUMS LAMUSICA_DIAGNOSTICS_ENDPOINT non-empty explicit HTTPS URL missing or non-HTTPS override values block upload vulnerability scan\n")
  file(WRITE "${self_test_root}/share/doc/LaMusica/developer/localization.md"
       "Add apps/daw/resources/i18n/<locale>.txt, register it in apps/daw/src/i18n/StringTables.cpp, "
       "add it to CFBundleLocalizations in apps/daw/Info.plist.in, add apps/daw/resources/macos/<locale>.lproj/InfoPlist.strings, "
       "register it in LAMUSICA_DAW_LOCALIZED_BUNDLE_RESOURCES, and run lamusica_i18n_tests.\n"
       "At startup the DAW resolves the active locale from ApplicationPreferences::preferredLocale, "
       "then juce::SystemStats::getDisplayLanguage(), then English.\n"
       "preferredLocale is a UI preference only and accepts BCP-47-style ids such as es or es_MX.\n"
       "Project files, MCP payloads, command ids, and logs stay stable English/C-locale data.\n"
       "Use NumberFormat only for UI text. Do not call std::locale::global or imbue user locales into project serialization.\n"
       "Bundled production locales are strict, Spanish values fail when untranslated, and coverage: stub marks add-a-locale smoke fixtures.\n")
  file(WRITE "${self_test_root}/share/doc/LaMusica/developer/build-and-test.md"
       "ctest --preset debug ctest --preset release\n"
       "The release test preset runs onboarding, accessibility, localization, generic CLI, determinism, audio-correctness, plugin-hosting, privacy, and legacy first-track compatibility gates.\n"
       "ctest --preset release -R 'lamusica_daw_diagnostics_consent|lamusica_mcpd_diagnostics_crash_smoke'\n"
       "The determinism test compares independent renders, block-size variants, command-journal replay, the committed float golden, the committed WAV golden, and tests/determinism/golden/render-golden.sha256.\n"
       "build/unix-debug/tests/lamusica_render_determinism_tests build/unix-debug/tests/determinism . --update-golden\n"
       "Review render-golden.float32, render-golden.wav, and render-golden.sha256 together. Do not regenerate goldens from CI.\n"
       "The realtime deadline gate writes tests/perf/rt-history.jsonl and fails on xruns or p99 block time at or above the 1024-frame buffer period.\n"
       "Threshold changes require tools/perf/README.md and release-note context.\n"
       "lamusica_cli_(help|schema|prepare_arbitrary_id_project|query_arbitrary_id|arbitrary_id_edit|query_generic|generic_edit|generic_render)\n"
       "cmake -P cmake/CheckMarkdown.cmake cmake -P cmake/CheckDependencyLock.cmake scripts/verify-ci-workflow.sh --self-test\n"
       "cmake -DPACKAGE=LaMusica-0.1.0-Darwin.tar.gz -P cmake/VerifyPackage.cmake\n")
  file(WRITE "${self_test_root}/share/doc/LaMusica/developer/dependencies.md"
       "LaMusica does not download third-party source during configure or build.\n"
       "JUCE 8.0.13 commit 7c9d3783b127263d72bb65fe0a7e2dc8a02a7ac2 content manifest checksum e2ee824cf139a72e3720e996c1cdc70e9ff9dac9653c7f74ccf7d40cf1e3d1c4.\n"
       "The checkout is supplied through LAMUSICA_JUCE_PATH and is not vendored.\n"
       "Do not add Fetch" "Content, CPM, vc" "pkg, Co" "nan, or ExternalProject" "_Add without updating this lock strategy.\n"
       "Supply-chain checks recurse over every source CMakeLists.txt and *.cmake file outside generated build/package directories.\n"
       "cmake -DLAMUSICA_JUCE_PATH=/path/to/JUCE-8.0.13 -P cmake/CheckDependencyLock.cmake\n"
       "cmake -DLAMUSICA_DEPENDENCY_LOCK_SELF_TEST=ON -P cmake/CheckDependencyLock.cmake rejects nested unreviewed downloader integrations such as Fetch" "Content.\n")
  file(WRITE "${self_test_root}/share/doc/LaMusica/developer/project-format.md"
       "Project bundles use project.json with schema version 3 and docs/schemas/project-v3.schema.json.\n"
       "projectSampleRate defaults to 48000.0 for migrated legacy projects.\n"
       "schema 1 manifests upgrade to schema 2 with projectSampleRate = 48000.0.\n"
       "schema 2 manifests upgrade to schema 3 with empty takeLanes and comps.\n"
       "takeLanes persist nondestructive take metadata and comps persist selected take segments rendered by the graph compiler.\n"
       "Audio asset catalog records keep sourceSampleRate and waveform caches record the rate they were analyzed at.\n"
       "Asset paths must be relative and must not contain parent-directory traversal.\n")
  file(WRITE "${self_test_root}/share/doc/LaMusica/performance/realtime-policy.md"
       "Audio callback code must avoid Allocation Locks File I/O Logging JSON parsing MCP work.\n"
       "tests/perf/rt_deadline_bench.cpp renders 8, 16, 32, and 64 track sessions block-by-block.\n"
       "It records p99 block time, max block time, buffer deadline, xrun count, RSS, and measured WAV disk bytes.\n"
       "It appends JSONL to tests/perf/rt-history.jsonl keyed by MachineContext.\n"
       "The gate fails on any xrun and when p99 block time is greater than or equal to the buffer period.\n"
       "Update tools/perf/README.md and release notes when thresholds change.\n")
  file(WRITE "${self_test_root}/share/doc/LaMusica/release/versioning.md"
       "semantic versioning MAJOR MINOR PATCH\n"
       "Current project manifest schema: 3.\n"
       "Schema 1 manifests are upgraded to schema 2 with projectSampleRate = 48000.0.\n"
       "Schema 2 manifests are upgraded to schema 3 with empty comp takeLanes and comps.\n"
       "Future schema changes must add a deterministic migration in migrateProjectManifest and unit coverage.\n")
  foreach(project IN ITEMS examples/empty.Project.lamusica examples/generated-tone.Project.lamusica
                           tutorials/first-song.Project.lamusica)
    file(WRITE "${self_test_root}/share/lamusica/${project}/project.json" "{}\n")
  endforeach()
  file(WRITE "${self_test_root}/share/lamusica/i18n/en.txt"
       "language: en\n"
       "\"Record\" = \"Record\"\n"
       "\"Master pan\" = \"Master pan\"\n"
       "\"Export dialog\" = \"Export dialog\"\n"
       "\"onboarding.template.empty.name\" = \"Empty\"\n"
       "\"onboarding.template.basicMultitrack.name\" = \"Basic Multitrack\"\n"
       "\"onboarding.template.drumSynth.name\" = \"Drum + Synth\"\n"
       "\"onboarding.template.podcastVoice.name\" = \"Podcast / Voice\"\n"
       "\"onboarding.help.userManual\" = \"LaMusica User Manual\"\n"
       "\"onboarding.help.showWelcome\" = \"Show Welcome Window\"\n"
       "\"onboarding.help.restartTour\" = \"Restart Guided Tour\"\n"
       "\"onboarding.help.keyboardShortcuts\" = \"Keyboard Shortcuts\"\n"
       "\"onboarding.welcome.openProject\" = \"Open Project\"\n"
       "\"onboarding.welcome.openRecent\" = \"Open Most Recent\"\n"
       "\"onboarding.tour.skip\" = \"Skip Tour\"\n")
  file(WRITE "${self_test_root}/share/lamusica/i18n/es.txt"
       "language: es\n"
       "\"Record\" = \"Grabar\"\n"
       "\"Master pan\" = \"Paneo maestro\"\n"
       "\"Export dialog\" = \"Dialogo de exportacion\"\n"
       "\"onboarding.template.empty.name\" = \"Vacío\"\n"
       "\"onboarding.template.basicMultitrack.name\" = \"Multipista básico\"\n"
       "\"onboarding.template.drumSynth.name\" = \"Batería + Sintetizador\"\n"
       "\"onboarding.template.podcastVoice.name\" = \"Podcast / Voz\"\n"
       "\"onboarding.help.userManual\" = \"Manual de usuario de LaMusica\"\n"
       "\"onboarding.help.showWelcome\" = \"Mostrar ventana de bienvenida\"\n"
       "\"onboarding.help.restartTour\" = \"Reiniciar visita guiada\"\n"
       "\"onboarding.help.keyboardShortcuts\" = \"Atajos de teclado\"\n"
       "\"onboarding.welcome.openProject\" = \"Abrir proyecto\"\n"
       "\"onboarding.welcome.openRecent\" = \"Abrir más reciente\"\n"
       "\"onboarding.tour.skip\" = \"Omitir visita\"\n")
  file(WRITE "${self_test_root}/share/lamusica/i18n/fr.txt"
       "language: fr\n"
       "coverage: stub\n"
       "\"Record\" = \"Record\"\n"
       "\"Master pan\" = \"Master pan\"\n"
       "\"Export dialog\" = \"Export dialog\"\n"
       "\"onboarding.template.empty.name\" = \"Empty\"\n"
       "\"onboarding.template.basicMultitrack.name\" = \"Basic Multitrack\"\n"
       "\"onboarding.template.drumSynth.name\" = \"Drum + Synth\"\n"
       "\"onboarding.template.podcastVoice.name\" = \"Podcast / Voice\"\n"
       "\"onboarding.help.userManual\" = \"LaMusica User Manual\"\n"
       "\"onboarding.help.showWelcome\" = \"Show Welcome Window\"\n"
       "\"onboarding.help.restartTour\" = \"Restart Guided Tour\"\n"
       "\"onboarding.help.keyboardShortcuts\" = \"Keyboard Shortcuts\"\n"
       "\"onboarding.welcome.openProject\" = \"Open Project\"\n"
       "\"onboarding.welcome.openRecent\" = \"Open Most Recent\"\n"
       "\"onboarding.tour.skip\" = \"Skip Tour\"\n")

  set(good_package "${self_test_dir}/LaMusica-good.tar.gz")
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -E tar czf "${good_package}" "LaMusica-test"
    WORKING_DIRECTORY "${self_test_dir}"
    RESULT_VARIABLE good_tar_result
    ERROR_VARIABLE good_tar_error)
  if(NOT good_tar_result EQUAL 0)
    message(FATAL_ERROR "VerifyPackage self-test could not create good archive: ${good_tar_error}")
  endif()
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -DPACKAGE=${good_package} -P "${CMAKE_CURRENT_LIST_FILE}"
    RESULT_VARIABLE good_verify_result
    OUTPUT_VARIABLE good_verify_output
    ERROR_VARIABLE good_verify_error)
  if(NOT good_verify_result EQUAL 0)
    message(FATAL_ERROR "VerifyPackage self-test good archive failed: ${good_verify_error}")
  endif()

  file(WRITE "${self_test_root}/share/doc/LaMusica/schemas/project-v3.schema.json"
       "{ \"schemaVersion\": { \"const\": 1 }, \"projectSampleRate\": true }\n")
  set(stale_project_schema_package "${self_test_dir}/LaMusica-stale-project-schema.tar.gz")
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -E tar czf "${stale_project_schema_package}" "LaMusica-test"
    WORKING_DIRECTORY "${self_test_dir}"
    RESULT_VARIABLE stale_project_schema_tar_result
    ERROR_VARIABLE stale_project_schema_tar_error)
  if(NOT stale_project_schema_tar_result EQUAL 0)
    message(FATAL_ERROR
            "VerifyPackage self-test could not create stale-project-schema archive: ${stale_project_schema_tar_error}")
  endif()
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -DPACKAGE=${stale_project_schema_package} -P "${CMAKE_CURRENT_LIST_FILE}"
    RESULT_VARIABLE stale_project_schema_verify_result
    OUTPUT_VARIABLE stale_project_schema_verify_output
    ERROR_VARIABLE stale_project_schema_verify_error)
  if(stale_project_schema_verify_result EQUAL 0)
    message(FATAL_ERROR "VerifyPackage self-test failed to reject stale project-v3 schema")
  endif()
  if(NOT stale_project_schema_verify_error MATCHES "project-v3\\.schema\\.json")
    message(FATAL_ERROR
            "VerifyPackage self-test rejected stale project-v3 schema for the wrong reason: ${stale_project_schema_verify_error}")
  endif()
  file(WRITE "${self_test_root}/share/doc/LaMusica/schemas/project-v3.schema.json"
       "{\n"
       "  \"$id\": \"dev.lamusica.schemas.project-v3\",\n"
       "  \"additionalProperties\": false,\n"
       "  \"required\": [\"schemaVersion\", \"projectSampleRate\", \"takeLanes\", \"comps\"],\n"
       "  \"properties\": {\n"
       "    \"schemaVersion\": { \"const\": 3 },\n"
       "    \"projectSampleRate\": { \"type\": \"number\", \"exclusiveMinimum\": 0 },\n"
       "    \"sourceSampleRate\": { \"type\": \"number\", \"exclusiveMinimum\": 0 },\n"
       "    \"takeLanes\": { \"type\": \"array\" },\n"
       "    \"comps\": { \"type\": \"array\" }\n"
       "  }\n"
       "}\n")

  file(WRITE "${self_test_root}/share/doc/LaMusica/schemas/cli-output-v1.schema.json"
       "{ \"schemaVersion\": { \"const\": 1 }, \"render\": true }\n")
  set(stale_cli_schema_package "${self_test_dir}/LaMusica-stale-cli-schema.tar.gz")
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -E tar czf "${stale_cli_schema_package}" "LaMusica-test"
    WORKING_DIRECTORY "${self_test_dir}"
    RESULT_VARIABLE stale_cli_schema_tar_result
    ERROR_VARIABLE stale_cli_schema_tar_error)
  if(NOT stale_cli_schema_tar_result EQUAL 0)
    message(FATAL_ERROR
            "VerifyPackage self-test could not create stale-cli-schema archive: ${stale_cli_schema_tar_error}")
  endif()
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -DPACKAGE=${stale_cli_schema_package} -P "${CMAKE_CURRENT_LIST_FILE}"
    RESULT_VARIABLE stale_cli_schema_verify_result
    OUTPUT_VARIABLE stale_cli_schema_verify_output
    ERROR_VARIABLE stale_cli_schema_verify_error)
  if(stale_cli_schema_verify_result EQUAL 0)
    message(FATAL_ERROR "VerifyPackage self-test failed to reject stale CLI output schema")
  endif()
  if(NOT stale_cli_schema_verify_error MATCHES "cli-output-v1\\.schema\\.json")
    message(FATAL_ERROR
            "VerifyPackage self-test rejected stale CLI output schema for the wrong reason: ${stale_cli_schema_verify_error}")
  endif()
  file(WRITE "${self_test_root}/share/doc/LaMusica/schemas/cli-output-v1.schema.json"
       "{\n"
       "  \"$id\": \"https://lamusica.dev/schemas/cli-output-v1.schema.json\",\n"
       "  \"required\": [\"schemaVersion\"],\n"
       "  \"properties\": {\n"
       "    \"schemaVersion\": { \"const\": 1 },\n"
       "    \"project\": { \"required\": [\"name\", \"schemaVersion\", \"tracks\", \"clips\", \"plugins\", \"automation\"] },\n"
       "    \"preview\": { \"type\": \"boolean\" },\n"
       "    \"mutated\": { \"type\": \"boolean\" },\n"
       "    \"confirmationToken\": { \"type\": \"string\" },\n"
       "    \"render\": { \"required\": [\"project\", \"output\", \"format\", \"bitDepth\", \"startSample\", \"frames\", \"stems\", \"postDitherPeak\"] }\n"
       "  }\n"
       "}\n")

  file(WRITE "${self_test_root}/share/lamusica/i18n/es.txt"
       "language: es\n\"Record\" = \"Grabar\"\n\"Master pan\" = \"Paneo maestro\"\n\"Export dialog\" = \"Dialogo de exportacion\"\n")
  set(missing_onboarding_i18n_package "${self_test_dir}/LaMusica-missing-onboarding-i18n.tar.gz")
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -E tar czf "${missing_onboarding_i18n_package}" "LaMusica-test"
    WORKING_DIRECTORY "${self_test_dir}"
    RESULT_VARIABLE missing_onboarding_i18n_tar_result
    ERROR_VARIABLE missing_onboarding_i18n_tar_error)
  if(NOT missing_onboarding_i18n_tar_result EQUAL 0)
    message(FATAL_ERROR
            "VerifyPackage self-test could not create missing-onboarding-i18n archive: ${missing_onboarding_i18n_tar_error}")
  endif()
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -DPACKAGE=${missing_onboarding_i18n_package} -P "${CMAKE_CURRENT_LIST_FILE}"
    RESULT_VARIABLE missing_onboarding_i18n_verify_result
    OUTPUT_VARIABLE missing_onboarding_i18n_verify_output
    ERROR_VARIABLE missing_onboarding_i18n_verify_error)
  if(missing_onboarding_i18n_verify_result EQUAL 0)
    message(FATAL_ERROR "VerifyPackage self-test failed to reject missing onboarding i18n keys")
  endif()
  if(NOT missing_onboarding_i18n_verify_error MATCHES "onboarding")
    message(FATAL_ERROR
            "VerifyPackage self-test rejected missing onboarding i18n for the wrong reason: ${missing_onboarding_i18n_verify_error}")
  endif()
  file(WRITE "${self_test_root}/share/lamusica/i18n/es.txt"
       "language: es\n"
       "\"Record\" = \"Grabar\"\n"
       "\"Master pan\" = \"Paneo maestro\"\n"
       "\"Export dialog\" = \"Dialogo de exportacion\"\n"
       "\"onboarding.template.empty.name\" = \"Vacío\"\n"
       "\"onboarding.template.basicMultitrack.name\" = \"Multipista básico\"\n"
       "\"onboarding.template.drumSynth.name\" = \"Batería + Sintetizador\"\n"
       "\"onboarding.template.podcastVoice.name\" = \"Podcast / Voz\"\n"
       "\"onboarding.help.userManual\" = \"Manual de usuario de LaMusica\"\n"
       "\"onboarding.help.showWelcome\" = \"Mostrar ventana de bienvenida\"\n"
       "\"onboarding.help.restartTour\" = \"Reiniciar visita guiada\"\n"
       "\"onboarding.help.keyboardShortcuts\" = \"Atajos de teclado\"\n"
       "\"onboarding.welcome.openProject\" = \"Abrir proyecto\"\n"
       "\"onboarding.welcome.openRecent\" = \"Abrir más reciente\"\n"
       "\"onboarding.tour.skip\" = \"Omitir visita\"\n")

  file(REMOVE "${self_test_root}/bin/lamusica_plugin_scan_worker")
  set(missing_worker_package "${self_test_dir}/LaMusica-missing-worker.tar.gz")
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -E tar czf "${missing_worker_package}" "LaMusica-test"
    WORKING_DIRECTORY "${self_test_dir}"
    RESULT_VARIABLE missing_worker_tar_result
    ERROR_VARIABLE missing_worker_tar_error)
  if(NOT missing_worker_tar_result EQUAL 0)
    message(FATAL_ERROR
            "VerifyPackage self-test could not create missing-worker archive: ${missing_worker_tar_error}")
  endif()
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -DPACKAGE=${missing_worker_package} -P "${CMAKE_CURRENT_LIST_FILE}"
    RESULT_VARIABLE missing_worker_verify_result
    OUTPUT_VARIABLE missing_worker_verify_output
    ERROR_VARIABLE missing_worker_verify_error)
  if(missing_worker_verify_result EQUAL 0)
    message(FATAL_ERROR "VerifyPackage self-test failed to reject missing plugin scan worker")
  endif()
  if(NOT missing_worker_verify_error MATCHES "lamusica_plugin_scan_worker")
    message(FATAL_ERROR
            "VerifyPackage self-test rejected the missing-worker archive for the wrong reason: ${missing_worker_verify_error}")
  endif()
  file(WRITE "${self_test_root}/bin/lamusica_plugin_scan_worker" "#!/bin/sh\n")

  file(REMOVE "${self_test_root}/LaMusica.app/Contents/Info.plist")
  set(missing_plist_package "${self_test_dir}/LaMusica-missing-plist.tar.gz")
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -E tar czf "${missing_plist_package}" "LaMusica-test"
    WORKING_DIRECTORY "${self_test_dir}"
    RESULT_VARIABLE missing_plist_tar_result
    ERROR_VARIABLE missing_plist_tar_error)
  if(NOT missing_plist_tar_result EQUAL 0)
    message(FATAL_ERROR
            "VerifyPackage self-test could not create missing-plist archive: ${missing_plist_tar_error}")
  endif()
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -DPACKAGE=${missing_plist_package} -P "${CMAKE_CURRENT_LIST_FILE}"
    RESULT_VARIABLE missing_plist_verify_result
    OUTPUT_VARIABLE missing_plist_verify_output
    ERROR_VARIABLE missing_plist_verify_error)
  if(missing_plist_verify_result EQUAL 0)
    message(FATAL_ERROR "VerifyPackage self-test failed to reject missing app Info.plist")
  endif()
  if(NOT missing_plist_verify_error MATCHES "Info\\.plist")
    message(FATAL_ERROR
            "VerifyPackage self-test rejected the missing-plist archive for the wrong reason: ${missing_plist_verify_error}")
  endif()
  file(WRITE "${self_test_root}/LaMusica.app/Contents/Info.plist"
       "<plist><dict>"
       "<key>CFBundleIdentifier</key><string>dev.lamusica.daw</string>"
       "<key>CFBundleLocalizations</key><array><string>en</string><string>es</string><string>fr</string></array>"
       "<key>NSMicrophoneUsageDescription</key><string>Microphone access records audio.</string>"
       "<key>NSAppleEventsUsageDescription</key><string>Apple Events support automation.</string>"
       "<key>LSApplicationCategoryType</key><string>public.app-category.music</string>"
       "<key>LSMinimumSystemVersion</key><string>14.0</string>"
       "</dict></plist>\n")

  file(REMOVE "${self_test_root}/LaMusica.app/Contents/Resources/es.lproj/InfoPlist.strings")
  set(missing_lproj_package "${self_test_dir}/LaMusica-missing-lproj.tar.gz")
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -E tar czf "${missing_lproj_package}" "LaMusica-test"
    WORKING_DIRECTORY "${self_test_dir}"
    RESULT_VARIABLE missing_lproj_tar_result
    ERROR_VARIABLE missing_lproj_tar_error)
  if(NOT missing_lproj_tar_result EQUAL 0)
    message(FATAL_ERROR
            "VerifyPackage self-test could not create missing-lproj archive: ${missing_lproj_tar_error}")
  endif()
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -DPACKAGE=${missing_lproj_package} -P "${CMAKE_CURRENT_LIST_FILE}"
    RESULT_VARIABLE missing_lproj_verify_result
    OUTPUT_VARIABLE missing_lproj_verify_output
    ERROR_VARIABLE missing_lproj_verify_error)
  if(missing_lproj_verify_result EQUAL 0)
    message(FATAL_ERROR "VerifyPackage self-test failed to reject missing localized InfoPlist.strings")
  endif()
  if(NOT missing_lproj_verify_error MATCHES "es\\.lproj/InfoPlist\\.strings")
    message(FATAL_ERROR
            "VerifyPackage self-test rejected the missing-lproj archive for the wrong reason: ${missing_lproj_verify_error}")
  endif()
  file(WRITE "${self_test_root}/LaMusica.app/Contents/Resources/es.lproj/InfoPlist.strings"
       "\"CFBundleDisplayName\" = \"LaMusica\";\n"
       "\"NSMicrophoneUsageDescription\" = \"LaMusica necesita acceso al microfono para grabar audio en un proyecto.\";\n"
       "\"NSAppleEventsUsageDescription\" = \"LaMusica puede usar Apple Events solo para flujos de automatizacion local aprobados por el usuario.\";\n")

  file(WRITE "${self_test_root}/LaMusica.app/Contents/Info.plist"
       "<plist><dict>"
       "<key>CFBundleIdentifier</key><string>dev.lamusica.app</string>"
       "<key>CFBundleLocalizations</key><array><string>en</string><string>es</string><string>fr</string></array>"
       "<key>NSMicrophoneUsageDescription</key><string>Microphone access records audio.</string>"
       "<key>NSAppleEventsUsageDescription</key><string>Apple Events support automation.</string>"
       "<key>LSApplicationCategoryType</key><string>public.app-category.music</string>"
       "<key>LSMinimumSystemVersion</key><string>14.0</string>"
       "</dict></plist>\n")
  set(wrong_plist_package "${self_test_dir}/LaMusica-wrong-plist.tar.gz")
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -E tar czf "${wrong_plist_package}" "LaMusica-test"
    WORKING_DIRECTORY "${self_test_dir}"
    RESULT_VARIABLE wrong_plist_tar_result
    ERROR_VARIABLE wrong_plist_tar_error)
  if(NOT wrong_plist_tar_result EQUAL 0)
    message(FATAL_ERROR
            "VerifyPackage self-test could not create wrong-plist archive: ${wrong_plist_tar_error}")
  endif()
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -DPACKAGE=${wrong_plist_package} -P "${CMAKE_CURRENT_LIST_FILE}"
    RESULT_VARIABLE wrong_plist_verify_result
    OUTPUT_VARIABLE wrong_plist_verify_output
    ERROR_VARIABLE wrong_plist_verify_error)
  if(wrong_plist_verify_result EQUAL 0)
    message(FATAL_ERROR "VerifyPackage self-test failed to reject wrong app bundle identifier")
  endif()
  if(NOT wrong_plist_verify_error MATCHES "dev\\.lamusica\\.daw")
    message(FATAL_ERROR
            "VerifyPackage self-test rejected the wrong-plist archive for the wrong reason: ${wrong_plist_verify_error}")
  endif()
  file(WRITE "${self_test_root}/LaMusica.app/Contents/Info.plist"
       "<plist><dict>"
       "<key>CFBundleIdentifier</key><string>dev.lamusica.daw</string>"
       "<key>CFBundleLocalizations</key><array><string>en</string><string>es</string><string>fr</string></array>"
       "<key>NSMicrophoneUsageDescription</key><string>Microphone access records audio.</string>"
       "<key>NSAppleEventsUsageDescription</key><string>Apple Events support automation.</string>"
       "<key>LSApplicationCategoryType</key><string>public.app-category.music</string>"
       "<key>LSMinimumSystemVersion</key><string>14.0</string>"
       "</dict></plist>\n")

  file(WRITE "${self_test_root}/LaMusica.app/Contents/Info.plist"
       "<plist><dict>"
       "<key>CFBundleIdentifier</key><string>dev.lamusica.daw</string>"
       "<key>CFBundleLocalizations</key><array><string>en</string><string>es</string><string>fr</string></array>"
       "<key>NSAppleEventsUsageDescription</key><string>Apple Events support automation.</string>"
       "<key>LSApplicationCategoryType</key><string>public.app-category.music</string>"
       "<key>LSMinimumSystemVersion</key><string>14.0</string>"
       "</dict></plist>\n")
  set(missing_microphone_usage_package "${self_test_dir}/LaMusica-missing-microphone-usage.tar.gz")
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -E tar czf "${missing_microphone_usage_package}" "LaMusica-test"
    WORKING_DIRECTORY "${self_test_dir}"
    RESULT_VARIABLE missing_microphone_usage_tar_result
    ERROR_VARIABLE missing_microphone_usage_tar_error)
  if(NOT missing_microphone_usage_tar_result EQUAL 0)
    message(FATAL_ERROR
            "VerifyPackage self-test could not create missing-microphone-usage archive: ${missing_microphone_usage_tar_error}")
  endif()
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -DPACKAGE=${missing_microphone_usage_package} -P "${CMAKE_CURRENT_LIST_FILE}"
    RESULT_VARIABLE missing_microphone_usage_verify_result
    OUTPUT_VARIABLE missing_microphone_usage_verify_output
    ERROR_VARIABLE missing_microphone_usage_verify_error)
  if(missing_microphone_usage_verify_result EQUAL 0)
    message(FATAL_ERROR "VerifyPackage self-test failed to reject missing microphone usage text")
  endif()
  if(NOT missing_microphone_usage_verify_error MATCHES "NSMicrophoneUsageDescription")
    message(FATAL_ERROR
            "VerifyPackage self-test rejected the missing-microphone-usage archive for the wrong reason: ${missing_microphone_usage_verify_error}")
  endif()
  file(WRITE "${self_test_root}/LaMusica.app/Contents/Info.plist"
       "<plist><dict>"
       "<key>CFBundleIdentifier</key><string>dev.lamusica.daw</string>"
       "<key>CFBundleLocalizations</key><array><string>en</string><string>es</string><string>fr</string></array>"
       "<key>NSMicrophoneUsageDescription</key><string>Microphone access records audio.</string>"
       "<key>NSAppleEventsUsageDescription</key><string>Apple Events support automation.</string>"
       "<key>LSApplicationCategoryType</key><string>public.app-category.music</string>"
       "<key>LSMinimumSystemVersion</key><string>14.0</string>"
       "</dict></plist>\n")

  file(WRITE "${self_test_root}/share/lamusica/i18n/es.txt"
       "language: es\n"
       "\"Record\" = \"Record\"\n"
       "\"Master pan\" = \"Paneo maestro\"\n"
       "\"Export dialog\" = \"Dialogo de exportacion\"\n"
       "\"onboarding.template.empty.name\" = \"Vacío\"\n"
       "\"onboarding.template.basicMultitrack.name\" = \"Multipista básico\"\n"
       "\"onboarding.template.drumSynth.name\" = \"Batería + Sintetizador\"\n"
       "\"onboarding.template.podcastVoice.name\" = \"Podcast / Voz\"\n"
       "\"onboarding.help.userManual\" = \"Manual de usuario de LaMusica\"\n"
       "\"onboarding.help.showWelcome\" = \"Mostrar ventana de bienvenida\"\n"
       "\"onboarding.help.restartTour\" = \"Reiniciar visita guiada\"\n"
       "\"onboarding.help.keyboardShortcuts\" = \"Atajos de teclado\"\n"
       "\"onboarding.welcome.openProject\" = \"Abrir proyecto\"\n"
       "\"onboarding.welcome.openRecent\" = \"Abrir más reciente\"\n"
       "\"onboarding.tour.skip\" = \"Omitir visita\"\n")
  set(missing_spanish_package "${self_test_dir}/LaMusica-missing-spanish.tar.gz")
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -E tar czf "${missing_spanish_package}" "LaMusica-test"
    WORKING_DIRECTORY "${self_test_dir}"
    RESULT_VARIABLE missing_spanish_tar_result
    ERROR_VARIABLE missing_spanish_tar_error)
  if(NOT missing_spanish_tar_result EQUAL 0)
    message(FATAL_ERROR
            "VerifyPackage self-test could not create missing-spanish archive: ${missing_spanish_tar_error}")
  endif()
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -DPACKAGE=${missing_spanish_package} -P "${CMAKE_CURRENT_LIST_FILE}"
    RESULT_VARIABLE missing_spanish_verify_result
    OUTPUT_VARIABLE missing_spanish_verify_output
    ERROR_VARIABLE missing_spanish_verify_error)
  if(missing_spanish_verify_result EQUAL 0)
    message(FATAL_ERROR "VerifyPackage self-test failed to reject missing Spanish translation")
  endif()
  if(NOT missing_spanish_verify_error MATCHES "Grabar")
    message(FATAL_ERROR
            "VerifyPackage self-test rejected the missing-spanish archive for the wrong reason: ${missing_spanish_verify_error}")
  endif()
  file(WRITE "${self_test_root}/share/lamusica/i18n/es.txt"
       "language: es\n"
       "\"Record\" = \"Grabar\"\n"
       "\"Master pan\" = \"Paneo maestro\"\n"
       "\"Export dialog\" = \"Dialogo de exportacion\"\n"
       "\"onboarding.template.empty.name\" = \"Vacío\"\n"
       "\"onboarding.template.basicMultitrack.name\" = \"Multipista básico\"\n"
       "\"onboarding.template.drumSynth.name\" = \"Batería + Sintetizador\"\n"
       "\"onboarding.template.podcastVoice.name\" = \"Podcast / Voz\"\n"
       "\"onboarding.help.userManual\" = \"Manual de usuario de LaMusica\"\n"
       "\"onboarding.help.showWelcome\" = \"Mostrar ventana de bienvenida\"\n"
       "\"onboarding.help.restartTour\" = \"Reiniciar visita guiada\"\n"
       "\"onboarding.help.keyboardShortcuts\" = \"Atajos de teclado\"\n"
       "\"onboarding.welcome.openProject\" = \"Abrir proyecto\"\n"
       "\"onboarding.welcome.openRecent\" = \"Abrir más reciente\"\n"
       "\"onboarding.tour.skip\" = \"Omitir visita\"\n")

  file(WRITE "${self_test_root}/share/doc/LaMusica/NOTICE"
       "LaMusica AGPL-3.0-or-later\n")
  set(missing_source_notice_package "${self_test_dir}/LaMusica-missing-source-notice.tar.gz")
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -E tar czf "${missing_source_notice_package}" "LaMusica-test"
    WORKING_DIRECTORY "${self_test_dir}"
    RESULT_VARIABLE missing_source_notice_tar_result
    ERROR_VARIABLE missing_source_notice_tar_error)
  if(NOT missing_source_notice_tar_result EQUAL 0)
    message(FATAL_ERROR
            "VerifyPackage self-test could not create missing-source-notice archive: ${missing_source_notice_tar_error}")
  endif()
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -DPACKAGE=${missing_source_notice_package} -P "${CMAKE_CURRENT_LIST_FILE}"
    RESULT_VARIABLE missing_source_notice_verify_result
    OUTPUT_VARIABLE missing_source_notice_verify_output
    ERROR_VARIABLE missing_source_notice_verify_error)
  if(missing_source_notice_verify_result EQUAL 0)
    message(FATAL_ERROR
            "VerifyPackage self-test failed to reject NOTICE without corresponding-source text")
  endif()
  if(NOT missing_source_notice_verify_error MATCHES "Corresponding source")
    message(FATAL_ERROR
            "VerifyPackage self-test rejected the missing-source-notice archive for the wrong reason: ${missing_source_notice_verify_error}")
  endif()
  file(WRITE "${self_test_root}/share/doc/LaMusica/NOTICE"
       "LaMusica AGPL-3.0-or-later\nCorresponding source is available.\n")

  file(WRITE "${self_test_root}/share/doc/LaMusica/THIRD_PARTY-NOTICES.md"
       "Third-party notices without JUCE/license/source details.\n")
  set(incomplete_third_party_package "${self_test_dir}/LaMusica-incomplete-third-party.tar.gz")
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -E tar czf "${incomplete_third_party_package}" "LaMusica-test"
    WORKING_DIRECTORY "${self_test_dir}"
    RESULT_VARIABLE incomplete_third_party_tar_result
    ERROR_VARIABLE incomplete_third_party_tar_error)
  if(NOT incomplete_third_party_tar_result EQUAL 0)
    message(FATAL_ERROR
            "VerifyPackage self-test could not create incomplete-third-party archive: ${incomplete_third_party_tar_error}")
  endif()
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -DPACKAGE=${incomplete_third_party_package} -P "${CMAKE_CURRENT_LIST_FILE}"
    RESULT_VARIABLE incomplete_third_party_verify_result
    OUTPUT_VARIABLE incomplete_third_party_verify_output
    ERROR_VARIABLE incomplete_third_party_verify_error)
  if(incomplete_third_party_verify_result EQUAL 0)
    message(FATAL_ERROR "VerifyPackage self-test failed to reject incomplete third-party notices")
  endif()
  if(NOT incomplete_third_party_verify_error MATCHES "THIRD_PARTY-NOTICES\\.md")
    message(FATAL_ERROR
            "VerifyPackage self-test rejected the incomplete-third-party archive for the wrong reason: ${incomplete_third_party_verify_error}")
  endif()
  file(WRITE "${self_test_root}/share/doc/LaMusica/THIRD_PARTY-NOTICES.md"
       "# Third-Party Notices\n"
       "LaMusica currently links against JUCE when building the macOS application.\n"
       "## JUCE\n"
       "Version: 8.0.13\n"
       "Required commit: 7c9d3783b127263d72bb65fe0a7e2dc8a02a7ac2\n"
       "Source: https://github.com/juce-framework/JUCE\n"
       "License: GPL/commercial dual license. LaMusica uses the open-source GPL-compatible path and distributes LaMusica under AGPL-3.0-or-later.\n"
       "No third-party plugin binaries are bundled in release packages.\n"
       "Test plugin-hosting fixtures are in-repository mock components unless a future release note explicitly lists a redistributable plugin and its license.\n")

  file(WRITE "${self_test_root}/share/doc/LaMusica/privacy-and-diagnostics.md"
       "LaMusica does not send crash reports or telemetry unless the user grants diagnostics consent.\n"
       "Usage telemetry is a separate opt-in from crash reports, and LaMusica currently emits no usage telemetry events.\n"
       "The signal handler writes signal, pid, and a deferred backtrace marker using async-signal-safe operations.\n"
       "Scrubbing and any upload preparation happen later, outside the signal handler.\n"
       "Collected fields: application name, app version and Git commit, operating system version, crash signal, stack frames or backtrace.\n"
       "Payloads must not include project contents, audio, MIDI, usernames, home-directory paths, absolute file paths, or project bundle names.\n"
       "Scrubbed payloads replace sensitive values with <path> or <project> before upload.\n"
       "Endpoint: https://diagnostics.lamusica.dev/v1/crash. Override: LAMUSICA_DIAGNOSTICS_ENDPOINT.\n"
       "The environment override must resolve to a non-empty explicit HTTPS URL; missing or non-HTTPS override values block upload.\n"
       "Retention: within 30 days. Opt out by setting diagnostics consent to Declined.\n")
  set(missing_privacy_optout_package "${self_test_dir}/LaMusica-missing-privacy-optout.tar.gz")
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -E tar czf "${missing_privacy_optout_package}" "LaMusica-test"
    WORKING_DIRECTORY "${self_test_dir}"
    RESULT_VARIABLE missing_privacy_optout_tar_result
    ERROR_VARIABLE missing_privacy_optout_tar_error)
  if(NOT missing_privacy_optout_tar_result EQUAL 0)
    message(FATAL_ERROR
            "VerifyPackage self-test could not create missing-privacy-optout archive: ${missing_privacy_optout_tar_error}")
  endif()
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -DPACKAGE=${missing_privacy_optout_package} -P "${CMAKE_CURRENT_LIST_FILE}"
    RESULT_VARIABLE missing_privacy_optout_verify_result
    OUTPUT_VARIABLE missing_privacy_optout_verify_output
    ERROR_VARIABLE missing_privacy_optout_verify_error)
  if(missing_privacy_optout_verify_result EQUAL 0)
    message(FATAL_ERROR
            "VerifyPackage self-test failed to reject privacy disclosure without opt-out text")
  endif()
  if(NOT missing_privacy_optout_verify_error MATCHES "privacy-and-diagnostics\\.md is missing required text")
    message(FATAL_ERROR
            "VerifyPackage self-test rejected the missing-privacy-optout archive for the wrong reason: ${missing_privacy_optout_verify_error}")
  endif()
  file(WRITE "${self_test_root}/share/doc/LaMusica/privacy-and-diagnostics.md"
       "LaMusica does not send crash reports or telemetry unless the user grants diagnostics consent.\n"
       "Usage telemetry is a separate opt-in from crash reports, and LaMusica currently emits no usage telemetry events.\n"
       "The signal handler writes signal, pid, and a deferred backtrace marker using async-signal-safe operations.\n"
       "Scrubbing and any upload preparation happen later, outside the signal handler.\n"
       "Collected fields: application name, app version and Git commit, operating system version, crash signal, stack frames or backtrace.\n"
       "Payloads must not include project contents, audio, MIDI, usernames, home-directory paths, absolute file paths, or project bundle names.\n"
       "Scrubbed payloads replace sensitive values with <path> or <project> before upload.\n"
       "Endpoint: https://diagnostics.lamusica.dev/v1/crash. Override: LAMUSICA_DIAGNOSTICS_ENDPOINT.\n"
       "The environment override must resolve to a non-empty explicit HTTPS URL; missing or non-HTTPS override values block upload.\n"
       "Retention: within 30 days. Opt out by setting diagnostics consent to Declined and turning diagnostics sharing off in Preferences > Privacy.\n")

  file(WRITE "${self_test_root}/share/doc/LaMusica/release/security-disclosure.md"
       "Contact security@lamusica.dev. We acknowledge within 3 business days and keep reports private.\n")
  set(incomplete_security_package "${self_test_dir}/LaMusica-incomplete-security.tar.gz")
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -E tar czf "${incomplete_security_package}" "LaMusica-test"
    WORKING_DIRECTORY "${self_test_dir}"
    RESULT_VARIABLE incomplete_security_tar_result
    ERROR_VARIABLE incomplete_security_tar_error)
  if(NOT incomplete_security_tar_result EQUAL 0)
    message(FATAL_ERROR
            "VerifyPackage self-test could not create incomplete-security archive: ${incomplete_security_tar_error}")
  endif()
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -DPACKAGE=${incomplete_security_package} -P "${CMAKE_CURRENT_LIST_FILE}"
    RESULT_VARIABLE incomplete_security_verify_result
    OUTPUT_VARIABLE incomplete_security_verify_output
    ERROR_VARIABLE incomplete_security_verify_error)
  if(incomplete_security_verify_result EQUAL 0)
    message(FATAL_ERROR
            "VerifyPackage self-test failed to reject incomplete security disclosure")
  endif()
  if(NOT incomplete_security_verify_error MATCHES "security-disclosure\\.md is missing required")
    message(FATAL_ERROR
            "VerifyPackage self-test rejected the incomplete-security archive for the wrong reason: ${incomplete_security_verify_error}")
  endif()
  file(WRITE "${self_test_root}/share/doc/LaMusica/release/security-disclosure.md"
       "Contact security@lamusica.dev. We acknowledge within 3 business days, prepare a fix and release plan, use PGP, and keep reports private.\n")

  file(WRITE "${self_test_root}/share/doc/LaMusica/user-manual.md"
       "On first launch, LaMusica opens a welcome surface.\n"
       "New Project opens the welcome/template flow with Empty, Basic Multitrack, Drum + Synth, and Podcast / Voice templates.\n")
  set(missing_privacy_link_package "${self_test_dir}/LaMusica-missing-privacy-link.tar.gz")
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -E tar czf "${missing_privacy_link_package}" "LaMusica-test"
    WORKING_DIRECTORY "${self_test_dir}"
    RESULT_VARIABLE missing_privacy_link_tar_result
    ERROR_VARIABLE missing_privacy_link_tar_error)
  if(NOT missing_privacy_link_tar_result EQUAL 0)
    message(FATAL_ERROR
            "VerifyPackage self-test could not create missing-privacy-link archive: ${missing_privacy_link_tar_error}")
  endif()
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -DPACKAGE=${missing_privacy_link_package} -P "${CMAKE_CURRENT_LIST_FILE}"
    RESULT_VARIABLE missing_privacy_link_verify_result
    OUTPUT_VARIABLE missing_privacy_link_verify_output
    ERROR_VARIABLE missing_privacy_link_verify_error)
  if(missing_privacy_link_verify_result EQUAL 0)
    message(FATAL_ERROR
            "VerifyPackage self-test failed to reject user manual without privacy disclosure link")
  endif()
  if(NOT missing_privacy_link_verify_error MATCHES "user-manual\\.md does not link to privacy-and-diagnostics\\.md")
    message(FATAL_ERROR
            "VerifyPackage self-test rejected the missing-privacy-link archive for the wrong reason: ${missing_privacy_link_verify_error}")
  endif()
  file(WRITE "${self_test_root}/share/doc/LaMusica/user-manual.md"
       "See privacy-and-diagnostics.md for diagnostics policy.\n"
       "On first launch, LaMusica opens a welcome surface.\n"
       "New Project opens the welcome/template flow with Empty, Basic Multitrack, Drum + Synth, and Podcast / Voice templates.\n")

  file(REMOVE "${self_test_root}/share/doc/LaMusica/release/accessibility-voiceover-checklist.md")
  set(missing_voiceover_checklist_package "${self_test_dir}/LaMusica-missing-voiceover-checklist.tar.gz")
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -E tar czf "${missing_voiceover_checklist_package}" "LaMusica-test"
    WORKING_DIRECTORY "${self_test_dir}"
    RESULT_VARIABLE missing_voiceover_checklist_tar_result
    ERROR_VARIABLE missing_voiceover_checklist_tar_error)
  if(NOT missing_voiceover_checklist_tar_result EQUAL 0)
    message(FATAL_ERROR
            "VerifyPackage self-test could not create missing-voiceover-checklist archive: ${missing_voiceover_checklist_tar_error}")
  endif()
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -DPACKAGE=${missing_voiceover_checklist_package} -P "${CMAKE_CURRENT_LIST_FILE}"
    RESULT_VARIABLE missing_voiceover_checklist_verify_result
    OUTPUT_VARIABLE missing_voiceover_checklist_verify_output
    ERROR_VARIABLE missing_voiceover_checklist_verify_error)
  if(missing_voiceover_checklist_verify_result EQUAL 0)
    message(FATAL_ERROR
            "VerifyPackage self-test failed to reject package without VoiceOver checklist")
  endif()
  if(NOT missing_voiceover_checklist_verify_error MATCHES "accessibility-voiceover-checklist")
    message(FATAL_ERROR
            "VerifyPackage self-test rejected the missing-voiceover-checklist archive for the wrong reason: ${missing_voiceover_checklist_verify_error}")
  endif()
  file(WRITE "${self_test_root}/share/doc/LaMusica/release/accessibility-voiceover-checklist.md"
       "Accessibility VoiceOver Checklist\n"
       "ctest --preset release-universal -R lamusica_daw_accessibility_audit --output-on-failure\n"
       "Enable VoiceOver and Full Keyboard Access.\n"
       "Transport play/stop Record/arm/monitor controls Timeline clip Time ruler or playhead\n"
       "Mixer fader Pan control Meter Piano-roll note Drum pad or step cell Browser tree\n"
       "Inspector fields Plugin chooser/control Export dialog Welcome/templates Guided tour\n"
       "Start and stop transport Select a timeline clip Change a mixer fader value\n"
       "Open the plugin chooser Open the export path choose a template Restart and skip the guided tour\n"
       "Reduce Motion Increase Contrast Failure Policy keyboard with VoiceOver enabled\n")

  file(WRITE "${self_test_root}/share/doc/LaMusica/release/accessibility-voiceover-checklist.md"
       "VoiceOver checklist without workflow coverage.\n")
  set(incomplete_voiceover_checklist_package "${self_test_dir}/LaMusica-incomplete-voiceover-checklist.tar.gz")
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -E tar czf "${incomplete_voiceover_checklist_package}" "LaMusica-test"
    WORKING_DIRECTORY "${self_test_dir}"
    RESULT_VARIABLE incomplete_voiceover_checklist_tar_result
    ERROR_VARIABLE incomplete_voiceover_checklist_tar_error)
  if(NOT incomplete_voiceover_checklist_tar_result EQUAL 0)
    message(FATAL_ERROR
            "VerifyPackage self-test could not create incomplete-voiceover-checklist archive: ${incomplete_voiceover_checklist_tar_error}")
  endif()
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -DPACKAGE=${incomplete_voiceover_checklist_package} -P "${CMAKE_CURRENT_LIST_FILE}"
    RESULT_VARIABLE incomplete_voiceover_checklist_verify_result
    OUTPUT_VARIABLE incomplete_voiceover_checklist_verify_output
    ERROR_VARIABLE incomplete_voiceover_checklist_verify_error)
  if(incomplete_voiceover_checklist_verify_result EQUAL 0)
    message(FATAL_ERROR "VerifyPackage self-test failed to reject incomplete VoiceOver checklist")
  endif()
  if(NOT incomplete_voiceover_checklist_verify_error MATCHES "accessibility-voiceover-checklist")
    message(FATAL_ERROR
            "VerifyPackage self-test rejected the incomplete-voiceover-checklist archive for the wrong reason: ${incomplete_voiceover_checklist_verify_error}")
  endif()
  file(WRITE "${self_test_root}/share/doc/LaMusica/release/accessibility-voiceover-checklist.md"
       "Accessibility VoiceOver Checklist\n"
       "ctest --preset release-universal -R lamusica_daw_accessibility_audit --output-on-failure\n"
       "Enable VoiceOver and Full Keyboard Access.\n"
       "Transport play/stop Record/arm/monitor controls Timeline clip Time ruler or playhead\n"
       "Mixer fader Pan control Meter Piano-roll note Drum pad or step cell Browser tree\n"
       "Inspector fields Plugin chooser/control Export dialog Welcome/templates Guided tour\n"
       "Start and stop transport Select a timeline clip Change a mixer fader value\n"
       "Open the plugin chooser Open the export path choose a template Restart and skip the guided tour\n"
       "Reduce Motion Increase Contrast Failure Policy keyboard with VoiceOver enabled\n")

  file(REMOVE "${self_test_root}/share/doc/LaMusica/release/accessibility-voiceover-evidence-template.md")
  set(missing_voiceover_evidence_package "${self_test_dir}/LaMusica-missing-voiceover-evidence.tar.gz")
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -E tar czf "${missing_voiceover_evidence_package}" "LaMusica-test"
    WORKING_DIRECTORY "${self_test_dir}"
    RESULT_VARIABLE missing_voiceover_evidence_tar_result
    ERROR_VARIABLE missing_voiceover_evidence_tar_error)
  if(NOT missing_voiceover_evidence_tar_result EQUAL 0)
    message(FATAL_ERROR
            "VerifyPackage self-test could not create missing-voiceover-evidence archive: ${missing_voiceover_evidence_tar_error}")
  endif()
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -DPACKAGE=${missing_voiceover_evidence_package} -P "${CMAKE_CURRENT_LIST_FILE}"
    RESULT_VARIABLE missing_voiceover_evidence_verify_result
    OUTPUT_VARIABLE missing_voiceover_evidence_verify_output
    ERROR_VARIABLE missing_voiceover_evidence_verify_error)
  if(missing_voiceover_evidence_verify_result EQUAL 0)
    message(FATAL_ERROR
            "VerifyPackage self-test failed to reject package without VoiceOver evidence template")
  endif()
  if(NOT missing_voiceover_evidence_verify_error MATCHES "accessibility-voiceover-evidence-template")
    message(FATAL_ERROR
            "VerifyPackage self-test rejected the missing-voiceover-evidence archive for the wrong reason: ${missing_voiceover_evidence_verify_error}")
  endif()
  file(WRITE "${self_test_root}/share/doc/LaMusica/release/accessibility-voiceover-evidence-template.md"
       "Artifact name Signing identity Notarization request id Stapled artifact validated\n"
       "VoiceOver enabled Full Keyboard Access enabled Reduce Motion tested Increase Contrast tested\n"
       "ctest --preset release-universal -R lamusica_daw_accessibility_audit --output-on-failure\n"
       "Result\n"
       "Transport play/stop Record/arm/monitor controls Mixer fader Pan control Meter Timeline clip Time ruler or playhead\n"
       "Piano-roll note Drum pad or step cell Browser tree Inspector fields Plugin chooser/control Export dialog Welcome/templates Guided tour\n"
       "Start and stop transport Arm a track and toggle monitoring Select and edit a timeline clip Change a mixer fader value\n"
       "Inspect a plugin control Cancel and confirm export Choose an onboarding template Restart and skip guided tour\n"
       "Completed without mouse VoiceOver evidence Blocking failures Non-blocking observations Follow-up issue links Release approved by\n")
  file(WRITE "${self_test_root}/share/doc/LaMusica/release/accessibility-voiceover-evidence-template.md"
       "VoiceOver enabled, but no keyboard workflow evidence fields.\n")
  set(incomplete_voiceover_evidence_package "${self_test_dir}/LaMusica-incomplete-voiceover-evidence.tar.gz")
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -E tar czf "${incomplete_voiceover_evidence_package}" "LaMusica-test"
    WORKING_DIRECTORY "${self_test_dir}"
    RESULT_VARIABLE incomplete_voiceover_evidence_tar_result
    ERROR_VARIABLE incomplete_voiceover_evidence_tar_error)
  if(NOT incomplete_voiceover_evidence_tar_result EQUAL 0)
    message(FATAL_ERROR
            "VerifyPackage self-test could not create incomplete-voiceover-evidence archive: ${incomplete_voiceover_evidence_tar_error}")
  endif()
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -DPACKAGE=${incomplete_voiceover_evidence_package} -P "${CMAKE_CURRENT_LIST_FILE}"
    RESULT_VARIABLE incomplete_voiceover_evidence_verify_result
    OUTPUT_VARIABLE incomplete_voiceover_evidence_verify_output
    ERROR_VARIABLE incomplete_voiceover_evidence_verify_error)
  if(incomplete_voiceover_evidence_verify_result EQUAL 0)
    message(FATAL_ERROR
            "VerifyPackage self-test failed to reject incomplete VoiceOver evidence template")
  endif()
  if(NOT incomplete_voiceover_evidence_verify_error MATCHES "accessibility-voiceover-evidence-template")
    message(FATAL_ERROR
            "VerifyPackage self-test rejected the incomplete-voiceover-evidence archive for the wrong reason: ${incomplete_voiceover_evidence_verify_error}")
  endif()
  file(WRITE "${self_test_root}/share/doc/LaMusica/release/accessibility-voiceover-evidence-template.md"
       "Artifact name Signing identity Notarization request id Stapled artifact validated\n"
       "VoiceOver enabled Full Keyboard Access enabled Reduce Motion tested Increase Contrast tested\n"
       "ctest --preset release-universal -R lamusica_daw_accessibility_audit --output-on-failure\n"
       "Result\n"
       "Transport play/stop Record/arm/monitor controls Mixer fader Pan control Meter Timeline clip Time ruler or playhead\n"
       "Piano-roll note Drum pad or step cell Browser tree Inspector fields Plugin chooser/control Export dialog Welcome/templates Guided tour\n"
       "Start and stop transport Arm a track and toggle monitoring Select and edit a timeline clip Change a mixer fader value\n"
       "Inspect a plugin control Cancel and confirm export Choose an onboarding template Restart and skip guided tour\n"
       "Completed without mouse VoiceOver evidence Blocking failures Non-blocking observations Follow-up issue links Release approved by\n")

  file(REMOVE "${self_test_root}/share/doc/LaMusica/release/macos-release-evidence-template.md")
  set(missing_macos_release_evidence_package "${self_test_dir}/LaMusica-missing-macos-release-evidence.tar.gz")
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -E tar czf "${missing_macos_release_evidence_package}" "LaMusica-test"
    WORKING_DIRECTORY "${self_test_dir}"
    RESULT_VARIABLE missing_macos_release_evidence_tar_result
    ERROR_VARIABLE missing_macos_release_evidence_tar_error)
  if(NOT missing_macos_release_evidence_tar_result EQUAL 0)
    message(FATAL_ERROR
            "VerifyPackage self-test could not create missing-macos-release-evidence archive: ${missing_macos_release_evidence_tar_error}")
  endif()
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -DPACKAGE=${missing_macos_release_evidence_package} -P "${CMAKE_CURRENT_LIST_FILE}"
    RESULT_VARIABLE missing_macos_release_evidence_verify_result
    OUTPUT_VARIABLE missing_macos_release_evidence_verify_output
    ERROR_VARIABLE missing_macos_release_evidence_verify_error)
  if(missing_macos_release_evidence_verify_result EQUAL 0)
    message(FATAL_ERROR
            "VerifyPackage self-test failed to reject package without macOS release evidence template")
  endif()
  if(NOT missing_macos_release_evidence_verify_error MATCHES "macos-release-evidence-template")
    message(FATAL_ERROR
            "VerifyPackage self-test rejected the missing-macos-release-evidence archive for the wrong reason: ${missing_macos_release_evidence_verify_error}")
  endif()
  file(WRITE "${self_test_root}/share/doc/LaMusica/release/macos-release-evidence-template.md"
       "lipo -archs LaMusica.app/Contents/MacOS/LaMusica lamusica_plugin_scan_worker lamusica_mcpd lamusica_cli\n"
       "codesign --verify --strict --deep codesign -d --entitlements spctl --assess xcrun notarytool submit --wait xcrun stapler validate\n"
       "cmake -DPACKAGE=LaMusica-0.1.0-Darwin.tar.gz -P cmake/VerifyPackage.cmake\n"
       "Online launch from `/Applications` Offline launch from `/Applications` No Gatekeeper override required\n"
       "First record attempt triggered microphone TCC prompt Bundled CLI tools ran from package dSYMs archived atos llvm-symbolizer\n"
       "DAW induced crash produced local report lamusica_mcpd induced crash produced local report\n"
       "Diagnostics upload stayed disabled without consent DMG dSYMs archive SBOM SHA256SUMS SHA256SUMS.sig Blocking failures Non-blocking observations Follow-up issue links Release approved by\n")
  file(WRITE "${self_test_root}/share/doc/LaMusica/release/macos-release-evidence-template.md"
       "codesign --verify --strict --deep, but no Gatekeeper or launch evidence fields.\n")
  set(incomplete_macos_release_evidence_package "${self_test_dir}/LaMusica-incomplete-macos-release-evidence.tar.gz")
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -E tar czf "${incomplete_macos_release_evidence_package}" "LaMusica-test"
    WORKING_DIRECTORY "${self_test_dir}"
    RESULT_VARIABLE incomplete_macos_release_evidence_tar_result
    ERROR_VARIABLE incomplete_macos_release_evidence_tar_error)
  if(NOT incomplete_macos_release_evidence_tar_result EQUAL 0)
    message(FATAL_ERROR
            "VerifyPackage self-test could not create incomplete-macos-release-evidence archive: ${incomplete_macos_release_evidence_tar_error}")
  endif()
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -DPACKAGE=${incomplete_macos_release_evidence_package} -P "${CMAKE_CURRENT_LIST_FILE}"
    RESULT_VARIABLE incomplete_macos_release_evidence_verify_result
    OUTPUT_VARIABLE incomplete_macos_release_evidence_verify_output
    ERROR_VARIABLE incomplete_macos_release_evidence_verify_error)
  if(incomplete_macos_release_evidence_verify_result EQUAL 0)
    message(FATAL_ERROR
            "VerifyPackage self-test failed to reject incomplete macOS release evidence template")
  endif()
  if(NOT incomplete_macos_release_evidence_verify_error MATCHES "macos-release-evidence-template")
    message(FATAL_ERROR
            "VerifyPackage self-test rejected the incomplete-macos-release-evidence archive for the wrong reason: ${incomplete_macos_release_evidence_verify_error}")
  endif()
  file(WRITE "${self_test_root}/share/doc/LaMusica/release/macos-release-evidence-template.md"
       "lipo -archs LaMusica.app/Contents/MacOS/LaMusica lamusica_plugin_scan_worker lamusica_mcpd lamusica_cli\n"
       "codesign --verify --strict --deep codesign -d --entitlements spctl --assess xcrun notarytool submit --wait xcrun stapler validate\n"
       "cmake -DPACKAGE=LaMusica-0.1.0-Darwin.tar.gz -P cmake/VerifyPackage.cmake\n"
       "Online launch from `/Applications` Offline launch from `/Applications` No Gatekeeper override required\n"
       "First record attempt triggered microphone TCC prompt Bundled CLI tools ran from package dSYMs archived atos llvm-symbolizer\n"
       "DAW induced crash produced local report lamusica_mcpd induced crash produced local report\n"
       "Diagnostics upload stayed disabled without consent DMG dSYMs archive SBOM SHA256SUMS SHA256SUMS.sig Blocking failures Non-blocking observations Follow-up issue links Release approved by\n")

  file(WRITE "${self_test_root}/share/doc/LaMusica/release/release-checklist.md"
       "Release checklist without evidence template links.\n")
  set(incomplete_release_checklist_package "${self_test_dir}/LaMusica-incomplete-release-checklist.tar.gz")
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -E tar czf "${incomplete_release_checklist_package}" "LaMusica-test"
    WORKING_DIRECTORY "${self_test_dir}"
    RESULT_VARIABLE incomplete_release_checklist_tar_result
    ERROR_VARIABLE incomplete_release_checklist_tar_error)
  if(NOT incomplete_release_checklist_tar_result EQUAL 0)
    message(FATAL_ERROR
            "VerifyPackage self-test could not create incomplete-release-checklist archive: ${incomplete_release_checklist_tar_error}")
  endif()
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -DPACKAGE=${incomplete_release_checklist_package} -P "${CMAKE_CURRENT_LIST_FILE}"
    RESULT_VARIABLE incomplete_release_checklist_verify_result
    OUTPUT_VARIABLE incomplete_release_checklist_verify_output
    ERROR_VARIABLE incomplete_release_checklist_verify_error)
  if(incomplete_release_checklist_verify_result EQUAL 0)
    message(FATAL_ERROR
            "VerifyPackage self-test failed to reject incomplete release checklist evidence links")
  endif()
  if(NOT incomplete_release_checklist_verify_error MATCHES "release-checklist\\.md")
    message(FATAL_ERROR
            "VerifyPackage self-test rejected the incomplete-release-checklist archive for the wrong reason: ${incomplete_release_checklist_verify_error}")
  endif()
  file(WRITE "${self_test_root}/share/doc/LaMusica/release/release-checklist.md"
       "macos-release-evidence-template.md accessibility-voiceover-evidence-template.md\n"
       "GitHub Release includes the blank evidence templates.\n"
       "GitHub Release includes the validated completed evidence files: completed-macos-release-evidence.md and completed-accessibility-voiceover-evidence.md.\n"
       "The GitHub Release publish step sets fail_on_unmatched_files: true so every release asset glob must resolve before publication succeeds.\n"
       "Release workflow uses pinned macos-14 runner and Xcode 15.4.\n"
       "Manual workflow_dispatch releases check out the requested release_tag so build, signing, verification, and publication use the same release ref.\n"
       "The Vulnerability scan step uses fail-build: true with severity-cutoff: critical and runs after completed evidence validation but before Publish GitHub release.\n"
       "Both release artifact upload steps use if-no-files-found: error so missing files fail instead of publishing a partial release.\n"
       "ctest --preset release-universal -R lamusica_mcpd_diagnostics_crash_smoke\n"
       "Manual evidence uses the LaMusica-release-candidate workflow artifact DMG and dSYMs for Gatekeeper, TCC, symbolication, and VoiceOver evidence.\n"
       "scripts/verify-release-evidence.sh validates completed evidence before approval.\n"
       "scripts/verify-release-workflow.sh --self-test validates the release workflow publication gate and rejects broken workflow fixtures.\n"
       "scripts/verify-ci-workflow.sh --self-test validates the CI workflow verification gates and rejects broken workflow fixtures.\n"
       "The validator rejects blank fields, pass/fail or yes/no placeholders, unresolved TBD/TODO markers, pending-evidence notes, not-run or skipped markers, and explicit fail/failed/error/denied/rejected/blocked results; every row must contain concrete release evidence.\n"
       "It also rejects malformed release versions, malformed Git commits, malformed Developer ID identities, and malformed notarization request ids.\n"
       "Completed VoiceOver evidence must include the artifact name, signing identity, notarization request id, and stapled artifact validation for the same release artifact.\n"
       "Completed macOS and VoiceOver evidence must agree on release version, Git commit, signing identity, notarization request id, and the LaMusica-<version>-Darwin.dmg artifact name; the artifact name must match the declared release version.\n"
       "Completed evidence must include concrete CI/log references, Xcode version, tester, date, macOS version, hardware, clean-account name where applicable, and release approver fields.\n"
       "For manual workflow_dispatch releases, provide the semantic v* release_tag to publish and completed-evidence input paths that already exist in the checked-out release branch or tag.\n"
       "Verify launch on a clean macOS user account, online and offline, with no Gatekeeper override.\n"
       "scripts/archive-dsyms.sh scripts/verify-symbolication.sh scripts/verify-provenance.sh\n"
       "Every generated .dSYM contains a non-empty Contents/Resources/DWARF/<binary-basename> payload.\n"
       "Verify provenance reports the source checkout HEAD and dirty=false; --allow-dirty is forbidden for signed release artifacts.\n"
       "verify-symbolication the app, plugin scan worker, MCP daemon, and CLI arm64 x86_64 wrong-symbol\n"
       "LaMusica-dSYMs.tar.gz includes dSYM bundles for the app, plugin scan worker, MCP daemon, and CLI\n"
       "scripts/sign-macos.sh scripts/notarize-macos.sh scripts/verify-signature.sh\n"
       "Developer ID Application identity 10-character Team ID ad-hoc or malformed identity strings are rejected\n"
       "signing helper rejects bundles missing LaMusica.app/Contents/MacOS/LaMusica\n"
       "notarization helper is called only with .dmg or .zip artifacts and rejects any release DMG not named LaMusica-<version>-Darwin.dmg\n"
       "xcrun stapler validate LaMusica-<version>-Darwin.dmg\n"
       "scripts/notarize-macos.sh --artifact LaMusica-<version>-Darwin.dmg --key AuthKey.p8 --key-id <KEYID> --issuer <ISSUER_UUID>\n"
       "App Store Connect key id is 10 uppercase alphanumeric characters and issuer is a UUID.\n"
       "scripts/verify-signature.sh --app LaMusica.app --binary lamusica_plugin_scan_worker --binary lamusica_mcpd --binary lamusica_cli --artifact LaMusica-<version>-Darwin.dmg\n"
       "The release signature gate uses the stapled LaMusica-<version>-Darwin.dmg and requires --binary checks for the plugin scan worker, MCP daemon, and CLI.\n"
       "scripts/sbom.sh scripts/sign-checksums.sh SHA256SUMS.sig\n"
       "scripts/sbom.sh --artifact LaMusica-<version>-Darwin.dmg --artifact LaMusica-dSYMs.tar.gz --output build/release-metadata\n"
       "DMG release metadata must include LaMusica-dSYMs.tar.gz and rejects a DMG-only checksum set or a non-LaMusica-<version>-Darwin.dmg release DMG name.\n"
       "Checksum signing uses a Developer ID Application identity with a 10-character Team ID; ad-hoc or malformed identity strings are rejected.\n"
       "Checksum signing rejects SHA256SUMS files with a release DMG row that is not named LaMusica-<version>-Darwin.dmg or that omits LaMusica-dSYMs.tar.gz.\n"
       "lipo -archs codesign --verify --strict --deep spctl --assess xcrun stapler validate\n"
       "VoiceOver checklist and completed evidence attached to the release notes.\n")

  file(WRITE "${self_test_root}/share/doc/LaMusica/performance/realtime-policy.md"
       "Realtime policy without p99/xrun history gates.\n")
  set(incomplete_realtime_policy_package "${self_test_dir}/LaMusica-incomplete-realtime-policy.tar.gz")
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -E tar czf "${incomplete_realtime_policy_package}" "LaMusica-test"
    WORKING_DIRECTORY "${self_test_dir}"
    RESULT_VARIABLE incomplete_realtime_policy_tar_result
    ERROR_VARIABLE incomplete_realtime_policy_tar_error)
  if(NOT incomplete_realtime_policy_tar_result EQUAL 0)
    message(FATAL_ERROR
            "VerifyPackage self-test could not create incomplete-realtime-policy archive: ${incomplete_realtime_policy_tar_error}")
  endif()
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -DPACKAGE=${incomplete_realtime_policy_package} -P "${CMAKE_CURRENT_LIST_FILE}"
    RESULT_VARIABLE incomplete_realtime_policy_verify_result
    OUTPUT_VARIABLE incomplete_realtime_policy_verify_output
    ERROR_VARIABLE incomplete_realtime_policy_verify_error)
  if(incomplete_realtime_policy_verify_result EQUAL 0)
    message(FATAL_ERROR "VerifyPackage self-test failed to reject incomplete realtime policy")
  endif()
  if(NOT incomplete_realtime_policy_verify_error MATCHES "realtime-policy\\.md")
    message(FATAL_ERROR
            "VerifyPackage self-test rejected the incomplete-realtime-policy archive for the wrong reason: ${incomplete_realtime_policy_verify_error}")
  endif()
  file(WRITE "${self_test_root}/share/doc/LaMusica/performance/realtime-policy.md"
       "Audio callback code must avoid Allocation Locks File I/O Logging JSON parsing MCP work.\n"
       "tests/perf/rt_deadline_bench.cpp renders 8, 16, 32, and 64 track sessions block-by-block.\n"
       "It records p99 block time, max block time, buffer deadline, xrun count, RSS, and measured WAV disk bytes.\n"
       "It appends JSONL to tests/perf/rt-history.jsonl keyed by MachineContext.\n"
       "The gate fails on any xrun and when p99 block time is greater than or equal to the buffer period.\n"
       "Update tools/perf/README.md and release notes when thresholds change.\n")

  file(WRITE "${self_test_root}/share/doc/LaMusica/developer/build-and-test.md"
       "Build docs without determinism golden refresh or perf deadline policy.\n")
  set(incomplete_build_doc_package "${self_test_dir}/LaMusica-incomplete-build-doc.tar.gz")
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -E tar czf "${incomplete_build_doc_package}" "LaMusica-test"
    WORKING_DIRECTORY "${self_test_dir}"
    RESULT_VARIABLE incomplete_build_doc_tar_result
    ERROR_VARIABLE incomplete_build_doc_tar_error)
  if(NOT incomplete_build_doc_tar_result EQUAL 0)
    message(FATAL_ERROR
            "VerifyPackage self-test could not create incomplete-build-doc archive: ${incomplete_build_doc_tar_error}")
  endif()
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -DPACKAGE=${incomplete_build_doc_package} -P "${CMAKE_CURRENT_LIST_FILE}"
    RESULT_VARIABLE incomplete_build_doc_verify_result
    OUTPUT_VARIABLE incomplete_build_doc_verify_output
    ERROR_VARIABLE incomplete_build_doc_verify_error)
  if(incomplete_build_doc_verify_result EQUAL 0)
    message(FATAL_ERROR "VerifyPackage self-test failed to reject incomplete build-and-test doc")
  endif()
  if(NOT incomplete_build_doc_verify_error MATCHES "build-and-test\\.md")
    message(FATAL_ERROR
            "VerifyPackage self-test rejected the incomplete-build-doc archive for the wrong reason: ${incomplete_build_doc_verify_error}")
  endif()
  file(WRITE "${self_test_root}/share/doc/LaMusica/developer/build-and-test.md"
       "ctest --preset debug ctest --preset release\n"
       "The release test preset runs onboarding, accessibility, localization, generic CLI, determinism, audio-correctness, plugin-hosting, privacy, and legacy first-track compatibility gates.\n"
       "ctest --preset release -R 'lamusica_daw_diagnostics_consent|lamusica_mcpd_diagnostics_crash_smoke'\n"
       "The determinism test compares independent renders, block-size variants, command-journal replay, the committed float golden, the committed WAV golden, and tests/determinism/golden/render-golden.sha256.\n"
       "build/unix-debug/tests/lamusica_render_determinism_tests build/unix-debug/tests/determinism . --update-golden\n"
       "Review render-golden.float32, render-golden.wav, and render-golden.sha256 together. Do not regenerate goldens from CI.\n"
       "The realtime deadline gate writes tests/perf/rt-history.jsonl and fails on xruns or p99 block time at or above the 1024-frame buffer period.\n"
       "Threshold changes require tools/perf/README.md and release-note context.\n"
       "cmake -P cmake/CheckMarkdown.cmake cmake -P cmake/CheckDependencyLock.cmake\n"
       "cmake -DPACKAGE=LaMusica-0.1.0-Darwin.tar.gz -P cmake/VerifyPackage.cmake\n")

  file(APPEND "${self_test_root}/share/doc/LaMusica/README.md"
       "placeholder security@lamusica.invalid\n")
  set(bad_package "${self_test_dir}/LaMusica-bad.tar.gz")
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -E tar czf "${bad_package}" "LaMusica-test"
    WORKING_DIRECTORY "${self_test_dir}"
    RESULT_VARIABLE bad_tar_result
    ERROR_VARIABLE bad_tar_error)
  if(NOT bad_tar_result EQUAL 0)
    message(FATAL_ERROR "VerifyPackage self-test could not create bad archive: ${bad_tar_error}")
  endif()
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -DPACKAGE=${bad_package} -P "${CMAKE_CURRENT_LIST_FILE}"
    RESULT_VARIABLE bad_verify_result
    OUTPUT_VARIABLE bad_verify_output
    ERROR_VARIABLE bad_verify_error)
  if(bad_verify_result EQUAL 0)
    message(FATAL_ERROR "VerifyPackage self-test failed to reject placeholder .invalid contact")
  endif()
  if(NOT bad_verify_error MATCHES "\\.invalid")
    message(FATAL_ERROR
            "VerifyPackage self-test rejected the bad archive for the wrong reason: ${bad_verify_error}")
  endif()

  file(REMOVE_RECURSE "${self_test_dir}")
  message(STATUS "VerifyPackage self-test passed")
  return()
endif()

if(NOT DEFINED PACKAGE)
  message(FATAL_ERROR "Set PACKAGE to the archive path to verify")
endif()

if(NOT EXISTS "${PACKAGE}")
  message(FATAL_ERROR "Package does not exist: ${PACKAGE}")
endif()

get_filename_component(package_path "${PACKAGE}" ABSOLUTE)

execute_process(
  COMMAND "${CMAKE_COMMAND}" -E tar tf "${package_path}"
  RESULT_VARIABLE list_result
  OUTPUT_VARIABLE package_listing
  ERROR_VARIABLE package_error)

if(NOT list_result EQUAL 0)
  message(FATAL_ERROR "Could not list package ${package_path}: ${package_error}")
endif()

if(package_listing MATCHES "(^|\n)[^\n]*/LaMusica\\.app/Contents/MacOS/LaMusica(\n|$)")
  message(STATUS "Package contains macOS app bundle")
  set(has_app_bundle TRUE)
elseif(package_listing MATCHES "(^|\n)[^\n]*/bin/LaMusica(\n|$)")
  message(STATUS "Package contains non-macOS app executable")
  set(has_app_bundle FALSE)
else()
  message(FATAL_ERROR "Package ${package_path} is missing LaMusica app executable")
endif()

set(required_entries
    "(^|\n)[^\n]*/bin/lamusica_plugin_scan_worker(\n|$)"
    "(^|\n)[^\n]*/bin/lamusica_mcpd(\n|$)"
    "(^|\n)[^\n]*/bin/lamusica_cli(\n|$)"
    "(^|\n)[^\n]*/share/doc/LaMusica/NOTICE(\n|$)"
    "(^|\n)[^\n]*/share/doc/LaMusica/THIRD_PARTY-NOTICES\\.md(\n|$)"
    "(^|\n)[^\n]*/share/doc/LaMusica/privacy-and-diagnostics\\.md(\n|$)"
    "(^|\n)[^\n]*/share/doc/LaMusica/user-manual\\.md(\n|$)"
    "(^|\n)[^\n]*/share/doc/LaMusica/architecture/architecture-baseline\\.md(\n|$)"
    "(^|\n)[^\n]*/share/doc/LaMusica/developer/build-and-test\\.md(\n|$)"
    "(^|\n)[^\n]*/share/doc/LaMusica/developer/command-api\\.md(\n|$)"
    "(^|\n)[^\n]*/share/doc/LaMusica/developer/mcp-tools\\.md(\n|$)"
    "(^|\n)[^\n]*/share/doc/LaMusica/developer/project-format\\.md(\n|$)"
    "(^|\n)[^\n]*/share/doc/LaMusica/release/release-checklist\\.md(\n|$)"
    "(^|\n)[^\n]*/share/doc/LaMusica/release/accessibility-voiceover-checklist\\.md(\n|$)"
    "(^|\n)[^\n]*/share/doc/LaMusica/release/accessibility-voiceover-evidence-template\\.md(\n|$)"
    "(^|\n)[^\n]*/share/doc/LaMusica/release/macos-release-evidence-template\\.md(\n|$)"
    "(^|\n)[^\n]*/share/doc/LaMusica/release/macos-distribution\\.md(\n|$)"
    "(^|\n)[^\n]*/share/doc/LaMusica/release/security-disclosure\\.md(\n|$)"
    "(^|\n)[^\n]*/share/doc/LaMusica/release/versioning\\.md(\n|$)"
    "(^|\n)[^\n]*/share/doc/LaMusica/README\\.md(\n|$)"
    "(^|\n)[^\n]*/share/doc/LaMusica/CHANGELOG\\.md(\n|$)"
    "(^|\n)[^\n]*/share/doc/LaMusica/SECURITY\\.md(\n|$)"
    "(^|\n)[^\n]*/share/doc/LaMusica/CONTRIBUTING\\.md(\n|$)"
    "(^|\n)[^\n]*/share/doc/LaMusica/developer/dependencies\\.md(\n|$)"
    "(^|\n)[^\n]*/share/doc/LaMusica/developer/localization\\.md(\n|$)"
    "(^|\n)[^\n]*/share/doc/LaMusica/performance/realtime-policy\\.md(\n|$)"
    "(^|\n)[^\n]*/share/doc/LaMusica/schemas/cli-output-v1\\.schema\\.json(\n|$)"
    "(^|\n)[^\n]*/share/doc/LaMusica/schemas/project-v1\\.schema\\.json(\n|$)"
    "(^|\n)[^\n]*/share/doc/LaMusica/schemas/project-v3\\.schema\\.json(\n|$)"
    "(^|\n)[^\n]*/share/lamusica/examples/empty\\.Project\\.lamusica/project\\.json(\n|$)"
    "(^|\n)[^\n]*/share/lamusica/examples/generated-tone\\.Project\\.lamusica/project\\.json(\n|$)"
    "(^|\n)[^\n]*/share/lamusica/tutorials/first-song\\.Project\\.lamusica/project\\.json(\n|$)"
    "(^|\n)[^\n]*/share/lamusica/i18n/en\\.txt(\n|$)"
    "(^|\n)[^\n]*/share/lamusica/i18n/es\\.txt(\n|$)"
    "(^|\n)[^\n]*/share/lamusica/i18n/fr\\.txt(\n|$)"
)

foreach(required_entry IN LISTS required_entries)
  if(NOT package_listing MATCHES "${required_entry}")
    message(FATAL_ERROR "Package ${package_path} is missing required entry matching ${required_entry}")
  endif()
endforeach()

foreach(forbidden_entry IN ITEMS "(^|\n)[^\n]*/include/JUCE-[^\n]*"
                                 "(^|\n)[^\n]*/lib/cmake/JUCE-[^\n]*"
                                 "(^|\n)[^\n]*/bin/JUCE-[^\n]*")
  if(package_listing MATCHES "${forbidden_entry}")
    message(FATAL_ERROR "Package ${package_path} includes JUCE development install payload")
  endif()
endforeach()

set(extract_dir "${CMAKE_CURRENT_BINARY_DIR}/verify-package")
file(REMOVE_RECURSE "${extract_dir}")
file(MAKE_DIRECTORY "${extract_dir}")
execute_process(
  COMMAND "${CMAKE_COMMAND}" -E tar xf "${package_path}"
  WORKING_DIRECTORY "${extract_dir}"
  RESULT_VARIABLE extract_result
  ERROR_VARIABLE extract_error)
if(NOT extract_result EQUAL 0)
  message(FATAL_ERROR "Could not extract package ${package_path}: ${extract_error}")
endif()

file(GLOB_RECURSE extracted_files LIST_DIRECTORIES false "${extract_dir}/*")
foreach(extracted_file IN LISTS extracted_files)
  if(extracted_file MATCHES "\\.(md|txt|plist|json|cmake|hpp|cpp|h|in)$" OR extracted_file MATCHES
                                                                        "/NOTICE$")
    file(READ "${extracted_file}" extracted_text)
    if(extracted_text MATCHES "\\.invalid")
      message(FATAL_ERROR "Package contains placeholder .invalid contact in ${extracted_file}")
    endif()
  endif()
endforeach()

file(GLOB_RECURSE notice_files LIST_DIRECTORIES false "${extract_dir}/*/share/doc/LaMusica/NOTICE")
list(LENGTH notice_files notice_count)
if(notice_count EQUAL 0)
  message(FATAL_ERROR "Package is missing NOTICE after extraction")
endif()
list(GET notice_files 0 notice_file)
file(READ "${notice_file}" notice_text)
foreach(required_notice_text IN ITEMS "AGPL-3.0-or-later" "Corresponding source")
  if(NOT notice_text MATCHES "${required_notice_text}")
    message(FATAL_ERROR "NOTICE is missing required source-availability text: ${required_notice_text}")
  endif()
endforeach()

file(GLOB_RECURSE third_party_notice_files LIST_DIRECTORIES false
     "${extract_dir}/*/share/doc/LaMusica/THIRD_PARTY-NOTICES.md")
list(LENGTH third_party_notice_files third_party_notice_count)
if(third_party_notice_count EQUAL 0)
  message(FATAL_ERROR "Package is missing THIRD_PARTY-NOTICES.md after extraction")
endif()
list(GET third_party_notice_files 0 third_party_notice_file)
file(READ "${third_party_notice_file}" third_party_notice_text)
foreach(required_third_party_notice_text
        IN
        ITEMS
        "Third-Party Notices"
        "JUCE"
        "8\\.0\\.13"
        "7c9d3783b127263d72bb65fe0a7e2dc8a02a7ac2"
        "https://github.com/juce-framework/JUCE"
        "GPL/commercial dual license"
        "open-source GPL-compatible path"
        "AGPL-3.0-or-later"
        "No third-party plugin binaries are bundled"
        "in-repository mock components"
        "redistributable plugin"
        "license")
  if(NOT third_party_notice_text MATCHES "${required_third_party_notice_text}")
    message(FATAL_ERROR
            "THIRD_PARTY-NOTICES.md is missing required third-party notice text: ${required_third_party_notice_text}")
  endif()
endforeach()

file(GLOB_RECURSE privacy_files LIST_DIRECTORIES false
     "${extract_dir}/*/share/doc/LaMusica/privacy-and-diagnostics.md")
list(LENGTH privacy_files privacy_count)
if(privacy_count EQUAL 0)
  message(FATAL_ERROR "Package is missing privacy-and-diagnostics.md after extraction")
endif()
list(GET privacy_files 0 privacy_file)
file(READ "${privacy_file}" privacy_text)
foreach(required_privacy_text
        IN
        ITEMS
        "does not send crash reports or telemetry unless the user grants diagnostics consent"
        "Usage telemetry is a separate opt-in from crash reports"
        "currently emits no usage telemetry events"
        "signal handler writes"
        "signal, pid"
        "deferred backtrace marker"
        "async-signal-safe operations"
        "Scrubbing and any upload preparation happen later"
        "application name"
        "app version and Git commit"
        "operating system version"
        "crash signal"
        "stack frames or backtrace"
        "must not include project contents, audio, MIDI, usernames"
        "home-directory paths"
        "absolute file paths"
        "project bundle names"
        "<path>"
        "<project>"
        "https://diagnostics.lamusica.dev/v1/crash"
        "LAMUSICA_DIAGNOSTICS_ENDPOINT"
        "non-empty explicit HTTPS URL"
        "missing or non-HTTPS override values block upload"
        "within 30 days"
        "setting diagnostics consent to Declined"
        "turning diagnostics sharing off in Preferences > Privacy")
  if(NOT privacy_text MATCHES "${required_privacy_text}")
    message(FATAL_ERROR "privacy-and-diagnostics.md is missing required text: ${required_privacy_text}")
  endif()
endforeach()

foreach(link_source IN ITEMS "README.md" "user-manual.md")
  file(GLOB_RECURSE link_files LIST_DIRECTORIES false
       "${extract_dir}/*/share/doc/LaMusica/${link_source}")
  list(LENGTH link_files link_file_count)
  if(link_file_count EQUAL 0)
    message(FATAL_ERROR "Package is missing ${link_source} after extraction")
  endif()
  list(GET link_files 0 link_file)
  file(READ "${link_file}" link_text)
  if(NOT link_text MATCHES "privacy-and-diagnostics\\.md")
    message(FATAL_ERROR "${link_source} does not link to privacy-and-diagnostics.md")
  endif()
endforeach()

file(GLOB_RECURSE build_doc_files LIST_DIRECTORIES false
     "${extract_dir}/*/share/doc/LaMusica/developer/build-and-test.md")
list(LENGTH build_doc_files build_doc_count)
if(build_doc_count EQUAL 0)
  message(FATAL_ERROR "Package is missing developer/build-and-test.md after extraction")
endif()
list(GET build_doc_files 0 build_doc_file)
file(READ "${build_doc_file}" build_doc_text)
foreach(required_build_doc_text
        IN
        ITEMS
        "ctest --preset debug"
        "ctest --preset release"
        "onboarding, accessibility, localization, generic CLI, determinism"
        "audio-correctness, plugin-hosting, privacy"
        "lamusica_mcpd_diagnostics_crash_smoke"
        "independent renders"
        "block-size variants"
        "command-journal replay"
        "committed float golden"
        "committed WAV golden"
        "tests/determinism/golden/render-golden\\.sha256"
        "--update-golden"
        "render-golden\\.float32"
        "render-golden\\.wav"
        "Do not regenerate goldens from CI"
        "tests/perf/rt-history\\.jsonl"
        "xruns"
        "p99 block time"
        "1024-frame buffer period"
        "tools/perf/README\\.md"
        "prepare_arbitrary_id_project"
        "query_arbitrary_id"
        "arbitrary_id_edit"
        "scripts/verify-ci-workflow\\.sh --self-test"
        "cmake -P cmake/CheckMarkdown\\.cmake"
        "cmake -P cmake/CheckDependencyLock\\.cmake"
        "cmake -DPACKAGE=.*cmake/VerifyPackage\\.cmake")
  if(NOT build_doc_text MATCHES "${required_build_doc_text}")
    message(FATAL_ERROR
            "developer/build-and-test.md is missing required P26/P27 workflow text: ${required_build_doc_text}")
  endif()
endforeach()

file(GLOB_RECURSE dependency_doc_files LIST_DIRECTORIES false
     "${extract_dir}/*/share/doc/LaMusica/developer/dependencies.md")
list(LENGTH dependency_doc_files dependency_doc_count)
if(dependency_doc_count EQUAL 0)
  message(FATAL_ERROR "Package is missing developer/dependencies.md after extraction")
endif()
list(GET dependency_doc_files 0 dependency_doc_file)
file(READ "${dependency_doc_file}" dependency_doc_text)
set(required_fetch_content "Fetch" "Content")
list(JOIN required_fetch_content "" required_fetch_content)
set(required_vc_pkg "vc" "pkg")
list(JOIN required_vc_pkg "" required_vc_pkg)
set(required_co_nan "Co" "nan")
list(JOIN required_co_nan "" required_co_nan)
set(required_external_project_add "ExternalProject" "_Add")
list(JOIN required_external_project_add "" required_external_project_add)
foreach(required_dependency_doc_text
        IN
        ITEMS
        "does not download third-party source"
        "JUCE"
        "8\\.0\\.13"
        "7c9d3783b127263d72bb65fe0a7e2dc8a02a7ac2"
        "content manifest checksum"
        "e2ee824cf139a72e3720e996c1cdc70e9ff9dac9653c7f74ccf7d40cf1e3d1c4"
        "LAMUSICA_JUCE_PATH"
        "not vendored"
        "${required_fetch_content}"
        "CPM"
        "${required_vc_pkg}"
        "${required_co_nan}"
        "${required_external_project_add}"
        "recurse over every source `?CMakeLists\\.txt`?"
        "\\*\\.cmake"
        "outside generated build/package directories"
        "cmake -DLAMUSICA_JUCE_PATH=.*cmake/CheckDependencyLock\\.cmake"
        "LAMUSICA_DEPENDENCY_LOCK_SELF_TEST"
        "rejects nested unreviewed downloader integrations")
  if(NOT dependency_doc_text MATCHES "${required_dependency_doc_text}")
    message(FATAL_ERROR
            "developer/dependencies.md is missing required supply-chain lock text: ${required_dependency_doc_text}")
  endif()
endforeach()

file(GLOB_RECURSE release_checklist_files LIST_DIRECTORIES false
     "${extract_dir}/*/share/doc/LaMusica/release/release-checklist.md")
list(LENGTH release_checklist_files release_checklist_count)
if(release_checklist_count EQUAL 0)
  message(FATAL_ERROR "Package is missing release-checklist.md after extraction")
endif()
list(GET release_checklist_files 0 release_checklist_file)
file(READ "${release_checklist_file}" release_checklist_text)
foreach(required_release_checklist_text
        IN
        ITEMS
        "macos-release-evidence-template.md"
        "accessibility-voiceover-evidence-template.md"
        "GitHub Release"
        "macos-14"
        "Xcode"
        "15.4"
        "blank evidence templates"
        "validated completed evidence files"
        "completed-macos-release-evidence.md"
        "completed-accessibility-voiceover-evidence.md"
        "fail_on_unmatched_files: true"
        "if-no-files-found: error"
        "partial release"
        "lamusica_mcpd_diagnostics_crash_smoke"
        "LaMusica-release-candidate"
        "LaMusica-<version>-Darwin.dmg"
        "xcrun stapler validate LaMusica-<version>-Darwin.dmg"
        "scripts/notarize-macos.sh --artifact LaMusica-<version>-Darwin.dmg"
        "scripts/verify-signature.sh --app LaMusica.app --binary lamusica_plugin_scan_worker --binary lamusica_mcpd --binary lamusica_cli --artifact LaMusica-<version>-Darwin.dmg"
        "scripts/sbom.sh --artifact LaMusica-<version>-Darwin.dmg"
        "Gatekeeper"
        "TCC"
        "symbolication"
        "VoiceOver"
        "scripts/verify-release-evidence.sh"
        "scripts/verify-release-workflow.sh --self-test"
        "scripts/verify-ci-workflow.sh --self-test"
        "rejects blank fields"
        "pass/fail or yes/no placeholders"
        "unresolved TBD/TODO"
        "pending-evidence"
        "explicit fail/failed/error/denied/rejected/blocked results"
        "malformed release versions"
        "malformed Git commits"
        "malformed Developer ID"
        "identities"
        "malformed notarization request ids"
        "artifact name"
        "signing identity"
        "stapled artifact validation"
        "must agree"
        "LaMusica-<version>-Darwin.dmg"
        "artifact name must match the declared release version"
        "CI/log references"
        "tester"
        "macOS version"
        "hardware"
        "clean-account name"
        "release approver"
        "workflow_dispatch"
        "release_tag"
        "same release ref"
        "macos-14"
        "Xcode"
        "15.4"
        "Vulnerability scan"
        "fail-build: true"
        "severity-cutoff: critical"
        "Publish GitHub release"
        "semantic"
        "completed-evidence input paths"
        "checked-out release branch or tag"
        "concrete release evidence"
        "clean macOS user account"
        "online and offline"
        "no Gatekeeper override"
        "scripts/archive-dsyms.sh"
        "Contents/Resources/DWARF/<binary-basename>"
        "scripts/verify-symbolication.sh"
        "app, plugin scan worker, MCP daemon, and CLI"
        "LaMusica-dSYMs.tar.gz"
        "dSYM bundles for the app, plugin scan worker, MCP daemon, and CLI"
        "arm64"
        "x86_64"
        "scripts/verify-provenance.sh"
        "dirty=false"
        "--allow-dirty"
        "scripts/sign-macos.sh"
        "signing helper rejects bundles missing"
        "LaMusica.app/Contents/MacOS/LaMusica"
        "Developer ID Application identity"
        "10-character Team ID"
        "ad-hoc or malformed identity strings"
        "scripts/notarize-macos.sh"
        ".dmg or .zip"
        "App Store Connect key id"
        "issuer is a UUID"
        "scripts/verify-signature.sh"
        "stapled"
        "requires --binary checks"
        "--binary lamusica_plugin_scan_worker"
        "--binary lamusica_mcpd"
        "--binary lamusica_cli"
        "scripts/sbom.sh"
        "DMG-only checksum set"
        "LaMusica-<version>-Darwin.dmg"
        "release DMG name"
        "scripts/sign-checksums.sh"
        "Checksum signing"
        "SHA256SUMS.sig"
        "Checksum signing rejects"
        "SHA256SUMS"
        "release DMG row"
        "omits"
        "LaMusica-dSYMs.tar.gz"
        "lipo -archs"
        "codesign --verify --strict --deep"
        "spctl --assess"
        "xcrun stapler validate"
        "completed evidence"
        "release notes")
  if(NOT release_checklist_text MATCHES "${required_release_checklist_text}")
    message(FATAL_ERROR
            "release-checklist.md is missing required release evidence text: ${required_release_checklist_text}")
  endif()
endforeach()

file(GLOB_RECURSE voiceover_checklist_files LIST_DIRECTORIES false
     "${extract_dir}/*/share/doc/LaMusica/release/accessibility-voiceover-checklist.md")
list(LENGTH voiceover_checklist_files voiceover_checklist_count)
if(voiceover_checklist_count EQUAL 0)
  message(FATAL_ERROR "Package is missing accessibility-voiceover-checklist.md after extraction")
endif()
list(GET voiceover_checklist_files 0 voiceover_checklist_file)
file(READ "${voiceover_checklist_file}" voiceover_checklist_text)
foreach(required_voiceover_checklist_text
        IN
        ITEMS
        "Accessibility VoiceOver Checklist"
        "lamusica_daw_accessibility_audit"
        "VoiceOver"
        "Full Keyboard Access"
        "Transport play/stop"
        "Record/arm/monitor controls"
        "Timeline clip"
        "Time ruler or playhead"
        "Mixer fader"
        "Pan control"
        "Meter"
        "Piano-roll note"
        "Drum pad or step cell"
        "Browser tree"
        "Inspector fields"
        "Plugin chooser/control"
        "Export dialog"
        "Welcome/templates"
        "Guided tour"
        "Start and stop transport"
        "Select a timeline clip"
        "Change a mixer fader value"
        "Open the plugin chooser"
        "Open the export path"
        "choose a template"
        "Restart and skip the guided tour"
        "Reduce Motion"
        "Increase Contrast"
        "Failure Policy"
        "keyboard with VoiceOver enabled")
  if(NOT voiceover_checklist_text MATCHES "${required_voiceover_checklist_text}")
    message(FATAL_ERROR
            "accessibility-voiceover-checklist.md is missing required manual gate text: ${required_voiceover_checklist_text}")
  endif()
endforeach()

file(GLOB_RECURSE macos_distribution_files LIST_DIRECTORIES false
     "${extract_dir}/*/share/doc/LaMusica/release/macos-distribution.md")
list(LENGTH macos_distribution_files macos_distribution_count)
if(macos_distribution_count EQUAL 0)
  message(FATAL_ERROR "Package is missing macos-distribution.md after extraction")
endif()
list(GET macos_distribution_files 0 macos_distribution_file)
file(READ "${macos_distribution_file}" macos_distribution_text)
foreach(required_macos_distribution_text
        IN
        ITEMS
        "macos-release-evidence-template.md"
        "clean macOS user account"
        "offline-launch"
        "microphone TCC"
        "dSYM"
        "published asset evidence"
        "codesign --force --options runtime --timestamp"
        "--entitlements"
        "apps/daw/lamusica.entitlements"
        "scripts/sign-macos.sh"
        "Missing release binaries"
        "LaMusica.app/Contents/MacOS/LaMusica"
        "Developer ID Application identity"
        "10-character Team ID"
        "ad-hoc"
        "malformed identity strings"
        "codesign --verify --strict --deep"
        "spctl --assess"
        "xcrun notarytool submit"
        "xcrun stapler staple"
        "xcrun stapler validate"
        "scripts/notarize-macos.sh"
        ".dmg or .zip"
        "App Store Connect key id"
        "issuer UUID"
        "scripts/verify-signature.sh"
        "stapled release .dmg"
        "helper --binary checks"
        "--binary"
        "lamusica_plugin_scan_worker"
        "lamusica_mcpd"
        "lamusica_cli"
        "scripts/verify-provenance.sh"
        "dirty=false"
        "--allow-dirty"
        "lamusica_mcpd_diagnostics_crash_smoke"
        "malformed release versions"
        "malformed Git commits"
        "malformed Developer ID identities"
        "notarization request ids"
        "scripts/archive-dsyms.sh"
        "binary basename"
        "scripts/verify-symbolication.sh"
        "wrong-symbol"
        "app, plugin scan worker, MCP daemon, and CLI"
        "arm64"
        "x86_64"
        "scripts/sbom.sh"
        "required dSYM archive row"
        "LaMusica-<version>-Darwin.dmg"
        "DMG names"
        "DMG-only checksum set"
        "scripts/sign-checksums.sh"
        "Checksum signing"
        "SHA256SUMS"
        "release DMG rows not named"
        "release DMG checksum sets"
        "without"
        "LaMusica-dSYMs.tar.gz"
        "validate completed macOS and VoiceOver evidence before publication"
        "completed evidence files"
        "fail_on_unmatched_files: true"
        "LaMusica-release-candidate"
        "if-no-files-found: error"
        "partial release"
        "Gatekeeper"
        "TCC"
        "symbolication"
        "VoiceOver"
        "workflow_dispatch"
        "release_tag"
        "same release ref"
        "macos-14"
        "Xcode"
        "15.4"
        "Vulnerability scan"
        "fail-build: true"
        "severity-cutoff: critical"
        "Publish GitHub release"
        "semantic"
        "completed-evidence input paths"
        "checked-out release branch or tag"
        "prepare_arbitrary_id_project"
        "query_arbitrary_id"
        "arbitrary_id_edit"
        "Open `LaMusica-<version>-Darwin.dmg`"
        "xcrun notarytool submit LaMusica-<version>-Darwin.dmg"
        "xcrun stapler staple LaMusica-<version>-Darwin.dmg"
        "xcrun stapler validate LaMusica-<version>-Darwin.dmg"
        "scripts/notarize-macos.sh"
        "--artifact LaMusica-<version>-Darwin.dmg"
        "scripts/verify-signature.sh"
        "must agree"
        "LaMusica-<version>-Darwin.dmg"
        "artifact name must match the declared release version"
        "CI/log references"
        "tester"
        "macOS version"
        "hardware"
        "clean-account name"
        "release approver"
        "LAMUSICA_DIAGNOSTICS_ENDPOINT"
        "non-empty explicit HTTPS URL"
        "missing or non-HTTPS override values block upload"
        "InfoPlist.strings"
        "CFBundleLocalizations"
        "localized microphone and Apple Events usage strings"
        "vulnerability scan")
  if(NOT macos_distribution_text MATCHES "${required_macos_distribution_text}")
    message(FATAL_ERROR
            "macos-distribution.md is missing required release evidence text: ${required_macos_distribution_text}")
  endif()
endforeach()

file(GLOB_RECURSE macos_evidence_files LIST_DIRECTORIES false
     "${extract_dir}/*/share/doc/LaMusica/release/macos-release-evidence-template.md")
list(LENGTH macos_evidence_files macos_evidence_count)
if(macos_evidence_count EQUAL 0)
  message(FATAL_ERROR "Package is missing macos-release-evidence-template.md after extraction")
endif()
list(GET macos_evidence_files 0 macos_evidence_file)
file(READ "${macos_evidence_file}" macos_evidence_text)
foreach(required_macos_evidence_text
        IN
        ITEMS
        "lipo -archs"
        "LaMusica.app/Contents/MacOS/LaMusica"
        "lamusica_plugin_scan_worker"
        "lamusica_mcpd"
        "lamusica_cli"
        "codesign --verify --strict --deep"
        "codesign -d --entitlements"
        "spctl --assess"
        "xcrun notarytool submit --wait"
        "xcrun stapler validate"
        "cmake -DPACKAGE="
        "Darwin.tar.gz"
        "Online launch from `/Applications`"
        "Offline launch from `/Applications`"
        "No Gatekeeper override required"
        "First record attempt triggered microphone TCC prompt"
        "Bundled CLI tools ran from package"
        "dSYMs archived"
        "atos"
        "llvm-symbolizer"
        "DAW induced crash produced local report"
        "lamusica_mcpd induced crash produced local report"
        "Diagnostics upload stayed disabled without consent"
        "DMG"
        "dSYMs archive"
        "SBOM"
        "SHA256SUMS"
        "SHA256SUMS.sig"
        "Blocking failures"
        "Non-blocking observations"
        "Follow-up issue links"
        "Release approved by")
  if(NOT macos_evidence_text MATCHES "${required_macos_evidence_text}")
    message(FATAL_ERROR
            "macos-release-evidence-template.md is missing required release gate text: ${required_macos_evidence_text}")
  endif()
endforeach()

file(GLOB_RECURSE voiceover_evidence_files LIST_DIRECTORIES false
     "${extract_dir}/*/share/doc/LaMusica/release/accessibility-voiceover-evidence-template.md")
list(LENGTH voiceover_evidence_files voiceover_evidence_count)
if(voiceover_evidence_count EQUAL 0)
  message(FATAL_ERROR
          "Package is missing accessibility-voiceover-evidence-template.md after extraction")
endif()
list(GET voiceover_evidence_files 0 voiceover_evidence_file)
file(READ "${voiceover_evidence_file}" voiceover_evidence_text)
foreach(required_voiceover_evidence_text
        IN
        ITEMS
        "Artifact name"
        "Signing identity"
        "Notarization request id"
        "Stapled artifact validated"
        "VoiceOver enabled"
        "Full Keyboard Access enabled"
        "Reduce Motion tested"
        "Increase Contrast tested"
        "lamusica_daw_accessibility_audit"
        "Result"
        "Transport play/stop"
        "Record/arm/monitor controls"
        "Mixer fader"
        "Pan control"
        "Meter"
        "Timeline clip"
        "Time ruler or playhead"
        "Piano-roll note"
        "Drum pad or step cell"
        "Browser tree"
        "Inspector fields"
        "Plugin chooser/control"
        "Export dialog"
        "Welcome/templates"
        "Guided tour"
        "Start and stop transport"
        "Arm a track and toggle monitoring"
        "Select and edit a timeline clip"
        "Change a mixer fader value"
        "Inspect a plugin control"
        "Cancel and confirm export"
        "Choose an onboarding template"
        "Restart and skip guided tour"
        "Completed without mouse"
        "VoiceOver evidence"
        "Blocking failures"
        "Non-blocking observations"
        "Follow-up issue links"
        "Release approved by")
  if(NOT voiceover_evidence_text MATCHES "${required_voiceover_evidence_text}")
    message(FATAL_ERROR
            "accessibility-voiceover-evidence-template.md is missing required release gate text: ${required_voiceover_evidence_text}")
  endif()
endforeach()

file(GLOB_RECURSE user_manual_files LIST_DIRECTORIES false
     "${extract_dir}/*/share/doc/LaMusica/user-manual.md")
list(LENGTH user_manual_files user_manual_count)
if(user_manual_count EQUAL 0)
  message(FATAL_ERROR "Package is missing user-manual.md after extraction")
endif()
list(GET user_manual_files 0 user_manual_file)
file(READ "${user_manual_file}" user_manual_text)
foreach(required_user_manual_text
        IN
        ITEMS
        "On first launch, LaMusica opens a welcome surface"
        "New Project opens the welcome/template flow"
        "Empty, Basic Multitrack, Drum \\+ Synth, and Podcast / Voice")
  if(NOT user_manual_text MATCHES "${required_user_manual_text}")
    message(FATAL_ERROR "user-manual.md is missing onboarding text: ${required_user_manual_text}")
  endif()
endforeach()

file(GLOB_RECURSE localization_doc_files LIST_DIRECTORIES false
     "${extract_dir}/*/share/doc/LaMusica/developer/localization.md")
list(LENGTH localization_doc_files localization_doc_count)
if(localization_doc_count EQUAL 0)
  message(FATAL_ERROR "Package is missing developer/localization.md after extraction")
endif()
list(GET localization_doc_files 0 localization_doc_file)
file(READ "${localization_doc_file}" localization_doc_text)
foreach(required_localization_doc_text
        IN
        ITEMS
        "apps/daw/resources/i18n/<locale>\\.txt"
        "apps/daw/src/i18n/StringTables\\.cpp"
        "CFBundleLocalizations"
        "apps/daw/resources/macos/<locale>\\.lproj/InfoPlist\\.strings"
        "LAMUSICA_DAW_LOCALIZED_BUNDLE_RESOURCES"
        "lamusica_i18n_tests"
        "ApplicationPreferences::preferredLocale"
        "juce::SystemStats::getDisplayLanguage"
        "BCP-47-style"
        "UI preference only"
        "Project files, MCP payloads, command ids, and logs stay stable English/C-locale data"
        "NumberFormat"
        "std::locale::global"
        "project serialization"
        "Spanish values fail"
        "coverage: stub")
  if(NOT localization_doc_text MATCHES "${required_localization_doc_text}")
    message(FATAL_ERROR
            "developer/localization.md is missing required workflow text: ${required_localization_doc_text}")
  endif()
endforeach()

file(GLOB_RECURSE project_format_doc_files LIST_DIRECTORIES false
     "${extract_dir}/*/share/doc/LaMusica/developer/project-format.md")
list(LENGTH project_format_doc_files project_format_doc_count)
if(project_format_doc_count EQUAL 0)
  message(FATAL_ERROR "Package is missing developer/project-format.md after extraction")
endif()
list(GET project_format_doc_files 0 project_format_doc_file)
file(READ "${project_format_doc_file}" project_format_doc_text)
foreach(required_project_format_doc_text
        IN
        ITEMS
        "project\\.json"
        "schema version `?3`?"
        "docs/schemas/project-v3\\.schema\\.json"
        "projectSampleRate"
        "48000\\.0"
        "schema `?1`?.*schema `?2`?"
        "schema `?2`?.*schema `?3`?"
        "takeLanes"
        "comps"
        "sourceSampleRate"
        "relative"
        "parent-directory traversal")
  if(NOT project_format_doc_text MATCHES "${required_project_format_doc_text}")
    message(FATAL_ERROR
            "developer/project-format.md is missing required schema/migration text: ${required_project_format_doc_text}")
  endif()
endforeach()

file(GLOB_RECURSE realtime_policy_files LIST_DIRECTORIES false
     "${extract_dir}/*/share/doc/LaMusica/performance/realtime-policy.md")
list(LENGTH realtime_policy_files realtime_policy_count)
if(realtime_policy_count EQUAL 0)
  message(FATAL_ERROR "Package is missing performance/realtime-policy.md after extraction")
endif()
list(GET realtime_policy_files 0 realtime_policy_file)
file(READ "${realtime_policy_file}" realtime_policy_text)
foreach(required_realtime_policy_text
        IN
        ITEMS
        "Audio callback code must avoid"
        "Allocation"
        "Locks"
        "File I/O"
        "JSON parsing"
        "MCP work"
        "tests/perf/rt_deadline_bench\\.cpp"
        "8, 16, 32"
        "64 track"
        "p99 block time"
        "buffer deadline"
        "xrun count"
        "RSS"
        "measured WAV disk bytes"
        "tests/perf/rt-history\\.jsonl"
        "MachineContext"
        "fails on any xrun"
        "greater than or equal to the buffer period"
        "tools/perf/README\\.md")
  if(NOT realtime_policy_text MATCHES "${required_realtime_policy_text}")
    message(FATAL_ERROR
            "performance/realtime-policy.md is missing required P26 deadline policy text: ${required_realtime_policy_text}")
  endif()
endforeach()

file(GLOB_RECURSE versioning_files LIST_DIRECTORIES false
     "${extract_dir}/*/share/doc/LaMusica/release/versioning.md")
list(LENGTH versioning_files versioning_count)
if(versioning_count EQUAL 0)
  message(FATAL_ERROR "Package is missing release/versioning.md after extraction")
endif()
list(GET versioning_files 0 versioning_file)
file(READ "${versioning_file}" versioning_text)
foreach(required_versioning_text
        IN
        ITEMS
        "semantic versioning"
        "MAJOR"
        "MINOR"
        "PATCH"
        "Current project manifest schema: `?3`?"
        "Schema `?1`?.*schema `?2`?.*projectSampleRate = 48000\\.0"
        "Schema `?2`?.*schema `?3`?"
        "takeLanes"
        "comps"
        "migrateProjectManifest"
        "unit coverage")
  if(NOT versioning_text MATCHES "${required_versioning_text}")
    message(FATAL_ERROR
            "release/versioning.md is missing required schema migration text: ${required_versioning_text}")
  endif()
endforeach()

foreach(project_schema_source IN ITEMS "schemas/project-v3.schema.json" "schemas/project-v1.schema.json")
  file(GLOB_RECURSE project_schema_files LIST_DIRECTORIES false
       "${extract_dir}/*/share/doc/LaMusica/${project_schema_source}")
  list(LENGTH project_schema_files project_schema_file_count)
  if(project_schema_file_count EQUAL 0)
    message(FATAL_ERROR "Package is missing ${project_schema_source} after extraction")
  endif()
  list(GET project_schema_files 0 project_schema_file)
  file(READ "${project_schema_file}" project_schema_text)
  foreach(required_project_schema_text
          IN
          ITEMS
          "dev\\.lamusica\\.schemas\\.project-v3"
          "\"additionalProperties\": false"
          "\"schemaVersion\": \\{ \"const\": 3 \\}"
          "\"projectSampleRate\""
          "\"sourceSampleRate\""
          "\"exclusiveMinimum\": 0"
          "\"takeLanes\""
          "\"comps\"")
    if(NOT project_schema_text MATCHES "${required_project_schema_text}")
      message(FATAL_ERROR
              "${project_schema_source} is missing required schema text: ${required_project_schema_text}")
    endif()
  endforeach()
endforeach()

file(GLOB_RECURSE cli_schema_files LIST_DIRECTORIES false
     "${extract_dir}/*/share/doc/LaMusica/schemas/cli-output-v1.schema.json")
list(LENGTH cli_schema_files cli_schema_file_count)
if(cli_schema_file_count EQUAL 0)
  message(FATAL_ERROR "Package is missing cli-output-v1.schema.json after extraction")
endif()
list(GET cli_schema_files 0 cli_schema_file)
file(READ "${cli_schema_file}" cli_schema_text)
foreach(required_cli_schema_text
        IN
        ITEMS
        "https://lamusica.dev/schemas/cli-output-v1.schema.json"
        "\"schemaVersion\": \\{ \"const\": 1 \\}"
        "\"project\""
        "\"tracks\""
        "\"clips\""
        "\"plugins\""
        "\"automation\""
        "\"preview\""
        "\"mutated\""
        "\"confirmationToken\""
        "\"render\""
        "\"format\""
        "\"bitDepth\""
        "\"startSample\""
        "\"frames\""
        "\"stems\""
        "\"postDitherPeak\"")
  if(NOT cli_schema_text MATCHES "${required_cli_schema_text}")
    message(FATAL_ERROR
            "cli-output-v1.schema.json is missing required schema text: ${required_cli_schema_text}")
  endif()
endforeach()

foreach(security_source IN ITEMS "SECURITY.md" "release/security-disclosure.md")
  file(GLOB_RECURSE security_files LIST_DIRECTORIES false
       "${extract_dir}/*/share/doc/LaMusica/${security_source}")
  list(LENGTH security_files security_file_count)
  if(security_file_count EQUAL 0)
    message(FATAL_ERROR "Package is missing ${security_source} after extraction")
  endif()
  list(GET security_files 0 security_file)
  file(READ "${security_file}" security_text)
  foreach(required_security_text
          IN
          ITEMS "security@lamusica.dev" "3 business days"
                "fix and release plan" "PGP" "private")
    if(NOT security_text MATCHES "${required_security_text}")
      message(FATAL_ERROR "${security_source} is missing required security-disclosure text: ${required_security_text}")
    endif()
  endforeach()
endforeach()

foreach(locale IN ITEMS en es fr)
  file(GLOB_RECURSE locale_files LIST_DIRECTORIES false
       "${extract_dir}/*/share/lamusica/i18n/${locale}.txt")
  list(LENGTH locale_files locale_file_count)
  if(locale_file_count EQUAL 0)
    message(FATAL_ERROR "Package is missing i18n table after extraction: ${locale}")
  endif()
  list(GET locale_files 0 locale_file)
  file(READ "${locale_file}" locale_text)
  foreach(required_locale_text
          IN
          ITEMS
          "language: ${locale}"
          "\"Record\""
          "\"Master pan\""
          "\"Export dialog\""
          "\"onboarding.template.empty.name\""
          "\"onboarding.template.basicMultitrack.name\""
          "\"onboarding.template.drumSynth.name\""
          "\"onboarding.template.podcastVoice.name\""
          "\"onboarding.help.userManual\""
          "\"onboarding.help.showWelcome\""
          "\"onboarding.help.restartTour\""
          "\"onboarding.help.keyboardShortcuts\""
          "\"onboarding.welcome.openProject\""
          "\"onboarding.welcome.openRecent\""
          "\"onboarding.tour.skip\"")
    if(NOT locale_text MATCHES "${required_locale_text}")
      message(FATAL_ERROR
              "${locale}.txt is missing required accessibility/onboarding i18n text: ${required_locale_text}")
    endif()
  endforeach()
  if(locale STREQUAL "es")
    foreach(required_spanish_text
            IN
            ITEMS
            "Grabar"
            "Paneo maestro"
            "Vacío"
            "Multipista básico"
            "Batería \\+ Sintetizador"
            "Podcast / Voz"
            "Manual de usuario de LaMusica"
            "Mostrar ventana de bienvenida"
            "Reiniciar visita guiada"
            "Atajos de teclado"
            "Abrir proyecto"
            "Abrir más reciente"
            "Omitir visita")
      if(NOT locale_text MATCHES "${required_spanish_text}")
        message(FATAL_ERROR
                "es.txt is missing required Spanish accessibility/onboarding translation: ${required_spanish_text}")
      endif()
    endforeach()
  endif()
  if(locale STREQUAL "fr" AND NOT locale_text MATCHES "coverage: stub")
    message(FATAL_ERROR "fr.txt must declare stub coverage while it keeps English fallback values")
  endif()
endforeach()

if(has_app_bundle)
  file(GLOB_RECURSE plist_files LIST_DIRECTORIES false "${extract_dir}/*/LaMusica.app/Contents/Info.plist")
  list(LENGTH plist_files plist_count)
  if(plist_count EQUAL 0)
    message(FATAL_ERROR "Package is missing LaMusica.app Contents/Info.plist")
  endif()
  list(GET plist_files 0 plist_file)
  file(READ "${plist_file}" plist_text)
  foreach(required_plist_text IN ITEMS "CFBundleIdentifier" "dev.lamusica.daw"
                                       "CFBundleLocalizations" "en" "es" "fr"
                                       "NSMicrophoneUsageDescription"
                                       "NSAppleEventsUsageDescription"
                                       "LSApplicationCategoryType" "public.app-category.music"
                                       "LSMinimumSystemVersion" "14.0")
    if(NOT plist_text MATCHES "${required_plist_text}")
      message(FATAL_ERROR "Info.plist is missing required text: ${required_plist_text}")
    endif()
  endforeach()
  foreach(locale IN ITEMS en es fr)
    file(GLOB_RECURSE localized_plist_files LIST_DIRECTORIES false
         "${extract_dir}/*/LaMusica.app/Contents/Resources/${locale}.lproj/InfoPlist.strings")
    list(LENGTH localized_plist_files localized_plist_count)
    if(localized_plist_count EQUAL 0)
      message(FATAL_ERROR
              "Package is missing LaMusica.app Contents/Resources/${locale}.lproj/InfoPlist.strings")
    endif()
    list(GET localized_plist_files 0 localized_plist_file)
    file(READ "${localized_plist_file}" localized_plist_text)
    foreach(required_localized_plist_text
            IN
            ITEMS
            "CFBundleDisplayName"
            "NSMicrophoneUsageDescription"
            "NSAppleEventsUsageDescription")
      if(NOT localized_plist_text MATCHES "${required_localized_plist_text}")
        message(FATAL_ERROR
                "${locale}.lproj/InfoPlist.strings is missing required text: ${required_localized_plist_text}")
      endif()
    endforeach()
  endforeach()
endif()

message(STATUS "Verified package contents: ${package_path}")
