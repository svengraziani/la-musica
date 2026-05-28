#include "lamusica/session/Assets.hpp"

#include "lamusica/audio/WavFile.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <filesystem>
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

BrowserSectionItem makeAssetBrowserItem(const AssetRecord& asset,
                                        const std::filesystem::path& projectRoot) {
    return {.id = asset.id,
            .path = projectRoot / asset.relativePath,
            .assetKind = asset.kind,
            .favorite = asset.favorite,
            .missing = asset.missing};
}

double estimateTempoBpm(std::span<const std::int64_t> transientSamples, double sampleRate) {
    if (transientSamples.size() < 2 || sampleRate <= 0.0) {
        return 0.0;
    }

    double totalIntervalSamples = 0.0;
    std::size_t intervals = 0;
    for (std::size_t index = 1; index < transientSamples.size(); ++index) {
        const auto interval = transientSamples[index] - transientSamples[index - 1];
        if (interval > 0) {
            totalIntervalSamples += static_cast<double>(interval);
            ++intervals;
        }
    }
    if (intervals == 0) {
        return 0.0;
    }

    auto bpm = 60.0 * sampleRate / (totalIntervalSamples / static_cast<double>(intervals));
    while (bpm < 60.0) {
        bpm *= 2.0;
    }
    while (bpm > 200.0) {
        bpm *= 0.5;
    }
    return bpm;
}

