#include "lamusica/audio/Bounce.hpp"

#include "lamusica/audio/AudioGraph.hpp"
#include "lamusica/audio/WavFile.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace lamusica::audio {
namespace {

std::size_t sampleCount(std::uint32_t frames, std::uint32_t channels) noexcept {
    return static_cast<std::size_t>(frames) * static_cast<std::size_t>(channels);
}

std::uint32_t nextRandom(std::uint32_t& state) noexcept {
    state ^= state << 13U;
    state ^= state >> 17U;
    state ^= state << 5U;
    return state;
}

float randomUnit(std::uint32_t& state) noexcept {
    return static_cast<float>(nextRandom(state)) /
           static_cast<float>(std::numeric_limits<std::uint32_t>::max());
}

void validateOptions(const BounceOptions& options) {
    if (options.outputPath.empty()) {
        throw std::invalid_argument("Bounce output path must not be empty");
    }
    if (options.startSample < 0) {
        throw std::invalid_argument("Bounce start sample must not be negative");
    }
    if (options.frames == 0U) {
        throw std::invalid_argument("Bounce frame count must be positive");
    }
    if (options.sampleRate <= 0.0) {
        throw std::invalid_argument("Bounce sample rate must be positive");
    }
    if (options.channels == 0U) {
        throw std::invalid_argument("Bounce channel count must be positive");
    }
    if (options.normalizeTargetPeak <= 0.0F || options.normalizeTargetPeak > 1.0F) {
        throw std::invalid_argument("Bounce normalization target must be in (0, 1]");
    }
    switch (options.bitDepth) {
    case ExportBitDepth::Pcm16:
        break;
    case ExportBitDepth::Pcm24:
        break;
    }
}

} // namespace

float peakAbsoluteSample(const RenderedAudio& audio) noexcept {
    float peak = 0.0F;
    for (const auto sample : audio.interleavedSamples) {
        peak = std::max(peak, std::abs(sample));
    }
    return peak;
}

void normalizePeak(RenderedAudio& audio, float targetPeak) {
    if (targetPeak <= 0.0F || targetPeak > 1.0F) {
        throw std::invalid_argument("Normalization target must be in (0, 1]");
    }

    const auto peak = peakAbsoluteSample(audio);
    if (peak <= 0.0F) {
        return;
    }

    const auto gain = targetPeak / peak;
    for (auto& sample : audio.interleavedSamples) {
        sample = std::clamp(sample * gain, -1.0F, 1.0F);
    }
}

void applyDither(RenderedAudio& audio, DitherMode mode, ExportBitDepth bitDepth) {
    if (mode == DitherMode::None) {
        return;
    }

    float leastSignificantBit = 0.0F;
    switch (bitDepth) {
    case ExportBitDepth::Pcm16:
        leastSignificantBit = 1.0F / 32767.0F;
        break;
    case ExportBitDepth::Pcm24:
        leastSignificantBit = 1.0F / 8388607.0F;
        break;
    }

    std::uint32_t state = 0x4C4D5343U;
    for (auto& sample : audio.interleavedSamples) {
        switch (mode) {
        case DitherMode::None:
            break;
        case DitherMode::Triangular: {
            const auto noise = (randomUnit(state) - randomUnit(state)) * leastSignificantBit;
            sample = std::clamp(sample + noise, -1.0F, 1.0F);
            break;
        }
        }
    }
}

RenderedAudio renderGraphRange(const AudioGraph& graph, const BounceOptions& options) {
    validateOptions(options);
    RenderedAudio rendered{options.channels, options.frames,
                           std::vector<float>(sampleCount(options.frames, options.channels))};
    renderGraph(graph,
                {.sampleRate = options.sampleRate,
                 .maxBlockSize = options.frames,
                 .outputChannels = options.channels},
                options.startSample, options.frames, rendered.interleavedSamples);
    return rendered;
}

BounceResult bounceGraphToWav(const AudioGraph& graph, const BounceOptions& options) {
    auto rendered = renderGraphRange(graph, options);
    const auto peakBefore = peakAbsoluteSample(rendered);
    if (options.normalizePeak) {
        normalizePeak(rendered, options.normalizeTargetPeak);
    }
    applyDither(rendered, options.ditherMode, options.bitDepth);
    const auto peakAfter = peakAbsoluteSample(rendered);

    switch (options.bitDepth) {
    case ExportBitDepth::Pcm16:
        writePcm16Wav(options.outputPath, rendered, options.sampleRate);
        break;
    case ExportBitDepth::Pcm24:
        writePcm24Wav(options.outputPath, rendered, options.sampleRate);
        break;
    }

    return {.outputPath = options.outputPath,
            .startSample = options.startSample,
            .frames = rendered.frames,
            .channels = rendered.channels,
            .sampleRate = options.sampleRate,
            .bitDepth = options.bitDepth,
            .ditherMode = options.ditherMode,
            .peakBeforeNormalization = peakBefore,
            .peakAfterNormalization = peakAfter,
            .peakAfterDither = peakAfter};
}

} // namespace lamusica::audio
