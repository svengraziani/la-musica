#include "lamusica/session/Assets.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace lamusica::session {
namespace {

std::string lowercase(std::string_view value) {
    std::string result;
    result.reserve(value.size());
    for (const auto character : value) {
        result.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(character))));
    }
    return result;
}

bool containsText(std::string_view haystack, std::string_view needle) {
    return lowercase(haystack).find(lowercase(needle)) != std::string::npos;
}

bool assetExists(const AssetCatalog& catalog, std::string_view assetId) {
    return std::ranges::any_of(catalog.assets,
                               [assetId](const AssetRecord& asset) { return asset.id == assetId; });
}

bool assetRelativePathExists(const AssetCatalog& catalog,
                             const std::filesystem::path& relativePath) {
    const auto normalizedRelativePath = relativePath.lexically_normal();
    return std::ranges::any_of(catalog.assets, [&normalizedRelativePath](const AssetRecord& asset) {
        return asset.relativePath.lexically_normal() == normalizedRelativePath;
    });
}

std::filesystem::path normalizedAbsolute(std::filesystem::path path) {
    return std::filesystem::absolute(path).lexically_normal();
}

bool pathHasPrefix(const std::filesystem::path& path, const std::filesystem::path& prefix) {
    const auto normalizedPath = normalizedAbsolute(path);
    const auto normalizedPrefix = normalizedAbsolute(prefix);
    auto pathIt = normalizedPath.begin();
    auto prefixIt = normalizedPrefix.begin();
    for (; prefixIt != normalizedPrefix.end(); ++prefixIt, ++pathIt) {
        if (pathIt == normalizedPath.end() || *pathIt != *prefixIt) {
            return false;
        }
    }
    return true;
}

MediaAnalysisJob* findAnalysisJob(AssetCatalog& catalog, std::string_view jobId) noexcept {
    const auto found = std::ranges::find_if(
        catalog.analysisJobs, [jobId](const MediaAnalysisJob& job) { return job.id == jobId; });
    return found == catalog.analysisJobs.end() ? nullptr : &*found;
}

const UserFolderGrant* findUserFolderGrant(const AssetCatalog& catalog,
                                           std::string_view grantId) noexcept {
    const auto found =
        std::ranges::find_if(catalog.userFolders, [grantId](const UserFolderGrant& grant) {
            return grant.id == grantId;
        });
    return found == catalog.userFolders.end() ? nullptr : &*found;
}

std::filesystem::path uniqueCollectedRelativePath(const AssetCatalog& catalog,
                                                  const std::filesystem::path& filename) {
    if (filename.empty()) {
        throw std::runtime_error("Collected asset filename must not be empty");
    }

    const auto baseDirectory = std::filesystem::path{"Assets"};
    const auto stem = filename.stem().string();
    const auto extension = filename.extension().string();
    auto candidate = baseDirectory / filename;
    for (int suffix = 2; assetRelativePathExists(catalog, candidate) ||
                         std::filesystem::exists(catalog.projectRoot / candidate);
         ++suffix) {
        candidate = baseDirectory / (stem + "-" + std::to_string(suffix) + extension);
    }
    return candidate;
}

} // namespace

AssetRecord* findAsset(AssetCatalog& catalog, std::string_view assetId) noexcept {
    const auto found = std::ranges::find_if(
        catalog.assets, [assetId](const AssetRecord& asset) { return asset.id == assetId; });
    return found == catalog.assets.end() ? nullptr : &*found;
}

const AssetRecord* findAsset(const AssetCatalog& catalog, std::string_view assetId) noexcept {
    const auto found = std::ranges::find_if(
        catalog.assets, [assetId](const AssetRecord& asset) { return asset.id == assetId; });
    return found == catalog.assets.end() ? nullptr : &*found;
}

const AssetAnalysis* findAnalysis(const AssetCatalog& catalog, std::string_view assetId) noexcept {
    const auto found =
        std::ranges::find_if(catalog.analyses, [assetId](const AssetAnalysis& analysis) {
            return analysis.assetId == assetId;
        });
    return found == catalog.analyses.end() ? nullptr : &*found;
}