std::string estimateMusicalKey(const audio::RenderedAudio& audio, double sampleRate) {
    if (audio.frames < 3U || sampleRate <= 0.0) {
        return {};
    }

    std::vector<double> risingZeroCrossings;
    risingZeroCrossings.reserve(audio.frames / 32U);
    auto monoAt = [&audio](std::uint32_t frame) {
        double sum = 0.0;
        for (std::uint32_t channel = 0; channel < audio.channels; ++channel) {
            sum +=
                audio
                    .interleavedSamples[static_cast<std::size_t>(frame) * audio.channels + channel];
        }
        return sum / static_cast<double>(audio.channels);
    };

    auto previous = monoAt(0);
    for (std::uint32_t frame = 1; frame < audio.frames; ++frame) {
        const auto current = monoAt(frame);
        if (previous <= 0.0 && current > 0.0) {
            const auto denominator = current - previous;
            const auto fractionalOffset =
                denominator == 0.0 ? 0.0 : std::clamp(-previous / denominator, 0.0, 1.0);
            risingZeroCrossings.push_back(static_cast<double>(frame - 1U) + fractionalOffset);
        }
        previous = current;
    }
    if (risingZeroCrossings.size() < 2) {
        return {};
    }

    const auto periodSamples = (risingZeroCrossings.back() - risingZeroCrossings.front()) /
                               static_cast<double>(risingZeroCrossings.size() - 1U);
    if (periodSamples <= 0.0) {
        return {};
    }

    const auto frequency = sampleRate / periodSamples;
    if (frequency <= 0.0) {
        return {};
    }

    static constexpr std::array<std::string_view, 12> pitchClasses{
        "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    const auto midiNote =
        static_cast<int>(std::lround(69.0 + (12.0 * std::log2(frequency / 440.0))));
    const auto pitchClass = ((midiNote % 12) + 12) % 12;
    return std::string{pitchClasses[static_cast<std::size_t>(pitchClass)]} + " major";
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

bool isSupportedAudioImportExtension(const std::filesystem::path& path) {
    return lowercase(path.extension().string()) == ".wav";
}

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

ImportedAudioAsset importAudioAsset(AssetCatalog& catalog, AudioAssetImportOptions options) {
    if (!isSupportedAudioImportExtension(options.sourcePath)) {
        throw std::runtime_error("Unsupported audio import format: " +
                                 options.sourcePath.extension().string());
    }
    if (!std::filesystem::exists(options.sourcePath)) {
        throw std::runtime_error("Audio import source file was not found");
    }
    if (options.samplesPerWaveformBucket <= 0) {
        throw std::runtime_error("Audio import waveform bucket size must be positive");
    }

    auto plan = planAssetImport(catalog, options.sourcePath, std::move(options.assetId),
                                AssetKind::Audio, std::move(options.tags), options.copyIntoProject);
    const auto wav = audio::readPcm16Wav(plan.sourcePath);
    auto analysis = analyzeAudioAsset(plan.record.id, wav.audio, wav.sampleRate,
                                      options.samplesPerWaveformBucket);

    if (plan.copyIntoProject) {
        if (plan.destinationPath.has_parent_path()) {
            std::filesystem::create_directories(plan.destinationPath.parent_path());
        }
        const auto destinationAlreadyExists = std::filesystem::exists(plan.destinationPath);
        const auto sourceIsDestination =
            destinationAlreadyExists &&
            std::filesystem::equivalent(plan.sourcePath, plan.destinationPath);
        if (!sourceIsDestination) {
            std::filesystem::copy_file(plan.sourcePath, plan.destinationPath,
                                       std::filesystem::copy_options::none);
        }
    }

    plan.record.missing = !std::filesystem::exists(plan.destinationPath);
    addAsset(catalog, plan.record);
    upsertAnalysis(catalog, analysis.analysis);
    upsertWaveform(catalog, analysis.waveform);

    return {.plan = std::move(plan), .analysis = std::move(analysis)};
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
    analysis.tempoBpm = estimateTempoBpm(analysis.transientSamples, sampleRate);
    analysis.musicalKey = estimateMusicalKey(audio, sampleRate);

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

std::vector<BrowserSection> buildBrowserSections(const AssetCatalog& catalog,
                                                 std::size_t recentLimit) {
    std::vector<BrowserSection> sections{
        {.kind = BrowserSectionKind::ProjectMedia},  {.kind = BrowserSectionKind::UserFolders},
        {.kind = BrowserSectionKind::PluginPresets}, {.kind = BrowserSectionKind::DrumKits},
        {.kind = BrowserSectionKind::Templates},     {.kind = BrowserSectionKind::RecentFiles},
    };

    auto& projectMedia = sections[0].items;
    auto& userFolders = sections[1].items;
    auto& pluginPresets = sections[2].items;
    auto& drumKits = sections[3].items;
    auto& templates = sections[4].items;
    auto& recentFiles = sections[5].items;

    for (const auto& asset : catalog.assets) {
        switch (asset.kind) {
        case AssetKind::Audio:
        case AssetKind::Midi:
        case AssetKind::Other:
            projectMedia.push_back(makeAssetBrowserItem(asset, catalog.projectRoot));
            break;
        case AssetKind::Preset:
            pluginPresets.push_back(makeAssetBrowserItem(asset, catalog.projectRoot));
            break;
        case AssetKind::DrumKit:
            drumKits.push_back(makeAssetBrowserItem(asset, catalog.projectRoot));
            break;
        case AssetKind::Template:
            templates.push_back(makeAssetBrowserItem(asset, catalog.projectRoot));
            break;
        }
    }

    for (const auto& grant : catalog.userFolders) {
        userFolders.push_back({.id = grant.id, .path = grant.absolutePath});
    }

    const auto recentCount = std::min(recentLimit, catalog.recentItems.size());
    recentFiles.reserve(recentCount);
    for (std::size_t index = 0; index < recentCount; ++index) {
        const auto& recent = catalog.recentItems[index];
        const auto* asset = findAsset(catalog, recent.id);
        recentFiles.push_back({.id = recent.id,
                               .path = recent.path,
                               .assetKind = asset == nullptr
                                                ? std::optional<AssetKind>{}
                                                : std::optional<AssetKind>{asset->kind},
                               .favorite = asset != nullptr && asset->favorite,
                               .missing = asset != nullptr && asset->missing});
    }

    return sections;
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
