#include "onboarding/GuidedTour.hpp"
#include "onboarding/ProjectTemplates.hpp"
#include "onboarding/WelcomeWindow.hpp"

#include "lamusica/session/ProjectManifest.hpp"

#include <algorithm>
#include <exception>
#include <filesystem>
#include <iostream>
#include <ranges>
#include <stdexcept>
#include <string>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void requireTemplateRenderable(const lamusica::daw::onboarding::ProjectTemplate& projectTemplate,
                               const std::filesystem::path& root) {
    namespace onboarding = lamusica::daw::onboarding;

    lamusica::session::ApplicationSession session;
    const auto projectPath = root / (projectTemplate.id + ".Project.lamusica");
    onboarding::createProjectFromTemplate(session, projectTemplate.id, projectPath,
                                          projectTemplate.id);

    const auto chooser = onboarding::welcomeChooserState(session);
    require(chooser.templates.size() == 4U, "welcome chooser did not expose all templates");
    require(chooser.canOpenMostRecent, "template creation did not seed recent projects");
    require(!chooser.recentProjects.empty() && chooser.recentProjects.front() == projectPath,
            "welcome chooser did not expose the newly created project as most recent");

    const auto* document = session.currentDocument();
    require(document != nullptr && document->isOpen(), "template did not open a document");
    require(session.status().hasOpenProject, "template did not update session open-project status");
    lamusica::session::validateProjectManifest(document->manifest());
    require(document->manifest().schemaVersion == lamusica::session::currentProjectSchemaVersion,
            "template produced an outdated project schema");

    if (projectTemplate.id == "empty") {
        const auto block = session.auditionCurrentMixBlock(64);
        require(block.frames == 64U, "empty template did not render an audition block");
        require(std::ranges::all_of(block.interleavedSamples,
                                    [](float sample) { return sample == 0.0F; }),
                "empty template should render silence");
    } else {
        const auto mixPath = root / (projectTemplate.id + ".wav");
        const auto bounce = session.exportCurrentMix(mixPath);
        require(bounce.frames > 0U && std::filesystem::exists(mixPath),
                "template did not export a renderable mix");
    }

    lamusica::session::ApplicationSession recentSession;
    recentSession.openProject(projectPath);
    onboarding::openMostRecentProject(recentSession);
    require(recentSession.status().hasOpenProject, "chooser did not open the most recent project");
}

} // namespace

int main() {
    try {
        namespace onboarding = lamusica::daw::onboarding;
        const auto root = std::filesystem::temp_directory_path() / "lamusica-onboarding-unit";
        std::filesystem::remove_all(root);
        std::filesystem::create_directories(root);

        require(onboarding::projectTemplates().size() == 4U, "unexpected template count");
        require(onboarding::findProjectTemplate("empty") != nullptr, "empty template missing");
        require(onboarding::findProjectTemplate("basic-multitrack") != nullptr,
                "basic multitrack template missing");
        require(onboarding::findProjectTemplate("drum-synth") != nullptr,
                "drum synth template missing");
        require(onboarding::findProjectTemplate("podcast-voice") != nullptr,
                "podcast voice template missing");

        for (const auto& projectTemplate : onboarding::projectTemplates()) {
            requireTemplateRenderable(projectTemplate, root);
        }

        lamusica::session::ApplicationSession session;
        require(onboarding::shouldShowGuidedTour(session), "guided tour should default to visible");
        require(onboarding::guidedTourSteps().size() == 5U, "guided tour step count changed");
        onboarding::markGuidedTourSeen(session, true);
        require(!onboarding::shouldShowGuidedTour(session),
                "guided tour seen preference was not persisted");
        const auto helpItems = onboarding::helpMenuItems();
        require(helpItems.size() == 4U, "help menu item count changed");
        require(helpItems[0].action == onboarding::HelpAction::UserManual &&
                    helpItems[0].titleKey == "onboarding.help.userManual",
                "user manual help item is missing");
        require(std::ranges::any_of(helpItems, [](const auto& item) {
                    return item.action == onboarding::HelpAction::ShowWelcome &&
                           item.titleKey == "onboarding.help.showWelcome";
                }),
                "show welcome help item is missing");
        require(std::ranges::any_of(helpItems, [](const auto& item) {
                    return item.action == onboarding::HelpAction::RestartGuidedTour &&
                           item.titleKey == "onboarding.help.restartTour";
                }),
                "restart guided tour help item is missing");
        require(std::ranges::any_of(helpItems, [](const auto& item) {
                    return item.action == onboarding::HelpAction::KeyboardShortcuts &&
                           item.titleKey == "onboarding.help.keyboardShortcuts";
                }),
                "keyboard shortcuts help item is missing");
        const auto manual = onboarding::userManualPreview(std::filesystem::current_path());
        require(manual.find("# LaMusica User Manual") != std::string::npos &&
                    manual.find("Help includes the bundled user manual") != std::string::npos,
                "help menu user manual preview did not read bundled manual content");

        bool rejectedMissingTemplate = false;
        try {
            onboarding::createProjectFromTemplate(session, "missing-template",
                                                  root / "missing.Project.lamusica", "Missing");
        } catch (const std::exception&) {
            rejectedMissingTemplate = true;
        }
        require(rejectedMissingTemplate, "unknown template id was accepted");

        std::filesystem::remove_all(root);
        std::cout << "onboarding templates=4 renderable=true recents=true guidedTour=true\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "onboarding test failed: " << error.what() << '\n';
        return 1;
    }
}
