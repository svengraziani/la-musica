#pragma once

#include "onboarding/ProjectTemplates.hpp"

#include "lamusica/session/ApplicationSession.hpp"
#include "ui/a11y/AccessibleControl.hpp"
#include "ui/a11y/MotionPreferences.hpp"

#include <filesystem>
#include <span>
#include <string>
#include <string_view>

namespace lamusica::daw::onboarding {

struct WelcomeChooserState {
    std::span<const ProjectTemplate> templates;
    std::span<const std::filesystem::path> recentProjects;
    bool canOpenMostRecent{false};
};

struct WelcomeMotionPolicy {
    bool animated{true};
    unsigned transitionMilliseconds{180U};
};

enum class HelpAction {
    UserManual,
    ShowWelcome,
    RestartGuidedTour,
    KeyboardShortcuts,
};

struct HelpMenuItem {
    HelpAction action{HelpAction::UserManual};
    std::string_view titleKey;
};

[[nodiscard]] WelcomeChooserState welcomeChooserState(
    const session::ApplicationSession& session) noexcept;
[[nodiscard]] WelcomeMotionPolicy welcomeMotionPolicy(
    const a11y::MotionPreferences& motionPreferences) noexcept;
[[nodiscard]] a11y::AccessibleControl welcomeAccessibilityTree(
    const session::ApplicationSession& session, std::string_view locale = "en");
void openMostRecentProject(session::ApplicationSession& session);
[[nodiscard]] std::span<const HelpMenuItem> helpMenuItems() noexcept;
[[nodiscard]] std::string userManualPreview(const std::filesystem::path& sourceRoot,
                                            std::string_view locale = "en",
                                            std::size_t maxBytes = 6000U);

} // namespace lamusica::daw::onboarding
