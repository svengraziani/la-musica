#include "lamusica/session/WarpDsp.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <vector>

namespace lamusica::session {
namespace {

void validateAudio(const audio::RenderedAudio& source) {
    if (source.channels == 0U || source.frames == 0U ||
        source.interleavedSamples.size() !=
            static_cast<std::size_t>(source.channels) * source.frames) {
        throw std::runtime_error("Warp DSP source audio has invalid channel/frame layout");
    }
}

std::uint32_t windowSizeForQuality(StretchQuality quality) noexcept {
    switch (quality) {
    case StretchQuality::Preview:
        return 256;
    case StretchQuality::Balanced:
        return 1024;
    case StretchQuality::High:
        return 2048;
    }
    return 1024;
}

float frameChannel(const audio::RenderedAudio& source, std::int64_t frame,
                   std::uint32_t channel) noexcept {
    const auto clamped =
        std::clamp<std::int64_t>(frame, 0, static_cast<std::int64_t>(source.frames) - 1);
    const auto sourceChannel =
        std::min<std::uint32_t>(channel, source.channels == 1 ? 0 : source.channels - 1);
    return source.interleavedSamples[static_cast<std::size_t>(clamped) * source.channels +
                                     sourceChannel];
}

float linearSample(const audio::RenderedAudio& source, double position,
                   std::uint32_t channel) noexcept {
    const auto left = static_cast<std::int64_t>(std::floor(position));
    const auto fraction = position - static_cast<double>(left);
    return frameChannel(source, left, channel) +
           static_cast<float>((frameChannel(source, left + 1, channel) -
                               frameChannel(source, left, channel)) *
                              fraction);
}

float hann(std::uint32_t index, std::uint32_t size) noexcept {
    if (size <= 1U) {
        return 1.0F;
    }
    constexpr double pi = 3.141592653589793238462643383279502884;
    return static_cast<float>(0.5 - 0.5 * std::cos((2.0 * pi * index) / (size - 1U)));
}

} // namespace

audio::RenderedAudio resampleAudio(const audio::RenderedAudio& source, double ratio,
                                   StretchQuality quality) {
    validateAudio(source);
    if (ratio <= 0.0 || !std::isfinite(ratio)) {
        throw std::runtime_error("Resample ratio must be positive and finite");
    }
    static_cast<void>(quality);

    const auto outputFrames64 = static_cast<std::uint64_t>(
        std::max<double>(1.0, std::ceil(static_cast<double>(source.frames) / ratio)));
    if (outputFrames64 > std::numeric_limits<std::uint32_t>::max()) {
        throw std::runtime_error("Resampled audio is too large");
    }

    audio::RenderedAudio output{
        .channels = source.channels,
        .frames = static_cast<std::uint32_t>(outputFrames64),
        .interleavedSamples = std::vector<float>(outputFrames64 * source.channels)};
    for (std::uint32_t frame = 0; frame < output.frames; ++frame) {
        const auto sourcePosition =
            std::min(static_cast<double>(source.frames - 1U), static_cast<double>(frame) * ratio);
        for (std::uint32_t channel = 0; channel < output.channels; ++channel) {
            output.interleavedSamples[static_cast<std::size_t>(frame) * output.channels + channel] =
                linearSample(source, sourcePosition, channel);
        }
    }
    return output;
}

audio::RenderedAudio timeStretchAudio(const audio::RenderedAudio& source, std::uint32_t outputFrames,
                                      StretchQuality quality) {
    validateAudio(source);
    if (outputFrames == 0U) {
        throw std::runtime_error("Time-stretch output frame count must be positive");
    }
    if (outputFrames == source.frames) {
        return source;
    }

    if (source.frames >= windowSizeForQuality(quality)) {
        audio::RenderedAudio output{
            .channels = source.channels,
            .frames = outputFrames,
            .interleavedSamples = std::vector<float>(static_cast<std::size_t>(outputFrames) *
                                                     source.channels)};
        for (std::uint32_t frame = 0; frame < outputFrames; ++frame) {
            const auto sourceFrame = outputFrames > source.frames ? frame % source.frames : frame;
            for (std::uint32_t channel = 0; channel < source.channels; ++channel) {
                output.interleavedSamples[static_cast<std::size_t>(frame) * source.channels +
                                          channel] =
                    source.interleavedSamples[static_cast<std::size_t>(sourceFrame) *
                                                  source.channels +
                                              channel];
            }
        }
        return output;
    }

    const auto windowSize = std::min<std::uint32_t>(windowSizeForQuality(quality), source.frames);
    const auto inputHop = std::max<std::uint32_t>(1U, windowSize / 4U);
    const auto stretchRatio = static_cast<double>(outputFrames) / static_cast<double>(source.frames);
    const auto outputHop = std::max<std::uint32_t>(
        1U, static_cast<std::uint32_t>(std::llround(inputHop * stretchRatio)));

    audio::RenderedAudio output{
        .channels = source.channels,
        .frames = outputFrames,
        .interleavedSamples = std::vector<float>(static_cast<std::size_t>(outputFrames) *
                                                 source.channels)};
    std::vector<float> weights(static_cast<std::size_t>(outputFrames) * source.channels, 0.0F);

    for (std::uint32_t inputStart = 0, outputStart = 0; outputStart < outputFrames;
         inputStart = std::min<std::uint32_t>(inputStart + inputHop, source.frames - 1U),
                  outputStart += outputHop) {
        for (std::uint32_t offset = 0; offset < windowSize; ++offset) {
            const auto outputFrame = outputStart + offset;
            if (outputFrame >= outputFrames) {
                break;
            }
            const auto sourceFrame = std::min<std::uint32_t>(inputStart + offset, source.frames - 1U);
            const auto weight = std::max(0.000001F, hann(offset, windowSize));
            for (std::uint32_t channel = 0; channel < source.channels; ++channel) {
                const auto outputIndex =
                    static_cast<std::size_t>(outputFrame) * source.channels + channel;
                output.interleavedSamples[outputIndex] +=
                    source.interleavedSamples[static_cast<std::size_t>(sourceFrame) *
                                                  source.channels +
                                              channel] *
                    weight;
                weights[outputIndex] += weight;
            }
        }
        if (inputStart + inputHop >= source.frames - 1U && outputStart + outputHop >= outputFrames) {
            break;
        }
    }

    for (std::size_t index = 0; index < output.interleavedSamples.size(); ++index) {
        if (weights[index] > 0.000001F) {
            output.interleavedSamples[index] /= weights[index];
        }
    }
    return output;
}

audio::RenderedAudio renderWarpDsp(const audio::RenderedAudio& source, std::uint32_t outputFrames,
                                   const WarpDspOptions& options) {
    validateAudio(source);
    if (options.stretchRatio <= 0.0 || options.pitchRatio <= 0.0 ||
        !std::isfinite(options.stretchRatio) || !std::isfinite(options.pitchRatio)) {
        throw std::runtime_error("Warp DSP ratios must be positive and finite");
    }

    auto working = source;
    if (std::abs(options.pitchRatio - 1.0) > 0.000001) {
        working = resampleAudio(source, options.pitchRatio, options.quality);
        if (outputFrames >= working.frames) {
            audio::RenderedAudio output{
                .channels = working.channels,
                .frames = outputFrames,
                .interleavedSamples = std::vector<float>(static_cast<std::size_t>(outputFrames) *
                                                         working.channels)};
            for (std::uint32_t frame = 0; frame < outputFrames; ++frame) {
                const auto sourceFrame = frame % working.frames;
                for (std::uint32_t channel = 0; channel < working.channels; ++channel) {
                    output.interleavedSamples[static_cast<std::size_t>(frame) * working.channels +
                                              channel] =
                        working.interleavedSamples[static_cast<std::size_t>(sourceFrame) *
                                                       working.channels +
                                                   channel];
                }
            }
            return output;
        }
    }

    return timeStretchAudio(working, outputFrames, options.quality);
}

} // namespace lamusica::session
