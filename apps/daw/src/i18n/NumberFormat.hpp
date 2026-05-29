#pragma once

#include <string>
#include <string_view>

namespace lamusica::daw::i18n {

struct DisplayNumberFormat {
    char decimalSeparator{'.'};
    char groupingSeparator{','};
};

[[nodiscard]] DisplayNumberFormat numberFormatForLocale(std::string_view locale);
[[nodiscard]] std::string formatDisplayNumber(double value, int decimals,
                                              DisplayNumberFormat format);

} // namespace lamusica::daw::i18n
