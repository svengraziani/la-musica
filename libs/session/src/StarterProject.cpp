#include "lamusica/session/StarterProject.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace lamusica::session {
namespace {

constexpr std::array<std::string_view, 19> firstTrackRequirementIds{"marker.section-intro",
                                                                    "marker.section-verse",
                                                                    "track.drums.audio",
                                                                    "track.bass.midi",
                                                                    "track.master",
                                                                    "clip.drum-loop",
                                                                    "clip.bass-pattern",
                                                                    "loop.intro",
                                                                    "midi.bass-pattern",
                                                                    "route.drums-master",
                                                                    "route.bass-master",
                                                                    "plugin.drum-starter-sampler",
                                                                    "plugin.bass-starter-synth",
                                                                    "plugin.master-starter-limiter",
                                                                    "automation.drums-volume",
                                                                    "automation.bass-filter",
                                                                    "automation.master-limiter",
                                                                    "arrangement.renderable",
                                                                    "midi.starter-bass-notes"};

bool isMissingOnlyIntroLoop(const std::vector<std::string>& missingRequirements) {
    return !missingRequirements.empty() &&
           std::ranges::all_of(missingRequirements, [](const std::string& requirement) {
               return requirement == "loop.intro";
           });
}

bool hasTrack(const ProjectManifest& manifest, std::string_view trackId, TrackType type) noexcept {
    return std::ranges::any_of(manifest.tracks, [trackId, type](const Track& track) {
        return track.id == trackId && track.type == type;
    });
}

bool hasMarker(const ProjectManifest& manifest, std::string_view markerId,
               std::string_view markerName, std::int64_t samplePosition) noexcept {
    return std::ranges::any_of(manifest.markers, [&](const Marker& marker) {
        return marker.id == markerId && marker.name == markerName &&
               marker.samplePosition == samplePosition;
    });
}

bool hasClip(const ProjectManifest& manifest, std::string_view clipId, std::string_view trackId,
             ClipType type, std::int64_t lengthSamples) noexcept {
    return std::ranges::any_of(manifest.clips, [&](const Clip& clip) {
        return clip.id == clipId && clip.trackId == trackId && clip.type == type &&
               clip.startSample == 0 && clip.lengthSamples >= lengthSamples;
    });
}

bool hasMidiClipReference(const ProjectManifest& manifest, std::string_view clipId) noexcept {
    return std::ranges::any_of(manifest.midiClips, [&](const MidiClipReference& reference) {
        return reference.clipId == clipId && !reference.dataId.empty();
    });
}

int midiClipTransposeSemitones(const ProjectManifest& manifest, std::string_view clipId) noexcept {
    const auto found =
        std::ranges::find_if(manifest.midiClips, [&](const MidiClipReference& reference) {
            return reference.clipId == clipId && !reference.dataId.empty();
        });
    return found == manifest.midiClips.end() ? 0 : found->transposeSemitones;
}

bool hasRoute(const ProjectManifest& manifest, std::string_view sourceTrackId,
              std::string_view destinationTrackId) noexcept {
    return std::ranges::any_of(manifest.routing, [&](const RoutingConnection& route) {
        return route.sourceTrackId == sourceTrackId &&
               route.destinationTrackId == destinationTrackId;
    });
}

bool hasPlugin(const ProjectManifest& manifest, std::string_view pluginId,
               std::string_view trackId) noexcept {
    return std::ranges::any_of(manifest.plugins, [&](const PluginReference& plugin) {
        return plugin.id == pluginId && plugin.trackId == trackId;
    });
}

bool hasAutomationLane(const ProjectManifest& manifest, std::string_view laneId,
                       std::string_view targetId, std::string_view parameterId) noexcept {
    return std::ranges::any_of(manifest.automation, [&](const AutomationLane& lane) {
        return lane.id == laneId && lane.targetId == targetId && lane.parameterId == parameterId &&
               !lane.regions.empty() && !lane.regions.front().points.empty();
    });
}

} // namespace

std::vector<std::string_view> firstTrackStarterRequirementIds() noexcept {
    return {firstTrackRequirementIds.begin(), firstTrackRequirementIds.end()};
}

