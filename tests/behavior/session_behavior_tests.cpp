#include "lamusica/audio/WavFile.hpp"
#include "lamusica/audio/Recording.hpp"
#include "lamusica/session/ApplicationSession.hpp"
#include "lamusica/session/AudioClipEditing.hpp"
#include "lamusica/session/Automation.hpp"
#include "lamusica/session/ClipLauncher.hpp"
#include "lamusica/session/Mixer.hpp"
#include "lamusica/session/ProjectManifest.hpp"
#include "lamusica/session/Timeline.hpp"

#include <cmath>
#include <exception>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

lamusica::audio::RenderedAudio monoTake(std::initializer_list<float> samples) {
    return {.channels = 1,
            .frames = static_cast<std::uint32_t>(samples.size()),
            .interleavedSamples = std::vector<float>{samples}};
}

void assertRecordingArtifact(const std::filesystem::path& workDir) {
    const auto projectPath = workDir / "recording.Project.lamusica";
    std::filesystem::remove_all(projectPath);

    lamusica::session::ApplicationSession session;
    session.createFirstTrackProject(projectPath, "Behavior Recording");
    const auto recording =
        session.recordFirstTrackTake({.frames = 2048, .startSample = 512, .countInBars = 1});
    require(std::filesystem::exists(recording.committed.path), "recording WAV was not committed");
    require(recording.committed.frames == 2048U, "recording frame count mismatch");
    require(session.status().recordedTakeCount == 1U, "recorded take count was not updated");
    require(session.status().clipCount == 3U, "recorded take clip was not added");
}

void assertWavSampleRatePolicy(const std::filesystem::path& workDir) {
    const lamusica::audio::RenderedAudio audio{
        .channels = 1, .frames = 4, .interleavedSamples = {0.0F, 0.25F, -0.25F, 0.0F}};
    for (const auto sampleRate : {44100.0, 48000.0, 96000.0}) {
        const auto path = workDir / ("sample-rate-" + std::to_string(static_cast<int>(sampleRate)) +
                                     ".wav");
        lamusica::audio::writePcm16Wav(path, audio, sampleRate);
        require(lamusica::audio::readPcm16Wav(path).sampleRate == sampleRate,
                "WAV header did not preserve integer sample rate");
    }

    bool rejectedFractionalSampleRate = false;
    try {
        lamusica::audio::writePcm16Wav(workDir / "fractional-rate.wav", audio, 47999.5);
    } catch (const std::exception&) {
        rejectedFractionalSampleRate = true;
    }
    require(rejectedFractionalSampleRate,
            "WAV writer accepted a fractional sample rate that would be truncated");
}

void assertRecordingWorkflowPlan() {
    const std::vector<float> reference{0.0F, 1.0F, 0.0F, 0.0F, 0.0F};
    const std::vector<float> recorded{0.0F, 0.0F, 0.0F, 1.0F, 0.0F};
    const auto latency = lamusica::audio::measureRecordingLatency(reference, recorded, 0.75F);
    require(latency.valid && latency.measuredInputLatencySamples == 2,
            "recording behavior did not measure input latency from fixture impulses");

    const auto plan = lamusica::audio::makeRecordingPlan(
        {.trackId = "record-track",
         .transportStartSample = 1000,
         .punchInSample = 24000,
         .punchOutSample = 48000,
         .preRollSamples = 12000,
         .countInSamples = 24000,
         .measuredInputLatencySamples = latency.measuredInputLatencySamples,
         .punchEnabled = true,
         .inputMonitoringEnabled = true});
    require(plan.captureStartSample == 0 && plan.clipStartSample == 23998 &&
                plan.punchEnabled && plan.punchInSample == 24000 &&
                plan.punchOutSample == 48000 && plan.inputMonitoringEnabled,
            "recording behavior did not preserve punch, count-in, and input monitoring intent");

    bool rejectedBadPunch = false;
    try {
        (void)lamusica::audio::makeRecordingPlan(
            {.trackId = "record-track",
             .punchInSample = 96000,
             .punchOutSample = 96000,
             .punchEnabled = true,
             .inputMonitoringEnabled = true});
    } catch (const std::exception&) {
        rejectedBadPunch = true;
    }
    require(rejectedBadPunch, "recording behavior accepted an empty punch range");
}

