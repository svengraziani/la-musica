#include "ui/a11y/MotionPreferences.hpp"

namespace lamusica::daw::a11y {

bool MotionPreferences::reduceMotion() const noexcept {
    return reduceMotion_;
}

void MotionPreferences::setReduceMotionForTesting(bool enabled) noexcept {
    reduceMotion_ = enabled;
}

unsigned MotionPreferences::animationIntervalMilliseconds() const noexcept {
    return reduceMotion_ ? 1000U : 33U;
}

} // namespace lamusica::daw::a11y