const WaveformOverview* findWaveform(const AssetCatalog& catalog,
                                     std::string_view assetId) noexcept {
    const auto found =
        std::ranges::find_if(catalog.waveforms, [assetId](const WaveformOverview& waveform) {
            return waveform.assetId == assetId;
        });
    return found == catalog.waveforms.end() ? nullptr : &*found;
}

void addAsset(AssetCatalog& catalog, AssetRecord asset) {
    if (asset.id.empty()) {
        throw std::runtime_error("Asset id must not be empty");
    }
    if (asset.relativePath.empty()) {
        throw std::runtime_error("Asset relative path must not be empty");
    }
    if (assetExists(catalog, asset.id)) {
        throw std::runtime_error("Asset id already exists");
    }
    asset.missing = !std::filesystem::exists(catalog.projectRoot / asset.relativePath);
    catalog.assets.push_back(std::move(asset));
}

AssetImportPlan planAssetImport(const AssetCatalog& catalog, std::filesystem::path sourcePath,
                                std::string assetId, AssetKind kind, std::vector<std::string> tags,
                                bool copyIntoProject) {
    if (assetId.empty()) {
        throw std::runtime_error("Asset import id must not be empty");
    }
    if (sourcePath.empty()) {
        throw std::runtime_error("Asset import source path must not be empty");
    }
    if (assetExists(catalog, assetId)) {
        throw std::runtime_error("Asset import id already exists");
    }

    const auto relativePath =
        copyIntoProject ? uniqueCollectedRelativePath(catalog, sourcePath.filename()) : sourcePath;
    const auto destinationPath = catalog.projectRoot / relativePath;
    return {.record = {.id = std::move(assetId),
                       .relativePath = relativePath,
                       .kind = kind,
                       .tags = std::move(tags),
                       .missing = !std::filesystem::exists(destinationPath)},
            .sourcePath = std::move(sourcePath),
            .destinationPath = destinationPath,
            .copyIntoProject = copyIntoProject};
}

void markMissingAssets(AssetCatalog& catalog) {
    for (auto& asset : catalog.assets) {
        asset.missing = !std::filesystem::exists(catalog.projectRoot / asset.relativePath);
    }
}

void relinkAsset(AssetCatalog& catalog, std::string_view assetId,
                 std::filesystem::path newRelativePath) {
    auto* asset = findAsset(catalog, assetId);
    if (asset == nullptr) {
        throw std::runtime_error("Asset to relink was not found");
    }

    asset->relativePath = std::move(newRelativePath);
    asset->missing = !std::filesystem::exists(catalog.projectRoot / asset->relativePath);
    invalidateAssetAnalysis(catalog, assetId);
}

void upsertAnalysis(AssetCatalog& catalog, AssetAnalysis analysis) {
    if (analysis.assetId.empty()) {
        throw std::runtime_error("Asset analysis id must not be empty");
    }
    if (!assetExists(catalog, analysis.assetId)) {
        throw std::runtime_error("Asset analysis references missing asset");
    }
    const auto found =
        std::ranges::find_if(catalog.analyses, [&analysis](const AssetAnalysis& existing) {
            return existing.assetId == analysis.assetId;
        });
    if (found == catalog.analyses.end()) {
        catalog.analyses.push_back(std::move(analysis));
    } else {
        *found = std::move(analysis);
    }
}

void upsertWaveform(AssetCatalog& catalog, WaveformOverview waveform) {
    if (waveform.assetId.empty()) {
        throw std::runtime_error("Waveform overview asset id must not be empty");
    }
    if (!assetExists(catalog, waveform.assetId)) {
        throw std::runtime_error("Waveform overview references missing asset");
    }
    if (waveform.samplesPerBucket <= 0) {
        throw std::runtime_error("Waveform overview samples per bucket must be positive");
    }

    const auto found =
        std::ranges::find_if(catalog.waveforms, [&waveform](const WaveformOverview& existing) {
            return existing.assetId == waveform.assetId;
        });
    if (found == catalog.waveforms.end()) {
        catalog.waveforms.push_back(std::move(waveform));
    } else {
        *found = std::move(waveform);
    }
}

