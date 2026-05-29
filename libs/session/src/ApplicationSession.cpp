#include "lamusica/session/ApplicationSession.hpp"

#include "lamusica/audio/WavFile.hpp"
#include "lamusica/session/DiagnosticsScrubber.hpp"
#include "lamusica/session/Assets.hpp"
#include "lamusica/session/StarterProject.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <limits>
#include <optional>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace lamusica::session {
namespace {

constexpr std::string_view recordedTakeTrackId{"recorded-takes"};
constexpr std::string_view recordedTakeTrackName{"Recorded Takes"};
constexpr std::string_view importedAudioTrackId{"imported-audio"};
constexpr std::string_view importedAudioTrackName{"Imported Audio"};
constexpr std::string_view masterTrackId{"master"};
constexpr std::size_t maxApplicationUndoSnapshots{32};

std::uint32_t integralSampleRate(double sampleRate) {
    if (!std::isfinite(sampleRate) || sampleRate <= 0.0 ||
        sampleRate > static_cast<double>(std::numeric_limits<std::uint32_t>::max())) {
        throw std::runtime_error("Project sample rate must be positive, finite, and writable");
    }
    const auto rounded = std::llround(sampleRate);
    if (std::abs(sampleRate - static_cast<double>(rounded)) > 0.000001) {
        throw std::runtime_error("Project sample rate must be an integer WAV rate");
    }
    return static_cast<std::uint32_t>(rounded);
}

struct PackageAudioEntry {
    std::string label;
    std::filesystem::path path;
    std::uint32_t frames{0};
    std::string checksum;
};

struct PackageProjectAssetEntry {
    std::string id;
    std::filesystem::path path;
    std::string checksum;
};

bool pathContainsParentTraversal(const std::filesystem::path& path) {
    return std::ranges::any_of(path,
                               [](const std::filesystem::path& part) { return part == ".."; });
}

std::filesystem::path packageRelativeAudioPath(const std::filesystem::path& outputDirectory,
                                               const std::filesystem::path& audioPath) {
    auto relative = audioPath.lexically_relative(outputDirectory);
    if (relative.empty() || relative.is_absolute() || pathContainsParentTraversal(relative)) {
        throw std::runtime_error("First-track package audio path is outside package directory: " +
                                 audioPath.string());
    }
    return relative;
}

std::filesystem::path resolvePackageAudioPath(const std::filesystem::path& packageDirectory,
                                              const std::filesystem::path& manifestPath) {
    if (manifestPath.empty()) {
        throw std::runtime_error("First-track package manifest contains an empty audio path");
    }
    if (manifestPath.is_absolute()) {
        return manifestPath;
    }
    if (pathContainsParentTraversal(manifestPath)) {
        throw std::runtime_error("First-track package manifest contains an unsafe audio path: " +
                                 manifestPath.string());
    }
    return (packageDirectory / manifestPath).lexically_normal();
}

std::string packageFileChecksum(const std::filesystem::path& path) {
    std::ifstream input{path, std::ios::binary};
    if (!input) {
        throw std::runtime_error("Unable to checksum package file: " + path.string());
    }

    std::uint64_t hash = 14695981039346656037ULL;
    std::array<char, 4096> buffer{};
    while (input) {
        input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const auto read = input.gcount();
        for (std::streamsize index = 0; index < read; ++index) {
            hash ^= static_cast<std::uint8_t>(buffer[static_cast<std::size_t>(index)]);
            hash *= 1099511628211ULL;
        }
    }

    std::ostringstream output;
    output << "fnv1a64:" << std::hex << std::setfill('0') << std::setw(16) << hash;
    return output.str();
}

std::filesystem::path packageProjectAssetPath(const std::filesystem::path& projectSnapshotRoot,
                                              const Asset& asset) {
    if (asset.relativePath.empty() || asset.relativePath.is_absolute() ||
        pathContainsParentTraversal(asset.relativePath)) {
        throw std::runtime_error("First-track project snapshot contains unsafe asset path: " +
                                 asset.relativePath.string());
    }
    return projectSnapshotRoot / asset.relativePath;
}

bool idExists(const std::vector<Asset>& assets, std::string_view id) {
    return std::ranges::any_of(assets, [id](const Asset& asset) { return asset.id == id; });
}

bool idExists(const std::vector<Clip>& clips, std::string_view id) {
    return std::ranges::any_of(clips, [id](const Clip& clip) { return clip.id == id; });
}

std::size_t recordedTakeCount(const ProjectManifest& manifest) {
    return static_cast<std::size_t>(std::ranges::count_if(
        manifest.clips, [](const Clip& clip) { return clip.trackId == recordedTakeTrackId; }));
}

std::size_t mutedRecordedTakeCount(const ProjectManifest& manifest) {
    return static_cast<std::size_t>(std::ranges::count_if(manifest.clips, [](const Clip& clip) {
        return clip.trackId == recordedTakeTrackId && clip.muted;
    }));
}

std::size_t importedAudioClipCount(const ProjectManifest& manifest) {
    return static_cast<std::size_t>(std::ranges::count_if(
        manifest.clips, [](const Clip& clip) { return clip.trackId == importedAudioTrackId; }));
}

const Clip* latestClipOnTrack(const ProjectManifest& manifest, std::string_view trackId) {
    const Clip* latest = nullptr;
    for (const auto& clip : manifest.clips) {
        if (clip.trackId != trackId) {
            continue;
        }
        if (latest == nullptr || clip.startSample > latest->startSample ||
            (clip.startSample == latest->startSample && clip.id > latest->id)) {
            latest = &clip;
        }
    }
    return latest;
}

const Asset* findManifestAsset(const ProjectManifest& manifest, std::string_view assetId) {
    const auto found = std::ranges::find_if(
        manifest.assets, [assetId](const Asset& asset) { return asset.id == assetId; });
    return found == manifest.assets.end() ? nullptr : &*found;
}

Asset* findMutableManifestAsset(ProjectManifest& manifest, std::string_view assetId) {
    const auto found = std::ranges::find_if(
        manifest.assets, [assetId](const Asset& asset) { return asset.id == assetId; });
    return found == manifest.assets.end() ? nullptr : &*found;
}

std::string trackNameForClip(const ProjectManifest& manifest, std::string_view trackId) {
    const auto found = std::ranges::find_if(
        manifest.tracks, [trackId](const Track& track) { return track.id == trackId; });
    return found == manifest.tracks.end() ? std::string{} : found->name;
}

std::string uniqueDuplicateClipId(const ProjectManifest& manifest, std::string_view sourceClipId) {
    std::string base{sourceClipId};
    if (base.empty()) {
        base = "clip";
    }
    for (std::uint32_t index = 1; index < 10000; ++index) {
        const auto candidate = base + "-copy-" + std::to_string(index);
        if (!idExists(manifest.clips, candidate)) {
            return candidate;
        }
    }
    throw std::runtime_error("Unable to allocate a unique duplicate clip id");
}

double firstTempoBpm(const ProjectManifest& manifest) {
    if (manifest.tempoMap.empty() || manifest.tempoMap.front().bpm <= 0.0) {
        return 120.0;
    }
    return manifest.tempoMap.front().bpm;
}

TimeSignatureEvent firstTimeSignature(const ProjectManifest& manifest) {
    if (manifest.timeSignatures.empty() || manifest.timeSignatures.front().numerator == 0 ||
        manifest.timeSignatures.front().denominator == 0) {
        return {};
    }
    return manifest.timeSignatures.front();
}

std::uint32_t countInSamplesForBars(const ProjectManifest& manifest, double sampleRate,
                                    std::uint32_t bars) {
    if (bars == 0) {
        return 0;
    }
    const auto timeSignature = firstTimeSignature(manifest);
    const auto beatsPerBar =
        static_cast<double>(timeSignature.numerator) * (4.0 / timeSignature.denominator);
    const auto secondsPerBar = beatsPerBar * (60.0 / firstTempoBpm(manifest));
    const auto samples = std::llround(secondsPerBar * sampleRate * bars);
    if (samples < 0 || samples > std::numeric_limits<std::uint32_t>::max()) {
        throw std::runtime_error("First-track recording count-in is too long");
    }
    return static_cast<std::uint32_t>(samples);
}

std::uint32_t checkedRecordingFrameCount(std::int64_t frames) {
    if (frames <= 0) {
        throw std::runtime_error("First-track recording frame count must be positive");
    }
    if (frames > std::numeric_limits<std::uint32_t>::max()) {
        throw std::runtime_error("First-track recording frame count is too large");
    }
    return static_cast<std::uint32_t>(frames);
}

bool isFirstTrackEditable(const ProjectManifest& manifest) {
    return inspectFirstTrackReadiness(manifest).firstTrackEditable;
}

std::optional<std::string> firstTrackMediaError(const ProjectManifest& manifest,
                                                const std::filesystem::path& projectRoot) {
    if (projectRoot.empty()) {
        return std::nullopt;
    }
    try {
        (void)compileProjectAudioGraph(manifest, {}, {.projectRoot = projectRoot});
    } catch (const std::exception& error) {
        return std::string{error.what()};
    }
    return std::nullopt;
}

std::string firstMissingMediaAssetId(const ProjectManifest& manifest,
                                     const std::filesystem::path& projectRoot) {
    if (projectRoot.empty()) {
        return {};
    }
    for (const auto& clip : manifest.clips) {
        if (clip.type != ClipType::Audio || clip.assetId.empty()) {
            continue;
        }
        const auto* asset = findManifestAsset(manifest, clip.assetId);
        if (asset == nullptr || asset->relativePath.empty() || asset->relativePath.is_absolute() ||
            pathContainsParentTraversal(asset->relativePath) ||
            !std::filesystem::exists(projectRoot / asset->relativePath)) {
            return clip.assetId;
        }
    }
    return {};
}

std::filesystem::path clipAssetPathForStatus(const ProjectManifest& manifest,
                                             const std::filesystem::path& projectRoot,
                                             const Clip* clip) {
    if (clip == nullptr || clip->assetId.empty() || projectRoot.empty()) {
        return {};
    }
    const auto* asset = findManifestAsset(manifest, clip->assetId);
    if (asset == nullptr || asset->relativePath.empty() || asset->relativePath.is_absolute() ||
        pathContainsParentTraversal(asset->relativePath)) {
        return {};
    }
    return projectRoot / asset->relativePath;
}

std::uint32_t clipFramesForStatus(const Clip* clip) {
    if (clip == nullptr || clip->lengthSamples <= 0) {
        return 0;
    }
    return static_cast<std::uint32_t>(
        std::min<std::int64_t>(clip->lengthSamples, std::numeric_limits<std::uint32_t>::max()));
}

std::string escapeJson(std::string_view value) {
    std::string escaped;
    escaped.reserve(value.size());
    for (const char character : value) {
        if (character == '"' || character == '\\') {
            escaped.push_back('\\');
        }
        escaped.push_back(character);
    }
    return escaped;
}

std::string readTextFile(const std::filesystem::path& path) {
    std::ifstream input{path};
    if (!input) {
        throw std::runtime_error("Unable to read file: " + path.string());
    }
    return {std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
}

std::string requireJsonString(std::string_view json, std::string_view key) {
    const std::regex pattern{"\"" + std::string{key} + "\"\\s*:\\s*\"([^\"]+)\""};
    std::match_results<std::string_view::const_iterator> match;
    if (!std::regex_search(json.begin(), json.end(), match, pattern)) {
        throw std::runtime_error("Package manifest missing string field: " + std::string{key});
    }
    return match[1].str();
}

std::uint32_t requireJsonUint(std::string_view json, std::string_view key) {
    const std::regex pattern{"\"" + std::string{key} + "\"\\s*:\\s*([0-9]+)"};
    std::match_results<std::string_view::const_iterator> match;
    if (!std::regex_search(json.begin(), json.end(), match, pattern)) {
        throw std::runtime_error("Package manifest missing integer field: " + std::string{key});
    }
    return static_cast<std::uint32_t>(std::stoul(match[1].str()));
}

std::string jsonArrayBlock(std::string_view json, std::string_view key) {
    const std::regex pattern{"\"" + std::string{key} + "\"\\s*:\\s*\\["};
    std::match_results<std::string_view::const_iterator> match;
    if (!std::regex_search(json.begin(), json.end(), match, pattern)) {
        throw std::runtime_error("Package manifest missing array field: " + std::string{key});
    }

    const auto blockStart = static_cast<std::size_t>(std::distance(json.begin(), match[0].second));
    std::size_t depth = 1;
    bool inString = false;
    bool escaped = false;
    for (std::size_t index = blockStart; index < json.size(); ++index) {
        const char character = json[index];
        if (inString) {
            if (escaped) {
                escaped = false;
            } else if (character == '\\') {
                escaped = true;
            } else if (character == '"') {
                inString = false;
            }
            continue;
        }

        if (character == '"') {
            inString = true;
        } else if (character == '[') {
            ++depth;
        } else if (character == ']') {
            --depth;
            if (depth == 0) {
                return std::string{json.substr(blockStart, index - blockStart)};
            }
        }
    }

    throw std::runtime_error("Package manifest has an unterminated array field: " +
                             std::string{key});
}

PackageAudioEntry requireNamedPackageAudioEntry(std::string_view json, std::string_view key) {
    const std::regex pattern{"\"" + std::string{key} +
                             "\"\\s*:\\s*\\{[^}]*\"path\"\\s*:\\s*\"([^\"]+)\"[^}]*\"frames\"\\s*:"
                             "\\s*([0-9]+)(?:[^}]*\"checksum\"\\s*:\\s*\"([^\"]+)\")?"};
    std::match_results<std::string_view::const_iterator> match;
    if (!std::regex_search(json.begin(), json.end(), match, pattern)) {
        throw std::runtime_error("Package manifest missing audio entry: " + std::string{key});
    }
    return {.label = std::string{key},
            .path = match[1].str(),
            .frames = static_cast<std::uint32_t>(std::stoul(match[2].str())),
            .checksum = match[3].matched ? match[3].str() : std::string{}};
}

std::vector<PackageAudioEntry> packageStemEntries(std::string_view json) {
    const std::regex pattern{"\\{\\s*\"trackId\"\\s*:\\s*\"([^\"]+)\"\\s*,\\s*\"path\"\\s*:\\s*\"(["
                             "^\"]+)\"\\s*,\\s*\"frames\"\\s*:\\s*([0-9]+)(?:[^}]*\"checksum\"\\s*:"
                             "\\s*\"([^\"]+)\")?"};
    std::vector<PackageAudioEntry> entries;
    for (std::regex_iterator<std::string_view::const_iterator> iterator{json.begin(), json.end(),
                                                                        pattern};
         iterator != std::regex_iterator<std::string_view::const_iterator>{}; ++iterator) {
        entries.push_back(
            {.label = (*iterator)[1].str(),
             .path = (*iterator)[2].str(),
             .frames = static_cast<std::uint32_t>(std::stoul((*iterator)[3].str())),
             .checksum = (*iterator)[4].matched ? (*iterator)[4].str() : std::string{}});
    }
    return entries;
}

std::vector<PackageProjectAssetEntry> packageProjectAssetEntries(std::string_view json) {
    const auto assetBlock = jsonArrayBlock(json, "projectAssets");
    const std::regex pattern{
        "\\{[^}]*\"id\"\\s*:\\s*\"([^\"]+)\"[^}]*\"path\"\\s*:\\s*\"([^\"]+)\"[^}]*"
        "\"checksum\"\\s*:\\s*\"([^\"]+)\"[^}]*\\}"};
    std::vector<PackageProjectAssetEntry> entries;
    for (std::regex_iterator<std::string::const_iterator> iterator{assetBlock.begin(),
                                                                   assetBlock.end(), pattern};
         iterator != std::regex_iterator<std::string::const_iterator>{}; ++iterator) {
        entries.push_back({.id = (*iterator)[1].str(),
                           .path = (*iterator)[2].str(),
                           .checksum = (*iterator)[3].str()});
    }
    return entries;
}

void verifyPackageAudioEntry(const PackageAudioEntry& entry) {
    if (!std::filesystem::exists(entry.path)) {
        throw std::runtime_error("Package audio file missing: " + entry.path.string());
    }
    const auto wav = audio::readPcm16Wav(entry.path);
    if (wav.audio.frames != entry.frames || wav.audio.channels != 2 || wav.bitsPerSample != 16) {
        std::ostringstream message;
        message << "Package audio mismatch for " << entry.label
                << ": expected frames=" << entry.frames << " got frames=" << wav.audio.frames
                << " channels=" << wav.audio.channels << " bits=" << wav.bitsPerSample;
        throw std::runtime_error(message.str());
    }
    if (!entry.checksum.empty() && packageFileChecksum(entry.path) != entry.checksum) {
        throw std::runtime_error("Package audio checksum mismatch for " + entry.label);
    }
}

void requirePackageManifestCondition(bool condition, std::string_view message) {
    if (!condition) {
        throw std::runtime_error("First-track package manifest is inconsistent: " +
                                 std::string{message});
    }
}

void ensureAudioTrackRoutedToMaster(ProjectManifest& manifest, std::string_view trackId,
                                    std::string_view trackName) {
    if (!std::ranges::any_of(manifest.tracks,
                             [trackId](const Track& track) { return track.id == trackId; })) {
        manifest.tracks.push_back(
            {.id = std::string{trackId}, .name = std::string{trackName}, .type = TrackType::Audio});
    }
    if (!std::ranges::any_of(manifest.routing, [trackId](const RoutingConnection& route) {
            return route.sourceTrackId == trackId && route.destinationTrackId == masterTrackId;
        })) {
        manifest.routing.push_back({.sourceTrackId = std::string{trackId},
                                    .destinationTrackId = std::string{masterTrackId}});
    }
}

std::filesystem::path
copyFirstTrackPackageProjectSnapshot(const ProjectManifest& manifest,
                                     const std::filesystem::path& projectRoot,
                                     const std::filesystem::path& outputDirectory) {
    const auto snapshotRoot = outputDirectory / "project";
    const auto snapshotPath = snapshotRoot / "project.json";
    const auto temporarySnapshotPath = snapshotRoot / "project.json.tmp";
    std::filesystem::create_directories(snapshotRoot);

    for (const auto& asset : manifest.assets) {
        const auto sourcePath = projectRoot / asset.relativePath;
        const auto destinationPath = packageProjectAssetPath(snapshotRoot, asset);
        std::filesystem::create_directories(destinationPath.parent_path());
        std::filesystem::copy_file(sourcePath, destinationPath,
                                   std::filesystem::copy_options::overwrite_existing);
    }

    {
        std::ofstream snapshotFile{temporarySnapshotPath, std::ios::trunc};
        if (!snapshotFile) {
            throw std::runtime_error("Unable to write first-track project snapshot: " +
                                     temporarySnapshotPath.string());
        }
        snapshotFile << serializeProjectManifest(manifest);
        snapshotFile.flush();
        if (!snapshotFile) {
            throw std::runtime_error("Unable to flush first-track project snapshot: " +
                                     temporarySnapshotPath.string());
        }
    }

    std::error_code renameError;
    std::filesystem::rename(temporarySnapshotPath, snapshotPath, renameError);
    if (renameError) {
        std::filesystem::remove(temporarySnapshotPath);
        throw std::runtime_error("Unable to write first-track project snapshot: " +
                                 snapshotPath.string());
    }
    return snapshotPath;
}

std::filesystem::path writeFirstTrackPackageManifest(
    const ProjectManifest& manifest, const std::filesystem::path& projectPath,
    const std::filesystem::path& outputDirectory, const std::filesystem::path& projectSnapshotPath,
    const audio::BounceResult& mix, const audio::BounceResult& loop,
    const std::vector<StemExportResult>& stems) {
    const auto manifestPath = outputDirectory / "first-track-package.json";
    const auto temporaryManifestPath = outputDirectory / "first-track-package.json.tmp";
    std::filesystem::create_directories(outputDirectory);

    std::ostringstream output;
    output << "{\n";
    output << "  \"schemaVersion\": 1,\n";
    output << "  \"kind\": \"lamusica.firstTrackPackage\",\n";
    output << "  \"projectName\": \"" << escapeJson(manifest.name) << "\",\n";
    output << "  \"projectPath\": \"" << escapeJson(projectPath.generic_string()) << "\",\n";
    output << "  \"projectSnapshotPath\": \""
           << escapeJson(
                  packageRelativeAudioPath(outputDirectory, projectSnapshotPath).generic_string())
           << "\",\n";
    output << "  \"projectAssetCount\": " << manifest.assets.size() << ",\n";
    output << "  \"projectAssets\": [\n";
    const auto projectSnapshotRoot = projectSnapshotPath.parent_path();
    for (std::size_t index = 0; index < manifest.assets.size(); ++index) {
        const auto& asset = manifest.assets[index];
        const auto assetPath = packageProjectAssetPath(projectSnapshotRoot, asset);
        output << "    {\"id\": \"" << escapeJson(asset.id) << "\", \"path\": \""
               << escapeJson(asset.relativePath.generic_string()) << "\", \"checksum\": \""
               << packageFileChecksum(assetPath) << "\"}";
        if (index + 1U < manifest.assets.size()) {
            output << ',';
        }
        output << '\n';
    }
    output << "  ],\n";
    output << "  \"sampleRate\": " << integralSampleRate(manifest.projectSampleRate) << ",\n";
    output << "  \"renderFrames\": " << renderableArrangementFrames(manifest) << ",\n";
    output << "  \"loopStartSample\": " << manifest.loopStartSample << ",\n";
    output << "  \"loopEndSample\": " << manifest.loopEndSample << ",\n";
    output << "  \"trackCount\": " << manifest.tracks.size() << ",\n";
    output << "  \"clipCount\": " << manifest.clips.size() << ",\n";
    output << "  \"recordedTakeCount\": " << recordedTakeCount(manifest) << ",\n";
    output << "  \"importedAudioClipCount\": " << importedAudioClipCount(manifest) << ",\n";
    output << "  \"mix\": {\"path\": \""
           << escapeJson(packageRelativeAudioPath(outputDirectory, mix.outputPath).generic_string())
           << "\", \"frames\": " << mix.frames << ", \"peak\": " << mix.peakAfterNormalization
           << ", \"checksum\": \"" << packageFileChecksum(mix.outputPath) << "\"},\n";
    output << "  \"loop\": {\"path\": \""
           << escapeJson(
                  packageRelativeAudioPath(outputDirectory, loop.outputPath).generic_string())
           << "\", \"frames\": " << loop.frames << ", \"peak\": " << loop.peakAfterNormalization
           << ", \"checksum\": \"" << packageFileChecksum(loop.outputPath) << "\"},\n";
    output << "  \"stems\": [\n";
    for (std::size_t index = 0; index < stems.size(); ++index) {
        const auto& stem = stems[index];
        output << "    {\"trackId\": \"" << escapeJson(stem.trackId) << "\", \"path\": \""
               << escapeJson(packageRelativeAudioPath(outputDirectory, stem.bounce.outputPath)
                                 .generic_string())
               << "\", \"frames\": " << stem.bounce.frames
               << ", \"peak\": " << stem.bounce.peakAfterNormalization << ", \"checksum\": \""
               << packageFileChecksum(stem.bounce.outputPath) << "\"}";
        if (index + 1U < stems.size()) {
            output << ',';
        }
        output << '\n';
    }
    output << "  ]\n";
    output << "}\n";

    {
        std::ofstream manifestFile{temporaryManifestPath, std::ios::trunc};
        if (!manifestFile) {
            throw std::runtime_error("Unable to write first-track package manifest: " +
                                     temporaryManifestPath.string());
        }
        manifestFile << output.str();
        manifestFile.flush();
        if (!manifestFile) {
            throw std::runtime_error("Unable to flush first-track package manifest: " +
                                     temporaryManifestPath.string());
        }
    }

    std::error_code renameError;
    std::filesystem::rename(temporaryManifestPath, manifestPath, renameError);
    if (renameError) {
        std::filesystem::remove(temporaryManifestPath);
        throw std::runtime_error("Unable to write first-track package manifest: " +
                                 manifestPath.string());
    }
    return manifestPath;
}

} // namespace

const ApplicationSessionStatus& ApplicationSession::status() const noexcept {
    return status_;
}

const ProjectDocument* ApplicationSession::currentDocument() const noexcept {
    return document_.has_value() ? &*document_ : nullptr;
}

const std::vector<std::filesystem::path>& ApplicationSession::recentProjects() const noexcept {
    return recentProjects_;
}

const ApplicationPreferences& ApplicationSession::preferences() const noexcept {
    return preferences_;
}

ApplicationPanel ApplicationSession::focusedPanel() const noexcept {
    return focusedPanel_;
}

void ApplicationSession::createProject(std::filesystem::path path, std::string name) {
    clearEditHistory();
    document_.emplace(ProjectDocument::createEmpty(std::move(path), std::move(name)));
    rememberRecentProject(document_->path());
    syncTransportFromProject();
    updateStatus();
}

void ApplicationSession::createFirstTrackProject(std::filesystem::path path, std::string name) {
    clearEditHistory();
    document_.emplace(
        ProjectDocument::create(std::move(path), makeFirstTrackStarterManifest(std::move(name))));
    rememberRecentProject(document_->path());
    syncTransportFromProject();
    updateStatus();
}

void ApplicationSession::openProject(std::filesystem::path path) {
    clearEditHistory();
    document_.emplace(ProjectDocument::open(std::move(path)));
    rememberRecentProject(document_->path());
    syncTransportFromProject();
    updateStatus();
}

void ApplicationSession::saveProject() {
    if (document_.has_value()) {
        document_->save();
    }
    updateStatus();
}

void ApplicationSession::addTrack(Track track) {
    if (!document_.has_value() || !document_->isOpen()) {
        throw std::runtime_error("Cannot add track without an open project");
    }
    rememberUndoSnapshot();
    auto& manifest = document_->mutableManifest();
    manifest.tracks.push_back(std::move(track));
    validateProjectManifest(manifest);
    updateStatus();
}

void ApplicationSession::addRoutingConnection(RoutingConnection route) {
    if (!document_.has_value() || !document_->isOpen()) {
        throw std::runtime_error("Cannot add routing without an open project");
    }
    rememberUndoSnapshot();
    auto& manifest = document_->mutableManifest();
    manifest.routing.push_back(std::move(route));
    validateProjectManifest(manifest);
    updateStatus();
}

void ApplicationSession::addMarker(Marker marker) {
    if (!document_.has_value() || !document_->isOpen()) {
        throw std::runtime_error("Cannot add marker without an open project");
    }
    rememberUndoSnapshot();
    auto& manifest = document_->mutableManifest();
    manifest.markers.push_back(std::move(marker));
    validateProjectManifest(manifest);
    updateStatus();
}

void ApplicationSession::setTempo(double bpm) {
    if (!document_.has_value() || !document_->isOpen()) {
        throw std::runtime_error("Cannot set tempo without an open project");
    }
    rememberUndoSnapshot();
    auto& manifest = document_->mutableManifest();
    manifest.tempoMap = {{.samplePosition = 0, .bpm = bpm}};
    validateProjectManifest(manifest);
    updateStatus();
}

void ApplicationSession::setTimeSignature(std::uint32_t numerator,
                                          std::uint32_t denominator) {
    if (!document_.has_value() || !document_->isOpen()) {
        throw std::runtime_error("Cannot set time signature without an open project");
    }
    rememberUndoSnapshot();
    auto& manifest = document_->mutableManifest();
    manifest.timeSignatures = {
        {.samplePosition = 0, .numerator = numerator, .denominator = denominator}};
    validateProjectManifest(manifest);
    updateStatus();
}

void ApplicationSession::createAudioClip(Clip clip) {
    if (!document_.has_value() || !document_->isOpen()) {
        throw std::runtime_error("Cannot create audio clip without an open project");
    }
    if (clip.type != ClipType::Audio) {
        throw std::runtime_error("createAudioClip requires an audio clip");
    }
    rememberUndoSnapshot();
    auto& manifest = document_->mutableManifest();
    manifest.clips.push_back(std::move(clip));
    validateProjectManifest(manifest);
    updateStatus();
}

void ApplicationSession::createMidiClip(Clip clip, MidiClipReference reference) {
    if (!document_.has_value() || !document_->isOpen()) {
        throw std::runtime_error("Cannot create MIDI clip without an open project");
    }
    if (clip.type != ClipType::Midi) {
        throw std::runtime_error("createMidiClip requires a MIDI clip");
    }
    if (reference.clipId.empty()) {
        reference.clipId = clip.id;
    }
    rememberUndoSnapshot();
    auto& manifest = document_->mutableManifest();
    manifest.clips.push_back(std::move(clip));
    manifest.midiClips.push_back(std::move(reference));
    validateProjectManifest(manifest);
    updateStatus();
}

void ApplicationSession::addPlugin(PluginReference plugin) {
    if (!document_.has_value() || !document_->isOpen()) {
        throw std::runtime_error("Cannot add plugin without an open project");
    }
    rememberUndoSnapshot();
    auto& manifest = document_->mutableManifest();
    manifest.plugins.push_back(std::move(plugin));
    validateProjectManifest(manifest);
    updateStatus();
}

void ApplicationSession::addAutomationLane(AutomationLane lane) {
    if (!document_.has_value() || !document_->isOpen()) {
        throw std::runtime_error("Cannot add automation without an open project");
    }
    rememberUndoSnapshot();
    auto& manifest = document_->mutableManifest();
    manifest.automation.push_back(std::move(lane));
    validateProjectManifest(manifest);
    updateStatus();
}

void ApplicationSession::setClipGain(std::string_view clipId, float gainDb) {
    if (!document_.has_value() || !document_->isOpen()) {
        throw std::runtime_error("Cannot edit clip gain without an open project");
    }
    if (clipId.empty()) {
        throw std::runtime_error("Clip gain edit requires a clip id");
    }
    if (!std::isfinite(gainDb) || gainDb < -60.0F || gainDb > 24.0F) {
        throw std::runtime_error("Clip gain must be finite and between -60 and 24 dB");
    }

    auto& manifest = document_->mutableManifest();
    auto found = std::ranges::find_if(manifest.clips,
                                      [clipId](const Clip& clip) { return clip.id == clipId; });
    if (found == manifest.clips.end()) {
        throw std::runtime_error("Open project does not contain clip: " + std::string{clipId});
    }

    rememberUndoSnapshot();
    found->gainDb = gainDb;
    document_->save();
    updateStatus();
    status_.dirty = false;
}

void ApplicationSession::setClipFades(std::string_view clipId, std::int64_t fadeInSamples,
                                      std::int64_t fadeOutSamples) {
    if (!document_.has_value() || !document_->isOpen()) {
        throw std::runtime_error("Cannot edit clip fades without an open project");
    }
    if (clipId.empty()) {
        throw std::runtime_error("Clip fade edit requires a clip id");
    }
    if (fadeInSamples < 0 || fadeOutSamples < 0) {
        throw std::runtime_error("Clip fade lengths must not be negative");
    }

    auto& manifest = document_->mutableManifest();
    auto found = std::ranges::find_if(manifest.clips,
                                      [clipId](const Clip& clip) { return clip.id == clipId; });
    if (found == manifest.clips.end()) {
        throw std::runtime_error("Open project does not contain clip: " + std::string{clipId});
    }
    if (found->lengthSamples < 0 || fadeInSamples + fadeOutSamples > found->lengthSamples) {
        throw std::runtime_error("Clip fades must fit within the clip length");
    }
    if (found->fadeInSamples == fadeInSamples && found->fadeOutSamples == fadeOutSamples) {
        updateStatus();
        return;
    }

    rememberUndoSnapshot();
    found->fadeInSamples = fadeInSamples;
    found->fadeOutSamples = fadeOutSamples;
    document_->save();
    updateStatus();
    status_.dirty = false;
}

void ApplicationSession::setClipMuted(std::string_view clipId, bool muted) {
    if (!document_.has_value() || !document_->isOpen()) {
        throw std::runtime_error("Cannot edit clip mute state without an open project");
    }
    if (clipId.empty()) {
        throw std::runtime_error("Clip mute edit requires a clip id");
    }

    auto& manifest = document_->mutableManifest();
    auto found = std::ranges::find_if(manifest.clips,
                                      [clipId](const Clip& clip) { return clip.id == clipId; });
    if (found == manifest.clips.end()) {
        throw std::runtime_error("Open project does not contain clip: " + std::string{clipId});
    }
    if (found->muted == muted) {
        updateStatus();
        return;
    }

    rememberUndoSnapshot();
    found->muted = muted;
    document_->save();
    updateStatus();
    status_.dirty = false;
}

void ApplicationSession::setClipReversed(std::string_view clipId, bool reversed) {
    if (!document_.has_value() || !document_->isOpen()) {
        throw std::runtime_error("Cannot edit clip reverse state without an open project");
    }
    if (clipId.empty()) {
        throw std::runtime_error("Clip reverse edit requires a clip id");
    }

    auto& manifest = document_->mutableManifest();
    auto found = std::ranges::find_if(manifest.clips,
                                      [clipId](const Clip& clip) { return clip.id == clipId; });
    if (found == manifest.clips.end()) {
        throw std::runtime_error("Open project does not contain clip: " + std::string{clipId});
    }
    if (found->reversed == reversed) {
        updateStatus();
        return;
    }

    rememberUndoSnapshot();
    found->reversed = reversed;
    document_->save();
    updateStatus();
    status_.dirty = false;
}

void ApplicationSession::setClipTiming(std::string_view clipId, std::int64_t startSample,
                                       std::int64_t lengthSamples,
                                       std::int64_t sourceOffsetSamples) {
    if (!document_.has_value() || !document_->isOpen()) {
        throw std::runtime_error("Cannot edit clip timing without an open project");
    }
    if (clipId.empty()) {
        throw std::runtime_error("Clip timing edit requires a clip id");
    }
    if (startSample < 0 || lengthSamples <= 0 || sourceOffsetSamples < 0) {
        throw std::runtime_error(
            "Clip timing requires a non-negative start/source offset and positive length");
    }
    if (sourceOffsetSamples > std::numeric_limits<std::int64_t>::max() - lengthSamples) {
        throw std::runtime_error("Clip timing source range is too large");
    }

    auto& manifest = document_->mutableManifest();
    auto found = std::ranges::find_if(manifest.clips,
                                      [clipId](const Clip& clip) { return clip.id == clipId; });
    if (found == manifest.clips.end()) {
        throw std::runtime_error("Open project does not contain clip: " + std::string{clipId});
    }
    if (found->fadeInSamples + found->fadeOutSamples > lengthSamples) {
        throw std::runtime_error("Clip timing length must be long enough for existing fades");
    }
    if (found->type == ClipType::Audio && !found->assetId.empty()) {
        const auto* asset = findManifestAsset(manifest, found->assetId);
        if (asset == nullptr || asset->relativePath.empty() || asset->relativePath.is_absolute() ||
            pathContainsParentTraversal(asset->relativePath)) {
            throw std::runtime_error("Clip timing cannot resolve audio asset: " + found->assetId);
        }
        const auto wav = audio::readPcm16Wav(document_->path() / asset->relativePath);
        if (sourceOffsetSamples + lengthSamples > static_cast<std::int64_t>(wav.audio.frames)) {
            throw std::runtime_error("Clip timing source range exceeds audio asset length");
        }
    }
    if (found->startSample == startSample && found->lengthSamples == lengthSamples &&
        found->sourceOffsetSamples == sourceOffsetSamples) {
        updateStatus();
        return;
    }

    rememberUndoSnapshot();
    found->startSample = startSample;
    found->lengthSamples = lengthSamples;
    found->sourceOffsetSamples = sourceOffsetSamples;
    document_->save();
    syncTransportFromProject();
    updateStatus();
    status_.dirty = false;
}

FirstTrackClipDuplicateResult ApplicationSession::duplicateClip(std::string_view clipId,
                                                                std::string newClipId,
                                                                std::int64_t startSample) {
    if (!document_.has_value() || !document_->isOpen()) {
        throw std::runtime_error("Cannot duplicate clip without an open project");
    }
    if (clipId.empty()) {
        throw std::runtime_error("Clip duplicate edit requires a source clip id");
    }
    if (startSample < 0) {
        throw std::runtime_error("Clip duplicate start sample must be non-negative");
    }

    auto& manifest = document_->mutableManifest();
    const auto found = std::ranges::find_if(
        manifest.clips, [clipId](const Clip& clip) { return clip.id == clipId; });
    if (found == manifest.clips.end()) {
        throw std::runtime_error("Open project does not contain clip: " + std::string{clipId});
    }
    if (newClipId.empty()) {
        newClipId = uniqueDuplicateClipId(manifest, clipId);
    }
    if (newClipId == clipId || idExists(manifest.clips, newClipId)) {
        throw std::runtime_error("Duplicate clip id is already in use: " + newClipId);
    }

    auto duplicate = *found;
    duplicate.id = newClipId;
    duplicate.startSample = startSample;

    std::vector<MidiClipReference> duplicateMidiReferences;
    for (const auto& reference : manifest.midiClips) {
        if (reference.clipId == clipId) {
            auto copiedReference = reference;
            copiedReference.clipId = newClipId;
            duplicateMidiReferences.push_back(std::move(copiedReference));
        }
    }

    rememberUndoSnapshot();
    manifest.clips.push_back(duplicate);
    manifest.midiClips.insert(manifest.midiClips.end(), duplicateMidiReferences.begin(),
                              duplicateMidiReferences.end());
    document_->save();
    syncTransportFromProject();
    updateStatus();
    status_.dirty = false;

    return {.sourceClipId = std::string{clipId},
            .clipId = std::move(newClipId),
            .trackId = duplicate.trackId,
            .startSample = duplicate.startSample,
            .lengthSamples = duplicate.lengthSamples,
            .assetId = duplicate.assetId};
}

FirstTrackClipRemovalResult ApplicationSession::removeClip(std::string_view clipId) {
    if (!document_.has_value() || !document_->isOpen()) {
        throw std::runtime_error("Cannot remove clip without an open project");
    }
    if (clipId.empty()) {
        throw std::runtime_error("Clip removal requires a clip id");
    }

    auto& manifest = document_->mutableManifest();
    const auto found = std::ranges::find_if(
        manifest.clips, [clipId](const Clip& clip) { return clip.id == clipId; });
    if (found == manifest.clips.end()) {
        throw std::runtime_error("Open project does not contain clip: " + std::string{clipId});
    }

    const FirstTrackClipRemovalResult removed{
        .clipId = found->id,
        .trackId = found->trackId,
        .startSample = found->startSample,
        .lengthSamples = found->lengthSamples,
        .assetId = found->assetId,
        .removedMidiReferenceCount = static_cast<std::size_t>(
            std::ranges::count_if(manifest.midiClips, [clipId](const MidiClipReference& reference) {
                return reference.clipId == clipId;
            }))};

    rememberUndoSnapshot();
    manifest.clips.erase(found);
    std::erase_if(manifest.midiClips, [clipId](const MidiClipReference& reference) {
        return reference.clipId == clipId;
    });
    document_->save();
    syncTransportFromProject();
    updateStatus();
    status_.dirty = false;
    return removed;
}

void ApplicationSession::transposeFirstTrackBass(int semitones) {
    if (!document_.has_value() || !document_->isOpen()) {
        throw std::runtime_error("Cannot transpose first-track bass without an open project");
    }
    if (semitones < -24 || semitones > 24) {
        throw std::runtime_error("First-track bass transpose must be between -24 and 24 semitones");
    }

    auto& manifest = document_->mutableManifest();
    auto found = std::ranges::find_if(manifest.midiClips, [](const MidiClipReference& reference) {
        return reference.clipId == "bass-pattern" && reference.dataId == "starter-bass-midi";
    });
    if (found == manifest.midiClips.end()) {
        throw std::runtime_error("Open project does not contain editable first-track bass MIDI");
    }

    rememberUndoSnapshot();
    found->transposeSemitones = semitones;
    document_->save();
    updateStatus();
    status_.dirty = false;
}

void ApplicationSession::setLoopRegion(std::int64_t startSample, std::int64_t endSample) {
    if (!document_.has_value() || !document_->isOpen()) {
        throw std::runtime_error("Cannot set loop without an open project");
    }
    if (startSample < 0 || endSample <= startSample) {
        throw std::runtime_error("Loop range must be non-empty and non-negative");
    }
    if (endSample - startSample > std::numeric_limits<std::uint32_t>::max()) {
        throw std::runtime_error("Loop range is too large for export");
    }
    rememberUndoSnapshot();
    auto& manifest = document_->mutableManifest();
    manifest.loopEnabled = true;
    manifest.loopStartSample = startSample;
    manifest.loopEndSample = endSample;
    document_->save();
    syncTransportFromProject();
    updateStatus();
    status_.dirty = false;
}

void ApplicationSession::clearLoopRegion() {
    if (!document_.has_value() || !document_->isOpen()) {
        throw std::runtime_error("Cannot clear loop without an open project");
    }
    rememberUndoSnapshot();
    auto& manifest = document_->mutableManifest();
    manifest.loopEnabled = false;
    manifest.loopStartSample = 0;
    manifest.loopEndSample = 0;
    document_->save();
    syncTransportFromProject();
    updateStatus();
    status_.dirty = false;
}

void ApplicationSession::setFirstTrackLoopToIntro() {
    if (!document_.has_value() || !document_->isOpen()) {
        throw std::runtime_error("Cannot set first-track loop without an open project");
    }
    const auto& manifest = document_->manifest();
    if (!isFirstTrackEditable(manifest)) {
        throw std::runtime_error("Open project is not a first-track starter arrangement");
    }
    const auto intro = std::ranges::find_if(manifest.markers, [](const Marker& marker) {
        return marker.name == "Intro" || marker.id == "section-intro";
    });
    if (intro == manifest.markers.end()) {
        throw std::runtime_error("Open project does not contain an Intro section marker");
    }
    auto endSample = arrangementEndSample(manifest);
    for (const auto& marker : manifest.markers) {
        if (marker.samplePosition > intro->samplePosition) {
            endSample = std::min(endSample, marker.samplePosition);
        }
    }
    setLoopRegion(intro->samplePosition, endSample);
}

void ApplicationSession::extendFirstTrackArrangementToVerse() {
    if (!document_.has_value() || !document_->isOpen()) {
        throw std::runtime_error("Cannot extend first-track arrangement without an open project");
    }
    auto& manifest = document_->mutableManifest();
    if (!isFirstTrackEditable(manifest)) {
        throw std::runtime_error("Open project is not a first-track starter arrangement");
    }

    constexpr std::int64_t verseArrangementEnd = 192000;
    auto extendClip = [&](std::string_view clipId) {
        auto found = std::ranges::find_if(manifest.clips,
                                          [clipId](const Clip& clip) { return clip.id == clipId; });
        if (found == manifest.clips.end()) {
            throw std::runtime_error("Open project is missing first-track clip: " +
                                     std::string{clipId});
        }
        found->lengthSamples = std::max(found->lengthSamples, verseArrangementEnd);
    };
    rememberUndoSnapshot();
    extendClip("drum-loop");
    extendClip("bass-pattern");

    for (auto& lane : manifest.automation) {
        for (auto& region : lane.regions) {
            if (region.startSample == 0 && region.endSample == 96000) {
                region.endSample = verseArrangementEnd;
                if (!region.points.empty() &&
                    region.points.back().samplePosition < verseArrangementEnd) {
                    auto finalPoint = region.points.back();
                    finalPoint.samplePosition = verseArrangementEnd;
                    region.points.push_back(finalPoint);
                }
            }
        }
    }

    document_->save();
    syncTransportFromProject();
    updateStatus();
    status_.dirty = false;
}

void ApplicationSession::play() {
    if (!document_.has_value() || !document_->isOpen()) {
        throw std::runtime_error("Cannot play without an open project");
    }
    if (status_.renderFrames == 0) {
        throw std::runtime_error("Cannot play a project with no renderable arrangement");
    }
    if (engine_.transport().samplePosition >= static_cast<std::int64_t>(status_.renderFrames)) {
        engine_.seekSamples(0);
    }
    engine_.play();
    updateStatus();
}

void ApplicationSession::stop() noexcept {
    engine_.stop();
    updateStatus();
}

void ApplicationSession::seek(std::int64_t samplePosition) {
    if (!document_.has_value() || !document_->isOpen()) {
        throw std::runtime_error("Cannot seek without an open project");
    }
    if (samplePosition < 0) {
        throw std::runtime_error("Seek position must be non-negative");
    }
    const auto maxSample = static_cast<std::int64_t>(status_.renderFrames);
    engine_.seekSamples(std::min(samplePosition, maxSample));
    updateStatus();
}

audio::RenderedAudio ApplicationSession::auditionCurrentMixBlock(std::uint32_t frames) {
    if (!document_.has_value() || !document_->isOpen()) {
        throw std::runtime_error("Cannot audition without an open project");
    }
    if (frames == 0 || frames > engine_.config().maxBlockSize) {
        throw std::runtime_error("Audition block size must be non-zero and within engine bounds");
    }
    if (!engine_.transport().playing) {
        return {.channels = engine_.config().outputChannels,
                .frames = frames,
                .interleavedSamples =
                    std::vector<float>(static_cast<std::size_t>(frames) *
                                       static_cast<std::size_t>(engine_.config().outputChannels))};
    }

    auto graph =
        compileProjectAudioGraph(document_->manifest(), {}, {.projectRoot = document_->path()});
    auto rendered = engine_.renderGraphOffline(graph, frames);
    updateStatus();
    return rendered;
}

audio::BounceResult ApplicationSession::exportCurrentMix(std::filesystem::path outputPath) {
    if (!document_.has_value() || !document_->isOpen()) {
        throw std::runtime_error("Cannot export without an open project");
    }
    return exportCurrentMix(
        {.outputPath = std::move(outputPath),
         .startSample = 0,
         .frames = status_.renderFrames,
         .sampleRate = document_->manifest().projectSampleRate,
         .channels = 2,
         .projectRoot = document_->path(),
         .bitDepth = audio::ExportBitDepth::Pcm16,
         .ditherMode = audio::DitherMode::Triangular,
         .normalizePeak = true,
         .normalizeTargetPeak = 0.98F});
}

audio::BounceResult ApplicationSession::exportCurrentMix(ProjectExportOptions options) {
    if (!document_.has_value() || !document_->isOpen()) {
        throw std::runtime_error("Cannot export without an open project");
    }
    if (options.outputPath.empty()) {
        throw std::runtime_error("Mix export output path is required");
    }
    if (!status_.mediaReady) {
        throw std::runtime_error("Open project media is not ready for export: " +
                                 status_.mediaError);
    }
    if (options.frames == 0) {
        const auto frames = renderableArrangementFrames(document_->manifest());
        if (frames == 0 || frames > std::numeric_limits<std::uint32_t>::max()) {
            throw std::runtime_error("Open project has no renderable arrangement");
        }
        options.frames = static_cast<std::uint32_t>(frames);
    }
    if (options.projectRoot.empty()) {
        options.projectRoot = document_->path();
    }

    const auto result = exportProjectMixToWav(document_->manifest(), {}, options);
    status_.lastMixExportPath = result.outputPath;
    status_.lastMixExportFrames = result.frames;
    status_.lastMixExportPeak = result.peakAfterNormalization;
    return result;
}

audio::BounceResult ApplicationSession::exportCurrentLoop(std::filesystem::path outputPath) {
    if (!document_.has_value() || !document_->isOpen()) {
        throw std::runtime_error("Cannot export loop without an open project");
    }
    if (!document_->manifest().loopEnabled) {
        throw std::runtime_error("Current project has no enabled loop region");
    }
    if (!status_.mediaReady) {
        throw std::runtime_error("Open project media is not ready for loop export: " +
                                 status_.mediaError);
    }
    const auto& manifest = document_->manifest();
    return exportCurrentMix(
        {.outputPath = std::move(outputPath),
         .startSample = manifest.loopStartSample,
         .frames = static_cast<std::uint32_t>(manifest.loopEndSample - manifest.loopStartSample),
         .sampleRate = manifest.projectSampleRate,
         .channels = 2,
         .projectRoot = document_->path(),
         .bitDepth = audio::ExportBitDepth::Pcm16,
         .ditherMode = audio::DitherMode::Triangular,
         .normalizePeak = true,
         .normalizeTargetPeak = 0.98F});
}

std::vector<StemExportResult>
ApplicationSession::exportCurrentStems(std::filesystem::path outputDirectory) {
    if (!document_.has_value() || !document_->isOpen()) {
        throw std::runtime_error("Cannot export stems without an open project");
    }
    if (outputDirectory.empty()) {
        throw std::runtime_error("Stem export output directory is required");
    }
    if (!status_.mediaReady) {
        throw std::runtime_error("Open project media is not ready for stem export: " +
                                 status_.mediaError);
    }
    if (status_.renderFrames == 0) {
        throw std::runtime_error("Open project has no renderable arrangement");
    }

    std::vector<std::string> trackIds;
    for (const auto& track : document_->manifest().tracks) {
        if (track.type != TrackType::Master) {
            trackIds.push_back(track.id);
        }
    }
    if (trackIds.empty()) {
        throw std::runtime_error("Open project has no exportable source tracks");
    }

    const auto results = exportProjectStemsToWav(document_->manifest(), {},
                                                 {.outputDirectory = outputDirectory,
                                                  .trackIds = std::move(trackIds),
                                                  .startSample = 0,
                                                  .frames = status_.renderFrames,
                                                  .sampleRate =
                                                      document_->manifest().projectSampleRate,
                                                  .channels = 2,
                                                  .projectRoot = document_->path(),
                                                  .bitDepth = audio::ExportBitDepth::Pcm16,
                                                  .ditherMode = audio::DitherMode::Triangular,
                                                  .normalizePeak = true,
                                                  .normalizeTargetPeak = 0.98F});
    status_.lastStemExportDirectory = std::move(outputDirectory);
    status_.lastStemExportCount = results.size();
    status_.lastStemExportFrames = results.empty() ? 0 : results.front().bounce.frames;
    return results;
}

FirstTrackPackageExportResult
ApplicationSession::exportFirstTrackPackage(std::filesystem::path outputDirectory) {
    if (!document_.has_value() || !document_->isOpen()) {
        throw std::runtime_error("Cannot export first-track package without an open project");
    }
    if (outputDirectory.empty()) {
        throw std::runtime_error("First-track package output directory is required");
    }
    if (!status_.firstTrackReady || status_.renderFrames == 0) {
        throw std::runtime_error("Open project is not ready for first-track package export");
    }
    if (!document_->manifest().loopEnabled) {
        throw std::runtime_error("First-track package export requires an enabled loop region");
    }

    const auto& manifest = document_->manifest();
    const auto mixPath = outputDirectory / "first-track-mix.wav";
    const auto loopPath = outputDirectory / "first-track-intro-loop.wav";
    const auto stemDirectory = outputDirectory / "stems";

    auto mix = exportProjectMixToWav(manifest, {},
                                     {.outputPath = mixPath,
                                      .startSample = 0,
                                      .frames = status_.renderFrames,
                                      .sampleRate = manifest.projectSampleRate,
                                      .channels = 2,
                                      .projectRoot = document_->path(),
                                      .bitDepth = audio::ExportBitDepth::Pcm16,
                                      .ditherMode = audio::DitherMode::Triangular,
                                      .normalizePeak = true,
                                      .normalizeTargetPeak = 0.98F});
    auto loop = exportProjectMixToWav(
        manifest, {},
        {.outputPath = loopPath,
         .startSample = manifest.loopStartSample,
         .frames = static_cast<std::uint32_t>(manifest.loopEndSample - manifest.loopStartSample),
         .sampleRate = manifest.projectSampleRate,
         .channels = 2,
         .projectRoot = document_->path(),
         .bitDepth = audio::ExportBitDepth::Pcm16,
         .ditherMode = audio::DitherMode::Triangular,
         .normalizePeak = true,
         .normalizeTargetPeak = 0.98F});

    std::vector<std::string> trackIds;
    for (const auto& track : manifest.tracks) {
        if (track.type != TrackType::Master) {
            trackIds.push_back(track.id);
        }
    }
    auto stems = exportProjectStemsToWav(manifest, {},
                                         {.outputDirectory = stemDirectory,
                                          .trackIds = std::move(trackIds),
                                          .startSample = 0,
                                          .frames = status_.renderFrames,
                                          .sampleRate = manifest.projectSampleRate,
                                          .channels = 2,
                                          .projectRoot = document_->path(),
                                          .bitDepth = audio::ExportBitDepth::Pcm16,
                                          .ditherMode = audio::DitherMode::Triangular,
                                          .normalizePeak = true,
                                          .normalizeTargetPeak = 0.98F});
    const auto projectSnapshotPath =
        copyFirstTrackPackageProjectSnapshot(manifest, document_->path(), outputDirectory);
    auto packageManifestPath = writeFirstTrackPackageManifest(
        manifest, document_->path(), outputDirectory, projectSnapshotPath, mix, loop, stems);

    status_.lastMixExportPath = mix.outputPath;
    status_.lastMixExportFrames = mix.frames;
    status_.lastMixExportPeak = mix.peakAfterNormalization;
    status_.lastStemExportDirectory = stemDirectory;
    status_.lastStemExportCount = stems.size();
    status_.lastStemExportFrames = stems.empty() ? 0 : stems.front().bounce.frames;
    status_.lastPackageExportDirectory = outputDirectory;
    status_.lastPackageManifestPath = packageManifestPath;
    status_.lastPackageVerified = false;
    status_.lastPackageVerifiedDirectory = std::filesystem::path{};
    status_.lastPackageMixFrames = mix.frames;
    status_.lastPackageLoopFrames = loop.frames;
    status_.lastPackageStemCount = stems.size();

    return {.outputDirectory = std::move(outputDirectory),
            .manifestPath = std::move(packageManifestPath),
            .mix = std::move(mix),
            .loop = std::move(loop),
            .stems = std::move(stems)};
}

FirstTrackProjectVerificationResult ApplicationSession::verifyFirstTrackProject() const {
    if (!document_.has_value() || !document_->isOpen()) {
        throw std::runtime_error("Cannot verify first-track readiness without an open project");
    }
    const auto readiness = inspectFirstTrackReadiness(document_->manifest());
    const auto mediaError = firstTrackMediaError(document_->manifest(), document_->path());
    const bool mediaReady = !mediaError.has_value();

    return {.projectPath = status_.projectPath,
            .projectName = status_.projectName,
            .firstTrackReady = status_.firstTrackReady && mediaReady,
            .firstTrackEditable = status_.firstTrackEditable && mediaReady,
            .mediaReady = mediaReady,
            .mediaError = mediaError.value_or(std::string{}),
            .starterStructureReady = status_.starterStructureReady,
            .renderable = status_.renderable,
            .loopEnabled = status_.loopEnabled,
            .loopFrames = status_.loopFrames,
            .renderFrames = status_.renderFrames,
            .trackCount = status_.trackCount,
            .clipCount = status_.clipCount,
            .pluginCount = status_.pluginCount,
            .automationLaneCount = status_.automationLaneCount,
            .starterMidiNoteCount = status_.starterMidiNoteCount,
            .missingRequirements = readiness.missingRequirements};
}

FirstTrackPackageVerificationResult
ApplicationSession::verifyFirstTrackPackage(std::filesystem::path packageDirectory) {
    if (packageDirectory.empty()) {
        throw std::runtime_error("First-track package verification directory is required");
    }

    const auto manifestPath = packageDirectory / "first-track-package.json";
    const auto manifestJson = readTextFile(manifestPath);
    const auto kind = requireJsonString(manifestJson, "kind");
    if (kind != "lamusica.firstTrackPackage") {
        throw std::runtime_error("Unexpected first-track package manifest kind: " + kind);
    }

    auto mix = requireNamedPackageAudioEntry(manifestJson, "mix");
    auto loop = requireNamedPackageAudioEntry(manifestJson, "loop");
    auto stems = packageStemEntries(manifestJson);
    auto projectAssets = packageProjectAssetEntries(manifestJson);
    if (stems.empty()) {
        throw std::runtime_error("First-track package manifest does not list stems");
    }
    const auto sampleRate = requireJsonUint(manifestJson, "sampleRate");
    const auto renderFrames = requireJsonUint(manifestJson, "renderFrames");
    const auto loopStartSample = requireJsonUint(manifestJson, "loopStartSample");
    const auto loopEndSample = requireJsonUint(manifestJson, "loopEndSample");
    const auto trackCount = requireJsonUint(manifestJson, "trackCount");
    const auto clipCount = requireJsonUint(manifestJson, "clipCount");
    const auto projectAssetCount = requireJsonUint(manifestJson, "projectAssetCount");
    const auto recordedTakeManifestCount = requireJsonUint(manifestJson, "recordedTakeCount");
    const auto importedAudioManifestCount = requireJsonUint(manifestJson, "importedAudioClipCount");
    const auto projectSnapshotPath = resolvePackageAudioPath(
        packageDirectory, requireJsonString(manifestJson, "projectSnapshotPath"));

    requirePackageManifestCondition(renderFrames > 0, "renderFrames must be positive");
    requirePackageManifestCondition(loopEndSample > loopStartSample,
                                    "loop range must have positive length");
    requirePackageManifestCondition(mix.frames == renderFrames,
                                    "mix frames must match renderFrames");
    requirePackageManifestCondition(loop.frames == loopEndSample - loopStartSample,
                                    "loop frames must match loop range");
    requirePackageManifestCondition(trackCount > 0, "trackCount must include a master track");
    requirePackageManifestCondition(stems.size() == trackCount - 1U,
                                    "stem count must match non-master track count");
    requirePackageManifestCondition(clipCount > 0, "clipCount must be positive");
    for (const auto& stem : stems) {
        requirePackageManifestCondition(stem.frames == renderFrames,
                                        "stem frames must match renderFrames");
    }

    mix.path = resolvePackageAudioPath(packageDirectory, mix.path);
    loop.path = resolvePackageAudioPath(packageDirectory, loop.path);
    for (auto& stem : stems) {
        stem.path = resolvePackageAudioPath(packageDirectory, stem.path);
    }

    verifyPackageAudioEntry(mix);
    verifyPackageAudioEntry(loop);
    for (const auto& stem : stems) {
        verifyPackageAudioEntry(stem);
    }
    const auto projectName = requireJsonString(manifestJson, "projectName");
    const auto projectSnapshot = parseProjectManifest(readTextFile(projectSnapshotPath));
    validateProjectManifest(projectSnapshot);
    requirePackageManifestCondition(sampleRate == integralSampleRate(projectSnapshot.projectSampleRate),
                                    "sampleRate must match project snapshot sample rate");
    requirePackageManifestCondition(projectSnapshot.name == projectName,
                                    "project snapshot name must match package projectName");
    requirePackageManifestCondition(projectSnapshot.tracks.size() == trackCount,
                                    "project snapshot track count must match package trackCount");
    requirePackageManifestCondition(projectSnapshot.clips.size() == clipCount,
                                    "project snapshot clip count must match package clipCount");
    requirePackageManifestCondition(renderableArrangementFrames(projectSnapshot) == renderFrames,
                                    "project snapshot render range must match renderFrames");
    const auto projectSnapshotReadiness = inspectFirstTrackReadiness(projectSnapshot);
    requirePackageManifestCondition(projectSnapshotReadiness.starterStructureReady,
                                    "project snapshot must be first-track starter ready");
    requirePackageManifestCondition(projectSnapshotReadiness.loopReady,
                                    "project snapshot must include an exportable loop");
    requirePackageManifestCondition(projectSnapshot.assets.size() == projectAssetCount,
                                    "project asset count must match project snapshot");
    requirePackageManifestCondition(projectAssets.size() == projectSnapshot.assets.size(),
                                    "project asset checksum count must match project snapshot");
    requirePackageManifestCondition(
        recordedTakeCount(projectSnapshot) == recordedTakeManifestCount,
        "project snapshot recorded take count must match package manifest");
    requirePackageManifestCondition(
        importedAudioClipCount(projectSnapshot) == importedAudioManifestCount,
        "project snapshot imported audio count must match package manifest");
    const auto projectSnapshotRoot = projectSnapshotPath.parent_path();
    for (const auto& asset : projectSnapshot.assets) {
        const auto assetPath = packageProjectAssetPath(projectSnapshotRoot, asset);
        requirePackageManifestCondition(std::filesystem::exists(assetPath),
                                        "project snapshot asset files must exist");
        const auto expectedPath = asset.relativePath.generic_string();
        const auto assetEntry =
            std::ranges::find_if(projectAssets, [&asset, &expectedPath](const auto& entry) {
                return entry.id == asset.id && entry.path.generic_string() == expectedPath;
            });
        requirePackageManifestCondition(
            assetEntry != projectAssets.end(),
            "project snapshot assets must be listed in package manifest");
        requirePackageManifestCondition(!assetEntry->checksum.empty(),
                                        "project snapshot assets must include checksums");
        requirePackageManifestCondition(
            packageFileChecksum(assetPath) == assetEntry->checksum,
            "project snapshot asset checksum must match package manifest");
    }
    (void)compileProjectAudioGraph(projectSnapshot, {}, {.projectRoot = projectSnapshotRoot});

    const FirstTrackPackageVerificationResult result{
        .packageDirectory = packageDirectory,
        .manifestPath = manifestPath,
        .projectName = projectName,
        .sampleRate = sampleRate,
        .renderFrames = renderFrames,
        .loopFrames = loop.frames,
        .stemCount = stems.size(),
        .trackCount = trackCount,
        .clipCount = clipCount,
        .projectSnapshotPath = projectSnapshotPath,
        .projectAssetCount = projectSnapshot.assets.size(),
        .recordedTakeCount = recordedTakeCount(projectSnapshot),
        .importedAudioClipCount = importedAudioClipCount(projectSnapshot),
        .projectSnapshotVerified = true};

    status_.lastPackageVerified = true;
    status_.lastPackageVerifiedDirectory = packageDirectory;
    status_.lastPackageManifestPath = manifestPath;
    status_.lastPackageMixFrames = mix.frames;
    status_.lastPackageLoopFrames = loop.frames;
    status_.lastPackageStemCount = stems.size();
    return result;
}

FirstTrackRecordingResult ApplicationSession::recordFirstTrackTake(std::uint32_t frames) {
    FirstTrackRecordingOptions options;
    options.frames = frames;
    return recordFirstTrackTake(std::move(options));
}

FirstTrackRecordingPlan
ApplicationSession::prepareFirstTrackRecording(FirstTrackRecordingOptions options) const {
    if (!document_.has_value() || !document_->isOpen()) {
        throw std::runtime_error("Cannot prepare first-track recording without an open project");
    }
    if (!status_.firstTrackEditable || status_.renderFrames == 0) {
        throw std::runtime_error("Open project is not ready for first-track recording");
    }
    if (options.startSample < 0) {
        throw std::runtime_error("First-track recording start sample must be non-negative");
    }
    if (options.countInBars > 8) {
        throw std::runtime_error("First-track recording count-in cannot exceed 8 bars");
    }
    if (options.punchInSample.has_value() != options.punchOutSample.has_value()) {
        throw std::runtime_error("First-track punch recording requires both in and out samples");
    }

    FirstTrackRecordingPlan plan;
    plan.countInBars = options.countInBars;
    plan.countInSamples = countInSamplesForBars(document_->manifest(), engine_.config().sampleRate,
                                                options.countInBars);

    if (options.punchInSample.has_value()) {
        if (*options.punchInSample < 0) {
            throw std::runtime_error("First-track punch-in sample must be non-negative");
        }
        if (*options.punchOutSample <= *options.punchInSample) {
            throw std::runtime_error("First-track punch range must be non-empty");
        }
        const auto punchFrames = *options.punchOutSample - *options.punchInSample;
        if (options.frames != 0 && static_cast<std::int64_t>(options.frames) != punchFrames) {
            throw std::runtime_error("First-track punch range must match requested frame count");
        }
        plan.timelineStartSample = *options.punchInSample;
        plan.recordFrames = checkedRecordingFrameCount(punchFrames);
        plan.punchEnabled = true;
        plan.punchInSample = *options.punchInSample;
        plan.punchOutSample = *options.punchOutSample;
    } else {
        plan.timelineStartSample = options.startSample;
        plan.recordFrames = checkedRecordingFrameCount(options.frames);
        plan.punchInSample = plan.timelineStartSample;
        plan.punchOutSample =
            plan.timelineStartSample + static_cast<std::int64_t>(plan.recordFrames);
    }

    plan.prerollStartSample = std::max<std::int64_t>(
        0, plan.timelineStartSample - static_cast<std::int64_t>(plan.countInSamples));
    return plan;
}

FirstTrackRecordingResult
ApplicationSession::recordFirstTrackTake(FirstTrackRecordingOptions options) {
    const auto plan = prepareFirstTrackRecording(std::move(options));
    auto& manifest = document_->mutableManifest();

    const bool hasMaster = std::ranges::any_of(manifest.tracks, [](const Track& track) {
        return track.id == masterTrackId && track.type == TrackType::Master;
    });
    if (!hasMaster) {
        throw std::runtime_error("First-track recording requires a master track");
    }

    std::uint32_t takeNumber = 1;
    std::string assetId;
    std::string clipId;
    do {
        assetId = "recorded-take-" + std::to_string(takeNumber);
        clipId = assetId;
        ++takeNumber;
    } while (idExists(manifest.assets, assetId) || idExists(manifest.clips, clipId));

    const auto fileName = assetId + ".wav";
    const std::filesystem::path relativePath = std::filesystem::path{"Assets"} / fileName;
    const auto finalPath = document_->path() / relativePath;
    const auto temporaryPath = finalPath.parent_path() / ("." + fileName + ".tmp");

    const auto graph = compileProjectAudioGraph(manifest, {}, {.projectRoot = document_->path()});
    audio::RenderedAudio captured{.channels = engine_.config().outputChannels,
                                  .frames = plan.recordFrames,
                                  .interleavedSamples = std::vector<float>(
                                      static_cast<std::size_t>(plan.recordFrames) *
                                      static_cast<std::size_t>(engine_.config().outputChannels))};
    audio::renderGraph(graph, engine_.config(), plan.timelineStartSample, plan.recordFrames,
                       captured.interleavedSamples);

    audio::RecordingSession recording{{.finalPath = finalPath,
                                       .temporaryPath = temporaryPath,
                                       .sampleRate = engine_.config().sampleRate,
                                       .channels = engine_.config().outputChannels,
                                       .timelineStartSample = plan.timelineStartSample,
                                       .measuredInputLatencySamples = 0}};
    recording.appendInterleaved(captured.interleavedSamples);
    auto committed = recording.commit();

    rememberUndoSnapshot();
    ensureAudioTrackRoutedToMaster(manifest, recordedTakeTrackId, recordedTakeTrackName);

    const auto fadeSamples =
        std::min<std::int64_t>(128, static_cast<std::int64_t>(plan.recordFrames / 2U));
    manifest.assets.push_back(
        {.id = assetId, .relativePath = relativePath, .mediaType = "audio/wav"});
    manifest.clips.push_back({.id = clipId,
                              .trackId = std::string{recordedTakeTrackId},
                              .type = ClipType::Audio,
                              .startSample = committed.timelineStartSample,
                              .lengthSamples = static_cast<std::int64_t>(committed.frames),
                              .fadeInSamples = fadeSamples,
                              .fadeOutSamples = fadeSamples,
                              .gainDb = -3.0F,
                              .assetId = assetId});

    document_->save();
    syncTransportFromProject();
    updateStatus();
    status_.lastRecordingPath = committed.path;
    status_.lastRecordingFrames = committed.frames;
    status_.lastRecordingStartSample = committed.timelineStartSample;
    status_.lastRecordingCountInSamples = plan.countInSamples;
    status_.lastRecordingPrerollStartSample = plan.prerollStartSample;
    status_.lastRecordingPunchEnabled = plan.punchEnabled;
    status_.lastRecordingPunchInSample = plan.punchInSample;
    status_.lastRecordingPunchOutSample = plan.punchOutSample;
    status_.lastRecordingAssetId = assetId;
    status_.lastRecordingClipId = clipId;
    status_.recordedTakeCount = recordedTakeCount(manifest);
    status_.dirty = false;

    return {.committed = std::move(committed),
            .timelineStartSample = plan.timelineStartSample,
            .countInSamples = plan.countInSamples,
            .prerollStartSample = plan.prerollStartSample,
            .punchEnabled = plan.punchEnabled,
            .punchInSample = plan.punchInSample,
            .punchOutSample = plan.punchOutSample,
            .trackId = std::string{recordedTakeTrackId},
            .assetId = std::move(assetId),
            .clipId = std::move(clipId)};
}

std::vector<FirstTrackTakeSummary> ApplicationSession::recordedFirstTrackTakes() const {
    if (!document_.has_value() || !document_->isOpen()) {
        return {};
    }

    std::vector<FirstTrackTakeSummary> takes;
    for (const auto& clip : document_->manifest().clips) {
        if (clip.trackId != recordedTakeTrackId) {
            continue;
        }
        std::filesystem::path assetPath;
        bool mediaAvailable = false;
        if (const auto* asset = findManifestAsset(document_->manifest(), clip.assetId);
            asset != nullptr && !asset->relativePath.empty() &&
            !asset->relativePath.is_absolute() &&
            !pathContainsParentTraversal(asset->relativePath)) {
            assetPath = document_->path() / asset->relativePath;
            mediaAvailable = std::filesystem::exists(assetPath);
        }
        takes.push_back({.clipId = clip.id,
                         .assetId = clip.assetId,
                         .path = std::move(assetPath),
                         .startSample = clip.startSample,
                         .frames = clip.lengthSamples,
                         .fadeInSamples = clip.fadeInSamples,
                         .fadeOutSamples = clip.fadeOutSamples,
                         .reversed = clip.reversed,
                         .muted = clip.muted,
                         .mediaAvailable = mediaAvailable});
    }
    std::ranges::sort(takes,
                      [](const FirstTrackTakeSummary& lhs, const FirstTrackTakeSummary& rhs) {
                          if (lhs.startSample != rhs.startSample) {
                              return lhs.startSample < rhs.startSample;
                          }
                          return lhs.clipId < rhs.clipId;
                      });
    return takes;
}

std::vector<FirstTrackClipSummary> ApplicationSession::firstTrackClips() const {
    if (!document_.has_value() || !document_->isOpen()) {
        return {};
    }

    std::vector<FirstTrackClipSummary> clips;
    for (const auto& clip : document_->manifest().clips) {
        std::filesystem::path assetPath;
        bool assetBacked = false;
        bool mediaAvailable = false;
        if (!clip.assetId.empty()) {
            assetBacked = true;
            if (const auto* asset = findManifestAsset(document_->manifest(), clip.assetId);
                asset != nullptr && !asset->relativePath.empty() &&
                !asset->relativePath.is_absolute() &&
                !pathContainsParentTraversal(asset->relativePath)) {
                assetPath = document_->path() / asset->relativePath;
                mediaAvailable = std::filesystem::exists(assetPath);
            }
        }
        clips.push_back({.clipId = clip.id,
                         .trackId = clip.trackId,
                         .trackName = trackNameForClip(document_->manifest(), clip.trackId),
                         .type = clip.type,
                         .assetId = clip.assetId,
                         .path = std::move(assetPath),
                         .startSample = clip.startSample,
                         .lengthSamples = clip.lengthSamples,
                         .sourceOffsetSamples = clip.sourceOffsetSamples,
                         .fadeInSamples = clip.fadeInSamples,
                         .fadeOutSamples = clip.fadeOutSamples,
                         .gainDb = clip.gainDb,
                         .muted = clip.muted,
                         .reversed = clip.reversed,
                         .assetBacked = assetBacked,
                         .mediaAvailable = mediaAvailable});
    }
    return clips;
}

std::vector<FirstTrackTrackMixSummary> ApplicationSession::firstTrackTrackMix() const {
    if (!document_.has_value() || !document_->isOpen()) {
        return {};
    }

    const auto& manifest = document_->manifest();
    std::vector<FirstTrackTrackMixSummary> mixes;
    for (const auto& track : manifest.tracks) {
        const auto found = std::ranges::find_if(
            manifest.trackMix, [&](const TrackMixState& mix) { return mix.trackId == track.id; });
        if (found == manifest.trackMix.end()) {
            mixes.push_back({.trackId = track.id, .trackName = track.name, .type = track.type});
            continue;
        }
        mixes.push_back({.trackId = track.id,
                         .trackName = track.name,
                         .type = track.type,
                         .volumeDb = found->volumeDb,
                         .pan = found->pan,
                         .muted = found->muted,
                         .solo = found->solo});
    }
    return mixes;
}

void ApplicationSession::setFirstTrackTrackMix(std::string_view trackId, float volumeDb, float pan,
                                               bool muted, bool solo) {
    if (!document_.has_value() || !document_->isOpen()) {
        throw std::runtime_error("Cannot edit first-track mix without an open project");
    }
    if (trackId.empty()) {
        throw std::runtime_error("First-track mix edit requires a track id");
    }
    if (!std::isfinite(volumeDb) || volumeDb < -120.0F || volumeDb > 24.0F) {
        throw std::runtime_error("Track mix volume must be finite and between -120 and 24 dB");
    }
    if (!std::isfinite(pan) || pan < -1.0F || pan > 1.0F) {
        throw std::runtime_error("Track mix pan must be finite and between -1 and 1");
    }

    auto& manifest = document_->mutableManifest();
    const auto trackFound = std::ranges::find_if(
        manifest.tracks, [trackId](const Track& track) { return track.id == trackId; });
    if (trackFound == manifest.tracks.end()) {
        throw std::runtime_error("Open project does not contain track: " + std::string{trackId});
    }

    const auto mixFound = std::ranges::find_if(
        manifest.trackMix, [trackId](const TrackMixState& mix) { return mix.trackId == trackId; });
    const bool defaultMix = volumeDb == 0.0F && pan == 0.0F && !muted && !solo;
    if (mixFound == manifest.trackMix.end()) {
        if (defaultMix) {
            updateStatus();
            return;
        }
        rememberUndoSnapshot();
        manifest.trackMix.push_back({.trackId = std::string{trackId},
                                     .volumeDb = volumeDb,
                                     .pan = pan,
                                     .muted = muted,
                                     .solo = solo});
    } else {
        if (mixFound->volumeDb == volumeDb && mixFound->pan == pan && mixFound->muted == muted &&
            mixFound->solo == solo) {
            updateStatus();
            return;
        }
        rememberUndoSnapshot();
        if (defaultMix) {
            manifest.trackMix.erase(mixFound);
        } else {
            mixFound->volumeDb = volumeDb;
            mixFound->pan = pan;
            mixFound->muted = muted;
            mixFound->solo = solo;
        }
    }

    document_->save();
    updateStatus();
    status_.dirty = false;
}

void ApplicationSession::setFirstTrackTakeMuted(std::string_view clipId, bool muted) {
    if (!document_.has_value() || !document_->isOpen()) {
        throw std::runtime_error("Cannot edit first-track take without an open project");
    }
    if (clipId.empty()) {
        throw std::runtime_error("First-track take clip id is required");
    }
    auto& manifest = document_->mutableManifest();
    const auto found = std::ranges::find_if(manifest.clips, [clipId](const Clip& clip) {
        return clip.id == clipId && clip.trackId == recordedTakeTrackId;
    });
    if (found == manifest.clips.end()) {
        throw std::runtime_error("First-track recorded take was not found: " + std::string{clipId});
    }
    setClipMuted(clipId, muted);
}

FirstTrackAudioImportResult
ApplicationSession::importAudioFileToFirstTrack(std::filesystem::path sourcePath,
                                                std::int64_t startSample) {
    if (!document_.has_value() || !document_->isOpen()) {
        throw std::runtime_error("Cannot import audio without an open project");
    }
    if (!status_.firstTrackEditable || status_.renderFrames == 0) {
        throw std::runtime_error("Open project is not ready for first-track audio import");
    }
    if (sourcePath.empty()) {
        throw std::runtime_error("Audio import source path is required");
    }
    if (startSample < 0) {
        throw std::runtime_error("Audio import start sample must be non-negative");
    }
    if (!isSupportedAudioImportExtension(sourcePath)) {
        throw std::runtime_error("Unsupported audio import format: " +
                                 sourcePath.extension().string());
    }
    if (!std::filesystem::exists(sourcePath)) {
        throw std::runtime_error("Audio import source file was not found");
    }

    auto& manifest = document_->mutableManifest();
    const bool hasMaster = std::ranges::any_of(manifest.tracks, [](const Track& track) {
        return track.id == masterTrackId && track.type == TrackType::Master;
    });
    if (!hasMaster) {
        throw std::runtime_error("First-track audio import requires a master track");
    }

    std::uint32_t importNumber = 1;
    std::string assetId;
    std::string clipId;
    do {
        assetId = "imported-audio-" + std::to_string(importNumber);
        clipId = assetId;
        ++importNumber;
    } while (idExists(manifest.assets, assetId) || idExists(manifest.clips, clipId));

    const auto wav = audio::readPcm16Wav(sourcePath);
    if (wav.audio.frames == 0 || wav.audio.channels == 0) {
        throw std::runtime_error("Audio import source contains no renderable audio");
    }

    const auto extension =
        sourcePath.extension().empty() ? std::filesystem::path{".wav"} : sourcePath.extension();
    const auto relativePath = std::filesystem::path{"Assets"} / (assetId + extension.string());
    const auto destinationPath = document_->path() / relativePath;
    if (destinationPath.has_parent_path()) {
        std::filesystem::create_directories(destinationPath.parent_path());
    }
    if (!std::filesystem::exists(destinationPath) ||
        !std::filesystem::equivalent(sourcePath, destinationPath)) {
        std::filesystem::copy_file(sourcePath, destinationPath,
                                   std::filesystem::copy_options::overwrite_existing);
    }

    rememberUndoSnapshot();
    ensureAudioTrackRoutedToMaster(manifest, importedAudioTrackId, importedAudioTrackName);

    const auto fadeSamples =
        std::min<std::int64_t>(128, static_cast<std::int64_t>(wav.audio.frames / 2U));
    manifest.assets.push_back(
        {.id = assetId, .relativePath = relativePath, .mediaType = "audio/wav"});
    manifest.clips.push_back({.id = clipId,
                              .trackId = std::string{importedAudioTrackId},
                              .type = ClipType::Audio,
                              .startSample = startSample,
                              .lengthSamples = static_cast<std::int64_t>(wav.audio.frames),
                              .fadeInSamples = fadeSamples,
                              .fadeOutSamples = fadeSamples,
                              .gainDb = -3.0F,
                              .assetId = assetId});

    document_->save();
    syncTransportFromProject();
    updateStatus();
    status_.lastImportPath = destinationPath;
    status_.lastImportFrames = wav.audio.frames;
    status_.lastImportAssetId = assetId;
    status_.lastImportClipId = clipId;
    status_.importedAudioClipCount = importedAudioClipCount(manifest);
    status_.dirty = false;

    return {.copiedPath = destinationPath,
            .frames = wav.audio.frames,
            .channels = wav.audio.channels,
            .trackId = std::string{importedAudioTrackId},
            .assetId = std::move(assetId),
            .clipId = std::move(clipId)};
}

FirstTrackMediaRelinkResult
ApplicationSession::relinkFirstTrackAudioAsset(std::string_view assetId,
                                               std::filesystem::path sourcePath) {
    if (!document_.has_value() || !document_->isOpen()) {
        throw std::runtime_error("Cannot relink first-track media without an open project");
    }
    if (assetId.empty()) {
        throw std::runtime_error("First-track media asset id is required");
    }
    if (sourcePath.empty()) {
        throw std::runtime_error("First-track media relink source path is required");
    }
    if (!isSupportedAudioImportExtension(sourcePath)) {
        throw std::runtime_error("Unsupported first-track media relink format: " +
                                 sourcePath.extension().string());
    }
    if (!std::filesystem::exists(sourcePath)) {
        throw std::runtime_error("First-track media relink source file was not found");
    }

    auto& manifest = document_->mutableManifest();
    auto* asset = findMutableManifestAsset(manifest, assetId);
    if (asset == nullptr) {
        throw std::runtime_error("First-track media asset was not found: " + std::string{assetId});
    }
    if (asset->relativePath.empty() || asset->relativePath.is_absolute() ||
        pathContainsParentTraversal(asset->relativePath)) {
        throw std::runtime_error("First-track media asset has an unsafe project path: " +
                                 asset->relativePath.string());
    }
    if (asset->mediaType != "audio/wav") {
        throw std::runtime_error("First-track media relink only supports audio/wav assets");
    }
    const bool assetIsReferencedByAudioClip =
        std::ranges::any_of(manifest.clips, [assetId](const Clip& clip) {
            return clip.assetId == assetId && clip.type == ClipType::Audio;
        });
    if (!assetIsReferencedByAudioClip) {
        throw std::runtime_error("First-track media asset is not referenced by an audio clip: " +
                                 std::string{assetId});
    }

    const auto wav = audio::readPcm16Wav(sourcePath);
    if (wav.audio.frames == 0 || wav.audio.channels == 0) {
        throw std::runtime_error("First-track media relink source contains no renderable audio");
    }

    const auto destinationPath = document_->path() / asset->relativePath;
    if (destinationPath.has_parent_path()) {
        std::filesystem::create_directories(destinationPath.parent_path());
    }
    if (!std::filesystem::exists(destinationPath) ||
        !std::filesystem::equivalent(sourcePath, destinationPath)) {
        std::filesystem::copy_file(sourcePath, destinationPath,
                                   std::filesystem::copy_options::overwrite_existing);
    }

    document_->save();
    syncTransportFromProject();
    updateStatus();
    status_.dirty = false;

    return {.copiedPath = destinationPath,
            .frames = wav.audio.frames,
            .channels = wav.audio.channels,
            .assetId = std::string{assetId},
            .mediaReady = status_.mediaReady};
}

bool ApplicationSession::undoLastEdit() {
    return restoreEditSnapshot(undoSnapshots_, redoSnapshots_);
}

bool ApplicationSession::redoLastEdit() {
    return restoreEditSnapshot(redoSnapshots_, undoSnapshots_);
}

void ApplicationSession::closeProject() noexcept {
    if (document_.has_value()) {
        document_->close();
        document_.reset();
    }
    clearEditHistory();
    engine_.stop();
    engine_.seekSamples(0);
    engine_.setLoop(false, 0, 0);
    updateStatus();
}

bool ApplicationSession::recoverLastProject(const std::filesystem::path& path) {
    if (path.empty() || !std::filesystem::exists(path / ProjectDocument::manifestFileName)) {
        closeProject();
        return false;
    }
    openProject(path);
    return true;
}

void ApplicationSession::setPreferences(ApplicationPreferences preferences) {
    validatePreferences(preferences);
    preferences_ = std::move(preferences);
}

void ApplicationSession::setKeyboardShortcut(std::string command, std::string keyEquivalent) {
    if (command.empty() || keyEquivalent.empty()) {
        throw std::runtime_error("Keyboard shortcut command and key equivalent are required");
    }

    const auto found = std::ranges::find_if(preferences_.keyboardShortcuts,
                                            [&command](const KeyboardShortcutPreference& shortcut) {
                                                return shortcut.command == command;
                                            });
    if (found == preferences_.keyboardShortcuts.end()) {
        preferences_.keyboardShortcuts.push_back(
            {.command = std::move(command), .keyEquivalent = std::move(keyEquivalent)});
    } else {
        found->keyEquivalent = std::move(keyEquivalent);
    }
}

void ApplicationSession::focusPanel(ApplicationPanel panel) noexcept {
    focusedPanel_ = panel;
}

MenuCommandRoute ApplicationSession::routeMenuCommand(std::string_view command) {
    if (command.empty()) {
        throw std::runtime_error("Menu command must not be empty");
    }

    MenuCommandRoute route{
        .command = std::string{command}, .panel = focusedPanel_, .handled = true, .enabled = true};
    if (command == "view.browser") {
        focusPanel(ApplicationPanel::Browser);
        route.panel = focusedPanel_;
        return route;
    }
    if (command == "view.timeline") {
        focusPanel(ApplicationPanel::Timeline);
        route.panel = focusedPanel_;
        return route;
    }
    if (command == "view.inspector") {
        focusPanel(ApplicationPanel::Inspector);
        route.panel = focusedPanel_;
        return route;
    }
    if (command == "view.mixer") {
        focusPanel(ApplicationPanel::Mixer);
        route.panel = focusedPanel_;
        return route;
    }
    if (command == "transport.play" || command == "transport.stop") {
        focusPanel(ApplicationPanel::Transport);
        route.panel = focusedPanel_;
        route.enabled = status_.hasOpenProject;
        return route;
    }
    if (command == "project.save" || command == "project.close" ||
        command == "project.verifyFirstTrackProject") {
        route.enabled = status_.hasOpenProject;
        return route;
    }
    if (command == "project.exportMix" || command == "project.exportStems") {
        route.enabled = status_.hasOpenProject && status_.mediaReady && status_.renderFrames > 0;
        return route;
    }
    if (command == "project.exportLoop") {
        route.enabled = status_.hasOpenProject && status_.mediaReady && status_.loopEnabled &&
                        status_.loopFrames > 0;
        return route;
    }
    if (command == "project.exportFirstTrackPackage") {
        route.enabled = status_.hasOpenProject && status_.firstTrackReady;
        return route;
    }
    if (command == "project.importAudio" || command == "project.recordFirstTrackTake" ||
        command == "project.extendIntroToVerse" || command == "project.transposeBassUpOctave" ||
        command == "project.transposeBassDownOctave" || command == "project.drumsGainUp" ||
        command == "project.drumsGainDown" || command == "project.bassGainUp" ||
        command == "project.bassGainDown" || command == "project.drumsTrackVolumeUp" ||
        command == "project.drumsTrackVolumeDown" || command == "project.drumsPanLeft" ||
        command == "project.drumsPanRight" || command == "project.toggleDrumsMute" ||
        command == "project.toggleDrumsSolo" || command == "project.bassTrackVolumeUp" ||
        command == "project.bassTrackVolumeDown" || command == "project.bassPanLeft" ||
        command == "project.bassPanRight" || command == "project.toggleBassMute" ||
        command == "project.toggleBassSolo") {
        route.enabled = status_.firstTrackEditable;
        return route;
    }
    if (command == "project.recordedTrackVolumeUp" ||
        command == "project.recordedTrackVolumeDown" || command == "project.recordedPanLeft" ||
        command == "project.recordedPanRight" || command == "project.toggleRecordedMute" ||
        command == "project.toggleRecordedSolo") {
        route.enabled = status_.firstTrackEditable && status_.recordedTakeCount > 0;
        return route;
    }
    if (command == "project.importedTrackVolumeUp" ||
        command == "project.importedTrackVolumeDown" || command == "project.importedPanLeft" ||
        command == "project.importedPanRight" || command == "project.toggleImportedMute" ||
        command == "project.toggleImportedSolo") {
        route.enabled = status_.firstTrackEditable && status_.importedAudioClipCount > 0;
        return route;
    }
    if (command == "project.softenLastTakeFades" || command == "project.toggleLastTakeMute" ||
        command == "project.toggleLastTakeReverse" ||
        command == "project.duplicateLastTakeAtPlayhead" || command == "project.removeLastTake") {
        route.enabled = status_.firstTrackEditable && !status_.lastRecordingClipId.empty();
        return route;
    }
    if (command == "project.trimLastTakeToLoop") {
        route.enabled = status_.firstTrackEditable && !status_.lastRecordingClipId.empty() &&
                        status_.loopEnabled && status_.loopFrames > 0;
        return route;
    }
    if (command == "project.softenLastImportFades" || command == "project.toggleLastImportMute" ||
        command == "project.toggleLastImportReverse" ||
        command == "project.duplicateLastImportAtPlayhead" ||
        command == "project.removeLastImport") {
        route.enabled = status_.firstTrackEditable && !status_.lastImportClipId.empty();
        return route;
    }
    if (command == "project.trimLastImportToLoop") {
        route.enabled = status_.firstTrackEditable && !status_.lastImportClipId.empty() &&
                        status_.loopEnabled && status_.loopFrames > 0;
        return route;
    }
    if (command == "project.relinkFirstTrackMedia") {
        route.enabled =
            status_.hasOpenProject && !status_.mediaReady && !status_.missingMediaAssetId.empty();
        return route;
    }
    if (command == "project.loopIntro") {
        route.enabled = status_.firstTrackEditable;
        return route;
    }
    if (command == "project.verifyFirstTrackPackage") {
        route.enabled = true;
        return route;
    }
    if (command == "edit.undo") {
        route.enabled = status_.canUndo;
        return route;
    }
    if (command == "edit.redo") {
        route.enabled = status_.canRedo;
        return route;
    }
    if (command == "edit.cut" || command == "edit.copy" || command == "edit.paste" ||
        command == "edit.delete" || command == "edit.duplicate" || command == "edit.split" ||
        command == "edit.trim") {
        route.enabled = status_.hasOpenProject;
        return route;
    }

    route.handled = false;
    route.enabled = false;
    return route;
}

void ApplicationSession::updateStatus() {
    if (!document_.has_value() || !document_->isOpen()) {
        status_ = {};
        return;
    }
    const auto readiness = inspectFirstTrackReadiness(document_->manifest());
    const auto mediaError = firstTrackMediaError(document_->manifest(), document_->path());
    const bool mediaReady = !mediaError.has_value();
    const auto arrangement = readiness.renderable
                                 ? summarizeFirstTrackArrangement(document_->manifest())
                                 : FirstTrackArrangementSummary{};
    const auto clipGain = [&](std::string_view clipId) {
        const auto found = std::ranges::find_if(
            document_->manifest().clips, [clipId](const Clip& clip) { return clip.id == clipId; });
        return found == document_->manifest().clips.end() ? 0.0F : found->gainDb;
    };
    const auto* latestRecordingClip = latestClipOnTrack(document_->manifest(), recordedTakeTrackId);
    const auto* latestImportClip = latestClipOnTrack(document_->manifest(), importedAudioTrackId);
    status_ = ApplicationSessionStatus{
        .hasOpenProject = true,
        .dirty = false,
        .canUndo = !undoSnapshots_.empty(),
        .canRedo = !redoSnapshots_.empty(),
        .undoDepth = undoSnapshots_.size(),
        .redoDepth = redoSnapshots_.size(),
        .projectPath = document_->path(),
        .projectName = std::string{document_->project().name()},
        .firstTrackReady = readiness.starterStructureReady && readiness.renderable &&
                           readiness.loopReady && mediaReady,
        .firstTrackEditable = readiness.firstTrackEditable && mediaReady,
        .mediaReady = mediaReady,
        .mediaError = mediaError.value_or(std::string{}),
        .missingMediaAssetId =
            mediaReady ? std::string{}
                       : firstMissingMediaAssetId(document_->manifest(), document_->path()),
        .starterStructureReady = readiness.starterStructureReady,
        .renderable = readiness.renderable,
        .trackCount = readiness.trackCount,
        .clipCount = readiness.clipCount,
        .markerCount = readiness.markerCount,
        .pluginCount = readiness.pluginCount,
        .automationLaneCount = readiness.automationLaneCount,
        .starterMidiNoteCount = readiness.starterMidiNoteCount,
        .starterBassTransposeSemitones = readiness.starterBassTransposeSemitones,
        .drumClipGainDb = clipGain("drum-loop"),
        .bassClipGainDb = clipGain("bass-pattern"),
        .loopEnabled = document_->manifest().loopEnabled,
        .loopStartSample = document_->manifest().loopStartSample,
        .loopEndSample = document_->manifest().loopEndSample,
        .loopFrames = readiness.loopReady ? static_cast<std::uint32_t>(readiness.loopEndSample -
                                                                       readiness.loopStartSample)
                                          : 0,
        .transportPlaying = engine_.transport().playing,
        .transportRecording = engine_.transport().recording,
        .playheadSample = engine_.transport().samplePosition,
        .renderFrames = readiness.renderFrames,
        .tempoBpm = arrangement.tempoBpm,
        .timeSignatureNumerator = arrangement.timeSignatureNumerator,
        .timeSignatureDenominator = arrangement.timeSignatureDenominator,
        .firstSectionName = arrangement.firstSectionName,
        .finalSectionName = arrangement.finalSectionName,
        .lastMixExportPath = {},
        .lastMixExportFrames = 0,
        .lastMixExportPeak = 0.0F,
        .lastStemExportDirectory = {},
        .lastStemExportCount = 0,
        .lastStemExportFrames = 0,
        .lastPackageExportDirectory = {},
        .lastPackageManifestPath = {},
        .lastPackageVerified = false,
        .lastPackageVerifiedDirectory = {},
        .lastPackageMixFrames = 0,
        .lastPackageLoopFrames = 0,
        .lastPackageStemCount = 0,
        .lastRecordingPath =
            clipAssetPathForStatus(document_->manifest(), document_->path(), latestRecordingClip),
        .lastRecordingFrames = clipFramesForStatus(latestRecordingClip),
        .lastRecordingAssetId =
            latestRecordingClip == nullptr ? std::string{} : latestRecordingClip->assetId,
        .lastRecordingClipId =
            latestRecordingClip == nullptr ? std::string{} : latestRecordingClip->id,
        .recordedTakeCount = recordedTakeCount(document_->manifest()),
        .mutedRecordedTakeCount = mutedRecordedTakeCount(document_->manifest()),
        .lastImportPath =
            clipAssetPathForStatus(document_->manifest(), document_->path(), latestImportClip),
        .lastImportFrames = clipFramesForStatus(latestImportClip),
        .lastImportAssetId =
            latestImportClip == nullptr ? std::string{} : latestImportClip->assetId,
        .lastImportClipId = latestImportClip == nullptr ? std::string{} : latestImportClip->id,
        .importedAudioClipCount = importedAudioClipCount(document_->manifest())};
}

void ApplicationSession::syncTransportFromProject() {
    if (document_.has_value()) {
        const auto projectSampleRate = document_->manifest().projectSampleRate;
        if (std::abs(engine_.config().sampleRate - projectSampleRate) > 0.000001) {
            const auto selectedDevice = engine_.device();
            engine_ = audio::AudioEngine{{.sampleRate = projectSampleRate,
                                          .maxBlockSize = engine_.config().maxBlockSize,
                                          .outputChannels = engine_.config().outputChannels}};
            engine_.selectDevice(selectedDevice);
        }
        const auto endSample = arrangementEndSample(document_->manifest());
        const auto frames =
            endSample > 0 && endSample <= static_cast<std::int64_t>(
                                              std::numeric_limits<std::uint32_t>::max())
                ? static_cast<std::uint32_t>(endSample)
                : 0U;
        if (frames > 0U) {
            const auto arrangement = summarizeFirstTrackArrangement(document_->manifest());
            engine_.setTempo(arrangement.tempoBpm);
            engine_.setTimeSignature({.numerator = arrangement.timeSignatureNumerator,
                                      .denominator = arrangement.timeSignatureDenominator});
        } else {
            engine_.setTempo(120.0);
            engine_.setTimeSignature({});
        }
        engine_.setLoop(document_->manifest().loopEnabled, document_->manifest().loopStartSample,
                        document_->manifest().loopEndSample);
        engine_.seekSamples(std::min<std::int64_t>(engine_.transport().samplePosition,
                                                   static_cast<std::int64_t>(frames)));
    } else {
        engine_.setTempo(120.0);
        engine_.setTimeSignature({});
        engine_.setLoop(false, 0, 0);
        engine_.seekSamples(0);
    }
}

void ApplicationSession::rememberRecentProject(const std::filesystem::path& path) {
    std::erase(recentProjects_, path);
    recentProjects_.insert(recentProjects_.begin(), path);
    if (recentProjects_.size() > 10) {
        recentProjects_.resize(10);
    }
}

void ApplicationSession::validatePreferences(const ApplicationPreferences& preferences) const {
    if (preferences.allowMcpProjectMutation && !preferences.mcpEnabled) {
        throw std::runtime_error("MCP project mutation requires MCP to be enabled");
    }
    if ((preferences.shareDiagnostics || preferences.telemetryEnabled) &&
        preferences.diagnosticsConsent != DiagnosticsConsent::Granted) {
        throw std::runtime_error("Diagnostics and telemetry require explicit granted consent");
    }
    if (!diagnosticsEndpointAllowed(preferences.diagnosticsEndpoint)) {
        throw std::runtime_error("Diagnostics endpoint must be empty, HTTPS, or an environment override");
    }

    for (const auto& midiInputId : preferences.enabledMidiInputIds) {
        if (midiInputId.empty()) {
            throw std::runtime_error("MIDI input preference id must not be empty");
        }
    }
    for (const auto& pluginSearchPath : preferences.pluginSearchPaths) {
        if (pluginSearchPath.empty()) {
            throw std::runtime_error("Plugin search path preference must not be empty");
        }
    }
    for (const auto& shortcut : preferences.keyboardShortcuts) {
        if (shortcut.command.empty() || shortcut.keyEquivalent.empty()) {
            throw std::runtime_error("Keyboard shortcut command and key equivalent are required");
        }
    }
}

void ApplicationSession::rememberUndoSnapshot() {
    if (!document_.has_value() || !document_->isOpen()) {
        return;
    }
    undoSnapshots_.push_back(document_->manifest());
    if (undoSnapshots_.size() > maxApplicationUndoSnapshots) {
        undoSnapshots_.erase(undoSnapshots_.begin());
    }
    redoSnapshots_.clear();
}

void ApplicationSession::clearEditHistory() noexcept {
    undoSnapshots_.clear();
    redoSnapshots_.clear();
}

bool ApplicationSession::restoreEditSnapshot(std::vector<ProjectManifest>& sourceStack,
                                             std::vector<ProjectManifest>& destinationStack) {
    if (!document_.has_value() || !document_->isOpen() || sourceStack.empty()) {
        updateStatus();
        return false;
    }

    destinationStack.push_back(document_->manifest());
    if (destinationStack.size() > maxApplicationUndoSnapshots) {
        destinationStack.erase(destinationStack.begin());
    }
    document_->mutableManifest() = std::move(sourceStack.back());
    sourceStack.pop_back();
    document_->save();
    syncTransportFromProject();
    updateStatus();
    status_.dirty = false;
    return true;
}

} // namespace lamusica::session
