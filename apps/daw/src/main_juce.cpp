#include "lamusica/session/ApplicationSession.hpp"
#include "lamusica/session/DiagnosticsScrubber.hpp"
#include "lamusica/version.hpp"
#include "lamusica/crash_report/CrashReporter.hpp"
#include "i18n/Localization.hpp"
#include "onboarding/GuidedTour.hpp"
#include "onboarding/ProjectTemplates.hpp"
#include "onboarding/WelcomeWindow.hpp"

#include <juce_gui_extra/juce_gui_extra.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

std::filesystem::path templateProjectPath(std::string_view templateId) {
    const auto root = std::filesystem::temp_directory_path();
    const std::string baseName{"LaMusica " + std::string{templateId}};
    for (int index = 0; index < 1000; ++index) {
        const auto suffix = index == 0 ? std::string{} : " " + std::to_string(index + 1);
        auto candidate = root / (baseName + suffix + ".Project.lamusica");
        if (!std::filesystem::exists(candidate)) {
            return candidate;
        }
    }
    throw std::runtime_error("Unable to find a free untitled project path");
}

std::vector<std::string> splitCommandLine(const juce::String& commandLine) {
    std::istringstream input{commandLine.toStdString()};
    std::vector<std::string> tokens;
    std::string token;
    while (input >> token) {
        tokens.push_back(std::move(token));
    }
    return tokens;
}

std::filesystem::path headlessBindingProjectPath(const std::vector<std::string>& tokens) {
    for (std::size_t index = 0; index + 1U < tokens.size(); ++index) {
        if (tokens[index] == "--headless-binding") {
            return tokens[index + 1U];
        }
    }
    return {};
}

bool hasVersionArgument(const std::vector<std::string>& tokens) {
    return std::ranges::find(tokens, "--version") != tokens.end();
}

void printVersion() {
    std::cout << "LaMusica " << lamusica::build::version
              << " commit=" << lamusica::build::gitCommit
              << " dirty=" << (lamusica::build::gitDirty ? "true" : "false")
              << " buildDate=" << lamusica::build::buildDate << '\n';
}

bool runHeadlessBindingCheck(const std::filesystem::path& projectPath) {
    if (projectPath.empty()) {
        std::cerr << "LaMusica headless binding failed: missing project path\n";
        return false;
    }

    try {
        std::filesystem::remove_all(projectPath);
        lamusica::session::ApplicationSession session;
        lamusica::daw::onboarding::createProjectFromTemplate(session, "drum-synth", projectPath,
                                                             "Headless Binding");
        const auto& status = session.status();
        const bool ok = status.hasOpenProject && status.trackCount == 3U &&
                        status.clipCount == 2U && status.pluginCount == 1U &&
                        status.automationLaneCount == 1U && status.renderFrames == 96000U;
        std::cout << "LaMusica headless binding project=\"" << status.projectName
                  << "\" ready=" << (status.firstTrackReady ? "true" : "false")
                  << " editable=" << (status.firstTrackEditable ? "true" : "false")
                  << " mediaReady=" << (status.mediaReady ? "true" : "false")
                  << " tracks=" << status.trackCount << " clips=" << status.clipCount
                  << " plugins=" << status.pluginCount
                  << " automation=" << status.automationLaneCount
                  << " renderFrames=" << status.renderFrames << '\n';
        return ok;
    } catch (const std::exception& error) {
        std::cerr << "LaMusica headless binding failed: " << error.what() << '\n';
        return false;
    }
}

class StatusPanel final : public juce::Component {
  public:
    StatusPanel(juce::String title, juce::Colour background)
        : title_(std::move(title)), background_(background) {
        setAccessible(true);
        setWantsKeyboardFocus(true);
        setTitle(title_);
        setDescription(title_);
        setHelpText(title_);
        label_.setJustificationType(juce::Justification::topLeft);
        label_.setColour(juce::Label::textColourId, juce::Colours::whitesmoke);
        label_.setFont(juce::FontOptions{15.0F, juce::Font::bold});
        label_.setMinimumHorizontalScale(0.8F);
        label_.setAccessible(false);
        addAndMakeVisible(label_);
        setText(title_);
    }