ProjectManifest makeFirstTrackStarterManifest(std::string name) {
    ProjectManifest manifest;
    manifest.name = std::move(name);
    manifest.loopEnabled = true;
    manifest.loopStartSample = 0;
    manifest.loopEndSample = 96000;
    manifest.markers = {{.id = "section-intro", .name = "Intro", .samplePosition = 0},
                        {.id = "section-verse", .name = "Verse", .samplePosition = 96000}};
    manifest.tracks = {{.id = "drums", .name = "Generated Drums", .type = TrackType::Audio},
                       {.id = "bass", .name = "Generated Bass", .type = TrackType::Midi},
                       {.id = "master", .name = "Master", .type = TrackType::Master}};
    manifest.clips = {{.id = "drum-loop",
                       .trackId = "drums",
                       .type = ClipType::Audio,
                       .startSample = 0,
                       .lengthSamples = 96000,
                       .fadeOutSamples = 128,
                       .gainDb = -9.0F,
                       .assetId = ""},
                      {.id = "bass-pattern",
                       .trackId = "bass",
                       .type = ClipType::Midi,
                       .startSample = 0,
                       .lengthSamples = 96000,
                       .assetId = ""}};
    manifest.midiClips = {{.clipId = "bass-pattern", .dataId = "starter-bass-midi"}};
    manifest.routing = {{"drums", "master"}, {"bass", "master"}};
    manifest.plugins = {{.id = "drum-starter-sampler",
                         .trackId = "drums",
                         .format = "built_in",
                         .identifier = "lamusica.devices.drum-starter"},
                        {.id = "bass-starter-synth",
                         .trackId = "bass",
                         .format = "built_in",
                         .identifier = "lamusica.devices.bass-starter"},
                        {.id = "master-starter-limiter",
                         .trackId = "master",
                         .format = "built_in",
                         .identifier = "lamusica.devices.clean-limiter"}};
    manifest.automation = {{.id = "drums-volume-automation",
                            .targetKind = AutomationTargetKind::Mixer,
                            .targetId = "drums",
                            .parameterId = "volumeDb",
                            .mode = AutomationMode::Read,
                            .defaultValue = -9.0F,
                            .regions = {{.startSample = 0,
                                         .endSample = 96000,
                                         .points = {{.samplePosition = 0, .value = -12.0F},
                                                    {.samplePosition = 24000, .value = -9.0F},
                                                    {.samplePosition = 96000, .value = -8.0F}}}}},
                           {.id = "bass-filter-automation",
                            .targetKind = AutomationTargetKind::Plugin,
                            .targetId = "bass-starter-synth",
                            .parameterId = "filterCutoff",
                            .mode = AutomationMode::Read,
                            .defaultValue = 0.35F,
                            .regions = {{.startSample = 0,
                                         .endSample = 96000,
                                         .points = {{.samplePosition = 0, .value = 0.25F},
                                                    {.samplePosition = 48000, .value = 0.45F},
                                                    {.samplePosition = 96000, .value = 0.6F}}}}},
                           {.id = "master-limiter-threshold",
                            .targetKind = AutomationTargetKind::Plugin,
                            .targetId = "master-starter-limiter",
                            .parameterId = "thresholdDb",
                            .mode = AutomationMode::Read,
                            .defaultValue = -1.0F,
                            .regions = {{.startSample = 0,
                                         .endSample = 96000,
                                         .points = {{.samplePosition = 0, .value = -1.0F},
                                                    {.samplePosition = 96000, .value = -1.0F}}}}}};
    return manifest;
}

MidiClipData makeFirstTrackStarterBassMidi() {
    MidiClipData clip;
    clip.clipId = "bass-pattern";
    clip.metadata = {{"dataId", "starter-bass-midi"}, {"role", "bass"}, {"key", "C minor"}};
    clip.notes = {
        {.id = "bass-c1", .startSample = 0, .lengthSamples = 12000, .pitch = 36, .velocity = 104},
        {.id = "bass-c1-octave",
         .startSample = 12000,
         .lengthSamples = 6000,
         .pitch = 48,
         .velocity = 88},
        {.id = "bass-eb1",
         .startSample = 24000,
         .lengthSamples = 12000,
         .pitch = 39,
         .velocity = 96},
        {.id = "bass-bb0",
         .startSample = 36000,
         .lengthSamples = 6000,
         .pitch = 34,
         .velocity = 90},
        {.id = "bass-g0",
         .startSample = 48000,
         .lengthSamples = 12000,
         .pitch = 31,
         .velocity = 102},
        {.id = "bass-bb0-pickup",
         .startSample = 66000,
         .lengthSamples = 6000,
         .pitch = 34,
         .velocity = 86},
        {.id = "bass-c1-return",
         .startSample = 72000,
         .lengthSamples = 12000,
         .pitch = 36,
         .velocity = 105},
        {.id = "bass-g0-turnaround",
         .startSample = 90000,
         .lengthSamples = 6000,
         .pitch = 31,
         .velocity = 92}};
    return clip;
}

std::vector<MidiClipData> makeFirstTrackStarterMidiClips() {
    return {makeFirstTrackStarterBassMidi()};
}

