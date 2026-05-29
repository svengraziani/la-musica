#include "i18n/Localization.hpp"

#include "i18n/StringTables.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <stdexcept>

namespace lamusica::daw::i18n {
namespace {

std::string trim(std::string_view value) {
    auto begin = value.begin();
    auto end = value.end();
    while (begin != end && std::isspace(static_cast<unsigned char>(*begin))) {
        ++begin;
    }
    while (begin != end && std::isspace(static_cast<unsigned char>(*(end - 1)))) {
        --end;
    }
    return std::string{begin, end};
}

std::string unquote(std::string_view value) {
    const auto trimmed = trim(value);
    if (trimmed.size() < 2 || trimmed.front() != '"' || trimmed.back() != '"') {
        throw std::runtime_error("String table value must be quoted");
    }
    std::string output;
    bool escaped = false;
    for (std::size_t index = 1; index + 1 < trimmed.size(); ++index) {
        const char character = trimmed[index];
        if (escaped) {
            output.push_back(character);
            escaped = false;
        } else if (character == '\\') {
            escaped = true;
        } else {
            output.push_back(character);
        }
    }
    if (escaped) {
        throw std::runtime_error("String table value ends with an escape");
    }
    return output;
}

std::string localeRoot(std::string_view locale) {
    const auto separator = locale.find_first_of("-_");
    return std::string{locale.substr(0, separator)};
}

} // namespace

StringTable parseStringTable(std::string_view tableText) {
    StringTable table;
    std::istringstream input{std::string{tableText}};
    std::string line;
    while (std::getline(input, line)) {
        const auto trimmed = trim(line);
        if (trimmed.empty() || trimmed.starts_with('#')) {
            continue;
        }
        if (trimmed.starts_with("language:")) {
            table.locale = trim(std::string_view{trimmed}.substr(9));
            continue;
        }
        if (trimmed.starts_with("countries:")) {
            continue;
        }
        if (trimmed.starts_with("coverage:")) {
            table.allowEnglishFallbackValues = trim(std::string_view{trimmed}.substr(9)) == "stub";
            continue;
        }

        const auto equals = trimmed.find('=');
        if (equals == std::string::npos) {
            throw std::runtime_error("String table line is missing '='");
        }
        auto key = unquote(std::string_view{trimmed}.substr(0, equals));
        auto value = unquote(std::string_view{trimmed}.substr(equals + 1));
        if (key.empty() || value.empty()) {
            throw std::runtime_error("String table keys and values must not be empty");
        }
        table.entries.emplace(std::move(key), std::move(value));
    }
    if (table.locale.empty()) {
        throw std::runtime_error("String table is missing language header");
    }
    return table;
}

void LocalizationCatalog::loadBundledTables() {
    tables_.clear();
    for (const auto& bundled : bundledStringTables()) {
        auto table = parseStringTable(bundled.contents);
        tables_.emplace(table.locale, std::move(table));
    }
    activeLocale_ = "en";
}

void LocalizationCatalog::setActiveLocale(std::string_view locale) {
    const auto root = localeRoot(locale);
    activeLocale_ = tables_.contains(root) ? root : "en";
}

std::string_view LocalizationCatalog::activeLocale() const noexcept {
    return activeLocale_;
}

std::string LocalizationCatalog::translate(std::string_view key) const {
    if (const auto table = tables_.find(activeLocale_); table != tables_.end()) {
        if (const auto entry = table->second.entries.find(std::string{key});
            entry != table->second.entries.end()) {
            return entry->second;
        }
    }
    if (const auto english = tables_.find("en"); english != tables_.end()) {
        if (const auto entry = english->second.entries.find(std::string{key});
            entry != english->second.entries.end()) {
            return entry->second;
        }
    }
    return std::string{key};
}

const std::map<std::string, StringTable>& LocalizationCatalog::tables() const noexcept {
    return tables_;
}

std::vector<TranslationIssue>
validateBundledTranslations(const std::map<std::string, StringTable>& tables) {
    std::vector<TranslationIssue> issues;
    const auto english = tables.find("en");
    if (english == tables.end()) {
        issues.push_back({.locale = "en", .key = "", .message = "missing English base table"});
        return issues;
    }

    for (const auto& [locale, table] : tables) {
        if (locale == "en") {
            continue;
        }
        for (const auto& [key, englishValue] : english->second.entries) {
            const auto translated = table.entries.find(key);
            if (translated == table.entries.end()) {
                issues.push_back({.locale = locale, .key = key, .message = "missing translation"});
            } else if (translated->second.empty()) {
                issues.push_back({.locale = locale, .key = key, .message = "empty translation"});
            } else if (!table.allowEnglishFallbackValues && translated->second == englishValue &&
                       key != "LaMusica" && key != "MIDI" && key != "Audio" &&
                       key != "Inspector" && key != "status.no") {
                issues.push_back({.locale = locale,
                                  .key = key,
                                  .message = "translation matches English"});
            }
        }
    }
    return issues;
}

std::string resolveLocale(std::optional<std::string_view> preference, std::string_view systemLocale) {
    if (preference.has_value() && !preference->empty()) {
        return localeRoot(*preference);
    }
    if (!systemLocale.empty()) {
        return localeRoot(systemLocale);
    }
    return "en";
}

} // namespace lamusica::daw::i18n
