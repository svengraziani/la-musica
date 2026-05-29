#include "i18n/Localization.hpp"
#include "i18n/NumberFormat.hpp"
#include "i18n/StringTables.hpp"
#include "lamusica/session/ProjectManifest.hpp"

#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <locale>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>

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

        catalog.setActiveLocale(lamusica::daw::i18n::resolveLocale("es_MX", "en_US"));
        require(catalog.activeLocale() == "es", "locale preference did not select Spanish");
        require(catalog.translate("Transport") == "Transporte",
                "Spanish transport translation failed");
        require(catalog.translate("Play") == "Reproducir", "Spanish play translation failed");
        catalog.setActiveLocale(lamusica::daw::i18n::resolveLocale(std::nullopt, "fr_FR"));
        require(catalog.activeLocale() == "fr", "French locale did not load from bundled tables");
        require(catalog.translate("Play") == "Lecture", "French play translation failed");
        catalog.setActiveLocale(lamusica::daw::i18n::resolveLocale(std::nullopt, "de_DE"));
        require(catalog.activeLocale() == "en",
                "unsupported locale did not fall back to English");

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

        if (argc > 1) {
            const auto root = std::filesystem::path{argv[1]};
            verifyResourceFile(root, "en", lamusica::daw::i18n::englishStringTable());
            verifyResourceFile(root, "es", lamusica::daw::i18n::spanishStringTable());
            verifyResourceFile(root, "fr", lamusica::daw::i18n::frenchStringTable());
            const auto juceShell = readFile(root / "apps" / "daw" / "src" / "main_juce.cpp");
            for (const auto* forbiddenLiteral :
                 {"Share Diagnostics", "Keep Private", "Recent projects:", "No project open",
                  "Choose a template or open a recent project.", "Space: Play/Stop",
                  "Share scrubbed crash diagnostics with the LaMusica project?"}) {
                require(juceShell.find(forbiddenLiteral) == std::string::npos,
                        "main_juce.cpp contains unlocalized shell literal: " +
                            std::string{forbiddenLiteral});
            }
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
