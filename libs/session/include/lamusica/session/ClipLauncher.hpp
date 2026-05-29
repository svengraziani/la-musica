#pragma once

#include "lamusica/session/Pattern.hpp"

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace lamusica::session {

enum class LaunchQuantize {
    None,
    Beat,
    Bar,
};

struct ClipSlot {
    std::string id;
    std::string patternClipId;
    std::string midiClipId;
    std::string audioClipId;
};

struct Scene {
    std::string id;
    std::string name;
    std::vector<ClipSlot> slots;
};

struct ClipLauncher {
    LaunchQuantize quantize{LaunchQuantize::Bar};
    std::int64_t beatLengthSamples{24000};
    std::uint32_t beatsPerBar{4};
    std::vector<Scene> scenes;
};

struct ClipLaunchRequest {
    std::string sceneId;
    std::string slotId;
    std::int64_t requestSample{0};
};

struct ScheduledClipLaunch {
    std::string sceneId;
    std::string slotId;
    std::string patternClipId;
    std::string midiClipId;
    std::string audioClipId;
    std::int64_t timelineStartSample{0};
};

void validateClipLauncher(const ClipLauncher& launcher);
[[nodiscard]] std::int64_t quantizeLaunchSample(const ClipLauncher& launcher,
                                                std::int64_t requestSample);
[[nodiscard]] ScheduledClipLaunch scheduleClipLaunch(const ClipLauncher& launcher,
                                                     const ClipLaunchRequest& request);
[[nodiscard]] std::vector<ScheduledClipLaunch> scheduleSceneLaunch(
    const ClipLauncher& launcher, std::string_view sceneId, std::int64_t requestSample);
[[nodiscard]] std::vector<MidiPlaybackEvent> clipLauncherPatternPlaybackEventsInRange(
    const ScheduledClipLaunch& launch, std::span<const PatternClip> patterns,
    std::int64_t rangeStartSample, std::int64_t rangeEndSample);

} // namespace lamusica::session