    void setText(const juce::String& text) {
        label_.setText(text, juce::dontSendNotification);
    }

    void setAccessibilityText(const juce::String& name, const juce::String& description) {
        setTitle(name);
        setDescription(description);
        setHelpText(description);
        invalidateAccessibilityHandler();
    }

    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override {
        return std::make_unique<juce::AccessibilityHandler>(*this, juce::AccessibilityRole::group);
    }

    void paint(juce::Graphics& graphics) override {
        graphics.fillAll(background_);
        graphics.setColour(juce::Colour{0xff30363d});
        graphics.drawRect(getLocalBounds());
    }

    void resized() override {
        label_.setBounds(getLocalBounds().reduced(12, 10));
    }

  private:
    juce::String title_;
    juce::Colour background_;
    juce::Label label_;
};

class MainComponent final : public juce::Component, public juce::MenuBarModel {
  public:
    MainComponent() {
        catalog_.loadBundledTables();
        setAccessible(true);
        setWantsKeyboardFocus(true);
        setTitle(tr("LaMusica"));
        setDescription(tr("Main DAW window"));
        setHelpText(tr("Main DAW window"));

        welcome_.setAccessibilityText(tr("onboarding.welcome.title"),
                                      tr("onboarding.welcome.description"));
        transport_.setAccessibilityText(tr("Transport"), tr("Playback and loop controls"));
        browser_.setAccessibilityText(tr("Browser"), tr("Project media and library browser"));
        timeline_.setAccessibilityText(tr("Timeline"), tr("Arrangement timeline"));
        inspector_.setAccessibilityText(tr("Inspector"),
                                        tr("Selected clip and track properties"));
        mixer_.setAccessibilityText(tr("Mixer"), tr("Track levels, pan, meters, and routing"));
        privacy_.setAccessibilityText(tr("Privacy"), tr("privacy.disclosure"));
        tour_.setAccessibilityText(tr("onboarding.tour.transport.title"),
                                   tr("onboarding.tour.transport.body"));

        addAndMakeVisible(menuBar_);
        for (auto* panel : {&welcome_, &transport_, &browser_, &timeline_, &inspector_, &mixer_,
                            &privacy_, &tour_}) {
            addAndMakeVisible(panel);
        }
        configureTemplateButton(emptyTemplateButton_, "empty");
        configureTemplateButton(multitrackTemplateButton_, "basic-multitrack");
        configureTemplateButton(drumSynthTemplateButton_, "drum-synth");
        configureTemplateButton(podcastTemplateButton_, "podcast-voice");

        openRecentButton_.setButtonText(tr("onboarding.welcome.openRecent"));
        openRecentButton_.onClick = [this] { openMostRecentProject(); };
        openRecentButton_.setHelpText(tr("onboarding.welcome.openRecent.help"));
        addAndMakeVisible(openRecentButton_);

        showWelcomeButton_.setButtonText(tr("onboarding.help.showWelcome"));
        showWelcomeButton_.onClick = [this] {
            welcomeVisible_ = true;
            refresh();
        };
        addAndMakeVisible(showWelcomeButton_);

        userManualButton_.setButtonText(tr("onboarding.help.userManual"));
        userManualButton_.onClick = [this] { showUserManual(); };
        addAndMakeVisible(userManualButton_);

        restartTourButton_.setButtonText(tr("onboarding.help.restartTour"));
        restartTourButton_.onClick = [this] { restartGuidedTour(); };
        addAndMakeVisible(restartTourButton_);

        skipTourButton_.setButtonText(tr("onboarding.tour.skip"));
        skipTourButton_.onClick = [this] { dismissGuidedTour(); };
        addAndMakeVisible(skipTourButton_);

        shareDiagnosticsButton_.setButtonText(tr("privacy.shareDiagnostics"));
        shareDiagnosticsButton_.onClick = [this] { setDiagnosticsConsent(true); };
        addAndMakeVisible(shareDiagnosticsButton_);

        keepPrivateButton_.setButtonText(tr("privacy.keepPrivate"));
        keepPrivateButton_.onClick = [this] { setDiagnosticsConsent(false); };
        addAndMakeVisible(keepPrivateButton_);

        setSize(1280, 800);
        refresh();
        juce::MessageManager::callAsync([this] { showFirstRunConsentDialog(); });
    }

