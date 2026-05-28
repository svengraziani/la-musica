#pragma once

#include "lamusica/audio/AudioEngine.hpp"
#include "lamusica/audio/Bounce.hpp"
#include "lamusica/audio/Recording.hpp"
#include "lamusica/session/Export.hpp"
#include "lamusica/session/ProjectDocument.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace lamusica::session {

struct ApplicationSessionStatus {
    bool hasOpenProject{false};
    bool dirty{false};
    bool canUndo{false};
    bool canRedo{false};
    std::size_t undoDepth{0};
    std::size_t redoDepth{0};
    std::filesystem::path projectPath;
    std::string projectName{"Untitled"};
    bool firstTrackReady{false};
    bool firstTrackEditable{false};
    bool mediaReady{true};
    std::string mediaError;
    std::string missingMediaAssetId;
    bool starterStructureReady{false};
    bool renderable{false};
    std::size_t trackCount{0};
    std::size_t clipCount{0};
    std::size_t markerCount{0};
    std::size_t pluginCount{0};
    std::size_t automationLaneCount{0};
    std::size_t starterMidiNoteCount{0};
    int starterBassTransposeSemitones{0};
    float drumClipGainDb{0.0F};
    float bassClipGainDb{0.0F};
    bool loopEnabled{false};
    std::int64_t loopStartSample{0};
    std::int64_t loopEndSample{0};
    std::uint32_t loopFrames{0};
    bool transportPlaying{false};
    bool transportRecording{false};
    std::int64_t playheadSample{0};
    std::uint32_t renderFrames{0};
    double tempoBpm{120.0};
    std::uint32_t timeSignatureNumerator{4};
    std::uint32_t timeSignatureDenominator{4};
    std::string firstSectionName;
    std::string finalSectionName;
    std::filesystem::path lastMixExportPath;
    std::uint32_t lastMixExportFrames{0};
    float lastMixExportPeak{0.0F};
    std::filesystem::path lastStemExportDirectory;
    std::size_t lastStemExportCount{0};
    std::uint32_t lastStemExportFrames{0};
    std::filesystem::path lastPackageExportDirectory;
    std::filesystem::path lastPackageManifestPath;
    bool lastPackageVerified{false};
    std::filesystem::path lastPackageVerifiedDirectory;
    std::uint32_t lastPackageMixFrames{0};
    std::uint32_t lastPackageLoopFrames{0};
    std::size_t lastPackageStemCount{0};
    std::filesystem::path lastRecordingPath;
    std::uint32_t lastRecordingFrames{0};
    std::int64_t lastRecordingStartSample{0};
    std::uint32_t lastRecordingCountInSamples{0};
    std::int64_t lastRecordingPrerollStartSample{0};
    bool lastRecordingPunchEnabled{false};
    std::int64_t lastRecordingPunchInSample{0};
    std::int64_t lastRecordingPunchOutSample{0};
    std::string lastRecordingAssetId;
    std::string lastRecordingClipId;
    std::size_t recordedTakeCount{0};
    std::size_t mutedRecordedTakeCount{0};
    std::filesystem::path lastImportPath;
    std::uint32_t lastImportFrames{0};
    std::string lastImportAssetId;
    std::string lastImportClipId;
    std::size_t importedAudioClipCount{0};
};

struct FirstTrackPackageExportResult {
    std::filesystem::path outputDirectory;
    std::filesystem::path manifestPath;
    audio::BounceResult mix;
    audio::BounceResult loop;
    std::vector<StemExportResult> stems;
};

struct FirstTrackPackageVerificationResult {
    std::filesystem::path packageDirectory;
    std::filesystem::path manifestPath;
    std::string projectName;
    std::uint32_t renderFrames{0};
    std::uint32_t loopFrames{0};
    std::size_t stemCount{0};
    std::size_t trackCount{0};
    std::size_t clipCount{0};
    std::filesystem::path projectSnapshotPath;
    std::size_t projectAssetCount{0};
    std::size_t recordedTakeCount{0};
    std::size_t importedAudioClipCount{0};
    bool projectSnapshotVerified{false};
};

struct FirstTrackProjectVerificationResult {
    std::filesystem::path projectPath;
    std::string projectName;
    bool firstTrackReady{false};
    bool firstTrackEditable{false};
    bool mediaReady{false};
    std::string mediaError;
    bool starterStructureReady{false};
    bool renderable{false};
    bool loopEnabled{false};
    std::uint32_t loopFrames{0};
    std::uint32_t renderFrames{0};
    std::size_t trackCount{0};
    std::size_t clipCount{0};
    std::size_t pluginCount{0};
    std::size_t automationLaneCount{0};
    std::size_t starterMidiNoteCount{0};
    std::vector<std::string> missingRequirements;
};