void invalidateAssetAnalysis(AssetCatalog& catalog, std::string_view assetId) {
    for (auto& waveform : catalog.waveforms) {
        if (waveform.assetId == assetId) {
            waveform.valid = false;
        }
    }
    std::erase_if(catalog.analyses,
                  [assetId](const AssetAnalysis& analysis) { return analysis.assetId == assetId; });
}

MediaAnalysisResult analyzeAudioAsset(std::string assetId, const audio::RenderedAudio& audio,
                                      double sampleRate, std::int64_t samplesPerBucket) {
    if (assetId.empty()) {
        throw std::runtime_error("Media analysis asset id must not be empty");
    }
    if (audio.channels == 0U) {
        throw std::runtime_error("Media analysis requires at least one channel");
    }
    if (audio.frames == 0U) {
        throw std::runtime_error("Media analysis requires at least one frame");
    }
    if (audio.interleavedSamples.size() <
        static_cast<std::size_t>(audio.frames) * static_cast<std::size_t>(audio.channels)) {
        throw std::runtime_error("Media analysis audio buffer is incomplete");
    }
    if (sampleRate <= 0.0) {
        throw std::runtime_error("Media analysis sample rate must be positive");
    }
    if (samplesPerBucket <= 0) {
        throw std::runtime_error("Media analysis samples per bucket must be positive");
    }

    auto analysis = AssetAnalysis{.assetId = assetId,
                                  .durationSamples = static_cast<std::int64_t>(audio.frames),
                                  .channels = audio.channels,
                                  .sampleRate = sampleRate};
    auto waveform =
        WaveformOverview{.assetId = assetId, .samplesPerBucket = samplesPerBucket, .valid = true};

    double squaredTotal = 0.0;
    std::int64_t totalSamples = 0;
    float previousFramePeak = 0.0F;
    bool previousTransientHigh = false;

    for (std::uint32_t frame = 0; frame < audio.frames; ++frame) {
        float framePeak = 0.0F;
        for (std::uint32_t channel = 0; channel < audio.channels; ++channel) {
            const auto sample =
                audio
                    .interleavedSamples[static_cast<std::size_t>(frame) * audio.channels + channel];
            const auto absoluteSample = std::abs(sample);
            framePeak = std::max(framePeak, absoluteSample);
            analysis.peakAmplitude = std::max(analysis.peakAmplitude, absoluteSample);
            squaredTotal += static_cast<double>(sample) * static_cast<double>(sample);
            ++totalSamples;
        }

        const bool transientHigh = framePeak > 0.25F && framePeak > previousFramePeak * 3.0F;
        if (transientHigh && !previousTransientHigh) {
            analysis.transientSamples.push_back(static_cast<std::int64_t>(frame));
        }
        previousTransientHigh = transientHigh;
        previousFramePeak = framePeak;
    }

    analysis.rmsAmplitude =
        totalSamples == 0 ? 0.0F : static_cast<float>(std::sqrt(squaredTotal / totalSamples));
    analysis.loudnessLufs =
        analysis.rmsAmplitude <= std::numeric_limits<float>::epsilon()
            ? -std::numeric_limits<float>::infinity()
            : static_cast<float>(20.0 * std::log10(analysis.rmsAmplitude) - 0.691);

    for (std::int64_t bucketStart = 0; bucketStart < static_cast<std::int64_t>(audio.frames);
         bucketStart += samplesPerBucket) {
        const auto bucketEnd = std::min<std::int64_t>(static_cast<std::int64_t>(audio.frames),
                                                      bucketStart + samplesPerBucket);
        WaveformBucket bucket{.minSample = std::numeric_limits<float>::max(),
                              .maxSample = std::numeric_limits<float>::lowest()};
        double bucketSquares = 0.0;
        std::int64_t bucketSamples = 0;
        for (std::int64_t frame = bucketStart; frame < bucketEnd; ++frame) {
            for (std::uint32_t channel = 0; channel < audio.channels; ++channel) {
                const auto sample =
                    audio.interleavedSamples[static_cast<std::size_t>(frame) * audio.channels +
                                             channel];
                bucket.minSample = std::min(bucket.minSample, sample);
                bucket.maxSample = std::max(bucket.maxSample, sample);
                bucketSquares += static_cast<double>(sample) * static_cast<double>(sample);
                ++bucketSamples;
            }
        }
        bucket.rmsAmplitude = bucketSamples == 0
                                  ? 0.0F
                                  : static_cast<float>(std::sqrt(bucketSquares / bucketSamples));
        waveform.buckets.push_back(bucket);
    }

    return {.analysis = std::move(analysis), .waveform = std::move(waveform)};
}

