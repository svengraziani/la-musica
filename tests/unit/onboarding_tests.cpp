#include "onboarding/GuidedTour.hpp"
#include "onboarding/ProjectTemplates.hpp"
#include "onboarding/WelcomeWindow.hpp"
#include "ui/a11y/AccessibleControl.hpp"
#include "ui/a11y/MotionPreferences.hpp"

#include "lamusica/session/ProjectManifest.hpp"

#include <algorithm>
#include <array>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <ranges>
#include <stdexcept>
#include <string>
#include <string_view>

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

    const auto& manifest = document->manifest();
    std::map<lamusica::session::TrackType, std::size_t> trackCounts;
    for (const auto& track : manifest.tracks) {
        ++trackCounts[track.type];
    }

    if (projectTemplate.id == "empty") {
        require(manifest.tracks.size() == 1U &&
                    trackCounts[lamusica::session::TrackType::Master] == 1U,
                "empty template should contain exactly one master track");
        require(manifest.clips.empty(), "empty template should not contain clips");
        const auto block = session.auditionCurrentMixBlock(64);
        require(block.frames == 64U, "empty template did not render an audition block");
        require(std::ranges::all_of(block.interleavedSamples,
                                    [](float sample) { return sample == 0.0F; }),
                "empty template should render silence");
    } else {
        if (projectTemplate.id == "basic-multitrack") {
            require(trackCounts[lamusica::session::TrackType::Audio] == 3U &&
                        trackCounts[lamusica::session::TrackType::Master] == 1U,
                    "basic multitrack template should contain three audio tracks and a master");
            require(manifest.clips.size() == 2U,
                    "basic multitrack template should contain two placeholder clips");
        } else if (projectTemplate.id == "drum-synth") {
            require(trackCounts[lamusica::session::TrackType::Instrument] == 2U &&
                        trackCounts[lamusica::session::TrackType::Master] == 1U,
                    "drum synth template should contain two instrument tracks and a master");
            require(manifest.clips.size() == 2U && manifest.plugins.size() == 1U &&
                        manifest.automation.size() == 1U,
                    "drum synth template should exercise midi clips, plugin, and automation");
        } else if (projectTemplate.id == "podcast-voice") {
            require(trackCounts[lamusica::session::TrackType::Audio] == 2U &&
                        trackCounts[lamusica::session::TrackType::Group] == 1U &&
                        trackCounts[lamusica::session::TrackType::Master] == 1U,
                    "podcast voice template should contain host/guest, voice bus, and master");
            require(manifest.markers.size() == 3U,
                    "podcast voice template should contain intro/segment/outro markers");
        }
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

std::string readSourceFile(const std::filesystem::path& path) {
    std::ifstream input{path};
    require(input.good(), "unable to read source file for onboarding scan: " + path.string());
    return {std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
}

void requireNoFirstTrackBootstrapInProductOnboarding(const std::filesystem::path& sourceRoot) {
    constexpr std::array<std::string_view, 6> forbidden{
        "createFirstTrackProject",
        "makeFirstTrackStarterManifest",
        "loopIntro",
        "extendIntroToVerse",
        "transposeBassUpOctave",
        "recordFirstTrackTake",
    };
    const std::array<std::filesystem::path, 7> productSources{
        sourceRoot / "apps/daw/src/main_juce.cpp",
        sourceRoot / "apps/daw/src/onboarding/GuidedTour.cpp",
        sourceRoot / "apps/daw/src/onboarding/GuidedTour.hpp",
        sourceRoot / "apps/daw/src/onboarding/ProjectTemplates.cpp",
        sourceRoot / "apps/daw/src/onboarding/ProjectTemplates.hpp",
        sourceRoot / "apps/daw/src/onboarding/WelcomeWindow.cpp",
        sourceRoot / "apps/daw/src/onboarding/WelcomeWindow.hpp",
    };

    for (const auto& path : productSources) {
        const auto source = readSourceFile(path);
        for (const auto needle : forbidden) {
            require(source.find(needle) == std::string::npos,
                    "product onboarding/menu source reintroduced FirstTrack bootstrap API: " +
                        path.string() + " contains " + std::string{needle});
        }
    }
}

} // namespace

int main(int argc, char** argv) {
    try {
        namespace onboarding = lamusica::daw::onboarding;
        const auto root = argc >= 2 ? std::filesystem::path{argv[1]}
                                    : std::filesystem::temp_directory_path() /
                                          "lamusica-onboarding-unit";
        const auto sourceRoot = argc >= 3 ? std::filesystem::path{argv[2]}
                                          : std::filesystem::current_path();
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
        requireNoFirstTrackBootstrapInProductOnboarding(sourceRoot);

        for (const auto& projectTemplate : onboarding::projectTemplates()) {
            requireTemplateRenderable(projectTemplate, root);
        }

        lamusica::session::ApplicationSession recentsSession;
        std::vector<std::filesystem::path> createdRecentProjects;
        for (int index = 0; index < 11; ++index) {
            auto path = root / ("recent-" + std::to_string(index) + ".Project.lamusica");
            onboarding::createProjectFromTemplate(recentsSession, "empty", path,
                                                  "Recent " + std::to_string(index));
            createdRecentProjects.push_back(std::move(path));
        }
        const auto recentsChooser = onboarding::welcomeChooserState(recentsSession);
        require(recentsChooser.recentProjects.size() == 10U,
                "welcome chooser did not cap recent projects at ten entries");
        require(recentsChooser.recentProjects.front() == createdRecentProjects.back(),
                "welcome chooser did not put the newest template project first");
        require(std::ranges::find(recentsChooser.recentProjects, createdRecentProjects.front()) ==
                    recentsChooser.recentProjects.end(),
                "welcome chooser kept an eleventh stale recent project");
        onboarding::openMostRecentProject(recentsSession);
        require(recentsSession.currentDocument() != nullptr &&
                    recentsSession.currentDocument()->path() == createdRecentProjects.back(),
                "open-most-recent did not open the newest template project");

        lamusica::session::ApplicationSession session;
        onboarding::createProjectFromTemplate(session, "empty",
                                              root / "a11y-empty.Project.lamusica",
                                              "Accessible Empty");
        const auto welcomeA11y = onboarding::welcomeAccessibilityTree(session, "es");
        const auto welcomeAudit = lamusica::daw::a11y::auditAccessibilityTree(welcomeA11y);
        require(welcomeAudit.ok(), "welcome chooser accessibility tree has audit issues");
        const auto welcomeFocusOrder = lamusica::daw::a11y::focusOrder(welcomeA11y);
        require(welcomeFocusOrder.size() >= 10U, "welcome chooser focus order is incomplete");
        require(welcomeFocusOrder[0] == "welcome-template-empty" &&
                    welcomeFocusOrder[1] == "welcome-template-basic-multitrack",
                "welcome chooser should focus starter templates first");
        require(lamusica::daw::a11y::findAccessibleControl(welcomeA11y,
                                                           "welcome-open-most-recent")
                    .has_value(),
                "welcome chooser missing accessible open-most-recent control");
        require(std::ranges::any_of(welcomeFocusOrder, [](const auto& id) {
                    return id.starts_with("welcome-recent-");
                }),
                "welcome chooser did not expose recent projects in keyboard focus order");
        require(std::ranges::any_of(welcomeFocusOrder, [](const auto& id) {
                    return id == "welcome-help-onboarding-help-usermanual";
                }),
                "welcome chooser did not expose user manual help action in focus order");
        const auto emptyTemplateControl =
            lamusica::daw::a11y::findAccessibleControl(welcomeA11y, "welcome-template-empty");
        const auto helpManualControl = lamusica::daw::a11y::findAccessibleControl(
            welcomeA11y, "welcome-help-onboarding-help-usermanual");
        const auto openProjectControl =
            lamusica::daw::a11y::findAccessibleControl(welcomeA11y, "welcome-open");
        require(emptyTemplateControl.has_value() && (*emptyTemplateControl)->name == "Vacío" &&
                    (*emptyTemplateControl)->description.find("proyecto silencioso") !=
                        std::string::npos &&
                    helpManualControl.has_value() &&
                    (*helpManualControl)->name == "Manual de usuario de LaMusica" &&
                    openProjectControl.has_value() &&
                    (*openProjectControl)->name == "Abrir proyecto",
                "welcome accessibility tree exposed untranslated onboarding keys");

        require(onboarding::shouldShowGuidedTour(session), "guided tour should default to visible");
        require(onboarding::guidedTourSteps().size() == 5U, "guided tour step count changed");
        lamusica::daw::a11y::MotionPreferences motion;
        const auto normalWelcomeMotion = onboarding::welcomeMotionPolicy(motion);
        const auto normalTourMotion = onboarding::guidedTourMotionPolicy(motion);
        motion.setReduceMotionForTesting(true);
        const auto reducedWelcomeMotion = onboarding::welcomeMotionPolicy(motion);
        const auto reducedTourMotion = onboarding::guidedTourMotionPolicy(motion);
        require(normalWelcomeMotion.animated && normalWelcomeMotion.transitionMilliseconds > 0U &&
                    normalTourMotion.animated && normalTourMotion.transitionMilliseconds > 0U,
                "onboarding should allow transitions when Reduce Motion is off");
        require(!reducedWelcomeMotion.animated &&
                    reducedWelcomeMotion.transitionMilliseconds == 0U &&
                    !reducedTourMotion.animated && reducedTourMotion.transitionMilliseconds == 0U,
                "onboarding must disable animated transitions when Reduce Motion is on");
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
        require(manual.find("New Project opens the welcome/template flow") != std::string::npos &&
                    manual.find("Empty, Basic Multitrack, Drum + Synth, and Podcast / Voice") !=
                        std::string::npos,
                "bundled user manual does not describe the template-based first launch flow");
        const auto packageRoot = root / "package-layout";
        const auto executableDir = packageRoot / "LaMusica.app" / "Contents" / "MacOS";
        const auto bundledDocDir = packageRoot / "share" / "doc" / "LaMusica";
        std::filesystem::create_directories(executableDir);
        std::filesystem::create_directories(bundledDocDir);
        {
            std::ofstream bundledManual{bundledDocDir / "user-manual.md"};
            bundledManual << "# Bundled Manual\n\nOffline help from package layout.\n";
        }
        const auto bundledManualPreview = onboarding::userManualPreview(executableDir);
        require(bundledManualPreview.find("# Bundled Manual") != std::string::npos &&
                    bundledManualPreview.find("Offline help from package layout") !=
                        std::string::npos,
                "help menu user manual preview did not resolve the packaged share/doc layout");
        const auto missingManualRoot =
            std::filesystem::temp_directory_path() / "lamusica-missing-manual-preview-root";
        std::filesystem::remove_all(missingManualRoot);
        std::filesystem::create_directories(missingManualRoot);
        const auto missingManualPreview = onboarding::userManualPreview(missingManualRoot, "es");
        require(missingManualPreview.find("manual de usuario incluido") != std::string::npos &&
                    missingManualPreview.find("bundled user manual") == std::string::npos,
                "help menu user manual fallback was not localized: " + missingManualPreview);
        std::filesystem::remove_all(missingManualRoot);

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
