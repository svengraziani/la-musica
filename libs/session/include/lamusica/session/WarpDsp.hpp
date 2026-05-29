#pragma once

#include "lamusica/audio/AudioEngine.hpp"
#include "lamusica/session/Warp.hpp"

namespace lamusica::session {

struct WarpDspOptions {
    double stretchRatio{1.0};
    double pitchRatio{1.0};
    StretchQuality quality{StretchQuality::Balanced};
};

[[nodiscard]] audio::RenderedAudio resampleAudio(const audio::RenderedAudio& source,
                                                 double ratio,
                                                 StretchQuality quality);
[[nodiscard]] audio::RenderedAudio timeStretchAudio(const audio::RenderedAudio& source,
                                                    std::uint32_t outputFrames,
                                                    StretchQuality quality);
[[nodiscard]] audio::RenderedAudio renderWarpDsp(const audio::RenderedAudio& source,
                                                 std::uint32_t outputFrames,
                                                 const WarpDspOptions& options);

} // namespace lamusica::session