    ~MainComponent() override {
        menuBar_.setModel(nullptr);
    }

    void resized() override {
        auto area = getLocalBounds();
        menuBar_.setBounds(area.removeFromTop(24));
        auto welcomeArea = area.removeFromTop(welcomeVisible_ ? 142 : 0);
        welcome_.setBounds(welcomeArea);
        auto welcomeControls = welcomeArea.reduced(12, 58);
        for (auto* button : {&emptyTemplateButton_, &multitrackTemplateButton_,
                             &drumSynthTemplateButton_, &podcastTemplateButton_}) {
            button->setBounds(welcomeControls.removeFromLeft(156));
            welcomeControls.removeFromLeft(8);
        }
        openRecentButton_.setBounds(welcomeControls.removeFromLeft(156));

        transport_.setBounds(area.removeFromTop(72));
        mixer_.setBounds(area.removeFromBottom(160));
        auto privacyArea = area.removeFromBottom(124);
        privacy_.setBounds(privacyArea);
        auto privacyControls = privacyArea.reduced(12, 78);
        keepPrivateButton_.setBounds(privacyControls.removeFromRight(124));
        privacyControls.removeFromRight(8);
        shareDiagnosticsButton_.setBounds(privacyControls.removeFromRight(156));
        restartTourButton_.setBounds(privacyControls.removeFromLeft(150));
        privacyControls.removeFromLeft(8);
        userManualButton_.setBounds(privacyControls.removeFromLeft(170));
        privacyControls.removeFromLeft(8);
        showWelcomeButton_.setBounds(privacyControls.removeFromLeft(170));

        auto tourArea = area.removeFromBottom(tourVisible_ ? 88 : 0);
        tour_.setBounds(tourArea);
        skipTourButton_.setBounds(tourArea.reduced(12, 48).removeFromRight(110));

        browser_.setBounds(area.removeFromLeft(240));
        inspector_.setBounds(area.removeFromRight(300));
        timeline_.setBounds(area);
    }

    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override {
        return std::make_unique<juce::AccessibilityHandler>(*this, juce::AccessibilityRole::group);
    }

