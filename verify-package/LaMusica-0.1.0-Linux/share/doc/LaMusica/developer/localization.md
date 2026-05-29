# Localization

LaMusica UI strings are looked up through the DAW i18n catalog in `apps/daw/src/i18n`.
Project files, MCP payloads, command ids, and logs stay stable English/C-locale data.

## Add A Locale

1. Add `apps/daw/resources/i18n/<locale>.txt` using the table format
   `"English source" = "Translated value"`.
2. Register the locale in `apps/daw/src/i18n/StringTables.cpp`.
3. Add the locale to `CFBundleLocalizations` in `apps/daw/Info.plist.in`.
4. Add `apps/daw/resources/macos/<locale>.lproj/InfoPlist.strings` and register it in
   `LAMUSICA_DAW_LOCALIZED_BUNDLE_RESOURCES` so macOS app bundles ship localized usage strings.
5. Run `lamusica_i18n_tests`.

## Locale Selection

At startup the DAW resolves the active locale in this order:

1. `ApplicationPreferences::preferredLocale`, when set.
2. The macOS/system display language from `juce::SystemStats::getDisplayLanguage()`.
3. English (`en`) as the fallback.

`preferredLocale` should be a BCP-47-style language or language-region id such as `es` or
`es_MX`. It is a UI preference only; it must not affect project files, command ids, MCP payloads,
logs, or other stable machine-readable data.

Bundled production locales are strict: every English key must exist in each shipped locale, values
must not be empty, and untranslated Spanish values fail unless they are explicitly allowed product
terms such as `LaMusica`. A locale marked `coverage: stub` is allowed to reuse English strings only
as an add-a-locale smoke fixture; it must still carry every key and should not be treated as a
complete production translation.

## Display Formatting

Use `NumberFormat` only for UI text. Do not call `std::locale::global` or imbue user locales into
project serialization. The project writer must remain byte-stable and use `.` decimal formatting
regardless of the user's language.

## Current Locales

- `en`: English base table and key inventory.
- `es`: Spanish translation.
- `fr`: add-a-locale smoke fixture with `coverage: stub` and a small translated subset.
