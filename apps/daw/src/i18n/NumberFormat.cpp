#include "i18n/NumberFormat.hpp"

#include <cmath>
#include <iomanip>
#include <sstream>

namespace lamusica::daw::i18n {

DisplayNumberFormat numberFormatForLocale(std::string_view locale) {
    if (locale.starts_with("es")) {
        return {.decimalSeparator = ',', .groupingSeparator = '.'};
    }
    return {};
}

std::string formatDisplayNumber(double value, int decimals, DisplayNumberFormat format) {
    std::ostringstream output;
    output << std::fixed << std::setprecision(decimals) << value;
    auto text = output.str();
    if (format.decimalSeparator != '.') {
        for (auto& character : text) {
            if (character == '.') {
                character = format.decimalSeparator;
            }
        }
    }
    return text;
}

} // namespace lamusica::daw::i18n