MediaAnalysisJob scheduleMediaAnalysis(AssetCatalog& catalog, std::string jobId,
                                       std::string assetId) {
    if (jobId.empty()) {
        throw std::runtime_error("Media analysis job id must not be empty");
    }
    if (!assetExists(catalog, assetId)) {
        throw std::runtime_error("Media analysis job references missing asset");
    }
    if (findAnalysisJob(catalog, jobId) != nullptr) {
        throw std::runtime_error("Media analysis job id already exists");
    }

    MediaAnalysisJob job{.id = std::move(jobId), .assetId = std::move(assetId)};
    catalog.analysisJobs.push_back(job);
    return job;
}

void completeMediaAnalysis(AssetCatalog& catalog, std::string_view jobId,
                           MediaAnalysisResult result) {
    auto* job = findAnalysisJob(catalog, jobId);
    if (job == nullptr) {
        throw std::runtime_error("Media analysis job was not found");
    }
    if (result.analysis.assetId != job->assetId || result.waveform.assetId != job->assetId) {
        throw std::runtime_error("Media analysis result asset id does not match job");
    }

    upsertAnalysis(catalog, result.analysis);
    upsertWaveform(catalog, result.waveform);
    job->status = MediaAnalysisJobStatus::Completed;
    job->message = "completed";
    job->result = std::move(result);
}

void failMediaAnalysis(AssetCatalog& catalog, std::string_view jobId, std::string message) {
    auto* job = findAnalysisJob(catalog, jobId);
    if (job == nullptr) {
        throw std::runtime_error("Media analysis job was not found");
    }
    job->status = MediaAnalysisJobStatus::Failed;
    job->message = std::move(message);
    job->result.reset();
}

void grantUserFolder(AssetCatalog& catalog, UserFolderGrant grant) {
    if (grant.id.empty()) {
        throw std::runtime_error("User folder grant id must not be empty");
    }
    if (grant.absolutePath.empty()) {
        throw std::runtime_error("User folder grant path must not be empty");
    }
    grant.absolutePath = normalizedAbsolute(grant.absolutePath);
    const auto found =
        std::ranges::find_if(catalog.userFolders, [&grant](const UserFolderGrant& existing) {
            return existing.id == grant.id;
        });
    if (found == catalog.userFolders.end()) {
        catalog.userFolders.push_back(std::move(grant));
    } else {
        *found = std::move(grant);
    }
}

void revokeUserFolder(AssetCatalog& catalog, std::string_view grantId) {
    std::erase_if(catalog.userFolders,
                  [grantId](const UserFolderGrant& grant) { return grant.id == grantId; });
}

bool isPathInsideGrantedUserFolder(const AssetCatalog& catalog,
                                   const std::filesystem::path& absolutePath) {
    const auto path = normalizedAbsolute(absolutePath);
    return std::ranges::any_of(catalog.userFolders, [&path](const UserFolderGrant& grant) {
        if (grant.recursive) {
            return pathHasPrefix(path, grant.absolutePath);
        }
        return path.parent_path() == grant.absolutePath;
    });
}

UserFolderScanPlan planUserFolderScan(const AssetCatalog& catalog, std::string_view grantId) {
    const auto* grant = findUserFolderGrant(catalog, grantId);
    if (grant == nullptr) {
        throw std::runtime_error("User folder scan requires an explicit folder grant");
    }
    return {
        .grantId = grant->id, .absoluteRoot = grant->absolutePath, .recursive = grant->recursive};
}

