#pragma once

#include "onboarding/ProjectTemplates.hpp"

#include "lamusica/session/ApplicationSession.hpp"

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
void openMostRecentProject(session::ApplicationSession& session);
[[nodiscard]] std::span<const HelpMenuItem> helpMenuItems() noexcept;
[[nodiscard]] std::string userManualPreview(const std::filesystem::path& sourceRoot,
                                            std::size_t maxBytes = 1800U);

} // namespace lamusica::daw::onboarding
