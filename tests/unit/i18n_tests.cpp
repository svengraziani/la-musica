#include "i18n/Localization.hpp"
#include "i18n/NumberFormat.hpp"
#include "i18n/StringTables.hpp"
#include "lamusica/session/ApplicationSession.hpp"
#include "lamusica/session/ProjectManifest.hpp"

#include <algorithm>
#include <array>
#include <exception>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <iostream>
#include <locale>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

std::string readFile(const std::filesystem::path& path) {
    std::ifstream input{path};
    require(static_cast<bool>(input), "could not open " + path.string());
    std::ostringstream output;
    output << input.rdbuf();
    return output.str();
}

void verifyResourceFile(const std::filesystem::path& root, std::string_view locale,
                        std::string_view embedded) {
    const auto path = root / "apps" / "daw" / "resources" / "i18n" /
                      (std::string{locale} + ".txt");
    require(readFile(path) == embedded,
            "resource file does not match embedded table: " + path.string());
}

void requireNoUnlocalizedLiterals(const std::string& source, std::string_view sourceName,
                                  std::initializer_list<std::string_view> literals) {
    for (const auto literal : literals) {
        require(source.find(literal) == std::string::npos,
                std::string{sourceName} + " contains unlocalized literal: " +
                    std::string{literal});
    }
}

void verifyInfoPlistStrings(const std::filesystem::path& root, std::string_view locale,
                            std::initializer_list<std::string_view> requiredText) {
    const auto path = root / "apps" / "daw" / "resources" / "macos" /
                      (std::string{locale} + ".lproj") / "InfoPlist.strings";
    const auto strings = readFile(path);
    for (const auto text : requiredText) {
        require(strings.find(text) != std::string::npos,
                "localized InfoPlist.strings is missing required text: " + path.string() +
                    " text=" + std::string{text});
    }
}

void requireSourceContains(const std::string& source, std::string_view sourceName,
                           std::initializer_list<std::string_view> requiredText) {
    for (const auto text : requiredText) {
        require(source.find(text) != std::string::npos,
                std::string{sourceName} + " is missing required localized UI wiring: " +
                    std::string{text});
    }
}

} // namespace

