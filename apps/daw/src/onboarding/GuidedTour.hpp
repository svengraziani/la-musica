#pragma once

#include "lamusica/session/ApplicationSession.hpp"

#include <span>
#include <string>

namespace lamusica::daw::onboarding {

struct GuidedTourStep {
    session::ApplicationPanel panel{session::ApplicationPanel::Transport};
    std::string titleKey;
    std::string bodyKey;
};

[[nodiscard]] std::span<const GuidedTourStep> guidedTourSteps() noexcept;
void markGuidedTourSeen(session::ApplicationSession& session, bool seen);
[[nodiscard]] bool shouldShowGuidedTour(const session::ApplicationSession& session) noexcept;

} // namespace lamusica::daw::onboarding
