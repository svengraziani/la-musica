#include "lamusica/session/ClipLauncher.hpp"

#include <algorithm>
#include <set>
#include <stdexcept>

namespace lamusica::session {
namespace {

const Scene* findScene(const ClipLauncher& launcher, std::string_view sceneId) noexcept {
    const auto found = std::ranges::find_if(
        launcher.scenes, [sceneId](const Scene& scene) { return scene.id == sceneId; });
    return found == launcher.scenes.end() ? nullptr : &*found;
}

const ClipSlot* findSlot(const Scene& scene, std::string_view slotId) noexcept {
    const auto found = std::ranges::find_if(
        scene.slots, [slotId](const ClipSlot& slot) { return slot.id == slotId; });
    return found == scene.slots.end() ? nullptr : &*found;
}

const PatternClip* findPattern(std::span<const PatternClip> patterns,
                               std::string_view patternClipId) noexcept {
    const auto found = std::ranges::find_if(
        patterns, [patternClipId](const PatternClip& pattern) { return pattern.id == patternClipId; });
    return found == patterns.end() ? nullptr : &*found;
}

bool slotHasClip(const ClipSlot& slot) noexcept {
    return !slot.patternClipId.empty() || !slot.midiClipId.empty() || !slot.audioClipId.empty();
}

ScheduledClipLaunch scheduledFromSlot(const Scene& scene, const ClipSlot& slot,
                                      std::int64_t timelineStartSample) {
    return {.sceneId = scene.id,
            .slotId = slot.id,
            .patternClipId = slot.patternClipId,
            .midiClipId = slot.midiClipId,
            .audioClipId = slot.audioClipId,
            .timelineStartSample = timelineStartSample};
}

} // namespace

void validateClipLauncher(const ClipLauncher& launcher) {
    if (launcher.beatLengthSamples <= 0) {
        throw std::runtime_error("Clip launcher beat length must be positive");
    }
    if (launcher.beatsPerBar == 0U) {
        throw std::runtime_error("Clip launcher beats per bar must be positive");
    }

    std::set<std::string> sceneIds;
    for (const auto& scene : launcher.scenes) {
        if (scene.id.empty() || scene.name.empty()) {
            throw std::runtime_error("Clip launcher scene id and name must not be empty");
        }
        if (!sceneIds.insert(scene.id).second) {
            throw std::runtime_error("Duplicate clip launcher scene id: " + scene.id);
        }

        std::set<std::string> slotIds;
        for (const auto& slot : scene.slots) {
            if (slot.id.empty()) {
                throw std::runtime_error("Clip launcher slot id must not be empty");
            }
            if (!slotIds.insert(slot.id).second) {
                throw std::runtime_error("Duplicate clip launcher slot id: " + slot.id);
            }
            const auto clipReferenceCount = static_cast<unsigned>(!slot.patternClipId.empty()) +
                                            static_cast<unsigned>(!slot.midiClipId.empty()) +
                                            static_cast<unsigned>(!slot.audioClipId.empty());
            if (clipReferenceCount > 1U) {
                throw std::runtime_error("Clip launcher slot must reference at most one clip");
            }
        }
    }
}

std::int64_t quantizeLaunchSample(const ClipLauncher& launcher, std::int64_t requestSample) {
    validateClipLauncher(launcher);
    if (requestSample < 0) {
        throw std::runtime_error("Clip launcher request sample must not be negative");
    }

    std::int64_t quantum = 1;
    switch (launcher.quantize) {
    case LaunchQuantize::None:
        return requestSample;
    case LaunchQuantize::Beat:
        quantum = launcher.beatLengthSamples;
        break;
    case LaunchQuantize::Bar:
        quantum = launcher.beatLengthSamples * static_cast<std::int64_t>(launcher.beatsPerBar);
        break;
    }

    const auto remainder = requestSample % quantum;
    return remainder == 0 ? requestSample : requestSample + (quantum - remainder);
}

ScheduledClipLaunch scheduleClipLaunch(const ClipLauncher& launcher,
                                       const ClipLaunchRequest& request) {
    validateClipLauncher(launcher);
    const auto* scene = findScene(launcher, request.sceneId);
    if (scene == nullptr) {
        throw std::runtime_error("Clip launcher request references missing scene: " +
                                 request.sceneId);
    }
    const auto* slot = findSlot(*scene, request.slotId);
    if (slot == nullptr) {
        throw std::runtime_error("Clip launcher request references missing slot: " + request.slotId);
    }
    if (!slotHasClip(*slot)) {
        throw std::runtime_error("Clip launcher slot is empty: " + slot->id);
    }
    return scheduledFromSlot(*scene, *slot, quantizeLaunchSample(launcher, request.requestSample));
}

std::vector<ScheduledClipLaunch> scheduleSceneLaunch(const ClipLauncher& launcher,
                                                     std::string_view sceneId,
                                                     std::int64_t requestSample) {
    validateClipLauncher(launcher);
    const auto* scene = findScene(launcher, sceneId);
    if (scene == nullptr) {
        throw std::runtime_error("Clip launcher request references missing scene: " +
                                 std::string{sceneId});
    }

    const auto timelineStartSample = quantizeLaunchSample(launcher, requestSample);
    std::vector<ScheduledClipLaunch> launches;
    for (const auto& slot : scene->slots) {
        if (slotHasClip(slot)) {
            launches.push_back(scheduledFromSlot(*scene, slot, timelineStartSample));
        }
    }
    return launches;
}

std::vector<MidiPlaybackEvent> clipLauncherPatternPlaybackEventsInRange(
    const ScheduledClipLaunch& launch, std::span<const PatternClip> patterns,
    std::int64_t rangeStartSample, std::int64_t rangeEndSample) {
    if (launch.patternClipId.empty()) {
        return {};
    }
    const auto* pattern = findPattern(patterns, launch.patternClipId);
    if (pattern == nullptr) {
        throw std::runtime_error("Clip launcher launch references missing pattern clip: " +
                                 launch.patternClipId);
    }
    return patternPlaybackEventsInRange(
        {.pattern = *pattern, .timelineStartSample = launch.timelineStartSample}, rangeStartSample,
        rangeEndSample);
}

} // namespace lamusica::session