void assertCompArtifact(const std::filesystem::path& workDir) {
    const lamusica::session::Clip clip{.id = "clip-a",
                                       .trackId = "track-a",
                                       .type = lamusica::session::ClipType::Audio,
                                       .lengthSamples = 6};
    const lamusica::session::ClipTakeLane takeLane{
        .clipId = "clip-a",
        .takes = {{.id = "take-a", .name = "Take A", .lengthSamples = 6},
                  {.id = "take-b", .name = "Take B", .lengthSamples = 6}}};
    const lamusica::session::ClipComp comp{
        .clipId = "clip-a",
        .segments = {{.takeId = "take-a", .clipStartSample = 0, .lengthSamples = 3},
                     {.takeId = "take-b", .clipStartSample = 3, .lengthSamples = 3}}};
    const std::vector<lamusica::session::ClipTakeSource> sources{
        {.takeId = "take-a", .audio = monoTake({0.1F, 0.2F, 0.3F, 0.4F, 0.5F, 0.6F})},
        {.takeId = "take-b", .audio = monoTake({0.7F, 0.8F, 0.9F, 1.0F, 0.9F, 0.8F})}};
    const auto rendered = lamusica::session::renderCompedClip(clip, takeLane, comp, sources);
    require(rendered.frames == 6U, "comped clip frame count mismatch");
    require(rendered.interleavedSamples[0] == 0.1F &&
                std::abs(rendered.interleavedSamples[3] - 0.3F) < 0.0001F &&
                rendered.interleavedSamples[5] > 0.8F,
            "comped clip did not select and crossfade the expected take segments");

    const auto outputPath = workDir / "comped.wav";
    lamusica::audio::writePcm16Wav(outputPath, rendered, 48000.0);
    const auto wav = lamusica::audio::readPcm16Wav(outputPath);
    require(wav.audio.frames == 6U && wav.audio.channels == 1U, "comped WAV artifact mismatch");

    lamusica::session::ProjectManifest manifest{
        .tracks = {{.id = "track-a", .name = "Track A"}},
        .clips = {clip},
        .takeLanes = {takeLane},
        .comps = {comp}};
    lamusica::session::validateProjectManifest(manifest);

    manifest.comps.front().segments.front().takeSourceOffsetSamples = 4;
    bool rejectedOutOfTakeRange = false;
    try {
        lamusica::session::validateProjectManifest(manifest);
    } catch (const std::exception&) {
        rejectedOutOfTakeRange = true;
    }
    require(rejectedOutOfTakeRange,
            "project manifest accepted a comp segment outside the referenced take range");
}

void assertFaderGroupAndTimelineGrouping() {
    lamusica::session::MixerState mixer;
    lamusica::session::addChannel(
        mixer, {.id = "drums", .name = "Drums", .type = lamusica::session::ChannelType::Audio});
    lamusica::session::addChannel(
        mixer, {.id = "bass", .name = "Bass", .type = lamusica::session::ChannelType::Audio});
    lamusica::session::addFaderGroup(mixer, {.id = "rhythm",
                                             .name = "Rhythm",
                                             .channelIds = {"drums", "bass"},
                                             .linkVolume = true,
                                             .linkMute = true});
    lamusica::session::applyFaderGroupVolumeDelta(mixer, "rhythm", -6.0F);
    lamusica::session::applyFaderGroupMute(mixer, "rhythm", true);
    require(lamusica::session::findChannel(mixer, "drums")->volumeDb == -6.0F &&
                lamusica::session::findChannel(mixer, "bass")->volumeDb == -6.0F,
            "fader group volume delta did not affect all grouped channels");
    require(lamusica::session::findChannel(mixer, "drums")->muted &&
                lamusica::session::findChannel(mixer, "bass")->muted,
            "fader group mute did not affect all grouped channels");

    const lamusica::session::ProjectManifest manifest{
        .tracks = {
            {.id = "drums", .name = "Drums"},
            {.id = "bass", .name = "Bass"},
            {.id = "master", .name = "Master", .type = lamusica::session::TrackType::Master}}};
    lamusica::session::TimelineOrganization organization;
    lamusica::session::addTrackFolder(
        organization, manifest,
        {.id = "rhythm-folder", .name = "Rhythm", .trackIds = {"drums", "bass"}});
    const auto grouped = lamusica::session::tracksInFolder(manifest, organization, "rhythm-folder");
    require(grouped.size() == 2U && grouped[0].id == "drums" && grouped[1].id == "bass",
            "timeline track folder did not preserve grouped tracks");
}