    void refresh() {
        const auto& status = session_.status();
        if (!lastError_.isEmpty()) {
            transport_.setText(tr("Transport") + "\n" + lastError_);
            return;
        }

        const auto chooser = lamusica::daw::onboarding::welcomeChooserState(session_);
        welcome_.setVisible(welcomeVisible_);
        for (auto* button : {&emptyTemplateButton_, &multitrackTemplateButton_,
                             &drumSynthTemplateButton_, &podcastTemplateButton_,
                             &openRecentButton_}) {
            button->setVisible(welcomeVisible_);
        }
        openRecentButton_.setEnabled(chooser.canOpenMostRecent);
        welcome_.setText(tr("onboarding.welcome.title") + "\n" +
                         tr("onboarding.welcome.description") + "\n" +
                         tr("onboarding.welcome.recentProjects") + ": " +
                         juce::String{static_cast<int>(chooser.recentProjects.size())});

        if (!status.hasOpenProject) {
            transport_.setText(tr("Transport") + "\n" + tr("status.noProjectOpen"));
            browser_.setText(tr("Browser") + "\n" + tr("onboarding.chooseTemplateOrRecent"));
            timeline_.setText(tr("Timeline") + "\n" + tr("status.noProjectOpen"));
            inspector_.setText(tr("Inspector") + "\n" + tr("status.noSelection"));
            mixer_.setText(tr("Mixer") + "\n" + tr("status.noProjectOpen"));
        } else {
        transport_.setText(juce::String{status.projectName.c_str()} + "\n" + tr("Stopped") +
                           " | " + juce::String{status.tempoBpm, 0} + " BPM | " +
                           juce::String{status.timeSignatureNumerator} + "/" +
                           juce::String{status.timeSignatureDenominator} + " | " +
                           juce::String{status.renderFrames} + " " + tr("status.frames") +
                           "\n" + tr("Loop") + " " +
                           (status.loopEnabled ? tr("status.on") + " " : tr("status.off") + " ") +
                           juce::String{status.loopStartSample} + "-" +
                           juce::String{status.loopEndSample} + " | " + tr("Playhead") + " " +
                           juce::String{status.playheadSample});

        browser_.setText(tr("Browser") + "\n" + tr("status.ready") + ": " +
                         (status.firstTrackReady ? tr("status.yes") : tr("status.no")) +
                         " | " + tr("status.editable") + ": " +
                         (status.firstTrackEditable ? tr("status.yes") : tr("status.no")) +
                         " | " + tr("status.media") + ": " +
                         (status.mediaReady ? tr("status.ok") : tr("status.missing")) + "\n" +
                         tr("status.tracks") + " " +
                         juce::String{static_cast<int>(status.trackCount)} + " | " +
                         tr("status.clips") + " " +
                         juce::String{static_cast<int>(status.clipCount)});

        timeline_.setText(tr("status.timelinePianoRoll") + "\n" +
                          juce::String{status.firstSectionName.c_str()} + " -> " +
                          juce::String{status.finalSectionName.c_str()} + "\n" +
                          tr("status.sections") + " " +
                          juce::String{static_cast<int>(status.markerCount)} + " | " +
                          tr("status.midiNotes") + " " +
                          juce::String{static_cast<int>(status.starterMidiNoteCount)} + " | " +
                          tr("status.bass") + " " +
                          juce::String{status.starterBassTransposeSemitones} + " " +
                          tr("status.semitonesShort"));

        inspector_.setText(
            tr("Inspector") + "\n" + tr("status.starterDevices") + " " +
            juce::String{static_cast<int>(status.pluginCount)} + " | " +
            tr("status.automationLanes") + " " +
            juce::String{static_cast<int>(status.automationLaneCount)} + "\n" +
            tr("status.recordedTakes") + " " +
            juce::String{static_cast<int>(status.recordedTakeCount)} + " | " +
            tr("status.imports") + " " +
            juce::String{static_cast<int>(status.importedAudioClipCount)});

        mixer_.setText(
            tr("Mixer") + "\n" + tr("status.generatedRouting") + "\n" +
            tr("status.lastExport") + " " + juce::String{status.lastMixExportFrames} + " " +
            tr("status.frames") + " | " + tr("status.peak") + " " +
            juce::String{status.lastMixExportPeak, 2} + "\n" + tr("status.packageMix") + " " +
            juce::String{status.lastPackageMixFrames} + " | " + tr("Loop") + " " +
            juce::String{status.lastPackageLoopFrames} + " | " + tr("status.stems") + " " +
            juce::String{static_cast<int>(status.lastPackageStemCount)});
        }

        const auto& preferences = session_.preferences();
        const bool uploadPermitted = lamusica::session::diagnosticsUploadPermitted(
            preferences.diagnosticsConsent, preferences.shareDiagnostics);
        const juce::String endpoint =
            preferences.diagnosticsEndpoint.empty()
                ? tr("privacy.defaultEndpoint")
                : juce::String{preferences.diagnosticsEndpoint.c_str()};
        privacy_.setText(
            tr("Privacy") + "\n" + tr("privacy.diagnostics") + ": " +
            (uploadPermitted ? tr("privacy.sharingCrashReports")
                             : tr("privacy.localCrashLogsOnly")) +
            " | " + tr("privacy.telemetry") + ": " +
            (preferences.telemetryEnabled ? tr("status.on") : tr("status.off")) + " | " +
            tr("privacy.endpoint") + ": " + endpoint + "\n" + tr("privacy.disclosure"));

        tourVisible_ = status.hasOpenProject &&
                       lamusica::daw::onboarding::shouldShowGuidedTour(session_);
        tour_.setVisible(tourVisible_);
        skipTourButton_.setVisible(tourVisible_);
        if (tourVisible_) {
            const auto steps = lamusica::daw::onboarding::guidedTourSteps();
            const auto& step = steps[std::min(tourStepIndex_, steps.size() - 1U)];
            tour_.setText(tr(step.titleKey) + "\n" + tr(step.bodyKey));
        }
        resized();
    }

