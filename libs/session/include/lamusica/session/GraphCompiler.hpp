#pragma once

#include "lamusica/audio/AudioGraph.hpp"
#include "lamusica/session/Mixer.hpp"
#include "lamusica/session/ProjectManifest.hpp"
#include "lamusica/session/Warp.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace lamusica::session {

struct GraphCompileOptions {
    bool includeMutedClips{false};
    bool synthesizeMissingMaster{true};
    bool synthesizeAssetBackedClipsWithoutProjectRoot{false};
    std::filesystem::path projectRoot;
    std::vector<WarpState> warpStates;
    std::vector<RenderCacheEntry> warpRenderCache;
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
