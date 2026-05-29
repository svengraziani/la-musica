#include "onboarding/WelcomeWindow.hpp"

#include <array>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace lamusica::daw::onboarding {
namespace {

const std::array<HelpMenuItem, 4> helpItems{{
    {.action = HelpAction::UserManual, .titleKey = "onboarding.help.userManual"},
    {.action = HelpAction::ShowWelcome, .titleKey = "onboarding.help.showWelcome"},
    {.action = HelpAction::RestartGuidedTour, .titleKey = "onboarding.help.restartTour"},
    {.action = HelpAction::KeyboardShortcuts, .titleKey = "onboarding.help.keyboardShortcuts"},
}};

} // namespace

WelcomeChooserState welcomeChooserState(const session::ApplicationSession& session) noexcept {
    return {.templates = projectTemplates(),
            .recentProjects = session.recentProjects(),
            .canOpenMostRecent = !session.recentProjects().empty()};
}

void openMostRecentProject(session::ApplicationSession& session) {
    if (session.recentProjects().empty()) {
        throw std::runtime_error("No recent project is available");
    }
    session.openProject(session.recentProjects().front());
}

std::span<const HelpMenuItem> helpMenuItems() noexcept {
    return helpItems;
}

std::string userManualPreview(const std::filesystem::path& sourceRoot, std::size_t maxBytes) {
    std::ifstream input{sourceRoot / "docs" / "user-manual.md"};
    if (!input) {
        return "The bundled user manual is installed with LaMusica.";
    }

    std::ostringstream output;
    std::string line;
    while (std::getline(input, line) && output.tellp() < static_cast<std::streampos>(maxBytes)) {
        output << line << '\n';
    }
    return output.str();
}

} // namespace lamusica::daw::onboarding