    juce::StringArray getMenuBarNames() override {
        return {tr("Help")};
    }

    juce::PopupMenu getMenuForIndex(int, const juce::String&) override {
        juce::PopupMenu menu;
        int itemId = 1;
        for (const auto& item : lamusica::daw::onboarding::helpMenuItems()) {
            menu.addItem(itemId++, tr(item.titleKey));
        }
        return menu;
    }

    void menuItemSelected(int menuItemID, int) override {
        if (menuItemID == 1) {
            showUserManual();
        } else if (menuItemID == 2) {
            welcomeVisible_ = true;
            refresh();
        } else if (menuItemID == 3) {
            restartGuidedTour();
        } else if (menuItemID == 4) {
            juce::AlertWindow::showMessageBoxAsync(
                juce::MessageBoxIconType::InfoIcon, tr("onboarding.help.keyboardShortcuts"),
                tr("onboarding.help.keyboardShortcuts.body"));
        }
    }

    juce::String tr(std::string_view key) const {
        return juce::String{catalog_.translate(key)};
    }

    void configureTemplateButton(juce::TextButton& button, std::string templateId) {
        const auto* projectTemplate =
            lamusica::daw::onboarding::findProjectTemplate(templateId);
        button.setButtonText(projectTemplate == nullptr ? juce::String{templateId}
                                                        : tr(projectTemplate->nameKey));
        button.setHelpText(projectTemplate == nullptr ? juce::String{}
                                                      : tr(projectTemplate->descriptionKey));
        button.onClick = [this, templateId = std::move(templateId)] {
            createTemplateProject(templateId);
        };
        addAndMakeVisible(button);
    }

    void createTemplateProject(std::string_view templateId) {
        try {
            const auto* projectTemplate =
                lamusica::daw::onboarding::findProjectTemplate(templateId);
            const auto projectName = projectTemplate == nullptr
                                         ? std::string{templateId}
                                         : std::string{tr(projectTemplate->nameKey).toStdString()};
            const auto path = templateProjectPath(templateId);
            lamusica::daw::onboarding::createProjectFromTemplate(session_, templateId, path,
                                                                 projectName);
            welcomeVisible_ = false;
            lastError_ = {};
        } catch (const std::exception& error) {
            lastError_ = error.what();
        }
        refresh();
    }

    void openMostRecentProject() {
        try {
            lamusica::daw::onboarding::openMostRecentProject(session_);
            welcomeVisible_ = false;
            lastError_ = {};
        } catch (const std::exception& error) {
            lastError_ = error.what();
        }
        refresh();
    }