struct FirstTrackRecordingResult {
    audio::CommittedRecording committed;
    std::int64_t timelineStartSample{0};
    std::uint32_t countInSamples{0};
    std::int64_t prerollStartSample{0};
    bool punchEnabled{false};
    std::int64_t punchInSample{0};
    std::int64_t punchOutSample{0};
    std::string trackId;
    std::string assetId;
    std::string clipId;
};

struct FirstTrackRecordingOptions {
    std::uint32_t frames{0};
    std::int64_t startSample{0};
    std::uint32_t countInBars{1};
    std::optional<std::int64_t> punchInSample;
    std::optional<std::int64_t> punchOutSample;
};

struct FirstTrackRecordingPlan {
    std::int64_t timelineStartSample{0};
    std::uint32_t recordFrames{0};
    std::uint32_t countInSamples{0};
    std::uint32_t countInBars{0};
    std::int64_t prerollStartSample{0};
    bool punchEnabled{false};
    std::int64_t punchInSample{0};
    std::int64_t punchOutSample{0};
};

struct FirstTrackTakeSummary {
    std::string clipId;
    std::string assetId;
    std::filesystem::path path;
    std::int64_t startSample{0};
    std::int64_t frames{0};
    std::int64_t fadeInSamples{0};
    std::int64_t fadeOutSamples{0};
    bool reversed{false};
    bool muted{false};
    bool mediaAvailable{false};
};

struct FirstTrackClipSummary {
    std::string clipId;
    std::string trackId;
    std::string trackName;
    ClipType type{ClipType::Audio};
    std::string assetId;
    std::filesystem::path path;
    std::int64_t startSample{0};
    std::int64_t lengthSamples{0};
    std::int64_t sourceOffsetSamples{0};
    std::int64_t fadeInSamples{0};
    std::int64_t fadeOutSamples{0};
    float gainDb{0.0F};
    bool muted{false};
    bool reversed{false};
    bool assetBacked{false};
    bool mediaAvailable{false};
};

struct FirstTrackTrackMixSummary {
    std::string trackId;
    std::string trackName;
    TrackType type{TrackType::Audio};
    float volumeDb{0.0F};
    float pan{0.0F};
    bool muted{false};
    bool solo{false};
};

struct FirstTrackAudioImportResult {
    std::filesystem::path copiedPath;
    std::uint32_t frames{0};
    std::uint32_t channels{0};
    std::string trackId;
    std::string assetId;
    std::string clipId;
};

struct FirstTrackMediaRelinkResult {
    std::filesystem::path copiedPath;
    std::uint32_t frames{0};
    std::uint32_t channels{0};
    std::string assetId;
    bool mediaReady{false};
};

struct FirstTrackClipDuplicateResult {
    std::string sourceClipId;
    std::string clipId;
    std::string trackId;
    std::int64_t startSample{0};
    std::int64_t lengthSamples{0};
    std::string assetId;
};

struct FirstTrackClipRemovalResult {
    std::string clipId;
    std::string trackId;
    std::int64_t startSample{0};
    std::int64_t lengthSamples{0};
    std::string assetId;
    std::size_t removedMidiReferenceCount{0};
};

enum class ApplicationPanel {
    Browser,
    Timeline,
    Inspector,
    Mixer,
    Transport,
};

struct MenuCommandRoute {
    std::string command;
    ApplicationPanel panel{ApplicationPanel::Timeline};
    bool handled{false};
    bool enabled{false};
};

struct KeyboardShortcutPreference {
    std::string command;
    std::string keyEquivalent;
};

struct ApplicationPreferences {
    std::string audioDeviceId;
    std::vector<std::string> enabledMidiInputIds;
    std::vector<std::string> pluginSearchPaths;
    bool mcpEnabled{false};
    bool allowMcpProjectMutation{false};
    std::vector<KeyboardShortcutPreference> keyboardShortcuts;
    bool allowUserFolderScanning{false};
    bool shareDiagnostics{false};
};

class ApplicationSession {
  public:
    [[nodiscard]] const ApplicationSessionStatus& status() const noexcept;
    [[nodiscard]] const ProjectDocument* currentDocument() const noexcept;
    [[nodiscard]] const std::vector<std::filesystem::path>& recentProjects() const noexcept;
    [[nodiscard]] const ApplicationPreferences& preferences() const noexcept;
    [[nodiscard]] ApplicationPanel focusedPanel() const noexcept;

