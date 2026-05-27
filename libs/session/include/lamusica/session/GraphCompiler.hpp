#pragma once

#include "lamusica/audio/AudioGraph.hpp"
#include "lamusica/session/Mixer.hpp"
#include "lamusica/session/ProjectManifest.hpp"

#include <string>

namespace lamusica::session {

struct GraphCompileOptions {
    bool includeMutedClips{false};
    bool synthesizeMissingMaster{true};
};

struct MixerGraphUpdatePlan {
    bool ready{false};
    std::string validationError;
    audio::AudioGraph graph;
};

[[nodiscard]] audio::AudioGraph compileProjectAudioGraph(const ProjectManifest& manifest,
                                                         const MixerState& mixer,
                                                         GraphCompileOptions options = {});
[[nodiscard]] MixerGraphUpdatePlan prepareMixerGraphUpdate(const ProjectManifest& manifest,
                                                           const MixerState& mixer,
                                                           GraphCompileOptions options = {});
[[nodiscard]] float dbToLinearGain(float db) noexcept;

} // namespace lamusica::session