void recordRecentBrowserItem(AssetCatalog& catalog, RecentBrowserItem item, std::size_t limit) {
    if (item.id.empty()) {
        throw std::runtime_error("Recent browser item id must not be empty");
    }
    if (item.path.empty()) {
        throw std::runtime_error("Recent browser item path must not be empty");
    }
    std::erase_if(catalog.recentItems, [&item](const RecentBrowserItem& existing) {
        return existing.id == item.id && existing.kind == item.kind;
    });
    catalog.recentItems.push_back(std::move(item));
    std::ranges::sort(catalog.recentItems,
                      [](const RecentBrowserItem& left, const RecentBrowserItem& right) {
                          return left.lastUsedUnixSeconds > right.lastUsedUnixSeconds;
                      });
    if (catalog.recentItems.size() > limit) {
        catalog.recentItems.resize(limit);
    }
}

std::vector<RecentBrowserItem> recentBrowserItems(const AssetCatalog& catalog,
                                                  RecentBrowserItemKind kind, std::size_t limit) {
    std::vector<RecentBrowserItem> items;
    for (const auto& item : catalog.recentItems) {
        if (item.kind == kind) {
            items.push_back(item);
        }
        if (items.size() == limit) {
            break;
        }
    }
    return items;
}

BrowserDropPlan planBrowserDrop(const AssetCatalog& catalog, BrowserDropRequest request) {
    const auto* asset = findAsset(catalog, request.assetId);
    if (asset == nullptr) {
        throw std::runtime_error("Browser drop references missing asset");
    }
    if (asset->missing) {
        throw std::runtime_error("Browser drop asset is missing");
    }
    if (request.targetId.empty()) {
        throw std::runtime_error("Browser drop target id must not be empty");
    }

    BrowserDropPlan plan{.assetId = asset->id,
                         .destination = request.destination,
                         .targetId = request.targetId,
                         .sourcePath = catalog.projectRoot / asset->relativePath,
                         .timelineSample = request.timelineSample};
    switch (request.destination) {
    case BrowserDropDestination::Timeline:
        if (asset->kind != AssetKind::Audio && asset->kind != AssetKind::Midi &&
            asset->kind != AssetKind::Template) {
            throw std::runtime_error("Only audio, MIDI, or template assets can drop to timeline");
        }
        plan.createsProjectClip = true;
        break;
    case BrowserDropDestination::DrumPad:
    case BrowserDropDestination::SamplerSlot:
        if (asset->kind != AssetKind::Audio) {
            throw std::runtime_error("Instrument sample drops require audio assets");
        }
        plan.assignsToInstrument = true;
        break;
    case BrowserDropDestination::PluginArea:
        if (asset->kind != AssetKind::Preset) {
            throw std::runtime_error("Plugin area drops require preset assets");
        }
        plan.opensPluginArea = true;
        break;
    }
    return plan;
}

std::vector<AssetRecord> searchAssets(const AssetCatalog& catalog, std::string_view query) {
    std::vector<AssetRecord> results;
    for (const auto& asset : catalog.assets) {
        if (containsText(asset.id, query) ||
            containsText(asset.relativePath.generic_string(), query) ||
            std::ranges::any_of(
                asset.tags, [query](const std::string& tag) { return containsText(tag, query); })) {
            results.push_back(asset);
        }
    }
    return results;
}

std::vector<AssetRecord> favoriteAssets(const AssetCatalog& catalog) {
    std::vector<AssetRecord> results;
    for (const auto& asset : catalog.assets) {
        if (asset.favorite) {
            results.push_back(asset);
        }
    }
    return results;
}

AssetPreview makeAssetPreview(const AssetCatalog& catalog, std::string_view assetId) {
    const auto* asset = findAsset(catalog, assetId);
    if (asset == nullptr) {
        throw std::runtime_error("Asset preview asset was not found");
    }

    const auto absolutePath = catalog.projectRoot / asset->relativePath;
    const auto* analysis = findAnalysis(catalog, assetId);
    return {.assetId = asset->id,
            .absolutePath = absolutePath,
            .available = std::filesystem::exists(absolutePath),
            .analysis =
                analysis == nullptr ? std::nullopt : std::optional<AssetAnalysis>{*analysis}};
}

std::filesystem::path collectedAssetPath(const AssetCatalog& catalog, const AssetRecord& asset) {
    return catalog.projectRoot /
           uniqueCollectedRelativePath(catalog, asset.relativePath.filename());
}

} // namespace lamusica::session
