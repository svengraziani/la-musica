#pragma once

#include "lamusica/audio/Bounce.hpp"
#include "lamusica/session/GraphCompiler.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace lamusica::session {

struct ProjectExportOptions {
    std::filesystem::path outputPath;
    std::int64_t startSample{0};
    std::uint32_t frames{0};
    double sampleRate{48000.0};
    std::uint32_t channels{2};
    std::filesystem::path projectRoot;
    audio::ExportBitDepth bitDepth{audio::ExportBitDepth::Pcm16};
    audio::DitherMode ditherMode{audio::DitherMode::None};
    bool normalizePeak{false};
    float normalizeTargetPeak{0.98F};
};

struct StemExportOptions {
    std::filesystem::path outputDirectory;
    std::vector<std::string> trackIds;
    std::int64_t startSample{0};
    std::uint32_t frames{0};
    double sampleRate{48000.0};
    std::uint32_t channels{2};
    std::filesystem::path projectRoot;
    audio::ExportBitDepth bitDepth{audio::ExportBitDepth::Pcm16};
    audio::DitherMode ditherMode{audio::DitherMode::None};
    bool normalizePeak{false};
    float normalizeTargetPeak{0.98F};
};

struct StemExportResult {
    std::string trackId;
    audio::BounceResult bounce;
};

[[nodiscard]] ProjectExportOptions
makeLoopMixExportOptions(std::filesystem::path outputPath, const audio::TransportState& transport,
                         double sampleRate = 48000.0, std::uint32_t channels = 2,
                         audio::ExportBitDepth bitDepth = audio::ExportBitDepth::Pcm16,
                         audio::DitherMode ditherMode = audio::DitherMode::None,
                         bool normalizePeak = false, float normalizeTargetPeak = 0.98F);
[[nodiscard]] StemExportOptions makeLoopStemExportOptions(
    std::filesystem::path outputDirectory, std::vector<std::string> selectedTrackIds,
    const audio::TransportState& transport, double sampleRate = 48000.0, std::uint32_t channels = 2,
    audio::ExportBitDepth bitDepth = audio::ExportBitDepth::Pcm16,
    audio::DitherMode ditherMode = audio::DitherMode::None, bool normalizePeak = false,
    float normalizeTargetPeak = 0.98F);
[[nodiscard]] audio::BounceResult exportProjectMixToWav(const ProjectManifest& manifest,
                                                        const MixerState& mixer,
                                                        const ProjectExportOptions& options);
[[nodiscard]] std::vector<StemExportResult>
exportProjectStemsToWav(const ProjectManifest& manifest, const MixerState& mixer,
                        const StemExportOptions& options);

} // namespace lamusica::session
