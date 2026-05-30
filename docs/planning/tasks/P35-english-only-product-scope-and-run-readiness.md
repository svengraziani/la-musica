# P35 English-only product scope and run-readiness cleanup

> **Area:** product-scope · **Severity:** high · **Effort:** M · **Phase:** Phase 7 - Verification, performance, and release
> **Depends on:** P19, P26, P31, P34 · **Blocks:** release readiness

## Objective
Bring the project back to an English-only product surface and make the release/run status
unambiguous. The current tree intentionally implements multilingual behavior through P31:
English is the base locale, Spanish is a production locale, and French is a `coverage: stub`
add-a-locale smoke fixture. If the product requirement is now English-only, the multilingual
scope must be removed deliberately from code, resources, tests, package verification, and docs.

This task also records the current readiness audit: the workspace has uncommitted localization
changes and the latest available debug CTest log is not green.

## Current Evidence
- `docs/planning/tasks/P31-localization-i18n-spanish.md` explicitly asks for English plus Spanish
  localization and a third-locale smoke fixture.
- `apps/daw/resources/i18n/` contains `en.txt`, `es.txt`, and `fr.txt`.
- `apps/daw/resources/macos/` contains `en.lproj`, `es.lproj`, and `fr.lproj` Info.plist strings.
- `apps/daw/src/i18n/StringTables.cpp` registers bundled locales `en`, `es`, and `fr`.
- `apps/daw/src/i18n/Localization.cpp` resolves the active locale from preferences/system locale
  and falls back to English only when the requested locale is not bundled.
- `apps/daw/src/i18n/NumberFormat.cpp` gives Spanish locale display formatting a comma decimal
  separator.
- `apps/daw/CMakeLists.txt`, `CMakeLists.txt`, `cmake/VerifyPackage.cmake`,
  `docs/developer/localization.md`, `docs/release/macos-distribution.md`, and
  `tests/unit/i18n_tests.cpp` all encode multilingual expectations.
- `git status --short` currently shows local modifications in:
  `apps/daw/src/main.cpp`, `apps/daw/src/main_juce.cpp`,
  `tests/unit/bootstrap_tests.cpp`, and `tests/unit/i18n_tests.cpp`.
- `build/unix-debug/Testing/Temporary/LastTest.log` from 2026-05-28 ran 82 tests and failed the
  final test: `lamusica_cli_verify_examples_rejects_missing_assets` expected `missing asset`, but
  the command printed `Missing required key: sourceOffsetSamples`.
- This audit environment cannot currently run the full build: `cmake`, `ctest`, and `make` are not
  available on `PATH`, although `build/unix-debug/CMakeCache.txt` exists and points
  `LAMUSICA_JUCE_PATH` at `external/JUCE`.

## Deliverables
- **Product decision cleanup:** replace P31's multilingual requirement with an English-only policy,
  or mark P31 superseded by this task. The product docs must say clearly that the shipped app has
  one UI language: English.
- **Remove non-English resources:** delete Spanish and French app string tables and macOS
  `.lproj` resources from source and package outputs:
  - `apps/daw/resources/i18n/es.txt`
  - `apps/daw/resources/i18n/fr.txt`
  - `apps/daw/resources/macos/es.lproj/InfoPlist.strings`
  - `apps/daw/resources/macos/fr.lproj/InfoPlist.strings`
- **Collapse locale registration:** keep only the English table in
  `apps/daw/src/i18n/StringTables.cpp` and remove APIs/tests that require registering bundled
  non-English locales.
- **Simplify locale behavior:** make the app resolve display strings to English only. Keep stable
  C-locale serialization rules, but remove user/system locale selection from product behavior
  unless it is needed only as an internal formatting invariant test.
- **Simplify number formatting:** remove Spanish-specific comma display behavior if English-only
  UI formatting is the product requirement.
- **Update macOS bundle metadata:** remove `es` and `fr` from `CFBundleLocalizations`,
  `LAMUSICA_DAW_LOCALIZED_BUNDLE_RESOURCES`, package verification, and release documentation.
- **Update tests:** replace multilingual i18n tests with English-only coverage:
  - English table parses and contains required UI/onboarding/privacy keys.
  - Unsupported or user-provided locale preferences resolve to English.
  - Project serialization remains byte-stable under non-C process locales.
  - Package verification rejects missing English resources, not missing Spanish/French resources.
- **Fix current run-readiness failure:** investigate why
  `lamusica_cli_verify_examples_rejects_missing_assets` receives a schema error before the missing
  asset error. Update the fixture or validator order so the test asserts the intended failure.
- **Re-run readiness gates in an environment with tools available:** configure, build, and test the
  current tree before calling it run-ready.

## Acceptance Gates
- `git status --short` has no unexpected local modifications before release verification starts.
- `cmake --preset debug`, `cmake --build --preset debug`, and `ctest --preset debug
  --output-on-failure` pass.
- `ctest --preset debug -L i18n --output-on-failure` passes with English-only expectations.
- `ctest --preset debug -R lamusica_cli_verify_examples_rejects_missing_assets
  --output-on-failure` passes.
- Release/package verification no longer expects `es.txt`, `fr.txt`, `es.lproj`, or `fr.lproj`.
- Docs no longer describe adding or shipping Spanish/French UI locales, except in historical notes
  that clearly say the previous multilingual scope was superseded.

## Notes For Agents
- Do not remove the `i18n` module blindly if UI code still uses it as the string lookup boundary.
  It can remain as an English-only facade if that keeps UI code consistent.
- Preserve the invariant that project files, MCP payloads, command ids, and logs are stable English
  data. English-only UI does not permit locale-dependent serialization.
- The existing uncommitted changes appear to add JUCE `LocalisedStrings` integration and locale
  preference smoke checks. Treat them as user/workspace changes until this task is implemented.
