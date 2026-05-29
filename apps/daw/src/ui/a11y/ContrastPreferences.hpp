#pragma once

namespace lamusica::daw::a11y {

struct ContrastPalette {
    unsigned foreground{0xfff5f5f5U};
    unsigned background{0xff181a1fU};
    unsigned focusRing{0xffffcc00U};
};

class ContrastPreferences {
  public:
    [[nodiscard]] bool increaseContrast() const noexcept;
    void setIncreaseContrastForTesting(bool enabled) noexcept;
    [[nodiscard]] ContrastPalette palette() const noexcept;

  private:
    bool increaseContrast_{false};
};

[[nodiscard]] double contrastRatio(unsigned foregroundArgb, unsigned backgroundArgb) noexcept;

} // namespace lamusica::daw::a11y