    void createProject(std::filesystem::path path, std::string name);
    void createFirstTrackProject(std::filesystem::path path, std::string name);
    void openProject(std::filesystem::path path);
    void saveProject();
    void setClipGain(std::string_view clipId, float gainDb);
    void setClipFades(std::string_view clipId, std::int64_t fadeInSamples,
                      std::int64_t fadeOutSamples);
    void setClipMuted(std::string_view clipId, bool muted);
    void setClipReversed(std::string_view clipId, bool reversed);
    void setClipTiming(std::string_view clipId, std::int64_t startSample,
                       std::int64_t lengthSamples, std::int64_t sourceOffsetSamples);
    [[nodiscard]] FirstTrackClipDuplicateResult
    duplicateClip(std::string_view clipId, std::string newClipId, std::int64_t startSample);
    [[nodiscard]] FirstTrackClipRemovalResult removeClip(std::string_view clipId);
    void transposeFirstTrackBass(int semitones);
    void setLoopRegion(std::int64_t startSample, std::int64_t endSample);
    void clearLoopRegion();
    void setFirstTrackLoopToIntro();
    void extendFirstTrackArrangementToVerse();
    void play();
    void stop() noexcept;
    void seek(std::int64_t samplePosition);
    [[nodiscard]] audio::RenderedAudio auditionCurrentMixBlock(std::uint32_t frames);
    [[nodiscard]] audio::BounceResult exportCurrentMix(std::filesystem::path outputPath);
    [[nodiscard]] audio::BounceResult exportCurrentMix(ProjectExportOptions options);
    [[nodiscard]] audio::BounceResult exportCurrentLoop(std::filesystem::path outputPath);
    [[nodiscard]] std::vector<StemExportResult>
    exportCurrentStems(std::filesystem::path outputDirectory);
    [[nodiscard]] FirstTrackPackageExportResult
    exportFirstTrackPackage(std::filesystem::path outputDirectory);
    [[nodiscard]] FirstTrackProjectVerificationResult verifyFirstTrackProject() const;
    [[nodiscard]] FirstTrackPackageVerificationResult
    verifyFirstTrackPackage(std::filesystem::path packageDirectory);
    [[nodiscard]] FirstTrackRecordingPlan
    prepareFirstTrackRecording(FirstTrackRecordingOptions options) const;
    [[nodiscard]] FirstTrackRecordingResult recordFirstTrackTake(std::uint32_t frames);
    [[nodiscard]] FirstTrackRecordingResult
    recordFirstTrackTake(FirstTrackRecordingOptions options);
    [[nodiscard]] std::vector<FirstTrackClipSummary> firstTrackClips() const;
    [[nodiscard]] std::vector<FirstTrackTrackMixSummary> firstTrackTrackMix() const;
    void setFirstTrackTrackMix(std::string_view trackId, float volumeDb, float pan, bool muted,
                               bool solo);
    [[nodiscard]] std::vector<FirstTrackTakeSummary> recordedFirstTrackTakes() const;
    void setFirstTrackTakeMuted(std::string_view clipId, bool muted);
    [[nodiscard]] FirstTrackAudioImportResult
    importAudioFileToFirstTrack(std::filesystem::path sourcePath, std::int64_t startSample);
    [[nodiscard]] FirstTrackMediaRelinkResult
    relinkFirstTrackAudioAsset(std::string_view assetId, std::filesystem::path sourcePath);
    [[nodiscard]] bool undoLastEdit();
    [[nodiscard]] bool redoLastEdit();
    void closeProject() noexcept;
    [[nodiscard]] bool recoverLastProject(const std::filesystem::path& path);
    void setPreferences(ApplicationPreferences preferences);
    void setKeyboardShortcut(std::string command, std::string keyEquivalent);
    void focusPanel(ApplicationPanel panel) noexcept;
    [[nodiscard]] MenuCommandRoute routeMenuCommand(std::string_view command);

  private:
    void updateStatus();
    void syncTransportFromProject() noexcept;
    void rememberRecentProject(const std::filesystem::path& path);
    void validatePreferences(const ApplicationPreferences& preferences) const;
    void rememberUndoSnapshot();
    void clearEditHistory() noexcept;
    bool restoreEditSnapshot(std::vector<ProjectManifest>& sourceStack,
                             std::vector<ProjectManifest>& destinationStack);

    std::optional<ProjectDocument> document_;
    audio::AudioEngine engine_{audio::EngineConfig{}};
    ApplicationSessionStatus status_;
    std::vector<std::filesystem::path> recentProjects_;
    ApplicationPreferences preferences_;
    ApplicationPanel focusedPanel_{ApplicationPanel::Timeline};
    std::vector<ProjectManifest> undoSnapshots_;
    std::vector<ProjectManifest> redoSnapshots_;
};

} // namespace lamusica::session
