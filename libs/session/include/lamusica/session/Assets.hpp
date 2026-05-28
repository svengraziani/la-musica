#pragma once

#include "lamusica/audio/AudioEngine.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace lamusica::session {

enum class AssetKind {
    Audio,
    Midi,
    Preset,
    DrumKit,
    Template,
    Other,
};

struct AssetRecord {
    std::string id;
    std::filesystem::path relativePath;
    AssetKind kind{AssetKind::Other};
    std::vector<std::string> tags;
    bool favorite{false};
    bool missing{false};
};

struct AssetAnalysis {
    std::string assetId;
    std::int64_t durationSamples{0};
    std::uint32_t channels{0};
    double sampleRate{0.0};
    float peakAmplitude{0.0F};
    float rmsAmplitude{0.0F};
    float loudnessLufs{0.0F};
    double tempoBpm{0.0};
    std::string musicalKey;
    std::vector<std::int64_t> transientSamples;
};

struct WaveformBucket {
    float minSample{0.0F};
    float maxSample{0.0F};
    float rmsAmplitude{0.0F};
};

struct WaveformOverview {
    std::string assetId;
    std::int64_t samplesPerBucket{0};
    std::vector<WaveformBucket> buckets;
    bool valid{true};
};

struct MediaAnalysisResult {
    AssetAnalysis analysis;
    WaveformOverview waveform;
};

enum class MediaAnalysisJobStatus {
    Pending,
    Completed,
    Failed,
};

struct MediaAnalysisJob {
    std::string id;
    std::string assetId;
    MediaAnalysisJobStatus status{MediaAnalysisJobStatus::Pending};
    std::string message;
    std::optional<MediaAnalysisResult> result;
};

struct UserFolderGrant {
    std::string id;
    std::filesystem::path absolutePath;
    bool recursive{false};
};

struct UserFolderScanPlan {
    std::string grantId;
    std::filesystem::path absoluteRoot;
    bool recursive{false};
};

enum class RecentBrowserItemKind {
    Asset,
    UserFolder,
    PluginPreset,
    DrumKit,
    Template,
};

struct RecentBrowserItem {
    std::string id;
    RecentBrowserItemKind kind{RecentBrowserItemKind::Asset};
    std::filesystem::path path;
    std::int64_t lastUsedUnixSeconds{0};
};

enum class BrowserDropDestination {
    Timeline,
    DrumPad,
    SamplerSlot,
    PluginArea,
};

struct BrowserDropRequest {
    std::string assetId;
    BrowserDropDestination destination{BrowserDropDestination::Timeline};
    std::string targetId;
    std::int64_t timelineSample{0};
};

struct BrowserDropPlan {
    std::string assetId;
    BrowserDropDestination destination{BrowserDropDestination::Timeline};
    std::string targetId;
    std::filesystem::path sourcePath;
    std::int64_t timelineSample{0};
    bool createsProjectClip{false};
    bool assignsToInstrument{false};
    bool opensPluginArea{false};
};

struct AssetCatalog {
    std::filesystem::path projectRoot;
    std::vector<AssetRecord> assets;
    std::vector<AssetAnalysis> analyses;
    std::vector<WaveformOverview> waveforms;
    std::vector<MediaAnalysisJob> analysisJobs;
    std::vector<UserFolderGrant> userFolders;
    std::vector<RecentBrowserItem> recentItems;
};

struct AssetImportPlan {
    AssetRecord record;
    std::filesystem::path sourcePath;
    std::filesystem::path destinationPath;
    bool copyIntoProject{true};
};

struct AudioAssetImportOptions {
    std::filesystem::path sourcePath;
    std::string assetId;
    std::vector<std::string> tags;
    bool copyIntoProject{true};
    std::int64_t samplesPerWaveformBucket{512};
};

struct ImportedAudioAsset {
    AssetImportPlan plan;
    MediaAnalysisResult analysis;
};

struct AssetPreview {
    std::string assetId;
    std::filesystem::path absolutePath;
    bool available{false};
    std::optional<AssetAnalysis> analysis;
};

[[nodiscard]] AssetRecord* findAsset(AssetCatalog& catalog, std::string_view assetId) noexcept;
[[nodiscard]] const AssetRecord* findAsset(const AssetCatalog& catalog,
                                           std::string_view assetId) noexcept;
[[nodiscard]] const AssetAnalysis* findAnalysis(const AssetCatalog& catalog,
                                                std::string_view assetId) noexcept;
[[nodiscard]] const WaveformOverview* findWaveform(const AssetCatalog& catalog,
                                                   std::string_view assetId) noexcept;
void addAsset(AssetCatalog& catalog, AssetRecord asset);
[[nodiscard]] AssetImportPlan planAssetImport(const AssetCatalog& catalog,
                                              std::filesystem::path sourcePath, std::string assetId,
                                              AssetKind kind, std::vector<std::string> tags = {},
                                              bool copyIntoProject = true);
[[nodiscard]] bool isSupportedAudioImportExtension(const std::filesystem::path& path);
[[nodiscard]] ImportedAudioAsset importAudioAsset(AssetCatalog& catalog,
                                                  AudioAssetImportOptions options);
void markMissingAssets(AssetCatalog& catalog);
void relinkAsset(AssetCatalog& catalog, std::string_view assetId,
                 std::filesystem::path newRelativePath);
void upsertAnalysis(AssetCatalog& catalog, AssetAnalysis analysis);
void upsertWaveform(AssetCatalog& catalog, WaveformOverview waveform);
void invalidateAssetAnalysis(AssetCatalog& catalog, std::string_view assetId);
[[nodiscard]] MediaAnalysisResult analyzeAudioAsset(std::string assetId,
                                                    const audio::RenderedAudio& audio,
                                                    double sampleRate,
                                                    std::int64_t samplesPerBucket);
[[nodiscard]] MediaAnalysisJob scheduleMediaAnalysis(AssetCatalog& catalog, std::string jobId,
                                                     std::string assetId);
void completeMediaAnalysis(AssetCatalog& catalog, std::string_view jobId,
                           MediaAnalysisResult result);
void failMediaAnalysis(AssetCatalog& catalog, std::string_view jobId, std::string message);
void grantUserFolder(AssetCatalog& catalog, UserFolderGrant grant);
void revokeUserFolder(AssetCatalog& catalog, std::string_view grantId);
[[nodiscard]] bool isPathInsideGrantedUserFolder(const AssetCatalog& catalog,
                                                 const std::filesystem::path& absolutePath);
[[nodiscard]] UserFolderScanPlan planUserFolderScan(const AssetCatalog& catalog,
                                                    std::string_view grantId);
void recordRecentBrowserItem(AssetCatalog& catalog, RecentBrowserItem item, std::size_t limit = 16);
[[nodiscard]] std::vector<RecentBrowserItem>
recentBrowserItems(const AssetCatalog& catalog, RecentBrowserItemKind kind, std::size_t limit = 16);
[[nodiscard]] BrowserDropPlan planBrowserDrop(const AssetCatalog& catalog,
                                              BrowserDropRequest request);
[[nodiscard]] std::vector<AssetRecord> searchAssets(const AssetCatalog& catalog,
                                                    std::string_view query);
[[nodiscard]] std::vector<AssetRecord> favoriteAssets(const AssetCatalog& catalog);
[[nodiscard]] AssetPreview makeAssetPreview(const AssetCatalog& catalog, std::string_view assetId);
[[nodiscard]] std::filesystem::path collectedAssetPath(const AssetCatalog& catalog,
                                                       const AssetRecord& asset);

} // namespace lamusica::session
