#pragma once

namespace lamusica::daw::a11y {

class MotionPreferences {
  public:
    [[nodiscard]] bool reduceMotion() const noexcept;
    void setReduceMotionForTesting(bool enabled) noexcept;
    [[nodiscard]] unsigned animationIntervalMilliseconds() const noexcept;

  private:
    bool reduceMotion_{false};
};

} // namespace lamusica::daw::a11y
