#include "onboarding/GuidedTour.hpp"

#include <array>

namespace lamusica::daw::onboarding {
namespace {

const std::array<GuidedTourStep, 5> steps{{
    {.panel = session::ApplicationPanel::Transport,
     .titleKey = "onboarding.tour.transport.title",
     .bodyKey = "onboarding.tour.transport.body"},
    {.panel = session::ApplicationPanel::Browser,
     .titleKey = "onboarding.tour.browser.title",
     .bodyKey = "onboarding.tour.browser.body"},
    {.panel = session::ApplicationPanel::Timeline,
     .titleKey = "onboarding.tour.timeline.title",
     .bodyKey = "onboarding.tour.timeline.body"},
    {.panel = session::ApplicationPanel::Inspector,
     .titleKey = "onboarding.tour.inspector.title",
     .bodyKey = "onboarding.tour.inspector.body"},
    {.panel = session::ApplicationPanel::Mixer,
     .titleKey = "onboarding.tour.mixer.title",
     .bodyKey = "onboarding.tour.mixer.body"},
}};

} // namespace

std::span<const GuidedTourStep> guidedTourSteps() noexcept {
    return steps;
}

GuidedTourMotionPolicy guidedTourMotionPolicy(
    const a11y::MotionPreferences& motionPreferences) noexcept {
    if (motionPreferences.reduceMotion()) {
        return {.animated = false, .transitionMilliseconds = 0U};
    }
    return {};
}

void markGuidedTourSeen(session::ApplicationSession& session, bool seen) {
    auto preferences = session.preferences();
    preferences.guidedTourSeen = seen;
    session.setPreferences(std::move(preferences));
}

bool shouldShowGuidedTour(const session::ApplicationSession& session) noexcept {
    return !session.preferences().guidedTourSeen;
}

} // namespace lamusica::daw::onboarding
