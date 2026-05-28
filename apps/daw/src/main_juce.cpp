#include "lamusica/session/ApplicationSession.hpp"

#include <juce_gui_extra/juce_gui_extra.h>

#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>

namespace {

std::filesystem::path uniqueUntitledProjectPath() {
    const auto root = std::filesystem::temp_directory_path();
    const std::string baseName{"LaMusica Untitled"};
    for (int index = 0; index < 1000; ++index) {
        const auto suffix = index == 0 ? std::string{} : " " + std::to_string(index + 1);
        auto candidate = root / (baseName + suffix + ".Project.lamusica");
        if (!std::filesystem::exists(candidate)) {
            return candidate;
        }
    }
    throw std::runtime_error("Unable to find a free untitled project path");
}

class StatusPanel final : public juce::Component {
  public:
    StatusPanel(juce::String title, juce::Colour background)
        : title_(std::move(title)), background_(background) {
        label_.setJustificationType(juce::Justification::topLeft);
        label_.setColour(juce::Label::textColourId, juce::Colours::whitesmoke);
        label_.setFont(juce::FontOptions{15.0F, juce::Font::bold});
        label_.setMinimumHorizontalScale(0.8F);
        addAndMakeVisible(label_);
        setText(title_);
    }

    void setText(const juce::String& text) {
        label_.setText(text, juce::dontSendNotification);
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

class MainComponent final : public juce::Component {
  public:
    MainComponent() {
        createUntitledProject();

        for (auto* panel : {&transport_, &browser_, &timeline_, &inspector_, &mixer_}) {
            addAndMakeVisible(panel);
        }

        setSize(1280, 800);
        refresh();
    }

    void resized() override {
        auto area = getLocalBounds();
        transport_.setBounds(area.removeFromTop(72));
        mixer_.setBounds(area.removeFromBottom(180));

        browser_.setBounds(area.removeFromLeft(240));
        inspector_.setBounds(area.removeFromRight(300));
        timeline_.setBounds(area);
    }

    void createUntitledProject() {
        try {
            session_.createFirstTrackProject(uniqueUntitledProjectPath(), "Untitled");
        } catch (const std::exception& error) {
            lastError_ = error.what();
        }
    }

    void refresh() {
        const auto& status = session_.status();
        if (!lastError_.isEmpty()) {
            transport_.setText("Transport\n" + lastError_);
            return;
        }

        transport_.setText(juce::String{status.projectName.c_str()} + "\nStopped | " +
                           juce::String{status.tempoBpm, 0} + " BPM | " +
                           juce::String{status.timeSignatureNumerator} + "/" +
                           juce::String{status.timeSignatureDenominator} + " | " +
                           juce::String{status.renderFrames} + " frames\nLoop " +
                           (status.loopEnabled ? "on " : "off ") +
                           juce::String{status.loopStartSample} + "-" +
                           juce::String{status.loopEndSample} + " | Playhead " +
                           juce::String{status.playheadSample});

        browser_.setText(juce::String{"Browser\nReady: "} +
                         juce::String{status.firstTrackReady ? "yes" : "no"} +
                         " | editable: " + juce::String{status.firstTrackEditable ? "yes" : "no"} +
                         " | media: " + juce::String{status.mediaReady ? "ok" : "missing"} +
                         "\nTracks " + juce::String{static_cast<int>(status.trackCount)} +
                         " | Clips " + juce::String{static_cast<int>(status.clipCount)});

        timeline_.setText(juce::String{"Timeline / Piano Roll\n"} +
                          juce::String{status.firstSectionName.c_str()} + " -> " +
                          juce::String{status.finalSectionName.c_str()} + "\nSections " +
                          juce::String{static_cast<int>(status.markerCount)} + " | MIDI notes " +
                          juce::String{static_cast<int>(status.starterMidiNoteCount)} + " | Bass " +
                          juce::String{status.starterBassTransposeSemitones} + " st");

        inspector_.setText(
            juce::String{"Inspector\nStarter devices "} +
            juce::String{static_cast<int>(status.pluginCount)} + " | Automation lanes " +
            juce::String{static_cast<int>(status.automationLaneCount)} + "\nRecorded takes " +
            juce::String{static_cast<int>(status.recordedTakeCount)} + " | Imports " +
            juce::String{static_cast<int>(status.importedAudioClipCount)});

        mixer_.setText(
            juce::String{"Mixer\nGenerated Drums -> Master | Generated Bass -> Master\nLast "
                         "export "} +
            juce::String{status.lastMixExportFrames} + " frames | peak " +
            juce::String{status.lastMixExportPeak, 2} + "\nPackage mix " +
            juce::String{status.lastPackageMixFrames} + " | loop " +
            juce::String{status.lastPackageLoopFrames} + " | stems " +
            juce::String{static_cast<int>(status.lastPackageStemCount)});
    }

  private:
    lamusica::session::ApplicationSession session_;
    juce::String lastError_;
    StatusPanel transport_{"Transport", juce::Colour{0xff202124}};
    StatusPanel browser_{"Browser", juce::Colour{0xff1b1f24}};
    StatusPanel timeline_{"Timeline / Piano Roll", juce::Colour{0xff181a1f}};
    StatusPanel inspector_{"Inspector", juce::Colour{0xff1b1f24}};
    StatusPanel mixer_{"Mixer", juce::Colour{0xff202124}};
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

    void initialise(const juce::String&) override {
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