int main(int argc, char** argv) {
    try {
        lamusica::daw::i18n::LocalizationCatalog catalog;
        catalog.loadBundledTables();
        const auto issues = lamusica::daw::i18n::validateBundledTranslations(catalog.tables());
        for (const auto& issue : issues) {
            std::cerr << "i18n issue locale=" << issue.locale << " key=\"" << issue.key
                      << "\" " << issue.message << '\n';
        }
        require(issues.empty(), "bundled translations are incomplete");

        auto missingTranslationTables = catalog.tables();
        missingTranslationTables["es"].entries.erase("Play");
        const auto missingTranslationIssues =
            lamusica::daw::i18n::validateBundledTranslations(missingTranslationTables);
        require(std::ranges::any_of(missingTranslationIssues, [](const auto& issue) {
                    return issue.locale == "es" && issue.key == "Play" &&
                           issue.message == "missing translation";
                }),
                "translation validator did not catch a missing Spanish key");

        auto emptyTranslationTables = catalog.tables();
        emptyTranslationTables["es"].entries["Play"] = "";
        const auto emptyTranslationIssues =
            lamusica::daw::i18n::validateBundledTranslations(emptyTranslationTables);
        require(std::ranges::any_of(emptyTranslationIssues, [](const auto& issue) {
                    return issue.locale == "es" && issue.key == "Play" &&
                           issue.message == "empty translation";
                }),
                "translation validator did not catch an empty Spanish value");

        auto untranslatedTables = catalog.tables();
        untranslatedTables["es"].entries["Play"] = "Play";
        const auto untranslatedIssues =
            lamusica::daw::i18n::validateBundledTranslations(untranslatedTables);
        require(std::ranges::any_of(untranslatedIssues, [](const auto& issue) {
                    return issue.locale == "es" && issue.key == "Play" &&
                           issue.message == "translation matches English";
                }),
                "translation validator did not catch an untranslated Spanish value");

        catalog.setActiveLocale(lamusica::daw::i18n::resolveLocale("es_MX", "en_US"));
        require(catalog.activeLocale() == "es", "locale preference did not select Spanish");
        require(catalog.translate("Transport") == "Transporte",
                "Spanish transport translation failed");
        require(catalog.translate("Play") == "Reproducir", "Spanish play translation failed");
        constexpr std::array<std::string_view, 33> onboardingKeys{
            "onboarding.template.empty.name",
            "onboarding.template.empty.description",
            "onboarding.template.basicMultitrack.name",
            "onboarding.template.basicMultitrack.description",
            "onboarding.template.drumSynth.name",
            "onboarding.template.drumSynth.description",
            "onboarding.template.podcastVoice.name",
            "onboarding.template.podcastVoice.description",
            "onboarding.help.userManual",
            "onboarding.help.showWelcome",
            "onboarding.help.restartTour",
            "onboarding.help.keyboardShortcuts",
            "onboarding.tour.transport.title",
            "onboarding.tour.transport.body",
            "onboarding.tour.browser.title",
            "onboarding.tour.browser.body",
            "onboarding.tour.timeline.title",
            "onboarding.tour.timeline.body",
            "onboarding.tour.inspector.title",
            "onboarding.tour.inspector.body",
            "onboarding.tour.mixer.title",
            "onboarding.tour.mixer.body",
            "onboarding.welcome.title",
            "onboarding.welcome.description",
            "onboarding.welcome.projectTemplates",
            "onboarding.welcome.recentProjects",
            "onboarding.welcome.openProject",
            "onboarding.welcome.openRecent.help",
            "onboarding.chooseTemplateOrRecent",
            "onboarding.welcome.openRecent",
            "onboarding.tour.skip",
            "onboarding.help.keyboardShortcuts.body",
            "onboarding.help.userManual.fallback",
        };
        for (const auto key : onboardingKeys) {
            const auto translated = catalog.translate(key);
            require(translated != key,
                    "Spanish onboarding key fell back to raw key: " + std::string{key});
            require(catalog.tables().at("en").entries.at(std::string{key}) != translated,
                    "Spanish onboarding key fell back to English: " + std::string{key});
        }
        constexpr std::array<std::string_view, 11> privacyKeys{
            "Privacy",
            "privacy.shareDiagnostics",
            "privacy.keepPrivate",
            "privacy.defaultEndpoint",
            "privacy.diagnostics",
            "privacy.sharingCrashReports",
            "privacy.localCrashLogsOnly",
            "privacy.telemetry",
            "privacy.endpoint",
            "privacy.disclosure",
            "privacy.firstRunPrompt",
        };
        for (const auto key : privacyKeys) {
            const auto translated = catalog.translate(key);
            require(translated != key,
                    "Spanish privacy key fell back to raw key: " + std::string{key});
            require(catalog.tables().at("en").entries.at(std::string{key}) != translated,
                    "Spanish privacy key fell back to English: " + std::string{key});
        }
        catalog.setActiveLocale(lamusica::daw::i18n::resolveLocale(std::nullopt, "fr_FR"));
        require(catalog.activeLocale() == "fr", "French locale did not load from bundled tables");
        require(catalog.translate("Play") == "Lecture", "French play translation failed");
        catalog.setActiveLocale(lamusica::daw::i18n::resolveLocale(std::nullopt, "de_DE"));
        require(catalog.activeLocale() == "en",
                "unsupported locale did not fall back to English");

        lamusica::session::ApplicationSession preferenceSession;
        auto preferences = preferenceSession.preferences();
        preferences.preferredLocale = "es_MX";
        preferenceSession.setPreferences(preferences);
        require(preferenceSession.preferences().preferredLocale == "es_MX",
                "application preferences did not preserve the locale override");
        bool rejectedInvalidLocalePreference = false;
        try {
            auto invalidPreferences = preferenceSession.preferences();
            invalidPreferences.preferredLocale = "es MX";
            preferenceSession.setPreferences(invalidPreferences);
        } catch (const std::exception&) {
            rejectedInvalidLocalePreference = true;
        }
        require(rejectedInvalidLocalePreference,
                "application preferences accepted an invalid locale override");

        const auto esFormat = lamusica::daw::i18n::numberFormatForLocale("es_ES");
        require(lamusica::daw::i18n::formatDisplayNumber(1234.5, 1, esFormat) == "1234,5",
                "Spanish display number did not use comma decimal separator");
        const auto enFormat = lamusica::daw::i18n::numberFormatForLocale("en_US");
        require(lamusica::daw::i18n::formatDisplayNumber(1234.5, 1, enFormat) == "1234.5",
                "English display number did not use dot decimal separator");

        lamusica::session::ProjectManifest manifest;
        manifest.name = "Locale Stable";
        manifest.tempoMap.front().bpm = 120.5;
        const auto beforeLocale = lamusica::session::serializeProjectManifest(manifest);
        try {
            std::locale::global(std::locale{"C"});
        } catch (const std::exception&) {
        }
        const auto cLocale = lamusica::session::serializeProjectManifest(manifest);
        require(beforeLocale == cLocale, "project serialization changed under C locale");
        for (const auto* localeName :
             {"es_ES.UTF-8", "es_ES.utf8", "de_DE.UTF-8", "de_DE.utf8"}) {
            try {
                std::locale::global(std::locale{localeName});
            } catch (const std::exception&) {
                continue;
            }
            require(beforeLocale == lamusica::session::serializeProjectManifest(manifest),
                    std::string{"project serialization changed under locale "} + localeName);
        }
        try {
            std::locale::global(std::locale{"C"});
        } catch (const std::exception&) {
        }

        if (argc > 1) {
            const auto root = std::filesystem::path{argv[1]};
            verifyResourceFile(root, "en", lamusica::daw::i18n::englishStringTable());
            verifyResourceFile(root, "es", lamusica::daw::i18n::spanishStringTable());
            verifyResourceFile(root, "fr", lamusica::daw::i18n::frenchStringTable());
            const auto infoPlist = readFile(root / "apps" / "daw" / "Info.plist.in");
            for (const auto* requiredPlistText :
                 {"CFBundleLocalizations", "<string>en</string>", "<string>es</string>",
                  "<string>fr</string>", "NSMicrophoneUsageDescription",
                  "NSAppleEventsUsageDescription"}) {
                require(infoPlist.find(requiredPlistText) != std::string::npos,
                        "Info.plist.in is missing localization/release text: " +
                            std::string{requiredPlistText});
            }
            verifyInfoPlistStrings(
                root, "en",
                {"CFBundleDisplayName", "NSMicrophoneUsageDescription",
                 "NSAppleEventsUsageDescription", "microphone access"});
            verifyInfoPlistStrings(
                root, "es",
                {"CFBundleDisplayName", "NSMicrophoneUsageDescription",
                 "NSAppleEventsUsageDescription", "microfono", "automatizacion"});
            verifyInfoPlistStrings(
                root, "fr",
                {"CFBundleDisplayName", "NSMicrophoneUsageDescription",
                 "NSAppleEventsUsageDescription", "microphone"});
            const auto dawCMake = readFile(root / "apps" / "daw" / "CMakeLists.txt");
            for (const auto* requiredBundleResourceText :
                 {"LAMUSICA_DAW_LOCALIZED_BUNDLE_RESOURCES", "resources/macos/en.lproj",
                  "resources/macos/es.lproj", "resources/macos/fr.lproj",
                  "MACOSX_PACKAGE_LOCATION", "Resources/${LAMUSICA_DAW_LOCALIZATION_NAME}"}) {
                require(dawCMake.find(requiredBundleResourceText) != std::string::npos,
                        "apps/daw/CMakeLists.txt is missing localized bundle resource wiring: " +
                            std::string{requiredBundleResourceText});
            }
            const auto juceShell = readFile(root / "apps" / "daw" / "src" / "main_juce.cpp");
            requireSourceContains(
                juceShell, "main_juce.cpp",
                {"button.setHelpText(projectTemplate == nullptr",
                 "catalog_.setActiveLocale(lamusica::daw::i18n::resolveLocale(",
                 "installJuceLocalisedStrings(catalog_.activeLocale())",
                 "juce::LocalisedStrings::setCurrentMappings",
                 "return TRANS(juceStringFromUtf8(key))",
                 "session_.preferences().preferredLocale",
                 "juce::SystemStats::getDisplayLanguage()",
                 "userManualPreview(std::filesystem::path{executableDir},",
                 "catalog_.activeLocale()",
                 "tr(projectTemplate->descriptionKey)",
                 "setHelpText(tr(\"onboarding.welcome.openRecent.help\"",
                 "showWelcomeButton_.setHelpText(tr(\"onboarding.help.showWelcome\"",
                 "userManualButton_.setHelpText(tr(\"onboarding.help.userManual\"",
                 "restartTourButton_.setHelpText(tr(\"onboarding.help.restartTour\"",
                 "skipTourButton_.setHelpText(tr(\"onboarding.tour.skip\"",
                 "shareDiagnosticsButton_.setHelpText(tr(\"privacy.shareDiagnostics\"",
                 "keepPrivateButton_.setHelpText(tr(\"privacy.keepPrivate\""});
            requireNoUnlocalizedLiterals(
                juceShell, "main_juce.cpp",
                {"Share Diagnostics", "Keep Private", "Recent projects:", "No project open",
                 "Choose a template or open a recent project.", "Space: Play/Stop",
                 "Share scrubbed crash diagnostics with the LaMusica project?",
                 "juce::String{status.tempoBpm",
                 "juce::String{status.lastMixExportPeak"});
            const auto welcomeWindow =
                readFile(root / "apps" / "daw" / "src" / "onboarding" / "WelcomeWindow.cpp");
            requireSourceContains(welcomeWindow, "WelcomeWindow.cpp",
                                  {"tr(\"Help\")",
                                   "catalog.translate(\"onboarding.help.userManual.fallback\")"});
            requireNoUnlocalizedLiterals(
                welcomeWindow, "WelcomeWindow.cpp",
                {"Project Templates", "Recent projects", "Open Project", "Open Most Recent",
                 "Show Welcome Window", "Restart Guided Tour", "Keyboard Shortcuts",
                 "Open the most recent LaMusica project.",
                 "The bundled user manual is installed with LaMusica."});
            const auto guidedTour =
                readFile(root / "apps" / "daw" / "src" / "onboarding" / "GuidedTour.cpp");
            requireNoUnlocalizedLiterals(
                guidedTour, "GuidedTour.cpp",
                {"Start and stop playback, set the loop, and watch the playhead.",
                 "Find project media and reusable sounds.",
                 "Arrange clips and automation over time.",
                 "Edit the selected clip, track, and routing details.",
                 "Balance tracks, sends, meters, and master output.", "Skip Tour"});
            const auto projectTemplates =
                readFile(root / "apps" / "daw" / "src" / "onboarding" / "ProjectTemplates.cpp");
            requireNoUnlocalizedLiterals(
                projectTemplates, "ProjectTemplates.cpp",
                {"A silent project with a master output.",
                 "Three audio tracks routed to master.",
                 "Instrument tracks with MIDI and automation.",
                 "Host and guest tracks through a voice bus."});
        }

        std::cout << "i18n locales=" << catalog.tables().size()
                  << " esTransport=Transporte frPlay=Lecture display="
                  << lamusica::daw::i18n::formatDisplayNumber(120.5, 1, esFormat) << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "i18n tests failed: " << error.what() << '\n';
        return 1;
    }
}
