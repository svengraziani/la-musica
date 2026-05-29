#pragma once

#include <span>
#include <string_view>

namespace lamusica::daw::i18n {

struct BundledStringTable {
    std::string_view locale;
    std::string_view resourceName;
    std::string_view contents;
};

[[nodiscard]] std::string_view englishStringTable() noexcept;
[[nodiscard]] std::string_view spanishStringTable() noexcept;
[[nodiscard]] std::string_view frenchStringTable() noexcept;
[[nodiscard]] std::span<const BundledStringTable> bundledStringTables() noexcept;

} // namespace lamusica::daw::i18n
