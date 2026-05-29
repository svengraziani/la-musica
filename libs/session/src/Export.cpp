#include "lamusica/session/Export.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace lamusica::session {
namespace {

std::string trackNodeId(std::string_view trackId) {
    return "track:" + std::string{trackId};
}

bool hasTrack(const ProjectManifest& manifest, std::string_view trackId) {
    return std::ranges::any_of(manifest.tracks,
                               [trackId](const Track& track) { return track.id == trackId; });
}

bool hasNode(const audio::AudioGraph& graph, std::string_view nodeId) {
    return std::ranges::any_of(
        graph.nodes, [nodeId](const audio::GraphNode& node) { return node.id == nodeId; });
}

audio::BounceOptions bounceOptionsForPath(const std::filesystem::path& path,
                                          const ProjectExportOptions& options) {
    return {.outputPath = path,
            .startSample = options.startSample,
            .frames = options.frames,
            .sampleRate = options.sampleRate,
            .channels = options.channels,
            .bitDepth = options.bitDepth,
            .ditherMode = options.ditherMode,
            .normalizePeak = options.normalizePeak,
            .normalizeTargetPeak = options.normalizeTargetPeak};
}

double resolveExportSampleRate(const ProjectManifest& manifest, double requestedSampleRate) {
    if (requestedSampleRate > 0.0) {
        return requestedSampleRate;
    }
    return manifest.projectSampleRate;
}

ProjectExportOptions resolveProjectExportOptions(const ProjectManifest& manifest,
                                                 ProjectExportOptions options) {
    options.sampleRate = resolveExportSampleRate(manifest, options.sampleRate);
    return options;
}

ProjectExportOptions projectOptionsForStem(const StemExportOptions& options,
                                           std::filesystem::path path) {
    return {.outputPath = std::move(path),
            .startSample = options.startSample,
            .frames = options.frames,
            .sampleRate = options.sampleRate,
            .channels = options.channels,
            .projectRoot = options.projectRoot,
            .bitDepth = options.bitDepth,
            .ditherMode = options.ditherMode,
            .normalizePeak = options.normalizePeak,
            .normalizeTargetPeak = options.normalizeTargetPeak};
}

std::uint32_t loopFrames(const audio::TransportState& transport) {
    if (!transport.loopEnabled || transport.loopEndSample <= transport.loopStartSample) {
        throw std::invalid_argument("Loop export requires an enabled non-empty loop region");
    }
    const auto frames = transport.loopEndSample - transport.loopStartSample;
    if (frames > std::numeric_limits<std::uint32_t>::max()) {
        throw std::invalid_argument("Loop export range is too large");
    }
    return static_cast<std::uint32_t>(frames);
}

} // namespace

ProjectExportOptions makeLoopMixExportOptions(std::filesystem::path outputPath,
                                              const audio::TransportState& transport,
                                              double sampleRate, std::uint32_t channels,
                                              audio::ExportBitDepth bitDepth,
                                              audio::DitherMode ditherMode, bool normalizePeak,
                                              float normalizeTargetPeak) {
    return {.outputPath = std::move(outputPath),
            .startSample = transport.loopStartSample,
            .frames = loopFrames(transport),
            .sampleRate = sampleRate,
            .channels = channels,
            .bitDepth = bitDepth,
            .ditherMode = ditherMode,
            .normalizePeak = normalizePeak,
            .normalizeTargetPeak = normalizeTargetPeak};
}

StemExportOptions makeLoopStemExportOptions(std::filesystem::path outputDirectory,
                                            std::vector<std::string> selectedTrackIds,
                                            const audio::TransportState& transport,
                                            double sampleRate, std::uint32_t channels,
                                            audio::ExportBitDepth bitDepth,
                                            audio::DitherMode ditherMode, bool normalizePeak,
                                            float normalizeTargetPeak) {
    return {.outputDirectory = std::move(outputDirectory),
            .trackIds = std::move(selectedTrackIds),
            .startSample = transport.loopStartSample,
            .frames = loopFrames(transport),
            .sampleRate = sampleRate,
            .channels = channels,
            .bitDepth = bitDepth,
            .ditherMode = ditherMode,
            .normalizePeak = normalizePeak,
            .normalizeTargetPeak = normalizeTargetPeak};
}

audio::BounceResult exportProjectMixToWav(const ProjectManifest& manifest, const MixerState& mixer,
                                          const ProjectExportOptions& options) {
    const auto resolvedOptions = resolveProjectExportOptions(manifest, options);
    auto graph =
        compileProjectAudioGraph(manifest, mixer, {.projectRoot = resolvedOptions.projectRoot});
    return audio::bounceGraphToWav(
        graph, bounceOptionsForPath(resolvedOptions.outputPath, resolvedOptions));
}

std::vector<StemExportResult> exportProjectStemsToWav(const ProjectManifest& manifest,
                                                      const MixerState& mixer,
                                                      const StemExportOptions& options) {
    if (options.outputDirectory.empty()) {
        throw std::invalid_argument("Stem export output directory must not be empty");
    }
    if (options.trackIds.empty()) {
        throw std::invalid_argument("Stem export requires at least one track");
    }

    std::vector<StemExportResult> results;
    results.reserve(options.trackIds.size());

    for (const auto& trackId : options.trackIds) {
        if (!hasTrack(manifest, trackId)) {
            throw std::runtime_error("Stem export references unknown track: " + trackId);
        }

        auto graph =
            compileProjectAudioGraph(manifest, mixer, {.projectRoot = options.projectRoot});
        graph.outputNodeId = trackNodeId(trackId);
        if (!hasNode(graph, graph.outputNodeId)) {
            throw std::runtime_error("Stem export graph is missing track node: " + trackId);
        }

        const auto outputPath = options.outputDirectory / (trackId + ".wav");
        const auto projectOptions = projectOptionsForStem(options, outputPath);
        const auto resolvedOptions = resolveProjectExportOptions(manifest, projectOptions);
        results.push_back(
            {.trackId = trackId,
             .bounce = audio::bounceGraphToWav(
                 graph, bounceOptionsForPath(outputPath, resolvedOptions))});
    }

    return results;
}

} // namespace lamusica::session