    void showUserManual() {
        const auto preview =
            lamusica::daw::onboarding::userManualPreview(std::filesystem::current_path());
        juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon,
                                               tr("onboarding.help.userManual"),
                                               juce::String{preview});
    }

    void restartGuidedTour() {
        lamusica::daw::onboarding::markGuidedTourSeen(session_, false);
        tourStepIndex_ = 0;
        refresh();
    }

    void dismissGuidedTour() {
        lamusica::daw::onboarding::markGuidedTourSeen(session_, true);
        refresh();
    }

    void setDiagnosticsConsent(bool granted) {
        auto preferences = session_.preferences();
        preferences.diagnosticsConsent = granted ? lamusica::session::DiagnosticsConsent::Granted
                                                 : lamusica::session::DiagnosticsConsent::Declined;
        preferences.shareDiagnostics = granted;
        preferences.telemetryEnabled = false;
        preferences.diagnosticsEndpoint.clear();
        try {
            session_.setPreferences(preferences);
            lastError_ = {};
        } catch (const std::exception& error) {
            lastError_ = error.what();
        }
        refresh();
    }

    void showFirstRunConsentDialog() {
        if (session_.preferences().diagnosticsConsent !=
            lamusica::session::DiagnosticsConsent::Undecided) {
            return;
        }

        auto options = juce::MessageBoxOptions{}
                           .withIconType(juce::MessageBoxIconType::QuestionIcon)
                           .withTitle(tr("Privacy"))
                           .withMessage(tr("privacy.firstRunPrompt"))
                           .withButton(tr("privacy.shareDiagnostics"))
                           .withButton(tr("privacy.keepPrivate"))
                           .withAssociatedComponent(this);
        juce::AlertWindow::showAsync(options, [this](int result) {
            setDiagnosticsConsent(result == 1);
        });
    }

  private:
    lamusica::session::ApplicationSession session_;
    lamusica::daw::i18n::LocalizationCatalog catalog_;
    juce::String lastError_;
    juce::MenuBarComponent menuBar_{this};
    juce::TooltipWindow tooltipWindow_{this, 700};
    bool welcomeVisible_{true};
    bool tourVisible_{false};
    std::size_t tourStepIndex_{0};
    StatusPanel welcome_{"Welcome", juce::Colour{0xff202124}};
    StatusPanel transport_{"Transport", juce::Colour{0xff202124}};
    StatusPanel browser_{"Browser", juce::Colour{0xff1b1f24}};
    StatusPanel timeline_{"Timeline / Piano Roll", juce::Colour{0xff181a1f}};
    StatusPanel inspector_{"Inspector", juce::Colour{0xff1b1f24}};
    StatusPanel mixer_{"Mixer", juce::Colour{0xff202124}};
    StatusPanel privacy_{"Privacy", juce::Colour{0xff17191d}};
    StatusPanel tour_{"Tour", juce::Colour{0xff202124}};
    juce::TextButton emptyTemplateButton_;
    juce::TextButton multitrackTemplateButton_;
    juce::TextButton drumSynthTemplateButton_;
    juce::TextButton podcastTemplateButton_;
    juce::TextButton openRecentButton_;
    juce::TextButton showWelcomeButton_;
    juce::TextButton userManualButton_;
    juce::TextButton restartTourButton_;
    juce::TextButton skipTourButton_;
    juce::TextButton shareDiagnosticsButton_;
    juce::TextButton keepPrivateButton_;
};

class MainWindow final : public juce::DocumentWindow {
  public:
    explicit MainWindow(juce::String name)
        : DocumentWindow(std::move(name), juce::Colour{0xff202124}, DocumentWindow::allButtons) {
        setUsingNativeTitleBar(true);
        setContentOwned(new MainComponent(), true);
        centreWithSize(getWidth(), getHeight());
        setResizable(true, true);
        setVisible(true);
    }

    void closeButtonPressed() override {
        juce::JUCEApplication::getInstance()->systemRequestedQuit();
    }
};

class LaMusicaApplication final : public juce::JUCEApplication {
  public:
    const juce::String getApplicationName() override {
        return JUCE_APPLICATION_NAME_STRING;
    }

    const juce::String getApplicationVersion() override {
        return JUCE_APPLICATION_VERSION_STRING;
    }

    bool moreThanOneInstanceAllowed() override {
        return true;
    }

    void initialise(const juce::String& commandLine) override {
        lamusica::crash_report::installCrashReporter(
            {.applicationName = "LaMusica", .directory = {}});
        const auto tokens = splitCommandLine(commandLine);
        if (hasVersionArgument(tokens)) {
            printVersion();
            std::exit(0);
        }
        const auto headlessPath = headlessBindingProjectPath(tokens);
        if (!headlessPath.empty()) {
            std::exit(runHeadlessBindingCheck(headlessPath) ? 0 : 1);
        }
        mainWindow_ = std::make_unique<MainWindow>(getApplicationName());
    }

    void shutdown() override {
        mainWindow_.reset();
    }

    void systemRequestedQuit() override {
        quit();
    }

  private:
    std::unique_ptr<MainWindow> mainWindow_;
};

} // namespace

START_JUCE_APPLICATION(LaMusicaApplication)
