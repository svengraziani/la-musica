#include "ui/a11y/ContrastPreferences.hpp"

#include <algorithm>
#include <cmath>

namespace lamusica::daw::a11y {
namespace {

double channel(unsigned argb, unsigned shift) noexcept {
    const auto srgb = static_cast<double>((argb >> shift) & 0xffU) / 255.0;
    return srgb <= 0.03928 ? srgb / 12.92 : std::pow((srgb + 0.055) / 1.055, 2.4);
}

double luminance(unsigned argb) noexcept {
    return 0.2126 * channel(argb, 16U) + 0.7152 * channel(argb, 8U) +
           0.0722 * channel(argb, 0U);
}

} // namespace

bool ContrastPreferences::increaseContrast() const noexcept {
    return increaseContrast_;
}

void ContrastPreferences::setIncreaseContrastForTesting(bool enabled) noexcept {
    increaseContrast_ = enabled;
}

ContrastPalette ContrastPreferences::palette() const noexcept {
    if (increaseContrast_) {
        return {.foreground = 0xffffffffU, .background = 0xff000000U, .focusRing = 0xffffff00U};
    }
    return {};
}

double contrastRatio(unsigned foregroundArgb, unsigned backgroundArgb) noexcept {
    const auto lighter = std::max(luminance(foregroundArgb), luminance(backgroundArgb));
    const auto darker = std::min(luminance(foregroundArgb), luminance(backgroundArgb));
    return (lighter + 0.05) / (darker + 0.05);
}

} // namespace lamusica::daw::a11y
