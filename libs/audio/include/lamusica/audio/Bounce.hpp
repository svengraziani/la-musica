#pragma once

#include "lamusica/audio/AudioEngine.hpp"

#include <cstdint>
#include <filesystem>

namespace lamusica::audio {

struct AudioGraph;

enum class ExportBitDepth {
    Pcm16,
};

enum class DitherMode {
    None,
    Triangular,
};

struct BounceOptions {
    std::filesystem::path outputPath;
    std::int64_t startSample{0};
    std::uint32_t frames{0};
    double sampleRate{48000.0};
    std::uint32_t channels{2};
    ExportBitDepth bitDepth{ExportBitDepth::Pcm16};
    DitherMode ditherMode{DitherMode::None};
    bool normalizePeak{false};
    float normalizeTargetPeak{0.98F};
};

struct BounceResult {
    std::filesystem::path outputPath;
    std::int64_t startSample{0};
    std::uint32_t frames{0};
    std::uint32_t channels{0};
    double sampleRate{0.0};
    ExportBitDepth bitDepth{ExportBitDepth::Pcm16};
    DitherMode ditherMode{DitherMode::None};
    float peakBeforeNormalization{0.0F};
    float peakAfterNormalization{0.0F};
};

[[nodiscard]] float peakAbsoluteSample(const RenderedAudio& audio) noexcept;
void normalizePeak(RenderedAudio& audio, float targetPeak);
void applyDither(RenderedAudio& audio, DitherMode mode, ExportBitDepth bitDepth);
[[nodiscard]] RenderedAudio renderGraphRange(const AudioGraph& graph, const BounceOptions& options);
[[nodiscard]] BounceResult bounceGraphToWav(const AudioGraph& graph, const BounceOptions& options);

} // namespace lamusica::audio