void assertAutomationWriteModes() {
    const std::vector<lamusica::session::AutomationWriteSample> samples{
        {.samplePosition = 0, .value = 0.1F, .touched = false},
        {.samplePosition = 10, .value = 0.5F, .touched = true},
        {.samplePosition = 20, .value = 0.9F, .touched = false}};

    lamusica::session::AutomationLaneData writeLane{
        .id = "write", .mode = lamusica::session::AutomationMode::Write};
    const auto writeBatch = lamusica::session::captureAutomationWrite(writeLane, samples);
    require(writeBatch.points.size() == 3U, "write mode did not capture every sample");

    lamusica::session::AutomationLaneData touchLane{
        .id = "touch", .mode = lamusica::session::AutomationMode::Touch};
    const auto touchBatch = lamusica::session::captureAutomationWrite(touchLane, samples);
    require(touchBatch.points.size() == 1U && touchBatch.points.front().samplePosition == 10,
            "touch mode captured samples that were not touched");

    lamusica::session::AutomationLaneData latchLane{
        .id = "latch", .mode = lamusica::session::AutomationMode::Latch};
    const auto latchBatch = lamusica::session::captureAutomationWrite(latchLane, samples);
    require(latchBatch.points.size() == 2U &&
                std::abs(latchBatch.points.back().value - 0.5F) < 0.0001F,
            "latch mode did not hold the touched value");
}

void assertClipLauncherArtifact() {
    const lamusica::session::PatternClip pattern{
        .id = "drum-pattern",
        .name = "Drum Pattern",
        .lengthSteps = 4,
        .stepLengthSamples = 6000,
        .seed = 42,
        .lanes = {{.id = "kick",
                   .name = "Kick",
                   .defaultPitch = 36,
                   .lengthSteps = 4,
                   .steps = {{.enabled = true, .pitch = 36, .velocity = 110},
                             {},
                             {.enabled = true, .pitch = 36, .velocity = 100},
                             {}}}}};
    const lamusica::session::ClipLauncher launcher{
        .quantize = lamusica::session::LaunchQuantize::Bar,
        .beatLengthSamples = 24000,
        .beatsPerBar = 4,
        .scenes = {{.id = "scene-a",
                    .name = "Scene A",
                    .slots = {{.id = "slot-drums", .patternClipId = "drum-pattern"},
                              {.id = "slot-empty"}}}}};

    const auto scheduled =
        lamusica::session::scheduleSceneLaunch(launcher, "scene-a", 1);
    require(scheduled.size() == 1U && scheduled.front().timelineStartSample == 96000,
            "clip launcher scene launch did not quantize to the next bar");
    const std::vector patterns{pattern};
    const auto eventsA = lamusica::session::clipLauncherPatternPlaybackEventsInRange(
        scheduled.front(), patterns, 96000, 120000);
    const auto eventsB = lamusica::session::clipLauncherPatternPlaybackEventsInRange(
        scheduled.front(), patterns, 96000, 120000);
    require(eventsA.size() == 4U && eventsB.size() == eventsA.size() &&
                eventsA[0].type == lamusica::session::MidiEventType::NoteOn &&
                eventsA[1].type == lamusica::session::MidiEventType::NoteOff &&
                eventsA[2].type == lamusica::session::MidiEventType::NoteOn &&
                eventsA[3].type == lamusica::session::MidiEventType::NoteOff &&
                eventsA[0].samplePosition == 0 &&
                eventsA[2].samplePosition == 12000 &&
                eventsA.front().samplePosition == eventsB.front().samplePosition &&
                eventsA.front().data1 == eventsB.front().data1 &&
                eventsA[2].samplePosition == eventsB[2].samplePosition &&
                eventsA[2].data1 == eventsB[2].data1,
            "clip launcher pattern playback events are not deterministic at sample positions");

    bool rejectedEmptySlot = false;
    try {
        (void)lamusica::session::scheduleClipLaunch(
            launcher, {.sceneId = "scene-a", .slotId = "slot-empty", .requestSample = 0});
    } catch (const std::exception&) {
        rejectedEmptySlot = true;
    }
    require(rejectedEmptySlot, "clip launcher rejects launching an empty slot");
}

} // namespace

int main(int argc, char** argv) {
    try {
        const std::filesystem::path workDir = argc >= 2 ? argv[1] : ".";
        std::filesystem::create_directories(workDir);
        assertWavSampleRatePolicy(workDir);
        assertRecordingArtifact(workDir);
        assertRecordingWorkflowPlan();
        assertCompArtifact(workDir);
        assertFaderGroupAndTimelineGrouping();
        assertAutomationWriteModes();
        assertClipLauncherArtifact();
        std::cout
            << "behavior artifacts wavSampleRate=true recording=true monitoring=true punch=true comping=true"
               " grouping=true automation=true clipLauncher=true\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "behavior test failed: " << error.what() << '\n';
        return 1;
    }
}
