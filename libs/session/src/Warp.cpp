#include "lamusica/session/Warp.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <numbers>
#include <sstream>
#include <stdexcept>

namespace lamusica::session {
namespace {

void validateSourceAudio(const audio::RenderedAudio& source) {
    if (source.channels == 0U || source.frames == 0U) {
        throw std::runtime_error("Warp render source audio must not be empty");
    }
    if (source.interleavedSamples.size() !=
        static_cast<std::size_t>(source.channels) * source.frames) {
        throw std::runtime_error("Warp render source audio size does not match channel layout");
    }
}

float sampleFrameChannel(const audio::RenderedAudio& source, std::int64_t frame,
                         std::uint32_t channel) {
    const auto clampedFrame =
        std::clamp<std::int64_t>(frame, 0, static_cast<std::int64_t>(source.frames) - 1);
    return source
        .interleavedSamples[static_cast<std::size_t>(clampedFrame) * source.channels + channel];
}

float cubicInterpolate(float previous, float current, float next, float following,
                       double position) noexcept {
    const auto a = (-0.5F * previous) + (1.5F * current) - (1.5F * next) + (0.5F * following);
    const auto b = previous - (2.5F * current) + (2.0F * next) - (0.5F * following);
    const auto c = (-0.5F * previous) + (0.5F * next);
    const auto d = current;
    return static_cast<float>(((a * position + b) * position + c) * position + d);
}

float interpolatedSample(const audio::RenderedAudio& source, double sourcePosition,
                         std::uint32_t channel, StretchQuality quality) {
    switch (quality) {
    case StretchQuality::Preview:
        return sampleFrameChannel(source, static_cast<std::int64_t>(std::llround(sourcePosition)),
                                  channel);
    case StretchQuality::Balanced: {
        const auto left = static_cast<std::int64_t>(std::floor(sourcePosition));
        const auto right = left + 1;
        const auto position = sourcePosition - static_cast<double>(left);
        return sampleFrameChannel(source, left, channel) +
               static_cast<float>((sampleFrameChannel(source, right, channel) -
                                   sampleFrameChannel(source, left, channel)) *
                                  position);
    }
    case StretchQuality::High: {
        const auto current = static_cast<std::int64_t>(std::floor(sourcePosition));
        const auto position = sourcePosition - static_cast<double>(current);
        return cubicInterpolate(sampleFrameChannel(source, current - 1, channel),
                                sampleFrameChannel(source, current, channel),
                                sampleFrameChannel(source, current + 1, channel),
                                sampleFrameChannel(source, current + 2, channel), position);
    }
    }
    return sampleFrameChannel(source, static_cast<std::int64_t>(std::llround(sourcePosition)),
                              channel);
}

} // namespace

std::int64_t conformSampleToTempo(std::int64_t sourceSample, double sourceTempoBpm,
                                  double targetTempoBpm) noexcept {
    if (sourceTempoBpm <= 0.0 || targetTempoBpm <= 0.0) {
        return sourceSample;
    }

    const auto ratio = sourceTempoBpm / targetTempoBpm;
    return static_cast<std::int64_t>(std::llround(static_cast<double>(sourceSample) * ratio));
}

std::int64_t quantizeSampleToGrid(std::int64_t sample, std::int64_t gridSamples, float strength) {
    if (sample < 0 || gridSamples <= 0) {
        throw std::runtime_error("Quantize sample and grid must be non-negative");
    }
    const auto clampedStrength = std::clamp(strength, 0.0F, 1.0F);
    const auto grid = static_cast<double>(gridSamples);
    const auto target = static_cast<std::int64_t>(
        std::llround(std::round(static_cast<double>(sample) / grid) * grid));
    return sample + static_cast<std::int64_t>(
                        std::llround(static_cast<double>(target - sample) * clampedStrength));
}

std::int64_t mapSourceToTimeline(const WarpState& warp, std::int64_t sourceSample) {
    if (!warp.enabled || warp.markers.size() < 2) {
        return conformSampleToTempo(sourceSample, warp.sourceTempoBpm, warp.targetTempoBpm);
    }

    auto markers = warp.markers;
    std::ranges::sort(markers, {}, &WarpMarker::sourceSample);

    if (sourceSample <= markers.front().sourceSample) {
        return markers.front().timelineSample;
    }

    for (std::size_t index = 0; index + 1 < markers.size(); ++index) {
        const auto& left = markers[index];
        const auto& right = markers[index + 1];
        if (sourceSample < left.sourceSample || sourceSample > right.sourceSample) {
            continue;
        }

        const auto sourceSpan = right.sourceSample - left.sourceSample;
        if (sourceSpan == 0) {
            return left.timelineSample;
        }

        const auto position =
            static_cast<double>(sourceSample - left.sourceSample) / static_cast<double>(sourceSpan);
        return left.timelineSample +
               static_cast<std::int64_t>(std::llround(
                   position * static_cast<double>(right.timelineSample - left.timelineSample)));
    }

    return markers.back().timelineSample;
}

WarpState retargetWarpTempo(const WarpState& warp, double newTargetTempoBpm) {
    validateWarpState(warp);
    if (newTargetTempoBpm <= 0.0) {
        throw std::runtime_error("Warp target tempo must be positive");
    }

    auto retargeted = warp;
    const auto oldTargetTempo = warp.targetTempoBpm;
    retargeted.targetTempoBpm = newTargetTempoBpm;
    if (!retargeted.markers.empty()) {
        const auto ratio = oldTargetTempo / newTargetTempoBpm;
        for (auto& marker : retargeted.markers) {
            marker.timelineSample = static_cast<std::int64_t>(
                std::llround(static_cast<double>(marker.timelineSample) * ratio));
        }
    }
    return retargeted;
}

WarpState conformWarpToTempo(const WarpState& warp, std::span<const Transient> transients,
                             std::int64_t sourceEndSample, double newTargetTempoBpm) {
    validateWarpState(warp);
    if (sourceEndSample <= 0) {
        throw std::runtime_error("Warp conform source end must be positive");
    }
    if (newTargetTempoBpm <= 0.0) {
        throw std::runtime_error("Warp target tempo must be positive");
    }

    auto conformed = retargetWarpTempo(warp, newTargetTempoBpm);
    conformed.enabled = true;

    const auto appendMarker = [&conformed](std::string id, std::int64_t sourceSample,
                                           std::int64_t timelineSample) {
        if (sourceSample < 0 || timelineSample < 0) {
            return;
        }
        const auto existing =
            std::ranges::find_if(conformed.markers, [sourceSample](const WarpMarker& marker) {
                return marker.sourceSample == sourceSample;
            });
        if (existing == conformed.markers.end()) {
            conformed.markers.push_back({.id = std::move(id),
                                         .sourceSample = sourceSample,
                                         .timelineSample = timelineSample});
        }
    };

    appendMarker(conformed.clipId + "-warp-start", 0,
                 conformSampleToTempo(0, conformed.sourceTempoBpm, conformed.targetTempoBpm));
    for (const auto& transient : transients) {
        if (transient.sourceSample <= 0 || transient.sourceSample >= sourceEndSample) {
            continue;
        }
        appendMarker(conformed.clipId + "-warp-" + std::to_string(transient.sourceSample),
                     transient.sourceSample,
                     conformSampleToTempo(transient.sourceSample, conformed.sourceTempoBpm,
                                          conformed.targetTempoBpm));
    }
    appendMarker(
        conformed.clipId + "-warp-end", sourceEndSample,
        conformSampleToTempo(sourceEndSample, conformed.sourceTempoBpm, conformed.targetTempoBpm));

    std::ranges::sort(conformed.markers, {}, &WarpMarker::sourceSample);
    return conformed;
}

std::vector<Transient> detectTransients(std::span<const float> monoSamples, float threshold) {
    std::vector<Transient> transients;
    if (monoSamples.size() < 2) {
        return transients;
    }

    for (std::size_t index = 1; index < monoSamples.size(); ++index) {
        const auto delta = std::abs(monoSamples[index] - monoSamples[index - 1]);
        if (delta >= threshold) {
            transients.push_back(
                {.sourceSample = static_cast<std::int64_t>(index), .strength = delta});
        }
    }
    return transients;
}

std::vector<BeatSlice> makeBeatSlices(const WarpState& warp, std::span<const Transient> transients,
                                      std::int64_t sourceEndSample) {
    validateWarpState(warp);
    if (sourceEndSample <= 0) {
        throw std::runtime_error("Beat slice source end must be positive");
    }

    std::vector<Transient> sorted{transients.begin(), transients.end()};
    std::ranges::sort(sorted, {}, &Transient::sourceSample);
    std::erase_if(sorted, [sourceEndSample](const Transient& transient) {
        return transient.sourceSample < 0 || transient.sourceSample >= sourceEndSample;
    });

    std::vector<BeatSlice> slices;
    slices.reserve(sorted.size());
    for (std::size_t index = 0; index < sorted.size(); ++index) {
        const auto start = sorted[index].sourceSample;
        const auto end =
            index + 1 < sorted.size() ? sorted[index + 1].sourceSample : sourceEndSample;
        if (end <= start) {
            continue;
        }
        slices.push_back({.id = warp.clipId + "-slice-" + std::to_string(index + 1U),
                          .sourceStartSample = start,
                          .sourceEndSample = end,
                          .timelineStartSample = mapSourceToTimeline(warp, start)});
    }
    return slices;
}

GrooveTemplate extractGroove(std::string id, std::span<const Transient> transients,
                             std::int64_t gridSamples) {
    if (id.empty()) {
        throw std::runtime_error("Groove id must not be empty");
    }
    if (gridSamples <= 0) {
        throw std::runtime_error("Groove grid must be positive");
    }

    GrooveTemplate groove{.id = std::move(id)};
    groove.points.reserve(transients.size());
    for (const auto& transient : transients) {
        if (transient.sourceSample < 0) {
            continue;
        }
        const auto grid = quantizeSampleToGrid(transient.sourceSample, gridSamples, 1.0F);
        groove.points.push_back({.gridSample = grid,
                                 .offsetSamples = transient.sourceSample - grid,
                                 .strength = transient.strength});
    }
    return groove;
}

void quantizeWarpMarkers(WarpState& warp, std::int64_t gridSamples, float strength) {
    validateWarpState(warp);
    for (auto& marker : warp.markers) {
        marker.timelineSample = quantizeSampleToGrid(marker.timelineSample, gridSamples, strength);
    }
}

void applyGrooveToWarpMarkers(WarpState& warp, const GrooveTemplate& groove, float strength) {
    validateWarpState(warp);
    if (groove.id.empty()) {
        throw std::runtime_error("Groove id must not be empty");
    }
    if (groove.points.empty()) {
        throw std::runtime_error("Groove must contain at least one point");
    }
    if (strength < 0.0F || strength > 1.0F) {
        throw std::runtime_error("Groove strength must be in the range 0..1");
    }

    for (auto& marker : warp.markers) {
        const auto nearest =
            std::ranges::min_element(groove.points, {}, [&marker](const GroovePoint& point) {
                return std::llabs(point.gridSample - marker.timelineSample);
            });
        if (nearest == groove.points.end()) {
            continue;
        }
        const auto target = nearest->gridSample + nearest->offsetSamples;
        marker.timelineSample += static_cast<std::int64_t>(
            std::llround(static_cast<double>(target - marker.timelineSample) * strength));
    }
}

std::string makeWarpCacheKey(const WarpState& warp) {
    std::ostringstream output;
    output << warp.clipId << ":" << warp.enabled << ":" << warp.sourceTempoBpm << ":"
           << warp.targetTempoBpm << ":" << warp.pitchShiftSemitones << ":"
           << static_cast<int>(warp.quality);
    for (const auto& marker : warp.markers) {
        output << ":" << marker.id << "@" << marker.sourceSample << ">" << marker.timelineSample;
    }
    return output.str();
}

double pitchShiftRatio(float semitones) noexcept {
    return std::pow(2.0, static_cast<double>(semitones) / 12.0);
}

const RenderCacheEntry* findValidRenderCacheEntry(std::span<const RenderCacheEntry> cache,
                                                  std::string_view clipId,
                                                  std::string_view cacheKey) noexcept {
    const auto found =
        std::ranges::find_if(cache, [clipId, cacheKey](const RenderCacheEntry& entry) {
            return entry.valid && entry.clipId == clipId && entry.cacheKey == cacheKey;
        });
    return found == cache.end() ? nullptr : &*found;
}

WarpRenderPlan makeWarpRenderPlan(const WarpState& warp, std::span<const RenderCacheEntry> cache,
                                  std::int64_t sourceStartSample, std::int64_t sourceEndSample,
                                  std::string relativePath) {
    validateWarpState(warp);
    if (sourceStartSample < 0 || sourceEndSample <= sourceStartSample) {
        throw std::runtime_error("Warp render source range must be non-empty and non-negative");
    }

    const auto cacheKey = makeWarpCacheKey(warp);
    const auto* cacheEntry = findValidRenderCacheEntry(cache, warp.clipId, cacheKey);
    const auto timelineStart = mapSourceToTimeline(warp, sourceStartSample);
    const auto timelineEnd = mapSourceToTimeline(warp, sourceEndSample);
    const auto sourceDuration = sourceEndSample - sourceStartSample;
    const auto timelineDuration = std::max<std::int64_t>(1, timelineEnd - timelineStart);

    return {
        .clipId = warp.clipId,
        .cacheKey = cacheKey,
        .relativePath = cacheEntry != nullptr ? cacheEntry->relativePath : std::move(relativePath),
        .sourceStartSample = sourceStartSample,
        .sourceEndSample = sourceEndSample,
        .timelineStartSample = timelineStart,
        .timelineEndSample = timelineEnd,
        .stretchRatio = static_cast<double>(timelineDuration) / static_cast<double>(sourceDuration),
        .pitchRatio = pitchShiftRatio(warp.pitchShiftSemitones),
        .quality = warp.quality,
        .cacheHit = cacheEntry != nullptr};
}

audio::RenderedAudio renderWarpedAudio(const audio::RenderedAudio& source,
                                       const WarpRenderPlan& plan) {
    validateSourceAudio(source);
    if (plan.sourceStartSample < 0 || plan.sourceEndSample <= plan.sourceStartSample ||
        plan.sourceEndSample > source.frames) {
        throw std::runtime_error("Warp render source range is outside source audio");
    }
    if (plan.timelineEndSample <= plan.timelineStartSample || plan.stretchRatio <= 0.0 ||
        plan.pitchRatio <= 0.0) {
        throw std::runtime_error("Warp render plan timing and ratios must be positive");
    }

    const auto outputFrames64 = plan.timelineEndSample - plan.timelineStartSample;
    if (outputFrames64 > std::numeric_limits<std::uint32_t>::max()) {
        throw std::runtime_error("Warp render output is too large");
    }

    audio::RenderedAudio rendered{.channels = source.channels,
                                  .frames = static_cast<std::uint32_t>(outputFrames64),
                                  .interleavedSamples = std::vector<float>(
                                      static_cast<std::size_t>(outputFrames64) * source.channels)};

    for (std::uint32_t frame = 0; frame < rendered.frames; ++frame) {
        const auto sourceOffset =
            (static_cast<double>(frame) / plan.stretchRatio) * plan.pitchRatio;
        const auto sourcePosition =
            std::clamp(static_cast<double>(plan.sourceStartSample) + sourceOffset,
                       static_cast<double>(plan.sourceStartSample),
                       static_cast<double>(plan.sourceEndSample - 1));
        for (std::uint32_t channel = 0; channel < source.channels; ++channel) {
            rendered
                .interleavedSamples[static_cast<std::size_t>(frame) * source.channels + channel] =
                interpolatedSample(source, sourcePosition, channel, plan.quality);
        }
    }

    return rendered;
}

audio::RenderedAudio renderWarpPreview(const audio::RenderedAudio& source,
                                       const WarpRenderPlan& plan) {
    return renderWarpedAudio(source, plan);
}

bool warpRenderPlansAgree(const WarpRenderPlan& offlinePlan, const WarpRenderPlan& previewPlan,
                          std::int64_t sampleTolerance) {
    if (sampleTolerance < 0) {
        throw std::runtime_error("Warp render plan tolerance must not be negative");
    }
    if (offlinePlan.clipId != previewPlan.clipId || offlinePlan.cacheKey != previewPlan.cacheKey ||
        offlinePlan.quality != previewPlan.quality) {
        return false;
    }

    const auto withinTolerance = [sampleTolerance](std::int64_t left, std::int64_t right) {
        return std::llabs(left - right) <= sampleTolerance;
    };
    return withinTolerance(offlinePlan.sourceStartSample, previewPlan.sourceStartSample) &&
           withinTolerance(offlinePlan.sourceEndSample, previewPlan.sourceEndSample) &&
           withinTolerance(offlinePlan.timelineStartSample, previewPlan.timelineStartSample) &&
           withinTolerance(offlinePlan.timelineEndSample, previewPlan.timelineEndSample) &&
           std::abs(offlinePlan.stretchRatio - previewPlan.stretchRatio) <= 0.000001 &&
           std::abs(offlinePlan.pitchRatio - previewPlan.pitchRatio) <= 0.000001;
}

void validateWarpState(const WarpState& warp) {
    if (warp.clipId.empty()) {
        throw std::runtime_error("Warp clip id must not be empty");
    }
    if (warp.sourceTempoBpm <= 0.0 || warp.targetTempoBpm <= 0.0) {
        throw std::runtime_error("Warp tempos must be positive");
    }
    for (const auto& marker : warp.markers) {
        if (marker.id.empty()) {
            throw std::runtime_error("Warp marker id must not be empty");
        }
        if (marker.sourceSample < 0 || marker.timelineSample < 0) {
            throw std::runtime_error("Warp marker samples must not be negative");
        }
    }
}

void upsertRenderCacheEntry(std::vector<RenderCacheEntry>& cache, RenderCacheEntry entry) {
    const auto found = std::ranges::find_if(cache, [&entry](const RenderCacheEntry& existing) {
        return existing.clipId == entry.clipId;
    });
    if (found == cache.end()) {
        cache.push_back(std::move(entry));
    } else {
        *found = std::move(entry);
    }
}

void invalidateRenderCache(std::vector<RenderCacheEntry>& cache, std::string_view clipId) {
    for (auto& entry : cache) {
        if (entry.clipId == clipId) {
            entry.valid = false;
        }
    }
}

} // namespace lamusica::session
