#pragma once

#include "lamusica/session/ApplicationSession.hpp"
#include "ui/a11y/MotionPreferences.hpp"

#include <span>
#include <string>

namespace lamusica::daw::onboarding {

struct GuidedTourStep {
    session::ApplicationPanel panel{session::ApplicationPanel::Transport};
    std::string titleKey;
    std::string bodyKey;
};

struct GuidedTourMotionPolicy {
    bool animated{true};
    unsigned transitionMilliseconds{180U};
};

[[nodiscard]] std::span<const GuidedTourStep> guidedTourSteps() noexcept;
[[nodiscard]] GuidedTourMotionPolicy guidedTourMotionPolicy(
    const a11y::MotionPreferences& motionPreferences) noexcept;
void markGuidedTourSeen(session::ApplicationSession& session, bool seen);
[[nodiscard]] bool shouldShowGuidedTour(const session::ApplicationSession& session) noexcept;

} // namespace lamusica::daw::onboarding
