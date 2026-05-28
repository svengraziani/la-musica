#pragma once

#include "lamusica/audio/AudioEngine.hpp"
#include "lamusica/session/ProjectManifest.hpp"

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace lamusica::session {

struct CrossfadeRenderOptions {
    std::int64_t samples{0};
};

struct ClipNormalizeResult {
    Clip clip;
    float sourcePeak{0.0F};
    float targetPeak{0.0F};
    float gainDeltaDb{0.0F};
};

struct ClipEnvelopePoint {
    std::int64_t samplePosition{0};
    float gain{1.0F};
};

struct ClipGainEnvelope {
    std::string clipId;
    std::vector<ClipEnvelopePoint> points;
};

struct ClipTake {
    std::string id;
    std::string name;
    std::int64_t sourceOffsetSamples{0};
    std::int64_t lengthSamples{0};
    bool muted{false};
};

struct ClipTakeLane {
    std::string clipId;
    std::vector<ClipTake> takes;
};

struct ClipCompSegment {
    std::string takeId;
    std::int64_t clipStartSample{0};
    std::int64_t lengthSamples{0};
    std::int64_t takeSourceOffsetSamples{0};
};

struct ClipComp {
    std::string clipId;
    std::vector<ClipCompSegment> segments;
};

struct ClipTakeSource {
    std::string takeId;
    audio::RenderedAudio audio;
};

[[nodiscard]] float clipFadeGain(const Clip& clip, std::int64_t clipRelativeSample) noexcept;
[[nodiscard]] float evaluateClipEnvelope(const ClipGainEnvelope& envelope,
                                         std::int64_t clipRelativeSample);
void validateClipTakeLane(const Clip& clip, const ClipTakeLane& takeLane, const ClipComp& comp,
                          std::span<const ClipTakeSource> sources);
[[nodiscard]] float clipPeakAmplitude(const Clip& clip, const audio::RenderedAudio& source);
[[nodiscard]] ClipNormalizeResult
normalizeClipGain(const Clip& clip, const audio::RenderedAudio& source, float targetPeak);
[[nodiscard]] audio::RenderedAudio renderClipRegion(const Clip& clip,
                                                    const audio::RenderedAudio& source);
[[nodiscard]] audio::RenderedAudio renderClipRegionWithEnvelope(const Clip& clip,
                                                                const audio::RenderedAudio& source,
                                                                const ClipGainEnvelope& envelope);
[[nodiscard]] audio::RenderedAudio renderCompedClip(const Clip& clip, const ClipTakeLane& takeLane,
                                                    const ClipComp& comp,
                                                    std::span<const ClipTakeSource> sources);
[[nodiscard]] audio::RenderedAudio renderLinearCrossfade(const audio::RenderedAudio& left,
                                                         const audio::RenderedAudio& right,
                                                         CrossfadeRenderOptions options);
[[nodiscard]] float maxAdjacentSampleDelta(const audio::RenderedAudio& audio) noexcept;

} // namespace lamusica::session
