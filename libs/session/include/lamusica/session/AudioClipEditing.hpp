#pragma once

#include "lamusica/audio/AudioEngine.hpp"
#include "lamusica/session/ProjectManifest.hpp"

#include <cstdint>

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

[[nodiscard]] float clipFadeGain(const Clip& clip, std::int64_t clipRelativeSample) noexcept;
[[nodiscard]] float clipPeakAmplitude(const Clip& clip, const audio::RenderedAudio& source);
[[nodiscard]] ClipNormalizeResult
normalizeClipGain(const Clip& clip, const audio::RenderedAudio& source, float targetPeak);
[[nodiscard]] audio::RenderedAudio renderClipRegion(const Clip& clip,
                                                    const audio::RenderedAudio& source);
[[nodiscard]] audio::RenderedAudio renderLinearCrossfade(const audio::RenderedAudio& left,
                                                         const audio::RenderedAudio& right,
                                                         CrossfadeRenderOptions options);
[[nodiscard]] float maxAdjacentSampleDelta(const audio::RenderedAudio& audio) noexcept;

} // namespace lamusica::session
