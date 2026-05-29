#pragma once

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace lamusica::daw::i18n {

struct StringTable {
    std::string locale;
    bool allowEnglishFallbackValues{false};
    std::map<std::string, std::string> entries;
};

struct TranslationIssue {
    std::string locale;
    std::string key;
    std::string message;
};

class LocalizationCatalog {
  public:
    void loadBundledTables();
    void setActiveLocale(std::string_view locale);
    [[nodiscard]] std::string_view activeLocale() const noexcept;
    [[nodiscard]] std::string translate(std::string_view key) const;
    [[nodiscard]] const std::map<std::string, StringTable>& tables() const noexcept;

  private:
    std::map<std::string, StringTable> tables_;
    std::string activeLocale_{"en"};
};

[[nodiscard]] StringTable parseStringTable(std::string_view tableText);
[[nodiscard]] std::vector<TranslationIssue>
validateBundledTranslations(const std::map<std::string, StringTable>& tables);
[[nodiscard]] std::string resolveLocale(std::optional<std::string_view> preference,
                                        std::string_view systemLocale);

} // namespace lamusica::daw::i18n