bool isFirstTrackStarterManifest(const ProjectManifest& manifest) noexcept {
    return hasMarker(manifest, "section-intro", "Intro", 0) &&
           hasMarker(manifest, "section-verse", "Verse", 96000) &&
           hasTrack(manifest, "drums", TrackType::Audio) &&
           hasTrack(manifest, "bass", TrackType::Midi) &&
           hasTrack(manifest, "master", TrackType::Master) &&
           hasClip(manifest, "drum-loop", "drums", ClipType::Audio, 96000) &&
           hasClip(manifest, "bass-pattern", "bass", ClipType::Midi, 96000) &&
           manifest.loopEnabled && manifest.loopStartSample == 0 &&
           manifest.loopEndSample == 96000 && hasMidiClipReference(manifest, "bass-pattern") &&
           hasRoute(manifest, "drums", "master") && hasRoute(manifest, "bass", "master") &&
           hasPlugin(manifest, "drum-starter-sampler", "drums") &&
           hasPlugin(manifest, "bass-starter-synth", "bass") &&
           hasPlugin(manifest, "master-starter-limiter", "master") &&
           hasAutomationLane(manifest, "drums-volume-automation", "drums", "volumeDb") &&
           hasAutomationLane(manifest, "bass-filter-automation", "bass-starter-synth",
                             "filterCutoff") &&
           hasAutomationLane(manifest, "master-limiter-threshold", "master-starter-limiter",
                             "thresholdDb");
}

std::int64_t arrangementEndSample(const ProjectManifest& manifest) noexcept {
    std::int64_t endSample = 0;
    for (const auto& clip : manifest.clips) {
        endSample = std::max(endSample, clip.startSample + clip.lengthSamples);
    }
    return endSample;
}

std::uint32_t renderableArrangementFrames(const ProjectManifest& manifest) {
    const auto endSample = arrangementEndSample(manifest);
    if (endSample <= 0) {
        throw std::runtime_error("project has no renderable arrangement range");
    }
    if (endSample > static_cast<std::int64_t>(std::numeric_limits<std::uint32_t>::max())) {
        throw std::runtime_error("project arrangement is too long for a single render");
    }
    return static_cast<std::uint32_t>(endSample);
}

