#include "lamusica/session/AudioClipEditing.hpp"

#include "lamusica/session/GraphCompiler.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
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

void validateClipEnvelope(const Clip& clip, const ClipGainEnvelope& envelope) {
    if (!envelope.clipId.empty() && envelope.clipId != clip.id) {
        throw std::runtime_error("Clip envelope references a different clip");
    }

    std::int64_t previousSample = -1;
    for (const auto& point : envelope.points) {
        if (point.samplePosition < 0 || point.samplePosition > clip.lengthSamples) {
            throw std::runtime_error("Clip envelope point is outside clip range");
        }
        if (point.samplePosition < previousSample) {
            throw std::runtime_error("Clip envelope points must be sorted by sample position");
        }
        if (!std::isfinite(point.gain) || point.gain < 0.0F) {
            throw std::runtime_error("Clip envelope gain must be finite and non-negative");
        }
        previousSample = point.samplePosition;
    }
}

float linearToDb(float gain) noexcept {
    if (gain <= 0.0F) {
        return -120.0F;
    }
    return 20.0F * std::log10(gain);
}

const ClipTake* findTake(const ClipTakeLane& takeLane, std::string_view takeId) noexcept {
    const auto found = std::ranges::find_if(
        takeLane.takes, [takeId](const ClipTake& take) { return take.id == takeId; });
    return found == takeLane.takes.end() ? nullptr : &*found;
}

const ClipTakeSource* findTakeSource(std::span<const ClipTakeSource> sources,
                                     std::string_view takeId) noexcept {
    const auto found = std::ranges::find_if(
        sources, [takeId](const ClipTakeSource& source) { return source.takeId == takeId; });
    return found == sources.end() ? nullptr : &*found;
}

float sourceSampleAt(const audio::RenderedAudio& source, std::int64_t frame,
                     std::uint32_t channel) noexcept {
    if (source.frames == 0U || source.channels == 0U) {
        return 0.0F;
    }
    const auto clampedFrame = std::clamp<std::int64_t>(frame, 0, source.frames - 1);
    return source.interleavedSamples[static_cast<std::size_t>(clampedFrame) * source.channels +
                                     channel];
}

