#pragma once

#include "lamusica/audio/AudioEngine.hpp"

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace lamusica::session {

enum class StretchQuality {
    Preview,
    Balanced,
    High,
};

struct WarpMarker {
    std::string id;
    std::int64_t sourceSample{0};
    std::int64_t timelineSample{0};
};

struct Transient {
    std::int64_t sourceSample{0};
    float strength{0.0F};
};

struct BeatSlice {
    std::string id;
    std::int64_t sourceStartSample{0};
    std::int64_t sourceEndSample{0};
    std::int64_t timelineStartSample{0};
};

struct GroovePoint {
    std::int64_t gridSample{0};
    std::int64_t offsetSamples{0};
    float strength{1.0F};
};

struct GrooveTemplate {
    std::string id;
    std::vector<GroovePoint> points;
};

struct WarpState {
    std::string clipId;
    bool enabled{false};
    double sourceTempoBpm{120.0};
    double targetTempoBpm{120.0};
    float pitchShiftSemitones{0.0F};
    StretchQuality quality{StretchQuality::Balanced};
    std::vector<WarpMarker> markers;
};

struct RenderCacheEntry {
    std::string clipId;
    std::string cacheKey;
    std::string relativePath;
    bool valid{false};
};

struct WarpRenderSegment {
    std::int64_t sourceStartSample{0};
    std::int64_t sourceEndSample{0};
    std::int64_t timelineStartSample{0};
    std::int64_t timelineEndSample{0};
};

struct WarpRenderPlan {
    std::string clipId;
    std::string cacheKey;
    std::string relativePath;
    std::int64_t sourceStartSample{0};
    std::int64_t sourceEndSample{0};
    std::int64_t timelineStartSample{0};
    std::int64_t timelineEndSample{0};
    double stretchRatio{1.0};
    double pitchRatio{1.0};
    StretchQuality quality{StretchQuality::Balanced};
    std::vector<WarpRenderSegment> segments;
    bool cacheHit{false};
};

[[nodiscard]] std::int64_t conformSampleToTempo(std::int64_t sourceSample, double sourceTempoBpm,
                                                double targetTempoBpm) noexcept;
[[nodiscard]] std::int64_t quantizeSampleToGrid(std::int64_t sample, std::int64_t gridSamples,
                                                float strength);
[[nodiscard]] std::int64_t mapSourceToTimeline(const WarpState& warp, std::int64_t sourceSample);
[[nodiscard]] WarpState retargetWarpTempo(const WarpState& warp, double newTargetTempoBpm);
[[nodiscard]] WarpState conformWarpToTempo(const WarpState& warp,
                                           std::span<const Transient> transients,
                                           std::int64_t sourceEndSample, double newTargetTempoBpm);
[[nodiscard]] std::vector<Transient> detectTransients(std::span<const float> monoSamples,
                                                      float threshold);
[[nodiscard]] std::vector<BeatSlice> makeBeatSlices(const WarpState& warp,
                                                    std::span<const Transient> transients,
                                                    std::int64_t sourceEndSample);
[[nodiscard]] GrooveTemplate extractGroove(std::string id, std::span<const Transient> transients,
                                           std::int64_t gridSamples);
void quantizeWarpMarkers(WarpState& warp, std::int64_t gridSamples, float strength);
void applyGrooveToWarpMarkers(WarpState& warp, const GrooveTemplate& groove, float strength);
[[nodiscard]] std::string makeWarpCacheKey(const WarpState& warp);
[[nodiscard]] double pitchShiftRatio(float semitones) noexcept;
[[nodiscard]] const RenderCacheEntry*
findValidRenderCacheEntry(std::span<const RenderCacheEntry> cache, std::string_view clipId,
                          std::string_view cacheKey) noexcept;
[[nodiscard]] WarpRenderPlan makeWarpRenderPlan(const WarpState& warp,
                                                std::span<const RenderCacheEntry> cache,
                                                std::int64_t sourceStartSample,
                                                std::int64_t sourceEndSample,
                                                std::string relativePath);
[[nodiscard]] audio::RenderedAudio renderWarpedAudio(const audio::RenderedAudio& source,
                                                     const WarpRenderPlan& plan);
[[nodiscard]] audio::RenderedAudio renderWarpPreview(const audio::RenderedAudio& source,
                                                     const WarpRenderPlan& plan);
[[nodiscard]] bool warpRenderPlansAgree(const WarpRenderPlan& offlinePlan,
                                        const WarpRenderPlan& previewPlan,
                                        std::int64_t sampleTolerance);
void validateWarpState(const WarpState& warp);
void upsertRenderCacheEntry(std::vector<RenderCacheEntry>& cache, RenderCacheEntry entry);
void invalidateRenderCache(std::vector<RenderCacheEntry>& cache, std::string_view clipId);

} // namespace lamusica::session
