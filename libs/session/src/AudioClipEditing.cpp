#include "lamusica/session/AudioClipEditing.hpp"

#include "lamusica/session/GraphCompiler.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace lamusica::session {
namespace {

void validateClipRenderRange(const Clip& clip, const audio::RenderedAudio& source) {
    if (source.channels == 0U) {
        throw std::runtime_error("Clip render source must have at least one channel");
    }
    if (clip.lengthSamples < 0 || clip.sourceOffsetSamples < 0) {
        throw std::runtime_error("Clip render range must not be negative");
    }
    if (clip.fadeInSamples < 0 || clip.fadeOutSamples < 0 ||
        clip.fadeInSamples + clip.fadeOutSamples > clip.lengthSamples) {
        throw std::runtime_error("Clip render fades are invalid");
    }
    if (clip.sourceOffsetSamples + clip.lengthSamples > source.frames) {
        throw std::runtime_error("Clip render range exceeds source audio");
    }
}

float linearToDb(float gain) noexcept {
    if (gain <= 0.0F) {
        return -120.0F;
    }
    return 20.0F * std::log10(gain);
}

} // namespace

float clipFadeGain(const Clip& clip, std::int64_t clipRelativeSample) noexcept {
    if (clip.lengthSamples <= 0 || clipRelativeSample < 0 ||
        clipRelativeSample >= clip.lengthSamples) {
        return 0.0F;
    }

    float gain = 1.0F;
    if (clip.fadeInSamples > 0 && clipRelativeSample < clip.fadeInSamples) {
        gain *= static_cast<float>(clipRelativeSample) / static_cast<float>(clip.fadeInSamples);
    }

    if (clip.fadeOutSamples > 0) {
        const auto fadeOutStart = clip.lengthSamples - clip.fadeOutSamples;
        if (clipRelativeSample >= fadeOutStart) {
            const auto remaining = clip.lengthSamples - clipRelativeSample - 1;
            gain *= std::max(0.0F, static_cast<float>(remaining) /
                                       static_cast<float>(clip.fadeOutSamples));
        }
    }

    return gain;
}

float clipPeakAmplitude(const Clip& clip, const audio::RenderedAudio& source) {
    validateClipRenderRange(clip, source);

    const auto linearGain = clip.muted ? 0.0F : dbToLinearGain(clip.gainDb);
    float peak = 0.0F;
    for (std::int64_t frame = 0; frame < clip.lengthSamples; ++frame) {
        const auto sourceFrame = clip.reversed
                                     ? clip.sourceOffsetSamples + clip.lengthSamples - frame - 1
                                     : clip.sourceOffsetSamples + frame;
        const auto fadeGain = clipFadeGain(clip, frame) * linearGain;
        for (std::uint32_t channel = 0; channel < source.channels; ++channel) {
            peak = std::max(
                peak, std::abs(source.interleavedSamples[static_cast<std::size_t>(sourceFrame) *
                                                             source.channels +
                                                         channel] *
                               fadeGain));
        }
    }
    return peak;
}

ClipNormalizeResult normalizeClipGain(const Clip& clip, const audio::RenderedAudio& source,
                                      float targetPeak) {
    if (!std::isfinite(targetPeak) || targetPeak <= 0.0F || targetPeak > 1.0F) {
        throw std::runtime_error("Clip normalize target peak must be within (0, 1]");
    }
    const auto peak = clipPeakAmplitude(clip, source);
    if (peak <= 0.0F) {
        return {.clip = clip, .sourcePeak = peak, .targetPeak = targetPeak, .gainDeltaDb = 0.0F};
    }

    auto normalized = clip;
    const auto gainDeltaDb = linearToDb(targetPeak / peak);
    normalized.gainDb += gainDeltaDb;
    return {.clip = std::move(normalized),
            .sourcePeak = peak,
            .targetPeak = targetPeak,
            .gainDeltaDb = gainDeltaDb};
}

audio::RenderedAudio renderClipRegion(const Clip& clip, const audio::RenderedAudio& source) {
    validateClipRenderRange(clip, source);

    audio::RenderedAudio rendered{
        .channels = source.channels,
        .frames = static_cast<std::uint32_t>(clip.lengthSamples),
        .interleavedSamples =
            std::vector<float>(static_cast<std::size_t>(clip.lengthSamples) * source.channels)};
    const auto linearGain = clip.muted ? 0.0F : dbToLinearGain(clip.gainDb);

    for (std::int64_t frame = 0; frame < clip.lengthSamples; ++frame) {
        const auto sourceFrame = clip.reversed
                                     ? clip.sourceOffsetSamples + clip.lengthSamples - frame - 1
                                     : clip.sourceOffsetSamples + frame;
        const auto fadeGain = clipFadeGain(clip, frame) * linearGain;
        for (std::uint32_t channel = 0; channel < source.channels; ++channel) {
            rendered
                .interleavedSamples[static_cast<std::size_t>(frame) * source.channels + channel] =
                source.interleavedSamples[static_cast<std::size_t>(sourceFrame) * source.channels +
                                          channel] *
                fadeGain;
        }
    }

    return rendered;
}

audio::RenderedAudio renderLinearCrossfade(const audio::RenderedAudio& left,
                                           const audio::RenderedAudio& right,
                                           CrossfadeRenderOptions options) {
    if (left.channels == 0U || left.channels != right.channels) {
        throw std::runtime_error("Crossfade sources must have matching non-zero channel counts");
    }
    if (options.samples <= 0) {
        throw std::runtime_error("Crossfade length must be positive");
    }
    if (options.samples > left.frames || options.samples > right.frames) {
        throw std::runtime_error("Crossfade length must fit inside both sources");
    }

    audio::RenderedAudio rendered{.channels = left.channels,
                                  .frames = static_cast<std::uint32_t>(options.samples),
                                  .interleavedSamples = std::vector<float>(
                                      static_cast<std::size_t>(options.samples) * left.channels)};

    for (std::int64_t frame = 0; frame < options.samples; ++frame) {
        const auto denominator = std::max<std::int64_t>(1, options.samples - 1);
        const auto rightGain = static_cast<float>(frame) / static_cast<float>(denominator);
        const auto leftGain = 1.0F - rightGain;
        const auto leftFrame = static_cast<std::int64_t>(left.frames) - options.samples + frame;
        for (std::uint32_t channel = 0; channel < left.channels; ++channel) {
            const auto leftSample =
                left.interleavedSamples[static_cast<std::size_t>(leftFrame) * left.channels +
                                        channel];
            const auto rightSample =
                right
                    .interleavedSamples[static_cast<std::size_t>(frame) * right.channels + channel];
            rendered.interleavedSamples[static_cast<std::size_t>(frame) * left.channels + channel] =
                leftSample * leftGain + rightSample * rightGain;
        }
    }

    return rendered;
}

float maxAdjacentSampleDelta(const audio::RenderedAudio& audio) noexcept {
    float maxDelta = 0.0F;
    if (audio.channels == 0U || audio.frames < 2U) {
        return maxDelta;
    }

    for (std::uint32_t frame = 1; frame < audio.frames; ++frame) {
        for (std::uint32_t channel = 0; channel < audio.channels; ++channel) {
            const auto current =
                audio
                    .interleavedSamples[static_cast<std::size_t>(frame) * audio.channels + channel];
            const auto previous =
                audio.interleavedSamples[static_cast<std::size_t>(frame - 1U) * audio.channels +
                                         channel];
            maxDelta = std::max(maxDelta, std::abs(current - previous));
        }
    }

    return maxDelta;
}

} // namespace lamusica::session