std::int64_t compBoundaryCrossfadeSamples(const ClipCompSegment& left,
                                          const ClipCompSegment& right) noexcept {
    constexpr std::int64_t defaultCrossfadeSamples = 64;
    return std::max<std::int64_t>(
        0, std::min({defaultCrossfadeSamples, left.lengthSamples, right.lengthSamples}));
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

float evaluateClipEnvelope(const ClipGainEnvelope& envelope, std::int64_t clipRelativeSample) {
    if (envelope.points.empty()) {
        return 1.0F;
    }
    if (clipRelativeSample <= envelope.points.front().samplePosition) {
        return envelope.points.front().gain;
    }

    for (std::size_t index = 1; index < envelope.points.size(); ++index) {
        const auto& left = envelope.points[index - 1U];
        const auto& right = envelope.points[index];
        if (clipRelativeSample <= right.samplePosition) {
            const auto distance = right.samplePosition - left.samplePosition;
            if (distance <= 0) {
                return right.gain;
            }
            const auto ratio = static_cast<float>(clipRelativeSample - left.samplePosition) /
                               static_cast<float>(distance);
            return left.gain + ((right.gain - left.gain) * ratio);
        }
    }

    return envelope.points.back().gain;
}

void validateClipTakeLane(const Clip& clip, const ClipTakeLane& takeLane, const ClipComp& comp,
                          std::span<const ClipTakeSource> sources) {
    if (!takeLane.clipId.empty() && takeLane.clipId != clip.id) {
        throw std::runtime_error("Clip take lane references a different clip");
    }
    if (!comp.clipId.empty() && comp.clipId != clip.id) {
        throw std::runtime_error("Clip comp references a different clip");
    }
    if (clip.lengthSamples < 0) {
        throw std::runtime_error("Clip comp range must not be negative");
    }
    if (takeLane.takes.empty()) {
        throw std::runtime_error("Clip take lane must contain at least one take");
    }
    if (sources.empty()) {
        throw std::runtime_error("Clip take lane render requires at least one take source");
    }

    std::vector<std::string> takeIds;
    for (const auto& take : takeLane.takes) {
        if (take.id.empty() || take.name.empty()) {
            throw std::runtime_error("Clip take id and name are required");
        }
        if (std::ranges::find(takeIds, take.id) != takeIds.end()) {
            throw std::runtime_error("Duplicate clip take id: " + take.id);
        }
        if (take.sourceOffsetSamples < 0 || take.lengthSamples < 0) {
            throw std::runtime_error("Clip take ranges must not be negative");
        }
        takeIds.push_back(take.id);
    }

    std::uint32_t channelCount = 0;
    for (const auto& source : sources) {
        if (source.audio.channels == 0U) {
            throw std::runtime_error("Clip take source must have at least one channel");
        }
        if (channelCount == 0U) {
            channelCount = source.audio.channels;
        } else if (channelCount != source.audio.channels) {
            throw std::runtime_error("Clip take sources must have matching channel counts");
        }
    }

    const ClipCompSegment* previousSegment = nullptr;
    std::int64_t previousSegmentEnd = 0;
    for (const auto& segment : comp.segments) {
        const auto* take = findTake(takeLane, segment.takeId);
        if (take == nullptr) {
            throw std::runtime_error("Clip comp references missing take id: " + segment.takeId);
        }
        const auto* source = findTakeSource(sources, segment.takeId);
        if (source == nullptr) {
            throw std::runtime_error("Clip comp references missing take source: " + segment.takeId);
        }
        if (segment.clipStartSample < 0 || segment.lengthSamples <= 0 ||
            segment.takeSourceOffsetSamples < 0) {
            throw std::runtime_error("Clip comp segment ranges are invalid");
        }
        if (segment.clipStartSample < previousSegmentEnd) {
            if (previousSegment == nullptr ||
                previousSegmentEnd - segment.clipStartSample >
                    compBoundaryCrossfadeSamples(*previousSegment, segment)) {
                throw std::runtime_error(
                    "Clip comp segments must be sorted and overlap only within the crossfade");
            }
        }
        const auto clipEnd = segment.clipStartSample + segment.lengthSamples;
        if (clipEnd > clip.lengthSamples) {
            throw std::runtime_error("Clip comp segment extends beyond clip length");
        }
        const auto takeEnd = segment.takeSourceOffsetSamples + segment.lengthSamples;
        if (segment.takeSourceOffsetSamples < take->sourceOffsetSamples ||
            takeEnd > take->sourceOffsetSamples + take->lengthSamples ||
            takeEnd > source->audio.frames) {
            throw std::runtime_error("Clip comp segment extends beyond take source");
        }
        previousSegment = &segment;
        previousSegmentEnd = std::max(previousSegmentEnd, clipEnd);
    }
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

audio::RenderedAudio renderClipRegionWithEnvelope(const Clip& clip,
                                                  const audio::RenderedAudio& source,
                                                  const ClipGainEnvelope& envelope) {
    validateClipRenderRange(clip, source);
    validateClipEnvelope(clip, envelope);

    auto rendered = renderClipRegion(clip, source);
    for (std::int64_t frame = 0; frame < clip.lengthSamples; ++frame) {
        const auto envelopeGain = evaluateClipEnvelope(envelope, frame);
        for (std::uint32_t channel = 0; channel < rendered.channels; ++channel) {
            rendered.interleavedSamples[static_cast<std::size_t>(frame) * rendered.channels +
                                        channel] *= envelopeGain;
        }
    }
    return rendered;
}

audio::RenderedAudio renderCompedClip(const Clip& clip, const ClipTakeLane& takeLane,
                                      const ClipComp& comp,
                                      std::span<const ClipTakeSource> sources) {
    validateClipTakeLane(clip, takeLane, comp, sources);

    const auto channels = sources.empty() ? 0U : sources.front().audio.channels;
    audio::RenderedAudio rendered{.channels = channels,
                                  .frames = static_cast<std::uint32_t>(clip.lengthSamples),
                                  .interleavedSamples = std::vector<float>(
                                      static_cast<std::size_t>(clip.lengthSamples) * channels)};
    const auto linearGain = clip.muted ? 0.0F : dbToLinearGain(clip.gainDb);

    for (std::size_t segmentIndex = 0; segmentIndex < comp.segments.size(); ++segmentIndex) {
        const auto& segment = comp.segments[segmentIndex];
        const auto* take = findTake(takeLane, segment.takeId);
        const auto* source = findTakeSource(sources, segment.takeId);
        if (take == nullptr || source == nullptr || take->muted) {
            continue;
        }
        for (std::int64_t frame = 0; frame < segment.lengthSamples; ++frame) {
            const auto clipFrame = segment.clipStartSample + frame;
            const auto sourceFrame = segment.takeSourceOffsetSamples + frame;
            const auto fadeGain = clipFadeGain(clip, clipFrame) * linearGain;
            for (std::uint32_t channel = 0; channel < channels; ++channel) {
                rendered
                    .interleavedSamples[static_cast<std::size_t>(clipFrame) * channels + channel] =
                    source->audio
                        .interleavedSamples[static_cast<std::size_t>(sourceFrame) * channels +
                                            channel] *
                    fadeGain;
            }
        }

        if (segmentIndex == 0U) {
            continue;
        }
        const auto& previousSegment = comp.segments[segmentIndex - 1U];
        const auto* previousTake = findTake(takeLane, previousSegment.takeId);
        const auto* previousSource = findTakeSource(sources, previousSegment.takeId);
        if (previousTake == nullptr || previousSource == nullptr || previousTake->muted) {
            continue;
        }
        const auto crossfadeSamples =
            compBoundaryCrossfadeSamples(previousSegment, segment);
        if (crossfadeSamples <= 1) {
            continue;
        }
        for (std::int64_t frame = 0; frame < crossfadeSamples; ++frame) {
            const auto clipFrame = segment.clipStartSample + frame;
            if (clipFrame >= clip.lengthSamples) {
                break;
            }
            const auto denominator = static_cast<float>(crossfadeSamples - 1);
            const auto rightRatio = static_cast<float>(frame) / denominator;
            const auto rightGain = std::sin(rightRatio * std::numbers::pi_v<float> * 0.5F);
            const auto leftGain = std::cos(rightRatio * std::numbers::pi_v<float> * 0.5F);
            const auto clipGain = clipFadeGain(clip, clipFrame) * linearGain;
            const auto previousSegmentEnd =
                previousSegment.clipStartSample + previousSegment.lengthSamples;
            const auto previousLocalFrame =
                segment.clipStartSample >= previousSegmentEnd
                    ? previousSegment.lengthSamples - crossfadeSamples + frame
                    : clipFrame - previousSegment.clipStartSample;
            const auto leftFrame = previousSegment.takeSourceOffsetSamples + previousLocalFrame;
            const auto rightFrame = segment.takeSourceOffsetSamples + frame;
            for (std::uint32_t channel = 0; channel < channels; ++channel) {
                const auto leftSample = sourceSampleAt(previousSource->audio, leftFrame, channel);
                const auto rightSample = sourceSampleAt(source->audio, rightFrame, channel);
                rendered
                    .interleavedSamples[static_cast<std::size_t>(clipFrame) * channels + channel] =
                    ((leftSample * leftGain) + (rightSample * rightGain)) * clipGain;
            }
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