FirstTrackReadiness inspectFirstTrackReadiness(const ProjectManifest& manifest) {
    std::vector<std::string> missingRequirements;
    auto require = [&missingRequirements](bool condition, std::string_view requirementId) {
        if (!condition) {
            missingRequirements.emplace_back(requirementId);
        }
        return condition;
    };

    const bool hasIntroMarker = hasMarker(manifest, "section-intro", "Intro", 0);
    const bool hasVerseMarker = hasMarker(manifest, "section-verse", "Verse", 96000);
    const bool hasDrumsTrack = hasTrack(manifest, "drums", TrackType::Audio);
    const bool hasBassTrack = hasTrack(manifest, "bass", TrackType::Midi);
    const bool hasMasterTrack = hasTrack(manifest, "master", TrackType::Master);
    const bool hasDrumLoop = hasClip(manifest, "drum-loop", "drums", ClipType::Audio, 96000);
    const bool hasBassPattern = hasClip(manifest, "bass-pattern", "bass", ClipType::Midi, 96000);
    const bool hasIntroLoop =
        manifest.loopEnabled && manifest.loopStartSample == 0 && manifest.loopEndSample == 96000;
    const bool hasBassMidi = hasMidiClipReference(manifest, "bass-pattern");
    const bool hasDrumsRoute = hasRoute(manifest, "drums", "master");
    const bool hasBassRoute = hasRoute(manifest, "bass", "master");
    const bool hasDrumPlugin = hasPlugin(manifest, "drum-starter-sampler", "drums");
    const bool hasBassPlugin = hasPlugin(manifest, "bass-starter-synth", "bass");
    const bool hasMasterPlugin = hasPlugin(manifest, "master-starter-limiter", "master");
    const bool hasDrumsAutomation =
        hasAutomationLane(manifest, "drums-volume-automation", "drums", "volumeDb");
    const bool hasBassAutomation =
        hasAutomationLane(manifest, "bass-filter-automation", "bass-starter-synth", "filterCutoff");
    const bool hasMasterAutomation = hasAutomationLane(manifest, "master-limiter-threshold",
                                                       "master-starter-limiter", "thresholdDb");
    const auto arrangementEnd = arrangementEndSample(manifest);
    const bool hasRenderableArrangement =
        arrangementEnd > 0 &&
        arrangementEnd <= static_cast<std::int64_t>(std::numeric_limits<std::uint32_t>::max());
    const bool hasStarterBassNotes = hasBassMidi && !makeFirstTrackStarterBassMidi().notes.empty();

    bool starterStructureReady = true;
    starterStructureReady &= require(hasIntroMarker, "marker.section-intro");
    starterStructureReady &= require(hasVerseMarker, "marker.section-verse");
    starterStructureReady &= require(hasDrumsTrack, "track.drums.audio");
    starterStructureReady &= require(hasBassTrack, "track.bass.midi");
    starterStructureReady &= require(hasMasterTrack, "track.master");
    starterStructureReady &= require(hasDrumLoop, "clip.drum-loop");
    starterStructureReady &= require(hasBassPattern, "clip.bass-pattern");
    starterStructureReady &= require(hasIntroLoop, "loop.intro");
    starterStructureReady &= require(hasBassMidi, "midi.bass-pattern");
    starterStructureReady &= require(hasDrumsRoute, "route.drums-master");
    starterStructureReady &= require(hasBassRoute, "route.bass-master");
    starterStructureReady &= require(hasDrumPlugin, "plugin.drum-starter-sampler");
    starterStructureReady &= require(hasBassPlugin, "plugin.bass-starter-synth");
    starterStructureReady &= require(hasMasterPlugin, "plugin.master-starter-limiter");
    starterStructureReady &= require(hasDrumsAutomation, "automation.drums-volume");
    starterStructureReady &= require(hasBassAutomation, "automation.bass-filter");
    starterStructureReady &= require(hasMasterAutomation, "automation.master-limiter");
    require(hasRenderableArrangement, "arrangement.renderable");
    require(hasStarterBassNotes, "midi.starter-bass-notes");

    const bool firstTrackEditable =
        hasRenderableArrangement &&
        (starterStructureReady || isMissingOnlyIntroLoop(missingRequirements));

    FirstTrackReadiness readiness{.starterStructureReady = starterStructureReady,
                                  .firstTrackEditable = firstTrackEditable,
                                  .trackCount = manifest.tracks.size(),
                                  .clipCount = manifest.clips.size(),
                                  .markerCount = manifest.markers.size(),
                                  .routingCount = manifest.routing.size(),
                                  .midiClipReferenceCount = manifest.midiClips.size(),
                                  .pluginCount = manifest.plugins.size(),
                                  .automationLaneCount = manifest.automation.size(),
                                  .loopReady = manifest.loopEnabled &&
                                               manifest.loopEndSample > manifest.loopStartSample,
                                  .loopStartSample = manifest.loopStartSample,
                                  .loopEndSample = manifest.loopEndSample,
                                  .arrangementEndSample = arrangementEnd,
                                  .missingRequirements = std::move(missingRequirements)};
    if (hasMidiClipReference(manifest, "bass-pattern")) {
        readiness.starterMidiNoteCount = makeFirstTrackStarterBassMidi().notes.size();
        readiness.starterBassTransposeSemitones =
            midiClipTransposeSemitones(manifest, "bass-pattern");
    }
    if (hasRenderableArrangement) {
        readiness.renderable = true;
        readiness.renderFrames = static_cast<std::uint32_t>(readiness.arrangementEndSample);
    }
    return readiness;
}

FirstTrackArrangementSummary summarizeFirstTrackArrangement(const ProjectManifest& manifest) {
    FirstTrackArrangementSummary summary;
    if (!manifest.tempoMap.empty()) {
        summary.tempoBpm = manifest.tempoMap.front().bpm;
    }
    if (!manifest.timeSignatures.empty()) {
        summary.timeSignatureNumerator = manifest.timeSignatures.front().numerator;
        summary.timeSignatureDenominator = manifest.timeSignatures.front().denominator;
    }
    summary.sectionCount = manifest.markers.size();
    if (!manifest.markers.empty()) {
        const auto [firstSection, finalSection] = std::ranges::minmax_element(
            manifest.markers, {}, [](const Marker& marker) { return marker.samplePosition; });
        summary.firstSectionName = firstSection->name;
        summary.finalSectionName = finalSection->name;
        summary.firstSectionSample = firstSection->samplePosition;
        summary.finalSectionSample = finalSection->samplePosition;
    }
    for (const auto& track : manifest.tracks) {
        switch (track.type) {
        case TrackType::Audio:
            ++summary.audioTrackCount;
            break;
        case TrackType::Midi:
        case TrackType::Instrument:
            ++summary.midiTrackCount;
            break;
        case TrackType::Master:
            ++summary.masterTrackCount;
            break;
        case TrackType::Group:
        case TrackType::Return:
            break;
        }
    }
    return summary;
}

} // namespace lamusica::session
