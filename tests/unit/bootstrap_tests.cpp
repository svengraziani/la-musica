#include "lamusica/audio/AudioEngine.hpp"
#include "lamusica/audio/AudioGraph.hpp"
#include "lamusica/audio/Bounce.hpp"
#include "lamusica/audio/Recording.hpp"
#include "lamusica/audio/WavFile.hpp"
#include "lamusica/commands/Command.hpp"
#include "lamusica/mcp_bridge/Capability.hpp"
#include "lamusica/mcp_bridge/DaemonSession.hpp"
#include "lamusica/mcp_bridge/EditTools.hpp"
#include "lamusica/mcp_bridge/Orchestration.hpp"
#include "lamusica/mcp_bridge/Protocol.hpp"
#include "lamusica/mcp_bridge/QueryTools.hpp"
#include "lamusica/mcp_bridge/RenderTools.hpp"
#include "lamusica/session/ApplicationSession.hpp"
#include "lamusica/session/Assets.hpp"
#include "lamusica/session/AudioClipEditing.hpp"
#include "lamusica/session/DiagnosticsScrubber.hpp"
#include "lamusica/session/Automation.hpp"
#include "lamusica/session/DrumMachine.hpp"
#include "lamusica/session/Export.hpp"
#include "lamusica/session/GraphCompiler.hpp"
#include "lamusica/session/Midi.hpp"
#include "lamusica/session/Mixer.hpp"
#include "lamusica/session/Pattern.hpp"
#include "lamusica/session/Performance.hpp"
#include "lamusica/session/PianoRoll.hpp"
#include "lamusica/session/Plugin.hpp"
#include "lamusica/session/Project.hpp"
#include "lamusica/session/ProjectDocument.hpp"
#include "lamusica/session/ProjectManifest.hpp"
#include "lamusica/session/StarterProject.hpp"
#include "lamusica/session/Timeline.hpp"
#include "lamusica/session/Warp.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <numbers>
#include <ranges>
#include <string>
#include <string_view>
#include <vector>

namespace {

void require(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(1);
    }
}

std::string replaceFirst(std::string value, std::string_view from, std::string_view to) {
    const auto position = value.find(from);
    require(position != std::string::npos, "test fixture replacement source exists");
    value.replace(position, from.size(), to);
    return value;
}

void writeTextFile(const std::filesystem::path& path, std::string_view content) {
    std::ofstream output{path};
    require(static_cast<bool>(output), "test fixture text file opens for writing");
    output << content;
}

std::string readTextFile(const std::filesystem::path& path) {
    std::ifstream input{path};
    require(static_cast<bool>(input), "test fixture text file opens for reading");
    return {std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
}

} // namespace

int main() {
    const lamusica::session::Project project{"Fixture"};
    require(project.name() == "Fixture", "project name round trip");

    const lamusica::audio::AudioEngine engine{{.sampleRate = 44100.0}};
    require(engine.config().sampleRate == 44100.0, "audio engine config round trip");

    lamusica::audio::AudioEngine deviceEngine{{.sampleRate = 48000.0, .maxBlockSize = 256}};
    deviceEngine.selectDevice({.id = "built-in",
                               .name = "Built In Output",
                               .sampleRate = 96000.0,
                               .bufferSize = 128,
                               .inputChannels = 2,
                               .outputChannels = 2});
    require(deviceEngine.device().id == "built-in", "audio device selection persists id");
    require(deviceEngine.device().sampleRate == 96000.0, "audio device stores hardware sample rate");
    require(deviceEngine.config().sampleRate == 48000.0,
            "audio device selection preserves project sample rate");
    require(deviceEngine.enqueueCommand({.type = lamusica::audio::RealtimeCommandType::Play}),
            "realtime command queue accepts play command");
    require(deviceEngine.enqueueCommand(
                {.type = lamusica::audio::RealtimeCommandType::SetTempo, .value = 90.0}),
            "realtime command queue accepts tempo command");
    auto deviceRender = deviceEngine.renderSilenceOffline(16);
    require(deviceRender.frames == 16, "audio engine processes queued commands during render");
    require(deviceEngine.transport().playing, "realtime play command updates transport");
    require(deviceEngine.transport().tempoBpm == 90.0, "realtime tempo command updates transport");
    deviceEngine.setLoop(true, 100, 148);
    deviceEngine.seekSamples(132);
    (void)deviceEngine.renderSilenceOffline(32);
    require(deviceEngine.transport().samplePosition == 116,
            "audio transport wraps when render crosses loop end");
    (void)deviceEngine.renderSineOffline(96, 440.0, 0.1F);
    require(deviceEngine.transport().samplePosition == 116,
            "audio oscillator render preserves stable loop position across exact loop multiples");
    deviceEngine.seekSamples(1000);
    (void)deviceEngine.renderMetronomeOffline(12);
    require(deviceEngine.transport().samplePosition == 112,
            "audio metronome render normalizes out-of-loop transport positions");

    lamusica::audio::RealtimeCommandQueue queue;
    for (std::size_t index = 0; index < 64; ++index) {
        require(queue.push({.type = lamusica::audio::RealtimeCommandType::Seek,
                            .sampleA = static_cast<std::int64_t>(index)}),
                "realtime command queue accepts bounded commands");
    }
    require(queue.full(), "realtime command queue reports full");
    require(!queue.push({.type = lamusica::audio::RealtimeCommandType::Stop}),
            "realtime command queue rejects overflow");
    require(queue.pop()->sampleA == 0, "realtime command queue preserves FIFO order");

    lamusica::audio::AudioGraph graph{
        .nodes = {{.id = "osc",
                   .kind = lamusica::audio::GraphNodeKind::Sine,
                   .frequencyHz = 440.0,
                   .gain = 0.25F,
                   .noteSequence = {}},
                  {.id = "bus", .kind = lamusica::audio::GraphNodeKind::Bus, .noteSequence = {}},
                  {.id = "master",
                   .kind = lamusica::audio::GraphNodeKind::Output,
                   .noteSequence = {}}},
        .connections = {{.sourceNodeId = "osc", .destinationNodeId = "bus", .gain = 1.0F},
                        {.sourceNodeId = "bus", .destinationNodeId = "master", .gain = 0.5F}},
        .outputNodeId = "master"};
    graph.nodes[1].gain = 0.5F;
    graph.nodes[2].gain = 0.5F;
    require(lamusica::audio::validateGraph(graph), "audio graph validates acyclic routing");
    const auto order = lamusica::audio::topologicalOrder(graph);
    require(order.size() == 3, "audio graph produces topological order");
    std::vector<float> graphOutput(128);
    lamusica::audio::renderGraph(graph,
                                 {.sampleRate = 48000.0, .maxBlockSize = 64, .outputChannels = 2},
                                 0, 64, graphOutput);
    require(std::ranges::any_of(graphOutput, [](float sample) { return sample != 0.0F; }),
            "audio graph renders signal");
    const auto expectedGraphSample = static_cast<float>(
        std::sin((440.0 * 2.0 * std::numbers::pi) / 48000.0) * 0.25 * 0.5 * 0.5 * 0.5);
    require(std::abs(graphOutput[2] - expectedGraphSample) < 0.000001F,
            "audio graph applies source, bus, connection, and output gains");
    lamusica::audio::AudioEngine deviceBoundaryEngine{
        {.sampleRate = 48000.0, .maxBlockSize = 512, .outputChannels = 2}};
    deviceBoundaryEngine.selectDevice({.id = "device-44k",
                                       .name = "44.1 kHz Device",
                                       .sampleRate = 44100.0,
                                       .bufferSize = 441,
                                       .outputChannels = 2});
    std::vector<float> deviceBoundaryOutput(441 * 2);
    deviceBoundaryEngine.renderGraphDeviceBlock(graph, deviceBoundaryOutput, 441);
    require(deviceBoundaryEngine.config().sampleRate == 48000.0 &&
                deviceBoundaryEngine.device().sampleRate == 44100.0 &&
                deviceBoundaryEngine.transport().samplePosition == 480 &&
                std::ranges::any_of(deviceBoundaryOutput,
                                    [](float sample) { return sample != 0.0F; }),
            "audio engine resamples project-rate graph blocks to device-rate output");
    lamusica::audio::GraphNode noteSequenceNode;
    noteSequenceNode.id = "note-sequence";
    noteSequenceNode.kind = lamusica::audio::GraphNodeKind::Sine;
    noteSequenceNode.noteSequence = {
        {.startSample = 0, .lengthSamples = 8, .frequencyHz = 12000.0, .velocity = 1.0F}};
    lamusica::audio::AudioGraph noteSequenceGraph{
        .nodes = {noteSequenceNode}, .connections = {}, .outputNodeId = "note-sequence"};
    const auto noteSequenceActive =
        lamusica::audio::renderGraphRange(noteSequenceGraph, {.outputPath = "unused.wav",
                                                              .startSample = 0,
                                                              .frames = 4,
                                                              .sampleRate = 48000.0,
                                                              .channels = 2});
    require(std::abs(noteSequenceActive.interleavedSamples[2]) > 0.5F,
            "audio graph note sequence renders active MIDI-style notes");
    const auto noteSequenceRest =
        lamusica::audio::renderGraphRange(noteSequenceGraph, {.outputPath = "unused.wav",
                                                              .startSample = 8,
                                                              .frames = 4,
                                                              .sampleRate = 48000.0,
                                                              .channels = 2});
    require(lamusica::audio::peakAbsoluteSample(noteSequenceRest) == 0.0F,
            "audio graph note sequence is silent outside note ranges");
    lamusica::audio::AudioEngine graphEngine{{.sampleRate = 48000.0, .maxBlockSize = 64}};
    const auto graphRendered = graphEngine.renderGraphOffline(graph, 64);
    require(graphRendered.frames == 64, "audio engine renders graph offline");
    require(graphEngine.transport().samplePosition == 64,
            "audio engine graph render advances transport");
    graph.connections.push_back(
        {.sourceNodeId = "master", .destinationNodeId = "osc", .gain = 1.0F});
    require(!lamusica::audio::validateGraph(graph), "audio graph rejects feedback cycle");

    lamusica::session::ProjectManifest graphManifest;
    graphManifest.tracks.push_back(
        {.id = "audio-track", .name = "Audio Track", .type = lamusica::session::TrackType::Audio});
    graphManifest.tracks.push_back(
        {.id = "master-track", .name = "Master", .type = lamusica::session::TrackType::Master});
    graphManifest.clips.push_back({.id = "compiled-clip",
                                   .trackId = "audio-track",
                                   .type = lamusica::session::ClipType::Audio,
                                   .startSample = 0,
                                   .lengthSamples = 48000,
                                   .gainDb = -6.0F});
    graphManifest.routing.push_back(
        {.sourceTrackId = "audio-track", .destinationTrackId = "master-track"});
    const auto compiledGraph =
        lamusica::session::compileProjectAudioGraph(graphManifest, lamusica::session::MixerState{});
    require(lamusica::audio::validateGraph(compiledGraph),
            "session graph compiler emits valid graph");
    require(compiledGraph.outputNodeId == "track:master-track",
            "session graph compiler uses master track as output");
    lamusica::audio::AudioEngine compiledEngine{{.sampleRate = 48000.0, .maxBlockSize = 64}};
    const auto compiledRender = compiledEngine.renderGraphOffline(compiledGraph, 64);
    require(std::ranges::any_of(compiledRender.interleavedSamples,
                                [](float sample) { return sample != 0.0F; }),
            "compiled session graph renders signal");
    lamusica::session::MixerState graphMixer;
    lamusica::session::addChannel(graphMixer, {.id = "audio-track",
                                               .name = "Audio Track",
                                               .type = lamusica::session::ChannelType::Audio,
                                               .volumeDb = -6.0F});
    const auto mixedCompiledGraph =
        lamusica::session::compileProjectAudioGraph(graphManifest, graphMixer);
    lamusica::audio::AudioEngine mixedCompiledEngine{{.sampleRate = 48000.0, .maxBlockSize = 64}};
    const auto mixedCompiledRender = mixedCompiledEngine.renderGraphOffline(mixedCompiledGraph, 64);
    require(std::abs(mixedCompiledRender.interleavedSamples[2]) <
                std::abs(compiledRender.interleavedSamples[2]),
            "session graph compiler applies matching mixer channel gain to track buses");
    lamusica::session::MixerState pannedGraphMixer;
    lamusica::session::addChannel(pannedGraphMixer, {.id = "audio-track",
                                                     .name = "Audio Track",
                                                     .type = lamusica::session::ChannelType::Audio,
                                                     .pan = 1.0F});
    const auto pannedCompiledGraph =
        lamusica::session::compileProjectAudioGraph(graphManifest, pannedGraphMixer);
    lamusica::audio::AudioEngine pannedCompiledEngine{{.sampleRate = 48000.0, .maxBlockSize = 64}};
    const auto pannedCompiledRender =
        pannedCompiledEngine.renderGraphOffline(pannedCompiledGraph, 64);
    require(std::abs(pannedCompiledRender.interleavedSamples[2]) <
                std::abs(pannedCompiledRender.interleavedSamples[3]),
            "session graph compiler applies mixer pan to matching track buses");
    graphManifest.trackMix.push_back({.trackId = "audio-track",
                                      .volumeDb = -12.0F,
                                      .pan = -0.5F,
                                      .muted = false,
                                      .solo = false});
    const auto persistedTrackMixGraph =
        lamusica::session::compileProjectAudioGraph(graphManifest, lamusica::session::MixerState{});
    const auto persistedTrackMixNode = std::ranges::find_if(
        persistedTrackMixGraph.nodes,
        [](const lamusica::audio::GraphNode& node) { return node.id == "track:audio-track"; });
    require(persistedTrackMixNode != persistedTrackMixGraph.nodes.end() &&
                std::abs(persistedTrackMixNode->gain - lamusica::session::dbToLinearGain(-12.0F)) <
                    0.0001F &&
                persistedTrackMixNode->pan == -0.5F,
            "session graph compiler applies persisted track mix to track buses");
    graphManifest.trackMix.push_back({.trackId = "master-track", .solo = true});
    const auto soloTrackMixGraph =
        lamusica::session::compileProjectAudioGraph(graphManifest, lamusica::session::MixerState{});
    const auto soloTrackMixNode =
        std::ranges::find_if(soloTrackMixGraph.nodes, [](const lamusica::audio::GraphNode& node) {
            return node.id == "track:audio-track";
        });
    require(soloTrackMixNode != soloTrackMixGraph.nodes.end() && soloTrackMixNode->gain == 0.0F,
            "session graph compiler mutes non-solo persisted track mix buses during solo");
    graphManifest.trackMix.clear();
    lamusica::session::MixerState invertedGraphMixer;
    lamusica::session::addChannel(invertedGraphMixer,
                                  {.id = "audio-track",
                                   .name = "Audio Track",
                                   .type = lamusica::session::ChannelType::Audio,
                                   .phaseInverted = true});
    const auto invertedCompiledGraph =
        lamusica::session::compileProjectAudioGraph(graphManifest, invertedGraphMixer);
    lamusica::audio::AudioEngine invertedCompiledEngine{
        {.sampleRate = 48000.0, .maxBlockSize = 64}};
    const auto invertedCompiledRender =
        invertedCompiledEngine.renderGraphOffline(invertedCompiledGraph, 64);
    require(invertedCompiledRender.interleavedSamples[2] * compiledRender.interleavedSamples[2] <
                0.0F,
            "session graph compiler applies mixer phase inversion");
    lamusica::session::MixerState hardwareOutputMixer;
    lamusica::session::addChannel(
        hardwareOutputMixer,
        {.id = "master-track", .name = "Master", .type = lamusica::session::ChannelType::Master});
    lamusica::session::addChannel(hardwareOutputMixer,
                                  {.id = "output-1-2",
                                   .name = "Output 1-2",
                                   .type = lamusica::session::ChannelType::HardwareOutput});
    lamusica::session::addRoute(hardwareOutputMixer, {.sourceChannelId = "master-track",
                                                      .destinationChannelId = "output-1-2"});
    const auto hardwareOutputGraph =
        lamusica::session::compileProjectAudioGraph(graphManifest, hardwareOutputMixer);
    require(hardwareOutputGraph.outputNodeId == "channel:output-1-2",
            "session graph compiler routes master output to hardware output channel");
    lamusica::audio::AudioEngine hardwareOutputEngine{{.sampleRate = 48000.0, .maxBlockSize = 64}};
    const auto hardwareOutputRender =
        hardwareOutputEngine.renderGraphOffline(hardwareOutputGraph, 64);
    require(std::ranges::any_of(hardwareOutputRender.interleavedSamples,
                                [](float sample) { return sample != 0.0F; }),
            "session graph compiler renders through hardware output routing");
    require(lamusica::session::dbToLinearGain(0.0F) == 1.0F,
            "session graph compiler maps 0 dB to unity");

    const auto bouncedRange =
        lamusica::audio::renderGraphRange(compiledGraph, {.outputPath = "unused.wav",
                                                          .startSample = 32,
                                                          .frames = 16,
                                                          .sampleRate = 48000.0,
                                                          .channels = 2});
    require(bouncedRange.frames == 16, "bounce renders selected range frame count");
    const auto compiledClip =
        std::ranges::find_if(compiledGraph.nodes, [](const lamusica::audio::GraphNode& node) {
            return node.id == "clip:compiled-clip";
        });
    require(compiledClip != compiledGraph.nodes.end(), "compiled graph contains clip source");
    const auto expectedBouncedSample = static_cast<float>(
        std::sin((33.0 * compiledClip->frequencyHz * 2.0 * std::numbers::pi) / 48000.0) *
        compiledClip->gain);
    require(std::abs(bouncedRange.interleavedSamples[2] - expectedBouncedSample) < 0.000001F,
            "bounce selected range starts at requested sample");

    const auto stemDirectory = std::filesystem::temp_directory_path() / "lamusica-stem-export";
    std::filesystem::remove_all(stemDirectory);
    lamusica::session::ProjectManifest stemManifest;
    stemManifest.tracks.push_back(
        {.id = "stem-a", .name = "Stem A", .type = lamusica::session::TrackType::Audio});
    stemManifest.tracks.push_back(
        {.id = "stem-b", .name = "Stem B", .type = lamusica::session::TrackType::Audio});
    stemManifest.tracks.push_back(
        {.id = "stem-master", .name = "Master", .type = lamusica::session::TrackType::Master});
    stemManifest.clips.push_back({.id = "clip-a",
                                  .trackId = "stem-a",
                                  .type = lamusica::session::ClipType::Audio,
                                  .lengthSamples = 48000,
                                  .gainDb = -3.0F});
    stemManifest.clips.push_back({.id = "clip-b",
                                  .trackId = "stem-b",
                                  .type = lamusica::session::ClipType::Audio,
                                  .lengthSamples = 48000,
                                  .gainDb = -9.0F});
    stemManifest.routing.push_back(
        {.sourceTrackId = "stem-a", .destinationTrackId = "stem-master"});
    stemManifest.routing.push_back(
        {.sourceTrackId = "stem-b", .destinationTrackId = "stem-master"});
    lamusica::session::MixerState soloStemMixer;
    lamusica::session::addChannel(soloStemMixer, {.id = "stem-a",
                                                  .name = "Stem A",
                                                  .type = lamusica::session::ChannelType::Audio,
                                                  .solo = true});
    lamusica::session::addChannel(
        soloStemMixer,
        {.id = "stem-b", .name = "Stem B", .type = lamusica::session::ChannelType::Audio});
    const auto soloCompiledGraph =
        lamusica::session::compileProjectAudioGraph(stemManifest, soloStemMixer);
    const auto soloedTrack =
        std::ranges::find_if(soloCompiledGraph.nodes, [](const lamusica::audio::GraphNode& node) {
            return node.id == "track:stem-a";
        });
    const auto mutedBySoloTrack =
        std::ranges::find_if(soloCompiledGraph.nodes, [](const lamusica::audio::GraphNode& node) {
            return node.id == "track:stem-b";
        });
    require(soloedTrack != soloCompiledGraph.nodes.end() &&
                mutedBySoloTrack != soloCompiledGraph.nodes.end() && soloedTrack->gain > 0.0F &&
                mutedBySoloTrack->gain == 0.0F,
            "session graph compiler mutes non-solo mixer tracks");
    lamusica::session::ProjectManifest sendManifest;
    sendManifest.tracks.push_back(
        {.id = "send-source", .name = "Send Source", .type = lamusica::session::TrackType::Audio});
    sendManifest.tracks.push_back(
        {.id = "return-a", .name = "Return A", .type = lamusica::session::TrackType::Return});
    sendManifest.tracks.push_back(
        {.id = "send-master", .name = "Master", .type = lamusica::session::TrackType::Master});
    sendManifest.clips.push_back({.id = "send-clip",
                                  .trackId = "send-source",
                                  .type = lamusica::session::ClipType::Audio,
                                  .lengthSamples = 48000,
                                  .gainDb = -6.0F});
    sendManifest.routing.push_back(
        {.sourceTrackId = "send-source", .destinationTrackId = "send-master"});
    sendManifest.routing.push_back(
        {.sourceTrackId = "return-a", .destinationTrackId = "send-master"});
    lamusica::session::MixerState sendMixer;
    lamusica::session::addChannel(
        sendMixer,
        {.id = "send-source",
         .name = "Send Source",
         .type = lamusica::session::ChannelType::Audio,
         .sends = {{.id = "send-1", .destinationChannelId = "return-a", .gainDb = 0.0F}}});
    lamusica::session::addChannel(
        sendMixer,
        {.id = "return-a", .name = "Return A", .type = lamusica::session::ChannelType::Return});
    const auto noSendGraph =
        lamusica::session::compileProjectAudioGraph(sendManifest, lamusica::session::MixerState{});
    const auto withSendGraph = lamusica::session::compileProjectAudioGraph(sendManifest, sendMixer);
    lamusica::audio::AudioEngine noSendEngine{{.sampleRate = 48000.0, .maxBlockSize = 64}};
    lamusica::audio::AudioEngine withSendEngine{{.sampleRate = 48000.0, .maxBlockSize = 64}};
    const auto noSendRender = noSendEngine.renderGraphOffline(noSendGraph, 64);
    const auto withSendRender = withSendEngine.renderGraphOffline(withSendGraph, 64);
    require(lamusica::audio::peakAbsoluteSample(withSendRender) >
                lamusica::audio::peakAbsoluteSample(noSendRender),
            "session graph compiler renders mixer sends into return routing");
    sendMixer.channels.front().muted = true;
    sendMixer.channels.front().sends.front().preFader = true;
    const auto preFaderSendGraph =
        lamusica::session::compileProjectAudioGraph(sendManifest, sendMixer);
    lamusica::audio::AudioEngine preFaderSendEngine{{.sampleRate = 48000.0, .maxBlockSize = 64}};
    const auto preFaderSendRender = preFaderSendEngine.renderGraphOffline(preFaderSendGraph, 64);
    require(lamusica::audio::peakAbsoluteSample(preFaderSendRender) > 0.0F,
            "session graph compiler lets pre-fader sends bypass source fader mute");
    const auto mixExport = lamusica::session::exportProjectMixToWav(
        stemManifest, {},
        {.outputPath = stemDirectory / "mix.wav",
         .startSample = 0,
         .frames = 64,
         .sampleRate = 48000.0,
         .channels = 2,
         .bitDepth = lamusica::audio::ExportBitDepth::Pcm16,
         .ditherMode = lamusica::audio::DitherMode::Triangular});
    require(mixExport.frames == 64 && std::filesystem::exists(stemDirectory / "mix.wav"),
            "project export writes mix WAV");
    require(mixExport.bitDepth == lamusica::audio::ExportBitDepth::Pcm16 &&
                mixExport.ditherMode == lamusica::audio::DitherMode::Triangular,
            "project export preserves bit depth and dithering options");
    const auto mix24Export = lamusica::session::exportProjectMixToWav(
        stemManifest, {},
        {.outputPath = stemDirectory / "mix-24.wav",
         .startSample = 0,
         .frames = 32,
         .sampleRate = 48000.0,
         .channels = 2,
         .bitDepth = lamusica::audio::ExportBitDepth::Pcm24,
         .ditherMode = lamusica::audio::DitherMode::Triangular});
    const auto importedMix24 = lamusica::audio::readPcm16Wav(stemDirectory / "mix-24.wav");
    require(mix24Export.bitDepth == lamusica::audio::ExportBitDepth::Pcm24 &&
                importedMix24.bitsPerSample == 24 && importedMix24.audio.frames == 32,
            "project mix export writes PCM24 WAV when requested");
    const auto stemExports = lamusica::session::exportProjectStemsToWav(
        stemManifest, {},
        {.outputDirectory = stemDirectory,
         .trackIds = {"stem-a", "stem-b"},
         .startSample = 0,
         .frames = 64,
         .sampleRate = 48000.0,
         .channels = 2,
         .bitDepth = lamusica::audio::ExportBitDepth::Pcm16,
         .ditherMode = lamusica::audio::DitherMode::Triangular});
    require(stemExports.size() == 2, "project export writes requested stem count");
    require(stemExports.front().bounce.ditherMode == lamusica::audio::DitherMode::Triangular,
            "project stem export preserves dithering options");
    require(std::filesystem::exists(stemDirectory / "stem-a.wav"),
            "project export writes first stem WAV");
    require(std::filesystem::exists(stemDirectory / "stem-b.wav"),
            "project export writes second stem WAV");
    const auto stemA = lamusica::audio::readPcm16Wav(stemDirectory / "stem-a.wav");
    const auto stemB = lamusica::audio::readPcm16Wav(stemDirectory / "stem-b.wav");
    require(stemA.audio.frames == 64 && stemB.audio.frames == 64,
            "project exported stems import with requested frames");
    require(lamusica::audio::peakAbsoluteSample(stemA.audio) >
                lamusica::audio::peakAbsoluteSample(stemB.audio),
            "project exported stems preserve independent track gain");

    lamusica::audio::AudioEngine loopEngine{{.sampleRate = 48000.0, .maxBlockSize = 64}};
    loopEngine.setLoop(true, 8, 40);
    const auto loopMixOptions = lamusica::session::makeLoopMixExportOptions(
        stemDirectory / "loop-mix.wav", loopEngine.transport(), 48000.0, 2);
    require(loopMixOptions.startSample == 8 && loopMixOptions.frames == 32,
            "loop mix export derives selected loop range");
    const auto loopMixExport =
        lamusica::session::exportProjectMixToWav(stemManifest, {}, loopMixOptions);
    require(loopMixExport.frames == 32 && std::filesystem::exists(stemDirectory / "loop-mix.wav"),
            "loop mix export writes loop region");
    const auto loopStemOptions = lamusica::session::makeLoopStemExportOptions(
        stemDirectory / "loop-stems", {"stem-a"}, loopEngine.transport(), 48000.0, 2);
    const auto loopStemExports =
        lamusica::session::exportProjectStemsToWav(stemManifest, {}, loopStemOptions);
    require(loopStemExports.size() == 1 && loopStemExports.front().bounce.frames == 32,
            "loop stem export writes selected tracks for loop region");
    lamusica::mcp_bridge::DaemonSession mcpExportSession;
    mcpExportSession.attachProject(
        "fixtures/empty.Project.lamusica",
        {lamusica::mcp_bridge::Capability::ReadOnly, lamusica::mcp_bridge::Capability::Render});
    lamusica::mcp_bridge::RenderJobQueue mcpExportQueue;
    const auto mcpMixPath = stemDirectory / "mcp-mix.wav";
    std::filesystem::remove(mcpMixPath);
    const auto mcpMixJob =
        mcpExportQueue.enqueueProjectMixExport(mcpExportSession, "mcp-mix-export", stemManifest, {},
                                               {.outputPath = mcpMixPath,
                                                .startSample = 0,
                                                .frames = 64,
                                                .sampleRate = 48000.0,
                                                .channels = 2});
    require(mcpMixJob.status == lamusica::mcp_bridge::RenderJobStatus::Completed &&
                mcpMixJob.resultManifestJson.find("\"type\":\"project_mix_export\"") !=
                    std::string::npos &&
                mcpMixJob.resultManifestJson.find("\"explicitExport\":true") != std::string::npos &&
                mcpMixJob.resultManifestJson.find("\"peakAfterDither\"") != std::string::npos,
            "MCP project mix export uses session export path");
    require(std::filesystem::exists(mcpMixPath), "MCP project mix export writes WAV");
    const auto mcpBatchAPath = stemDirectory / "mcp-batch-a.wav";
    const auto mcpBatchBPath = stemDirectory / "mcp-batch-b.wav";
    std::filesystem::remove(mcpBatchAPath);
    std::filesystem::remove(mcpBatchBPath);
    const auto mcpBatchJob = mcpExportQueue.enqueueBatchProjectMixExport(
        mcpExportSession, "mcp-batch-export", stemManifest, {},
        {{.outputPath = mcpBatchAPath,
          .startSample = 0,
          .frames = 32,
          .sampleRate = 48000.0,
          .channels = 2},
         {.outputPath = mcpBatchBPath,
          .startSample = 16,
          .frames = 32,
          .sampleRate = 48000.0,
          .channels = 2}});
    require(mcpBatchJob.status == lamusica::mcp_bridge::RenderJobStatus::Completed &&
                mcpBatchJob.resultManifestJson.find("\"type\":\"batch_project_mix_export\"") !=
                    std::string::npos &&
                mcpBatchJob.resultManifestJson.find("\"explicitExport\":true") !=
                    std::string::npos &&
                mcpBatchJob.resultManifestJson.find("\"peakAfterDither\"") != std::string::npos,
            "MCP batch project mix export writes explicit export manifest");
    require(std::filesystem::exists(mcpBatchAPath) && std::filesystem::exists(mcpBatchBPath),
            "MCP batch project mix export writes all requested files");
    const auto refusedBatchOverwrite = mcpExportQueue.enqueueBatchProjectMixExport(
        mcpExportSession, "mcp-batch-overwrite-denied", stemManifest, {},
        {{.outputPath = mcpBatchAPath,
          .startSample = 0,
          .frames = 32,
          .sampleRate = 48000.0,
          .channels = 2}});
    require(refusedBatchOverwrite.status == lamusica::mcp_bridge::RenderJobStatus::Failed &&
                !refusedBatchOverwrite.confirmationToken.empty(),
            "MCP batch project mix export refuses overwrite without confirmation");
    const auto mcpStemDirectory = stemDirectory / "mcp-stems";
    std::filesystem::remove_all(mcpStemDirectory);
    const auto mcpStemJob = mcpExportQueue.enqueueProjectStemExport(
        mcpExportSession, "mcp-stem-export", stemManifest, {},
        {.outputDirectory = mcpStemDirectory,
         .trackIds = {"stem-a", "stem-b"},
         .startSample = 0,
         .frames = 64,
         .sampleRate = 48000.0,
         .channels = 2});
    require(mcpStemJob.status == lamusica::mcp_bridge::RenderJobStatus::Completed &&
                mcpStemJob.resultManifestJson.find("\"type\":\"stem_export\"") !=
                    std::string::npos &&
                mcpStemJob.resultManifestJson.find("\"trackId\":\"stem-a\"") !=
                    std::string::npos &&
                mcpStemJob.resultManifestJson.find("\"peakAfterDither\"") != std::string::npos,
            "MCP stem export writes batch manifest");
    require(std::filesystem::exists(mcpStemDirectory / "stem-a.wav") &&
                std::filesystem::exists(mcpStemDirectory / "stem-b.wav"),
            "MCP stem export writes requested files");
    const auto refusedStemOverwrite = mcpExportQueue.enqueueProjectStemExport(
        mcpExportSession, "mcp-stem-overwrite-denied", stemManifest, {},
        {.outputDirectory = mcpStemDirectory,
         .trackIds = {"stem-a"},
         .startSample = 0,
         .frames = 64,
         .sampleRate = 48000.0,
         .channels = 2});
    require(refusedStemOverwrite.status == lamusica::mcp_bridge::RenderJobStatus::Failed &&
                !refusedStemOverwrite.confirmationToken.empty(),
            "MCP stem export refuses overwrite without confirmation");
    bool rejectedDisabledLoopExport = false;
    try {
        (void)lamusica::session::makeLoopMixExportOptions(
            stemDirectory / "disabled-loop.wav", lamusica::audio::TransportState{}, 48000.0, 2);
    } catch (const std::exception&) {
        rejectedDisabledLoopExport = true;
    }
    require(rejectedDisabledLoopExport, "loop export rejects disabled loop range");
    std::filesystem::remove_all(stemDirectory);

    lamusica::session::ProjectManifest commandManifest;
    lamusica::commands::CommandHistory history;
    auto addTrack =
        lamusica::commands::makeAddTrackCommand("cmd-1", "audit-1",
                                                {.id = "track-command-1",
                                                 .name = "Command Track",
                                                 .type = lamusica::session::TrackType::Audio});
    require(addTrack->validate(commandManifest).ok, "add track command validates");
    require(addTrack->preview(commandManifest) == "Add audio track \"Command Track\"",
            "add track command previews");
    require(history.execute(commandManifest, std::move(addTrack)).ok, "add track command applies");
    require(commandManifest.tracks.size() == 1, "add track command mutates manifest");
    require(history.undo(commandManifest).ok, "add track command undoes");
    require(commandManifest.tracks.empty(), "undo restores manifest");
    require(history.redo(commandManifest).ok, "add track command redoes");
    require(commandManifest.tracks.size() == 1, "redo restores manifest mutation");

    lamusica::session::ProjectManifest explicitRedoManifest;
    auto explicitRedoTrack =
        lamusica::commands::makeAddTrackCommand("cmd-1b", "audit-1b",
                                                {.id = "track-command-redo",
                                                 .name = "Explicit Redo Track",
                                                 .type = lamusica::session::TrackType::Audio});
    require(explicitRedoTrack->apply(explicitRedoManifest).ok, "command interface applies");
    require(explicitRedoTrack->undo(explicitRedoManifest).ok, "command interface undoes");
    require(explicitRedoTrack->redo(explicitRedoManifest).ok, "command interface exposes redo");
    require(explicitRedoManifest.tracks.size() == 1, "explicit redo reapplies command mutation");

    auto addClip =
        lamusica::commands::makeAddClipCommand("cmd-2", "audit-2",
                                               {.id = "clip-1",
                                                .trackId = "track-command-1",
                                                .type = lamusica::session::ClipType::Audio,
                                                .startSample = 100,
                                                .lengthSamples = 1000});
    require(history.execute(commandManifest, std::move(addClip)).ok, "add clip command applies");
    require(commandManifest.clips.size() == 1, "add clip command mutates manifest");

    auto fadeClip =
        lamusica::commands::makeSetClipFadeCommand("cmd-2a", "audit-2a", "clip-1", 20, 30);
    require(history.execute(commandManifest, std::move(fadeClip)).ok,
            "set clip fade command applies");
    require(commandManifest.clips.front().fadeInSamples == 20, "fade command sets fade in");
    require(commandManifest.clips.front().fadeOutSamples == 30, "fade command sets fade out");
    require(history.undo(commandManifest).ok, "set clip fade command undoes");
    require(commandManifest.clips.front().fadeInSamples == 0, "fade undo restores fade in");

    auto renderPropertiesClip = lamusica::commands::makeSetClipRenderPropertiesCommand(
        "cmd-2aa", "audit-2aa", "clip-1", -6.0F, true, true);
    require(history.execute(commandManifest, std::move(renderPropertiesClip)).ok,
            "set clip render properties command applies");
    require(commandManifest.clips.front().gainDb == -6.0F && commandManifest.clips.front().muted &&
                commandManifest.clips.front().reversed,
            "set clip render properties command mutates gain mute and reverse metadata");
    require(history.undo(commandManifest).ok, "set clip render properties command undoes");
    require(commandManifest.clips.front().gainDb == 0.0F && !commandManifest.clips.front().muted &&
                !commandManifest.clips.front().reversed,
            "set clip render properties undo restores clip metadata");

    auto trimClip =
        lamusica::commands::makeTrimClipCommand("cmd-2b", "audit-2b", "clip-1", 200, 900, 100);
    require(history.execute(commandManifest, std::move(trimClip)).ok, "trim clip command applies");
    require(commandManifest.clips.front().startSample == 200, "trim command sets start");
    require(commandManifest.clips.front().lengthSamples == 900, "trim command sets length");
    require(commandManifest.clips.front().sourceOffsetSamples == 100,
            "trim command sets source offset");
    require(history.undo(commandManifest).ok, "trim clip command undoes");
    require(commandManifest.clips.front().startSample == 100, "trim undo restores start");

    auto slipClip = lamusica::commands::makeSlipClipCommand("cmd-2bb", "audit-2bb", "clip-1", 384);
    require(slipClip->preview(commandManifest) == "Slip clip \"clip-1\" to source sample 384",
            "slip clip command previews");
    require(history.execute(commandManifest, std::move(slipClip)).ok, "slip clip command applies");
    require(commandManifest.clips.front().startSample == 100 &&
                commandManifest.clips.front().lengthSamples == 1000 &&
                commandManifest.clips.front().sourceOffsetSamples == 384,
            "slip clip command preserves timeline placement while changing source offset");
    require(history.undo(commandManifest).ok, "slip clip command undoes");
    require(commandManifest.clips.front().startSample == 100 &&
                commandManifest.clips.front().lengthSamples == 1000 &&
                commandManifest.clips.front().sourceOffsetSamples == 0,
            "slip clip undo restores source offset only");

    auto splitClip = lamusica::commands::makeSplitClipCommand(commandManifest, "cmd-2c", "audit-2c",
                                                              "clip-1", "clip-1-right", 600);
    require(history.execute(commandManifest, std::move(splitClip)).ok,
            "split clip command applies");
    require(commandManifest.clips.size() == 2, "split clip creates right clip");
    require(commandManifest.clips.front().lengthSamples == 500, "split clip trims left length");
    require(history.undo(commandManifest).ok, "split clip command undoes");
    require(commandManifest.clips.size() == 1, "split undo removes right clip");
    require(commandManifest.clips.front().lengthSamples == 1000, "split undo restores left length");

    lamusica::session::ProjectManifest transactionManifest;
    std::vector<lamusica::commands::CommandPtr> dependentCommands;
    dependentCommands.push_back(
        lamusica::commands::makeAddTrackCommand("cmd-transaction-track", "audit-transaction-track",
                                                {.id = "transaction-track",
                                                 .name = "Transaction Track",
                                                 .type = lamusica::session::TrackType::Audio}));
    dependentCommands.push_back(
        lamusica::commands::makeAddClipCommand("cmd-transaction-clip", "audit-transaction-clip",
                                               {.id = "transaction-clip",
                                                .trackId = "transaction-track",
                                                .type = lamusica::session::ClipType::Audio,
                                                .startSample = 0,
                                                .lengthSamples = 48000}));
    auto dependentTransaction = std::make_unique<lamusica::commands::TransactionCommand>(
        "cmd-transaction", "audit-transaction", "create_track_with_clip",
        std::move(dependentCommands));
    require(dependentTransaction->validate(transactionManifest).ok &&
                transactionManifest.tracks.empty() && transactionManifest.clips.empty(),
            "transaction validation simulates dependent commands without mutating manifest");
    require(history.execute(transactionManifest, std::move(dependentTransaction)).ok,
            "transaction command applies dependent multi-step edit");
    require(transactionManifest.tracks.size() == 1 && transactionManifest.clips.size() == 1,
            "transaction command applies all subcommands atomically");
    require(history.undo(transactionManifest).ok && transactionManifest.tracks.empty() &&
                transactionManifest.clips.empty(),
            "transaction command undo rolls back all subcommands");

    auto moveClip = lamusica::commands::makeMoveClipCommand("cmd-3", "audit-3", "clip-1", 24000);
    require(moveClip->preview(commandManifest) == "Move clip \"clip-1\" to sample 24000",
            "move clip command previews");
    require(history.execute(commandManifest, std::move(moveClip)).ok, "move clip command applies");
    require(commandManifest.clips.front().startSample == 24000,
            "move clip command mutates manifest");
    require(history.undo(commandManifest).ok, "move clip command undoes");
    require(commandManifest.clips.front().startSample == 100, "move clip undo restores start");
    auto duplicateClip = lamusica::commands::makeDuplicateClipCommand(
        "cmd-3a", "audit-3a", "clip-1", "clip-1-copy", 48000);
    require(history.execute(commandManifest, std::move(duplicateClip)).ok,
            "duplicate clip command applies");
    require(commandManifest.clips.size() == 2 && commandManifest.clips.back().id == "clip-1-copy",
            "duplicate clip command creates copied clip");
    require(history.undo(commandManifest).ok, "duplicate clip command undoes");
    require(commandManifest.clips.size() == 1, "duplicate clip undo removes copied clip");
    lamusica::audio::RenderedAudio clipSource{.channels = 1,
                                              .frames = 12,
                                              .interleavedSamples = {1.0F, 1.0F, 1.0F, 1.0F, 1.0F,
                                                                     1.0F, 1.0F, 1.0F, 1.0F, 1.0F,
                                                                     1.0F, 1.0F}};
    lamusica::session::Clip fadeRenderClip{.id = "fade-render",
                                           .trackId = "track-command-1",
                                           .type = lamusica::session::ClipType::Audio,
                                           .lengthSamples = 12,
                                           .fadeInSamples = 4,
                                           .fadeOutSamples = 4};
    const auto fadedClip = lamusica::session::renderClipRegion(fadeRenderClip, clipSource);
    require(fadedClip.interleavedSamples.front() == 0.0F &&
                fadedClip.interleavedSamples[4] == 1.0F &&
                fadedClip.interleavedSamples.back() == 0.0F,
            "audio clip render applies fade in and fade out metadata nondestructively");
    require(clipSource.interleavedSamples.front() == 1.0F,
            "audio clip render does not mutate source media");
    fadeRenderClip.fadeInSamples = 0;
    fadeRenderClip.fadeOutSamples = 0;
    fadeRenderClip.gainDb = 0.0F;
    const auto normalizedClip =
        lamusica::session::normalizeClipGain(fadeRenderClip, clipSource, 0.5F);
    const auto normalizedClipRender =
        lamusica::session::renderClipRegion(normalizedClip.clip, clipSource);
    require(std::abs(lamusica::session::clipPeakAmplitude(normalizedClip.clip, clipSource) - 0.5F) <
                0.000001F,
            "audio clip normalize computes nondestructive gain metadata");
    require(clipSource.interleavedSamples[4] == 1.0F &&
                std::abs(normalizedClipRender.interleavedSamples[4] - 0.5F) < 0.000001F,
            "audio clip normalize leaves source media unchanged");
    fadeRenderClip.reversed = true;
    fadeRenderClip.sourceOffsetSamples = 2;
    fadeRenderClip.lengthSamples = 4;
    fadeRenderClip.fadeInSamples = 0;
    fadeRenderClip.fadeOutSamples = 0;
    clipSource.interleavedSamples = {0.0F, 1.0F, 2.0F, 3.0F, 4.0F,  5.0F,
                                     6.0F, 7.0F, 8.0F, 9.0F, 10.0F, 11.0F};
    const auto reversedClip = lamusica::session::renderClipRegion(fadeRenderClip, clipSource);
    require(reversedClip.interleavedSamples.front() == 5.0F &&
                reversedClip.interleavedSamples.back() == 2.0F,
            "audio clip render applies source offset and reverse metadata");
    fadeRenderClip.reversed = false;
    fadeRenderClip.sourceOffsetSamples = 0;
    const lamusica::session::ClipGainEnvelope clipEnvelope{
        .clipId = "fade-render",
        .points = {{.samplePosition = 0, .gain = 0.0F}, {.samplePosition = 3, .gain = 1.0F}}};
    const auto envelopedClip =
        lamusica::session::renderClipRegionWithEnvelope(fadeRenderClip, clipSource, clipEnvelope);
    require(envelopedClip.interleavedSamples.front() == 0.0F &&
                std::abs(envelopedClip.interleavedSamples.back() - 3.0F) < 0.000001F &&
                clipSource.interleavedSamples[3] == 3.0F,
            "audio clip envelope renders nondestructive gain automation over clip content");
    fadeRenderClip.lengthSamples = 6;
    const lamusica::audio::RenderedAudio takeA{
        .channels = 1, .frames = 6, .interleavedSamples = {1.0F, 2.0F, 3.0F, 4.0F, 5.0F, 6.0F}};
    const lamusica::audio::RenderedAudio takeB{
        .channels = 1,
        .frames = 6,
        .interleavedSamples = {10.0F, 20.0F, 30.0F, 40.0F, 50.0F, 60.0F}};
    const lamusica::session::ClipTakeLane clipTakeLane{
        .clipId = "fade-render",
        .takes = {{.id = "take-a", .name = "Take A", .lengthSamples = 6},
                  {.id = "take-b", .name = "Take B", .lengthSamples = 6}}};
    const lamusica::session::ClipComp clipComp{
        .clipId = "fade-render",
        .segments = {{.takeId = "take-a", .clipStartSample = 0, .lengthSamples = 3},
                     {.takeId = "take-b", .clipStartSample = 3, .lengthSamples = 3}}};
    const std::vector<lamusica::session::ClipTakeSource> takeSources{
        {.takeId = "take-a", .audio = takeA}, {.takeId = "take-b", .audio = takeB}};
    const auto compedClip =
        lamusica::session::renderCompedClip(fadeRenderClip, clipTakeLane, clipComp, takeSources);
    require(compedClip.interleavedSamples[0] == 1.0F && compedClip.interleavedSamples[2] == 3.0F &&
                compedClip.interleavedSamples[3] == 1.0F &&
                compedClip.interleavedSamples[5] == 30.0F &&
                lamusica::session::maxAdjacentSampleDelta(compedClip) < 16.0F &&
                takeB.interleavedSamples[0] == 10.0F,
            "audio clip comping renders selected take segments with a nondestructive crossfade");
    require(compedClip.interleavedSamples[3] < compedClip.interleavedSamples[4] &&
                compedClip.interleavedSamples[4] < compedClip.interleavedSamples[5],
            "audio clip comping uses the previous take tail across the crossfade");
    const lamusica::session::ClipComp overlappingComp{
        .clipId = "fade-render",
        .segments = {{.takeId = "take-a", .clipStartSample = 0, .lengthSamples = 4},
                     {.takeId = "take-b", .clipStartSample = 3, .lengthSamples = 2}}};
    const auto overlappingCompedClip =
        lamusica::session::renderCompedClip(fadeRenderClip, clipTakeLane, overlappingComp,
                                            takeSources);
    require(overlappingCompedClip.interleavedSamples[3] == 4.0F &&
                overlappingCompedClip.interleavedSamples[4] == 20.0F,
            "audio clip comping permits boundary overlap inside the crossfade region");
    bool rejectedExcessiveOverlapComp = false;
    try {
        const lamusica::session::ClipComp excessiveOverlapComp{
            .clipId = "fade-render",
            .segments = {{.takeId = "take-a", .clipStartSample = 0, .lengthSamples = 4},
                         {.takeId = "take-b", .clipStartSample = 1, .lengthSamples = 2}}};
        (void)lamusica::session::renderCompedClip(fadeRenderClip, clipTakeLane,
                                                  excessiveOverlapComp, takeSources);
    } catch (const std::exception&) {
        rejectedExcessiveOverlapComp = true;
    }
    require(rejectedExcessiveOverlapComp,
            "audio clip comping rejects overlap beyond the crossfade region");
    const lamusica::audio::RenderedAudio abruptLeft{
        .channels = 1,
        .frames = 8,
        .interleavedSamples = {1.0F, 1.0F, 1.0F, 1.0F, 1.0F, 1.0F, 1.0F, 1.0F}};
    const lamusica::audio::RenderedAudio abruptRight{
        .channels = 1,
        .frames = 8,
        .interleavedSamples = {-1.0F, -1.0F, -1.0F, -1.0F, -1.0F, -1.0F, -1.0F, -1.0F}};
    const auto crossfade =
        lamusica::session::renderLinearCrossfade(abruptLeft, abruptRight, {.samples = 8});
    require(crossfade.interleavedSamples.front() == 1.0F &&
                crossfade.interleavedSamples.back() == -1.0F,
            "audio clip crossfade preserves endpoints");
    require(lamusica::session::maxAdjacentSampleDelta(crossfade) < 0.3F,
            "audio clip crossfade smooths abrupt fixture boundary");

    const auto compProjectRoot =
        std::filesystem::temp_directory_path() / "lamusica-bootstrap-comp.Project.lamusica";
    std::filesystem::remove_all(compProjectRoot);
    std::filesystem::create_directories(compProjectRoot / "Media");
    const auto compTakeAPath = compProjectRoot / "Media" / "take-a.wav";
    const auto compTakeBPath = compProjectRoot / "Media" / "take-b.wav";
    const std::vector<float> compTakeASamples(256, -0.75F);
    const std::vector<float> compTakeBSamples(256, 0.75F);
    lamusica::audio::writePcm16Wav(
        compTakeAPath,
        {.channels = 1, .frames = 256, .interleavedSamples = compTakeASamples}, 48000.0);
    lamusica::audio::writePcm16Wav(
        compTakeBPath,
        {.channels = 1, .frames = 256, .interleavedSamples = compTakeBSamples}, 48000.0);

    lamusica::session::ProjectManifest compManifest;
    compManifest.name = "Comp Render";
    compManifest.assets = {{.id = "take-a-asset",
                            .relativePath = "Media/take-a.wav",
                            .mediaType = "audio/wav"},
                           {.id = "take-b-asset",
                            .relativePath = "Media/take-b.wav",
                            .mediaType = "audio/wav"}};
    compManifest.tracks = {{.id = "comp-track",
                            .name = "Comp Track",
                            .type = lamusica::session::TrackType::Audio},
                           {.id = "master",
                            .name = "Master",
                            .type = lamusica::session::TrackType::Master}};
    compManifest.clips = {{.id = "comp-clip",
                           .trackId = "comp-track",
                           .type = lamusica::session::ClipType::Audio,
                           .startSample = 0,
                           .lengthSamples = 128,
                           .assetId = "take-a-asset"}};
    compManifest.takeLanes = {
        {.clipId = "comp-clip",
         .takes = {{.id = "take-a",
                    .name = "Take A",
                    .lengthSamples = 128,
                    .assetId = "take-a-asset"},
                   {.id = "take-b",
                    .name = "Take B",
                    .lengthSamples = 128,
                    .assetId = "take-b-asset"}}}};
    compManifest.comps = {
        {.clipId = "comp-clip",
         .segments = {{.takeId = "take-a", .clipStartSample = 0, .lengthSamples = 64},
                      {.takeId = "take-b", .clipStartSample = 64, .lengthSamples = 64}}}};
    compManifest.routing = {{.sourceTrackId = "comp-track", .destinationTrackId = "master"}};
    const auto roundTrippedCompManifest =
        lamusica::session::parseProjectManifest(
            lamusica::session::serializeProjectManifest(compManifest));
    require(roundTrippedCompManifest.schemaVersion ==
                    lamusica::session::currentProjectSchemaVersion &&
                roundTrippedCompManifest.takeLanes.size() == 1 &&
                roundTrippedCompManifest.comps.size() == 1,
            "project manifest round-trips comp take lanes and segments");
    const auto compGraph = lamusica::session::compileProjectAudioGraph(
        roundTrippedCompManifest, {}, {.projectRoot = compProjectRoot});
    const auto compSegmentNodeCount = std::ranges::count_if(
        compGraph.nodes, [](const lamusica::audio::GraphNode& node) {
            return node.id.find("clip:comp-clip#seg") == 0;
        });
    std::vector<float> compRender(128);
    const lamusica::audio::EngineConfig compEngineConfig{
        .sampleRate = 48000.0, .maxBlockSize = 32, .outputChannels = 1};
    lamusica::audio::renderGraph(compGraph, compEngineConfig, 0, 128, compRender);
    float compRenderMaxDelta = 0.0F;
    for (std::size_t frame = 1; frame < compRender.size(); ++frame) {
        compRenderMaxDelta =
            std::max(compRenderMaxDelta, std::abs(compRender[frame] - compRender[frame - 1U]));
    }
    require(compSegmentNodeCount == 2 && compRender[8] < -0.5F && compRender[120] > 0.5F &&
                compRenderMaxDelta < 0.8F && compTakeASamples.front() == -0.75F &&
                compTakeBSamples.front() == 0.75F,
            "graph compiler renders active comp segments click-free and nondestructively");
    auto editedCompManifest = roundTrippedCompManifest;
    editedCompManifest.comps.front().segments = {
        {.takeId = "take-b", .clipStartSample = 0, .lengthSamples = 64},
        {.takeId = "take-a", .clipStartSample = 64, .lengthSamples = 64}};
    const auto editedCompGraph = lamusica::session::compileProjectAudioGraph(
        editedCompManifest, {}, {.projectRoot = compProjectRoot});
    std::vector<float> editedCompRender(128);
    lamusica::audio::renderGraph(editedCompGraph, compEngineConfig, 0, 128, editedCompRender);
    require(editedCompRender != compRender &&
                lamusica::audio::readPcm16Wav(compTakeAPath).audio.interleavedSamples.front() <
                    -0.7F &&
                lamusica::audio::readPcm16Wav(compTakeBPath).audio.interleavedSamples.front() >
                    0.7F,
            "editing a comp changes rendered PCM without rewriting take source assets");
    const auto renderCompWithBlockSize = [&](std::uint32_t blockSize) {
        std::vector<float> rendered(128);
        const lamusica::audio::EngineConfig config{
            .sampleRate = 48000.0, .maxBlockSize = blockSize, .outputChannels = 1};
        lamusica::audio::renderGraph(compGraph, config, 0, 128, rendered);
        return rendered;
    };
    require(renderCompWithBlockSize(64) == compRender && renderCompWithBlockSize(128) == compRender &&
                renderCompWithBlockSize(512) == compRender,
            "comp render is deterministic across block sizes");
    const auto compSegmentNodePresent = [](const lamusica::audio::AudioGraph& graph) {
        return std::ranges::any_of(graph.nodes, [](const lamusica::audio::GraphNode& node) {
            return node.id.find("clip:comp-clip#seg") == 0;
        });
    };
    auto emptyCompManifest = roundTrippedCompManifest;
    emptyCompManifest.comps.front().segments.clear();
    const auto emptyCompGraph = lamusica::session::compileProjectAudioGraph(
        emptyCompManifest, {}, {.projectRoot = compProjectRoot});
    require(!compSegmentNodePresent(emptyCompGraph) &&
                std::ranges::any_of(emptyCompGraph.nodes, [](const lamusica::audio::GraphNode& node) {
                    return node.id == "clip:comp-clip";
                }),
            "empty comp falls back to the plain clip render path");
    auto singleSegmentCompManifest = roundTrippedCompManifest;
    singleSegmentCompManifest.comps.front().segments = {
        {.takeId = "take-a", .clipStartSample = 0, .lengthSamples = 128}};
    const auto singleSegmentCompGraph = lamusica::session::compileProjectAudioGraph(
        singleSegmentCompManifest, {}, {.projectRoot = compProjectRoot});
    require(!compSegmentNodePresent(singleSegmentCompGraph) &&
                std::ranges::any_of(singleSegmentCompGraph.nodes,
                                    [](const lamusica::audio::GraphNode& node) {
                                        return node.id == "clip:comp-clip";
                                    }),
            "single-segment comp falls back to the plain clip render path");
    auto mutedTakeCompManifest = roundTrippedCompManifest;
    mutedTakeCompManifest.takeLanes.front().takes[1].muted = true;
    const auto mutedTakeCompGraph = lamusica::session::compileProjectAudioGraph(
        mutedTakeCompManifest, {}, {.projectRoot = compProjectRoot});
    std::vector<float> mutedTakeCompRender(128);
    lamusica::audio::renderGraph(mutedTakeCompGraph, compEngineConfig, 0, 128,
                                 mutedTakeCompRender);
    float mutedTakeMaxDelta = 0.0F;
    for (std::size_t frame = 1; frame < mutedTakeCompRender.size(); ++frame) {
        mutedTakeMaxDelta = std::max(
            mutedTakeMaxDelta,
            std::abs(mutedTakeCompRender[frame] - mutedTakeCompRender[frame - 1U]));
    }
    require(compSegmentNodePresent(mutedTakeCompGraph) && mutedTakeCompRender[8] < -0.5F &&
                std::abs(mutedTakeCompRender[120]) < 0.000001F && mutedTakeMaxDelta < 0.8F,
            "muted comp take renders silence without breaking adjacent crossfades");
    std::filesystem::remove_all(compProjectRoot);
    auto renameTrack = lamusica::commands::makeSetTrackNameCommand("cmd-3b", "audit-3b",
                                                                   "track-command-1", "Renamed");
    require(history.execute(commandManifest, std::move(renameTrack)).ok,
            "set track name command applies");
    require(commandManifest.tracks.front().name == "Renamed", "set track name command mutates");
    require(history.undo(commandManifest).ok, "set track name command undoes");
    require(commandManifest.tracks.front().name == "Command Track",
            "set track name undo restores name");
    commandManifest.tracks.push_back({.id = "route-destination",
                                      .name = "Route Destination",
                                      .type = lamusica::session::TrackType::Group});
    auto addRoute = lamusica::commands::makeAddRoutingConnectionCommand(
        "cmd-3c", "audit-3c",
        {.sourceTrackId = "track-command-1", .destinationTrackId = "route-destination"});
    require(history.execute(commandManifest, std::move(addRoute)).ok,
            "add routing connection command applies");
    require(commandManifest.routing.size() == 1, "add routing connection mutates manifest");
    auto removeRoute = lamusica::commands::makeRemoveRoutingConnectionCommand(
        "cmd-3c-remove", "audit-3c-remove",
        {.sourceTrackId = "track-command-1", .destinationTrackId = "route-destination"});
    require(history.execute(commandManifest, std::move(removeRoute)).ok,
            "remove routing connection command applies");
    require(commandManifest.routing.empty(), "remove routing connection mutates manifest");
    require(history.undo(commandManifest).ok, "remove routing connection command undoes");
    require(commandManifest.routing.size() == 1, "remove routing connection undo restores route");
    require(history.undo(commandManifest).ok, "add routing connection command undoes");
    require(commandManifest.routing.empty(), "add routing connection undo removes route");
    commandManifest.tracks.pop_back();
    auto removeClip = lamusica::commands::makeRemoveClipCommand("cmd-3d", "audit-3d", "clip-1");
    require(history.execute(commandManifest, std::move(removeClip)).ok,
            "remove clip command applies");
    require(commandManifest.clips.empty(), "remove clip command mutates manifest");
    require(history.undo(commandManifest).ok, "remove clip command undoes");
    require(commandManifest.clips.size() == 1, "remove clip undo restores clip");
    require(history.undo(commandManifest).ok, "add clip command undoes");
    require(commandManifest.clips.empty(), "add clip undo removes clip");

    auto setProjectName = lamusica::commands::makeSetProjectNameCommand(
        "cmd-project-name", "audit-project-name", "Command Project");
    require(setProjectName->preview(commandManifest) == "Rename project to \"Command Project\"",
            "set project name command previews");
    require(history.execute(commandManifest, std::move(setProjectName)).ok,
            "set project name command applies");
    require(commandManifest.name == "Command Project", "set project name command mutates manifest");
    require(history.undo(commandManifest).ok, "set project name command undoes");
    require(commandManifest.name == "Untitled", "set project name undo restores manifest name");
    auto addMarker = lamusica::commands::makeAddMarkerCommand(
        "cmd-marker-add", "audit-marker-add",
        {.id = "marker-command", .name = "Verse", .samplePosition = 96000});
    require(history.execute(commandManifest, std::move(addMarker)).ok,
            "add marker command applies");
    require(commandManifest.markers.size() == 1 &&
                commandManifest.markers.front().samplePosition == 96000,
            "add marker command mutates manifest");
    auto removeMarker = lamusica::commands::makeRemoveMarkerCommand(
        "cmd-marker-remove", "audit-marker-remove", "marker-command");
    require(history.execute(commandManifest, std::move(removeMarker)).ok,
            "remove marker command applies");
    require(commandManifest.markers.empty(), "remove marker command mutates manifest");
    require(history.undo(commandManifest).ok, "remove marker command undoes");
    require(commandManifest.markers.size() == 1, "remove marker undo restores marker");
    require(history.undo(commandManifest).ok, "add marker command undoes");
    require(commandManifest.markers.empty(), "add marker undo removes marker");
    auto addTempo = lamusica::commands::makeAddTempoEventCommand(
        "cmd-tempo-add", "audit-tempo-add", {.samplePosition = 48000, .bpm = 90.0});
    require(history.execute(commandManifest, std::move(addTempo)).ok,
            "add tempo event command applies");
    require(commandManifest.tempoMap.size() == 2 &&
                commandManifest.tempoMap.front().samplePosition == 0 &&
                commandManifest.tempoMap.back().samplePosition == 48000,
            "add tempo event command keeps tempo map sorted");
    require(history.undo(commandManifest).ok, "add tempo event command undoes");
    require(commandManifest.tempoMap.size() == 1, "add tempo event undo restores tempo map");

    const auto registeredCommands = lamusica::commands::registeredProjectCommandNames();
    require(
        std::ranges::contains(registeredCommands, std::string{"add_track"}) &&
            std::ranges::contains(registeredCommands, std::string{"add_clip"}) &&
            std::ranges::contains(registeredCommands, std::string{"set_clip_render_properties"}) &&
            std::ranges::contains(registeredCommands, std::string{"slip_clip"}) &&
            std::ranges::contains(registeredCommands, std::string{"add_routing_connection"}) &&
            std::ranges::contains(registeredCommands, std::string{"remove_routing_connection"}) &&
            std::ranges::contains(registeredCommands, std::string{"set_project_name"}) &&
            std::ranges::contains(registeredCommands, std::string{"add_marker"}) &&
            std::ranges::contains(registeredCommands, std::string{"remove_marker"}) &&
            std::ranges::contains(registeredCommands, std::string{"add_tempo_event"}),
        "command registry lists project command names");
    auto journalTrack =
        lamusica::commands::makeAddTrackCommand("journal-track", "journal-audit-track",
                                                {.id = "journal-track",
                                                 .name = "Journal Track",
                                                 .type = lamusica::session::TrackType::Audio});
    auto journalBus = lamusica::commands::makeAddTrackCommand(
        "journal-bus", "journal-audit-bus",
        {.id = "journal-bus", .name = "Journal Bus", .type = lamusica::session::TrackType::Group});
    auto journalClip =
        lamusica::commands::makeAddClipCommand("journal-clip", "journal-audit-clip",
                                               {.id = "journal-clip",
                                                .trackId = "journal-track",
                                                .type = lamusica::session::ClipType::Midi,
                                                .startSample = 128,
                                                .lengthSamples = 512,
                                                .sourceOffsetSamples = 32,
                                                .fadeInSamples = 8,
                                                .fadeOutSamples = 16,
                                                .gainDb = -3.0F,
                                                .muted = true,
                                                .reversed = true,
                                                .assetId = "journal-asset"});
    auto journalRename = lamusica::commands::makeSetTrackNameCommand(
        "journal-rename", "journal-audit-rename", "journal-track", "Journal Renamed");
    auto journalSlip = lamusica::commands::makeSlipClipCommand("journal-slip", "journal-audit-slip",
                                                               "journal-clip", 96);
    auto journalAddRoute = lamusica::commands::makeAddRoutingConnectionCommand(
        "journal-route", "journal-audit-route",
        {.sourceTrackId = "journal-track", .destinationTrackId = "journal-bus"});
    auto journalRemoveRoute = lamusica::commands::makeRemoveRoutingConnectionCommand(
        "journal-remove-route", "journal-audit-remove-route",
        {.sourceTrackId = "journal-track", .destinationTrackId = "journal-bus"});
    auto journalProjectName = lamusica::commands::makeSetProjectNameCommand(
        "journal-project-name", "journal-audit-project-name", "Journal Project");
    auto journalMarker = lamusica::commands::makeAddMarkerCommand(
        "journal-marker", "journal-audit-marker",
        {.id = "journal-marker", .name = "Drop", .samplePosition = 12000});
    auto journalTempo = lamusica::commands::makeAddTempoEventCommand(
        "journal-tempo", "journal-audit-tempo", {.samplePosition = 24000, .bpm = 132.0});
    std::vector<std::string> serializedJournal;
    serializedJournal.push_back(journalTrack->serialize());
    serializedJournal.push_back(journalBus->serialize());
    serializedJournal.push_back(journalClip->serialize());
    serializedJournal.push_back(journalRename->serialize());
    serializedJournal.push_back(journalSlip->serialize());
    serializedJournal.push_back(journalAddRoute->serialize());
    serializedJournal.push_back(journalRemoveRoute->serialize());
    serializedJournal.push_back(journalProjectName->serialize());
    serializedJournal.push_back(journalMarker->serialize());
    serializedJournal.push_back(journalTempo->serialize());
    std::vector<lamusica::commands::CommandPtr> journalTransactionCommands;
    journalTransactionCommands.push_back(lamusica::commands::makeAddTrackCommand(
        "journal-transaction-track", "journal-transaction-audit-track",
        {.id = "journal-transaction-track",
         .name = "Journal Transaction",
         .type = lamusica::session::TrackType::Audio}));
    journalTransactionCommands.push_back(lamusica::commands::makeAddClipCommand(
        "journal-transaction-clip", "journal-transaction-audit-clip",
        {.id = "journal-transaction-clip",
         .trackId = "journal-transaction-track",
         .type = lamusica::session::ClipType::Audio,
         .startSample = 0,
         .lengthSamples = 24000}));
    auto journalTransaction = std::make_unique<lamusica::commands::TransactionCommand>(
        "journal-transaction", "journal-transaction-audit", "transaction",
        std::move(journalTransactionCommands));
    serializedJournal.push_back(journalTransaction->serialize());
    lamusica::session::ProjectManifest replayedManifest;
    const auto replayReport =
        lamusica::commands::replaySerializedCommands(replayedManifest, serializedJournal);
    require(replayReport.appliedCount == serializedJournal.size() &&
                std::ranges::all_of(
                    replayReport.results,
                    [](const lamusica::commands::CommandResult& result) { return result.ok; }),
            "serialized command journal replays successfully");
    require(replayedManifest.tracks.size() == 3 && replayedManifest.name == "Journal Project" &&
                replayedManifest.tracks.front().name == "Journal Renamed" &&
                replayedManifest.clips.size() == 2 &&
                replayedManifest.clips.front().trackId == "journal-track" &&
                replayedManifest.clips.front().type == lamusica::session::ClipType::Midi &&
                replayedManifest.clips.front().sourceOffsetSamples == 96 &&
                replayedManifest.clips.front().fadeOutSamples == 16 &&
                replayedManifest.clips.front().gainDb == -3.0F &&
                replayedManifest.clips.front().muted && replayedManifest.clips.front().reversed &&
                replayedManifest.clips.front().assetId == "journal-asset" &&
                replayedManifest.clips.back().id == "journal-transaction-clip" &&
                replayedManifest.markers.size() == 1 &&
                replayedManifest.markers.front().id == "journal-marker" &&
                replayedManifest.tempoMap.size() == 2 &&
                replayedManifest.tempoMap.back().bpm == 132.0 && replayedManifest.routing.empty(),
            "serialized command replay deterministically rebuilds manifest state");
    auto replayedTransactionCommand =
        lamusica::commands::commandFromSerialized(journalTransaction->serialize());
    lamusica::session::ProjectManifest replayedTransactionManifest;
    require(replayedTransactionCommand->validate(replayedTransactionManifest).ok,
            "serialized transaction command validates dependent child commands");
    lamusica::session::ProjectManifest failedReplayManifest;
    const auto failedReplay = lamusica::commands::replaySerializedCommands(
        failedReplayManifest, {serializedJournal[2], serializedJournal[0]});
    require(failedReplay.appliedCount == 0 && failedReplayManifest.tracks.empty() &&
                failedReplayManifest.clips.empty(),
            "serialized command replay stops on failed validation without later mutation");

    require(lamusica::mcp_bridge::toString(lamusica::mcp_bridge::Capability::Edit) == "edit",
            "MCP capability string");
    lamusica::mcp_bridge::DaemonSession daemonSession;
    require(daemonSession.health().message == "idle", "MCP daemon starts idle");
    daemonSession.install("com.lamusica.test.mcpd");
    require(daemonSession.lifecycleStatus() ==
                lamusica::mcp_bridge::DaemonLifecycleStatus::Installed,
            "MCP daemon lifecycle models install");
    daemonSession.launch();
    require(daemonSession.lifecycleStatus() == lamusica::mcp_bridge::DaemonLifecycleStatus::Running,
            "MCP daemon lifecycle models launch");
    require(daemonSession.daemonLog().size() == 2 &&
                daemonSession.daemonLog().front().event == "install",
            "MCP daemon lifecycle records logs");
    require(!daemonSession.canMutateProject(), "MCP daemon cannot mutate without project");
    const auto token = daemonSession.attachProject(
        "fixtures/empty.Project.lamusica",
        {lamusica::mcp_bridge::Capability::ReadOnly, lamusica::mcp_bridge::Capability::Edit});
    require(!token.empty(), "MCP daemon attach returns token");
    require(daemonSession.validateAuthToken(token) && !daemonSession.validateAuthToken("wrong"),
            "MCP daemon validates active local auth token");
    require(daemonSession.canMutateProject(), "MCP daemon edit capability allows mutation");
    daemonSession.recordRead("project_summary");
    daemonSession.markConnectionLost();
    require(daemonSession.connectionInterrupted() && !daemonSession.attached() &&
                !daemonSession.canMutateProject() && daemonSession.canRecoverConnection() &&
                daemonSession.validateRecoveryAuthToken(token),
            "MCP daemon lost connection clears active capabilities but preserves recovery state");
    require(!daemonSession.recoverConnection("wrong"), "MCP daemon rejects invalid recovery token");
    require(daemonSession.recoverConnection(token) && daemonSession.canMutateProject() &&
                daemonSession.readAuditLog().size() == 1,
            "MCP daemon recovers lost connection without corrupting session audit state");
    daemonSession.stop();
    require(daemonSession.lifecycleStatus() ==
                    lamusica::mcp_bridge::DaemonLifecycleStatus::Stopped &&
                !daemonSession.attached(),
            "MCP daemon lifecycle stop detaches active project");
    daemonSession.detachProject();
    require(!daemonSession.attached(), "MCP daemon detaches project");
    require(!daemonSession.canRecoverConnection(),
            "MCP daemon explicit detach clears recovery state");
    require(lamusica::mcp_bridge::serializeProtocolResponse(
                lamusica::mcp_bridge::handleProtocolLine(daemonSession, "health"))
                    .find("ok health=ok state=stopped lifecycle=stopped") == 0,
            "MCP protocol reports idle health");
    require(lamusica::mcp_bridge::handleProtocolLine(daemonSession, "launch").ok,
            "MCP protocol launches daemon lifecycle");
    require(lamusica::mcp_bridge::handleProtocolLine(daemonSession, "logs").body.find("launch") !=
                std::string::npos,
            "MCP protocol exposes daemon lifecycle logs");
    require(!lamusica::mcp_bridge::handleProtocolLine(
                 daemonSession, "attach fixtures/empty.Project.lamusica read_everything")
                 .ok,
            "MCP protocol rejects unknown attach capabilities");
    require(lamusica::mcp_bridge::handleProtocolLine(
                daemonSession, "attach fixtures/empty.Project.lamusica read_only")
                .ok,
            "MCP protocol attaches project");
    const auto protocolToken = daemonSession.authToken();
    require(!daemonSession.canMutateProject(),
            "MCP protocol read-only attachment cannot mutate project");
    require(!lamusica::mcp_bridge::handleProtocolLine(daemonSession, "can_mutate").ok,
            "MCP protocol requires auth for project-scoped status");
    require(!lamusica::mcp_bridge::handleProtocolLine(daemonSession, "auth wrong can_mutate").ok,
            "MCP protocol rejects invalid project auth token");
    require(lamusica::mcp_bridge::handleProtocolLine(daemonSession,
                                                     "auth " + protocolToken + " connection_lost")
                    .ok &&
                daemonSession.connectionInterrupted(),
            "MCP protocol records interrupted app connection");
    require(!lamusica::mcp_bridge::handleProtocolLine(daemonSession, "recover").ok,
            "MCP protocol requires auth for interrupted session recovery");
    require(lamusica::mcp_bridge::handleProtocolLine(daemonSession,
                                                     "auth " + protocolToken + " recover")
                    .ok &&
                daemonSession.attached() && !daemonSession.canMutateProject(),
            "MCP protocol recovers interrupted read-only attachment");
    const auto recoveredProtocolToken = daemonSession.authToken();
    require(lamusica::mcp_bridge::handleProtocolLine(
                daemonSession, "auth " + recoveredProtocolToken + " can_mutate")
                    .body == "false",
            "MCP protocol reports mutate scope");
    require(lamusica::mcp_bridge::handleProtocolLine(daemonSession,
                                                     "auth " + recoveredProtocolToken + " detach")
                .ok,
            "MCP protocol detaches project");
    require(!daemonSession.attached(), "MCP protocol detach clears session");
    const auto shellDenied = lamusica::mcp_bridge::handleProtocolLine(daemonSession, "shell ls");
    require(!shellDenied.ok && shellDenied.body == "forbidden_shell_execution",
            "MCP protocol explicitly rejects shell execution");
    const auto filesystemDenied =
        lamusica::mcp_bridge::handleProtocolLine(daemonSession, "filesystem.list /");
    require(!filesystemDenied.ok && filesystemDenied.body == "forbidden_filesystem_browsing",
            "MCP protocol explicitly rejects unrestricted filesystem browsing");
    require(daemonSession.deniedProtocolLog().size() == 2 &&
                daemonSession.deniedProtocolLog().front().reason == "shell_execution",
            "MCP protocol logs denied restricted requests");
    require(lamusica::mcp_bridge::classifyForbiddenProtocolRequest("query", "read_file") ==
                lamusica::mcp_bridge::ForbiddenProtocolSurface::FilesystemBrowsing,
            "MCP protocol classifies forbidden query tool names");

    lamusica::session::ProjectManifest queryManifest;
    queryManifest.name = "Query Fixture";
    queryManifest.tracks.push_back(
        {.id = "query-track", .name = "Query Track", .type = lamusica::session::TrackType::Audio});
    queryManifest.tracks.push_back(
        {.id = "master", .name = "Master", .type = lamusica::session::TrackType::Master});
    queryManifest.clips.push_back({.id = "query-clip",
                                   .trackId = "query-track",
                                   .type = lamusica::session::ClipType::Audio,
                                   .startSample = 0,
                                   .lengthSamples = 48000});
    queryManifest.markers.push_back(
        {.id = "query-marker", .name = "Drop", .samplePosition = 96000});
    queryManifest.assets.push_back(
        {.id = "query-asset", .relativePath = "Audio/query.wav", .mediaType = "audio/wav"});
    queryManifest.routing.push_back(
        {.sourceTrackId = "query-track", .destinationTrackId = "master"});
    queryManifest.plugins.push_back({.id = "query-plugin",
                                     .trackId = "query-track",
                                     .format = "built_in",
                                     .identifier = "gain"});
    queryManifest.automation.push_back(
        {.id = "query-lane",
         .targetId = "query-track",
         .parameterId = "volume",
         .regions = {{.startSample = 0,
                      .endSample = 96000,
                      .points = {{.samplePosition = 12000, .value = 0.25F},
                                 {.samplePosition = 72000,
                                  .value = 0.75F,
                                  .curveToNext = lamusica::session::AutomationCurve::Step}}}}});
    lamusica::session::TimelineSelection querySelection;
    querySelection.trackIds.push_back("query-track");
    querySelection.clipIds.push_back("query-clip");
    querySelection.range = lamusica::session::TimelineRange{0, 48000};
    lamusica::audio::TransportState queryTransport;
    queryTransport.playing = true;
    queryTransport.samplePosition = 12000;
    lamusica::session::MixerState queryMixer;
    queryMixer.channels.push_back({.id = "query-audio",
                                   .name = "Query Audio",
                                   .type = lamusica::session::ChannelType::Audio,
                                   .sends = {{.id = "query-send",
                                              .destinationChannelId = "query-return",
                                              .gainDb = -6.0F,
                                              .preFader = true}}});
    queryMixer.channels.push_back({.id = "query-return",
                                   .name = "Query Return",
                                   .type = lamusica::session::ChannelType::Return});
    queryMixer.channels.push_back(
        {.id = "query-bus", .name = "Query Bus", .type = lamusica::session::ChannelType::Group});
    queryMixer.routing.push_back(
        {.sourceChannelId = "query-audio", .destinationChannelId = "query-bus"});
    queryMixer.sidechains.push_back({.id = "query-sidechain",
                                     .sourceChannelId = "query-audio",
                                     .destinationChannelId = "query-bus",
                                     .targetInsertId = "query-compressor"});
    lamusica::session::AssetCatalog queryCatalog;
    queryCatalog.projectRoot = "/project";
    queryCatalog.assets.push_back({.id = "catalog-asset",
                                   .relativePath = "Samples/kick.wav",
                                   .kind = lamusica::session::AssetKind::Audio,
                                   .tags = {"kick"},
                                   .favorite = true});
    queryCatalog.analyses.push_back({.assetId = "catalog-asset",
                                     .durationSamples = 24000,
                                     .channels = 2,
                                     .sampleRate = 48000.0,
                                     .peakAmplitude = 0.5F,
                                     .rmsAmplitude = 0.25F,
                                     .loudnessLufs = -12.0F,
                                     .tempoBpm = 120.0,
                                     .musicalKey = "A major",
                                     .transientSamples = {0, 12000}});
    queryCatalog.waveforms.push_back(
        {.assetId = "catalog-asset",
         .samplesPerBucket = 256,
         .buckets = {{.minSample = -0.5F, .maxSample = 0.5F, .rmsAmplitude = 0.25F}}});
    const auto beforeQuery = lamusica::session::serializeProjectManifest(queryManifest);
    const auto requireQueryContract = [](const std::string& json, std::string_view toolName) {
        require(json.find("\"schemaVersion\":1") != std::string::npos,
                "MCP query contract includes schema version");
        require(json.find("\"tool\":\"" + std::string{toolName} + "\"") != std::string::npos,
                "MCP query contract includes stable tool name");
    };
    requireQueryContract(lamusica::mcp_bridge::projectSummaryJson(queryManifest),
                         "project_summary");
    requireQueryContract(lamusica::mcp_bridge::tracksJson(queryManifest), "tracks");
    requireQueryContract(lamusica::mcp_bridge::clipsJson(queryManifest), "clips");
    requireQueryContract(lamusica::mcp_bridge::clipsInRangeJson(
                             queryManifest, {.startSample = 0, .endSample = 96000}),
                         "clips_range");
    requireQueryContract(lamusica::mcp_bridge::selectionJson(querySelection), "selection");
    requireQueryContract(lamusica::mcp_bridge::transportJson(queryTransport), "transport");
    requireQueryContract(lamusica::mcp_bridge::tempoJson(queryManifest), "tempo");
    requireQueryContract(lamusica::mcp_bridge::markersJson(queryManifest), "markers");
    requireQueryContract(lamusica::mcp_bridge::routingJson(queryManifest), "routing");
    requireQueryContract(lamusica::mcp_bridge::pluginsJson(queryManifest), "plugins");
    requireQueryContract(lamusica::mcp_bridge::automationJson(queryManifest), "automation");
    requireQueryContract(lamusica::mcp_bridge::automationInRangeJson(
                             queryManifest, {.startSample = 0, .endSample = 48000}),
                         "automation_range");
    requireQueryContract(lamusica::mcp_bridge::assetsJson(queryManifest), "assets");
    requireQueryContract(lamusica::mcp_bridge::assetCatalogJson(queryCatalog), "assets");
    requireQueryContract(lamusica::mcp_bridge::renderCapabilitiesJson(), "render_capabilities");
    require(lamusica::mcp_bridge::tracksJson(
                queryManifest, {.offset = 0, .limit = lamusica::mcp_bridge::maxQueryPageLimit + 1})
                    .find("\"limit\":500") != std::string::npos,
            "MCP query contracts clamp oversized page limits");
    require(lamusica::mcp_bridge::projectSummaryJson(queryManifest)
                    .find("\"tool\":\"project_summary\"") != std::string::npos,
            "MCP project summary has stable tool name");
    require(lamusica::mcp_bridge::tracksJson(queryManifest, {.offset = 0, .limit = 1})
                    .find("\"schemaVersion\":1") != std::string::npos,
            "MCP tracks query has schema version");
    require(lamusica::mcp_bridge::clipsJson(queryManifest).find("query-clip") != std::string::npos,
            "MCP clips query includes clip");
    require(lamusica::mcp_bridge::tempoJson(queryManifest).find("\"tool\":\"tempo\"") !=
                std::string::npos,
            "MCP tempo query has tool name");
    require(lamusica::mcp_bridge::markersJson(queryManifest).find("query-marker") !=
                std::string::npos,
            "MCP markers query includes marker");
    require(lamusica::mcp_bridge::selectionJson(querySelection).find("\"tool\":\"selection\"") !=
                std::string::npos,
            "MCP selection query has tool name");
    require(lamusica::mcp_bridge::transportJson(queryTransport).find("\"playing\":true") !=
                std::string::npos,
            "MCP transport query includes play state");
    require(lamusica::mcp_bridge::routingJson(queryManifest).find("destinationTrackId") !=
                std::string::npos,
            "MCP manifest routing query has route endpoints");
    const auto mixerRoutingJson = lamusica::mcp_bridge::routingJson(queryMixer);
    require(mixerRoutingJson.find("\"sends\"") != std::string::npos &&
                mixerRoutingJson.find("\"id\":\"query-send\"") != std::string::npos &&
                mixerRoutingJson.find("\"destination\":\"query-return\"") != std::string::npos &&
                mixerRoutingJson.find("\"preFader\":true") != std::string::npos,
            "MCP mixer routing query includes sends");
    require(mixerRoutingJson.find("\"sidechains\"") != std::string::npos &&
                mixerRoutingJson.find("\"id\":\"query-sidechain\"") != std::string::npos &&
                mixerRoutingJson.find("\"targetInsertId\":\"query-compressor\"") !=
                    std::string::npos,
            "MCP mixer routing query includes sidechain routes");
    require(lamusica::mcp_bridge::pluginsJson(queryManifest).find("query-plugin") !=
                std::string::npos,
            "MCP project plugins query includes plugin references");
    require(lamusica::mcp_bridge::automationJson(queryManifest).find("query-lane") !=
                std::string::npos,
            "MCP automation query includes lane references");
    const auto automationRange = lamusica::mcp_bridge::automationInRangeJson(
        queryManifest, {.startSample = 0, .endSample = 48000});
    require(automationRange.find("\"tool\":\"automation_range\"") != std::string::npos &&
                automationRange.find("\"samplePosition\":12000") != std::string::npos &&
                automationRange.find("\"samplePosition\":72000") == std::string::npos,
            "MCP automation range query includes bounded point data");
    require(lamusica::mcp_bridge::assetsJson(queryManifest).find("query-asset") !=
                std::string::npos,
            "MCP manifest assets query includes assets");
    require(lamusica::mcp_bridge::assetCatalogJson(queryCatalog).find("\"favorite\":true") !=
                std::string::npos,
            "MCP asset catalog query includes catalog metadata");
    require(lamusica::mcp_bridge::assetCatalogJson(queryCatalog).find("\"waveform\"") !=
                std::string::npos,
            "MCP asset catalog query includes waveform analysis metadata");
    require(
        lamusica::mcp_bridge::assetCatalogJson(queryCatalog).find("\"musicalKey\":\"A major\"") !=
            std::string::npos,
        "MCP asset catalog query includes tempo and key analysis metadata");
    require(lamusica::mcp_bridge::renderCapabilitiesJson().find("\"wavPcm16\":true") !=
                std::string::npos,
            "MCP render capabilities describe WAV support");
    lamusica::mcp_bridge::DaemonSession querySession;
    const lamusica::mcp_bridge::ProtocolProjectState queryState{.manifest = &queryManifest,
                                                                .selection = &querySelection,
                                                                .transport = &queryTransport,
                                                                .mixer = nullptr,
                                                                .pluginScanCache = nullptr,
                                                                .assetCatalog = &queryCatalog};
    require(
        !lamusica::mcp_bridge::handleProtocolLine(querySession, queryState, "query project_summary")
             .ok,
        "MCP protocol query requires attached project");
    const auto editOnlyQueryToken = querySession.attachProject(
        "fixtures/empty.Project.lamusica", {lamusica::mcp_bridge::Capability::Edit});
    require(!lamusica::mcp_bridge::handleProtocolLine(
                 querySession, queryState, "auth " + editOnlyQueryToken + " query tracks")
                 .ok,
            "MCP protocol query requires read-only capability");
    require(!lamusica::mcp_bridge::handleProtocolLine(
                 querySession, queryState, "attach fixtures/empty.Project.lamusica read_only")
                 .ok,
            "MCP protocol requires auth before replacing an attached project scope");
    require(lamusica::mcp_bridge::handleProtocolLine(
                querySession, queryState,
                "auth " + editOnlyQueryToken + " attach fixtures/empty.Project.lamusica read_only")
                .ok,
            "MCP protocol allows authenticated project scope replacement");
    const auto queryToken = querySession.authToken();
    require(!lamusica::mcp_bridge::handleProtocolLine(querySession, queryState, "query tracks").ok,
            "MCP protocol rejects unauthenticated project query");
    require(lamusica::mcp_bridge::handleProtocolLine(querySession, queryState,
                                                     "auth " + queryToken + " query tracks 0 1")
                    .body.find("\"total\":2") != std::string::npos,
            "MCP protocol exposes paged track query");
    require(lamusica::mcp_bridge::handleProtocolLine(querySession, queryState,
                                                     "auth " + queryToken + " query transport")
                    .body.find("\"samplePosition\":12000") != std::string::npos,
            "MCP protocol exposes transport query");
    require(lamusica::mcp_bridge::handleProtocolLine(querySession, queryState,
                                                     "auth " + queryToken +
                                                         " query automation_range 0 48000 0 1")
                    .body.find("\"tool\":\"automation_range\"") != std::string::npos,
            "MCP protocol exposes ranged automation query");
    require(lamusica::mcp_bridge::handleProtocolLine(
                querySession, queryState, "auth " + queryToken + " query clips_range 0 96000 0 1")
                    .body.find("\"tool\":\"clips_range\"") != std::string::npos,
            "MCP protocol exposes ranged clip query");
    require(!lamusica::mcp_bridge::handleProtocolLine(
                 querySession, queryState, "auth " + queryToken + " query automation_range 48000 0")
                 .ok,
            "MCP protocol rejects invalid automation query ranges");
    require(!lamusica::mcp_bridge::handleProtocolLine(querySession, queryState,
                                                      "auth " + queryToken + " query tracks x")
                 .ok,
            "MCP protocol rejects invalid query page");
    require(!lamusica::mcp_bridge::handleProtocolLine(querySession, queryState,
                                                      "auth " + queryToken + " query read_file")
                 .ok,
            "MCP protocol rejects unrestricted filesystem query tools");
    require(!lamusica::mcp_bridge::handleProtocolLine(querySession, queryState,
                                                      "auth " + queryToken + " query shell")
                 .ok,
            "MCP protocol rejects shell-like query tools");
    require(querySession.readAuditLog().size() == 4 &&
                querySession.readAuditLog().front().toolName == "tracks",
            "MCP protocol records successful read queries outside the project manifest");
    const auto queryStress =
        lamusica::session::makeStressProject({.tracks = 16, .clipsPerTrack = 16, .markers = 8});
    require(lamusica::mcp_bridge::clipsJson(queryStress, {.offset = 8, .limit = 5})
                    .find("\"limit\":5,\"total\":256") != std::string::npos,
            "MCP large project clip query is paged");
    const auto rangedClipQuery = lamusica::mcp_bridge::clipsInRangeJson(
        queryStress, {.startSample = 0, .endSample = 96000}, {.offset = 0, .limit = 3});
    require(rangedClipQuery.find("\"tool\":\"clips_range\"") != std::string::npos &&
                rangedClipQuery.find("\"range\":{\"startSample\":0,\"endSample\":96000}") !=
                    std::string::npos &&
                rangedClipQuery.find("\"limit\":3") != std::string::npos &&
                rangedClipQuery.find("\"total\":32") != std::string::npos,
            "MCP large project clip query supports region filtering");
    const auto rangedAutomationQuery = lamusica::mcp_bridge::automationInRangeJson(
        queryStress, {.startSample = 0, .endSample = 48000}, {.offset = 0, .limit = 4});
    require(rangedAutomationQuery.find("\"limit\":4") != std::string::npos &&
                rangedAutomationQuery.find("\"points\"") != std::string::npos,
            "MCP large project automation query supports region filtering");
    require(beforeQuery == lamusica::session::serializeProjectManifest(queryManifest),
            "MCP read-only queries do not mutate manifest");

    lamusica::session::ProjectManifest editManifest;
    lamusica::commands::CommandHistory editHistory;
    lamusica::mcp_bridge::DaemonSession editSession;
    auto deniedEdit = lamusica::mcp_bridge::applyCommand(
        editSession, editManifest, editHistory,
        lamusica::commands::makeAddTrackCommand(
            "mcp-cmd-denied", "mcp-audit-denied",
            {.id = "denied-track", .name = "Denied", .type = lamusica::session::TrackType::Audio}));
    require(!deniedEdit.applied && editManifest.tracks.empty(),
            "MCP edit denied without capability does not mutate");
    editSession.attachProject(
        "fixtures/empty.Project.lamusica",
        {lamusica::mcp_bridge::Capability::ReadOnly, lamusica::mcp_bridge::Capability::Edit});
    auto previewTrack = lamusica::commands::AddTrackCommand{
        "mcp-cmd-1",
        "mcp-audit-1",
        {.id = "mcp-track", .name = "MCP Track", .type = lamusica::session::TrackType::Audio}};
    const auto validateResult =
        lamusica::mcp_bridge::validateCommand(editSession, editManifest, previewTrack);
    require(validateResult.validationOk && !validateResult.applied &&
                validateResult.commandId == "mcp-cmd-1" && validateResult.auditId == "mcp-audit-1",
            "MCP edit validate reports command metadata without mutation");
    const auto previewResult =
        lamusica::mcp_bridge::previewCommand(editSession, editManifest, previewTrack);
    require(previewResult.validationOk && !previewResult.applied && editManifest.tracks.empty(),
            "MCP edit preview validates without mutation");
    auto applyResult = lamusica::mcp_bridge::applyCommand(
        editSession, editManifest, editHistory,
        lamusica::commands::makeAddTrackCommand(
            "mcp-cmd-1", "mcp-audit-1",
            {.id = "mcp-track", .name = "MCP Track", .type = lamusica::session::TrackType::Audio}));
    require(applyResult.applied && applyResult.undoAvailable,
            "MCP edit apply mutates through history");
    require(editManifest.tracks.size() == 1, "MCP edit apply adds track");
    require(
        lamusica::mcp_bridge::editToolResultJson(applyResult).find("\"commandId\":\"mcp-cmd-1\"") !=
            std::string::npos,
        "MCP edit result serializes command id");
    const auto undoResult =
        lamusica::mcp_bridge::undoLastCommand(editSession, editManifest, editHistory);
    require(undoResult.applied && editManifest.tracks.empty(),
            "MCP edit undo uses command history");
    require(undoResult.redoAvailable, "MCP edit undo reports redo availability");
    const auto redoResult =
        lamusica::mcp_bridge::redoLastCommand(editSession, editManifest, editHistory);
    require(redoResult.applied && editManifest.tracks.size() == 1,
            "MCP edit redo uses command history");
    auto mcpRenameTrack = lamusica::mcp_bridge::applyCommand(
        editSession, editManifest, editHistory,
        lamusica::commands::makeSetTrackNameCommand("mcp-label", "mcp-label-audit", "mcp-track",
                                                    "MCP Label"));
    require(mcpRenameTrack.applied && editManifest.tracks.front().name == "MCP Label",
            "MCP label edit applies through command history");
    const auto undoMcpRename =
        lamusica::mcp_bridge::undoLastCommand(editSession, editManifest, editHistory);
    require(undoMcpRename.applied && editManifest.tracks.front().name == "MCP Track",
            "MCP label edit undo restores track name");
    editManifest.clips.push_back({.id = "mcp-cut-clip",
                                  .trackId = "mcp-track",
                                  .type = lamusica::session::ClipType::Audio,
                                  .startSample = 0,
                                  .lengthSamples = 48000});
    auto previewRemove = lamusica::commands::RemoveClipCommand{
        "mcp-cut-preview", "mcp-cut-audit-preview", "mcp-cut-clip"};
    const auto removePreview =
        lamusica::mcp_bridge::previewCommand(editSession, editManifest, previewRemove);
    require(removePreview.confirmationRequired && !removePreview.confirmationToken.empty(),
            "MCP destructive edit preview returns confirmation token");
    auto deniedRemove = lamusica::mcp_bridge::applyCommand(
        editSession, editManifest, editHistory,
        lamusica::commands::makeRemoveClipCommand("mcp-cut", "mcp-cut-audit", "mcp-cut-clip"));
    require(!deniedRemove.applied && editManifest.clips.size() == 1,
            "MCP destructive edit requires confirmation before mutation");
    auto confirmedRemoveCommand =
        lamusica::commands::makeRemoveClipCommand("mcp-cut", "mcp-cut-audit", "mcp-cut-clip");
    const auto confirmToken = lamusica::mcp_bridge::confirmationTokenFor(*confirmedRemoveCommand);
    auto confirmedRemove = lamusica::mcp_bridge::applyCommand(
        editSession, editManifest, editHistory, std::move(confirmedRemoveCommand),
        {.confirmationToken = confirmToken});
    require(confirmedRemove.applied && editManifest.clips.empty(),
            "MCP destructive edit applies with confirmation token");
    editManifest.tracks.push_back(
        {.id = "mcp-bus", .name = "MCP Bus", .type = lamusica::session::TrackType::Group});
    auto mcpAddRoute = lamusica::mcp_bridge::applyCommand(
        editSession, editManifest, editHistory,
        lamusica::commands::makeAddRoutingConnectionCommand(
            "mcp-route-add", "mcp-route-audit-add",
            {.sourceTrackId = "mcp-track", .destinationTrackId = "mcp-bus"}));
    require(mcpAddRoute.applied && editManifest.routing.size() == 1,
            "MCP routing edit applies through command history");
    auto deniedMcpRemoveRoute = lamusica::mcp_bridge::applyCommand(
        editSession, editManifest, editHistory,
        lamusica::commands::makeRemoveRoutingConnectionCommand(
            "mcp-route-remove", "mcp-route-audit-remove",
            {.sourceTrackId = "mcp-track", .destinationTrackId = "mcp-bus"}));
    require(!deniedMcpRemoveRoute.applied && deniedMcpRemoveRoute.confirmationRequired &&
                editManifest.routing.size() == 1,
            "MCP routing removal requires confirmation before mutation");
    auto confirmedMcpRemoveRouteCommand = lamusica::commands::makeRemoveRoutingConnectionCommand(
        "mcp-route-remove", "mcp-route-audit-remove",
        {.sourceTrackId = "mcp-track", .destinationTrackId = "mcp-bus"});
    const auto routeConfirmToken =
        lamusica::mcp_bridge::confirmationTokenFor(*confirmedMcpRemoveRouteCommand);
    auto mcpRemoveRoute = lamusica::mcp_bridge::applyCommand(
        editSession, editManifest, editHistory, std::move(confirmedMcpRemoveRouteCommand),
        {.confirmationToken = routeConfirmToken});
    require(mcpRemoveRoute.applied && editManifest.routing.empty(),
            "MCP routing removal applies through command history");
    const auto undoMcpRemoveRoute =
        lamusica::mcp_bridge::undoLastCommand(editSession, editManifest, editHistory);
    require(undoMcpRemoveRoute.applied && editManifest.routing.size() == 1,
            "MCP routing removal undo restores route");
    editManifest.clips.push_back({.id = "mcp-timeline",
                                  .trackId = "mcp-track",
                                  .type = lamusica::session::ClipType::Audio,
                                  .startSample = 1000,
                                  .lengthSamples = 4000});
    auto mcpTrimPreview = lamusica::commands::TrimClipCommand{
        "mcp-trim-preview", "mcp-trim-audit-preview", "mcp-timeline", 1200, 3000, 200};
    const auto mcpTimelineValidate =
        lamusica::mcp_bridge::validateCommand(editSession, editManifest, mcpTrimPreview);
    require(mcpTimelineValidate.validationOk && !mcpTimelineValidate.applied,
            "MCP timeline trim validates without mutation");
    const auto mcpTrim = lamusica::mcp_bridge::applyCommand(
        editSession, editManifest, editHistory,
        lamusica::commands::makeTrimClipCommand("mcp-trim", "mcp-trim-audit", "mcp-timeline", 1200,
                                                3000, 200));
    require(mcpTrim.applied && mcpTrim.commandId == "mcp-trim" &&
                editManifest.clips.back().startSample == 1200 &&
                editManifest.clips.back().lengthSamples == 3000 &&
                editManifest.clips.back().sourceOffsetSamples == 200,
            "MCP timeline trim applies through command history");
    const auto beforeInvalidTimelineEdit =
        lamusica::session::serializeProjectManifest(editManifest);
    const auto invalidMcpMove = lamusica::mcp_bridge::applyCommand(
        editSession, editManifest, editHistory,
        lamusica::commands::makeMoveClipCommand("mcp-invalid-move", "mcp-invalid-move-audit",
                                                "missing-clip", 2000));
    require(!invalidMcpMove.applied && !invalidMcpMove.validationOk &&
                beforeInvalidTimelineEdit ==
                    lamusica::session::serializeProjectManifest(editManifest),
            "MCP invalid timeline edit leaves state unchanged");
    const auto mcpMove =
        lamusica::mcp_bridge::applyCommand(editSession, editManifest, editHistory,
                                           lamusica::commands::makeMoveClipCommand(
                                               "mcp-move", "mcp-move-audit", "mcp-timeline", 2400));
    require(mcpMove.applied && editManifest.clips.back().startSample == 2400,
            "MCP timeline move applies through command history");
    const auto mcpDuplicate = lamusica::mcp_bridge::applyCommand(
        editSession, editManifest, editHistory,
        lamusica::commands::makeDuplicateClipCommand("mcp-duplicate", "mcp-duplicate-audit",
                                                     "mcp-timeline", "mcp-timeline-copy", 8000));
    require(mcpDuplicate.applied && editManifest.clips.back().id == "mcp-timeline-copy",
            "MCP timeline duplicate applies through command history");
    auto mcpSplitCommand = lamusica::commands::makeSplitClipCommand(
        editManifest, "mcp-split", "mcp-split-audit", "mcp-timeline", "mcp-timeline-right", 3400);
    const auto mcpSplit = lamusica::mcp_bridge::applyCommand(editSession, editManifest, editHistory,
                                                             std::move(mcpSplitCommand));
    const auto mcpTimelineLeft =
        std::ranges::find_if(editManifest.clips, [](const lamusica::session::Clip& clip) {
            return clip.id == "mcp-timeline";
        });
    require(mcpSplit.applied && mcpTimelineLeft != editManifest.clips.end() &&
                mcpTimelineLeft->lengthSamples == 1000 &&
                std::ranges::any_of(editManifest.clips,
                                    [](const lamusica::session::Clip& clip) {
                                        return clip.id == "mcp-timeline-right";
                                    }),
            "MCP timeline split applies as a transaction through command history");
    const auto undoMcpSplit =
        lamusica::mcp_bridge::undoLastCommand(editSession, editManifest, editHistory);
    require(undoMcpSplit.applied && !std::ranges::any_of(editManifest.clips,
                                                         [](const lamusica::session::Clip& clip) {
                                                             return clip.id == "mcp-timeline-right";
                                                         }),
            "MCP timeline split undo restores transaction state");
    lamusica::commands::MidiClipStore mcpMidiStore;
    lamusica::mcp_bridge::MidiEditHistory mcpMidiHistory;
    auto mcpMidiPreview = lamusica::mcp_bridge::previewMidiCommand(
        editSession, mcpMidiStore,
        lamusica::mcp_bridge::MidiEditCommand{lamusica::commands::AddMidiNoteCommand{
            "mcp-midi-1",
            "mcp-midi-audit-1",
            "mcp-midi-clip",
            {.id = "mcp-note", .startSample = 12001, .lengthSamples = 2400, .pitch = 60}}});
    require(mcpMidiPreview.validationOk && !mcpMidiPreview.applied,
            "MCP MIDI edit preview validates without mutation");
    auto mcpMidiApply = lamusica::mcp_bridge::applyMidiCommand(
        editSession, mcpMidiStore, mcpMidiHistory,
        lamusica::mcp_bridge::MidiEditCommand{lamusica::commands::AddMidiNoteCommand{
            "mcp-midi-1",
            "mcp-midi-audit-1",
            "mcp-midi-clip",
            {.id = "mcp-note", .startSample = 12001, .lengthSamples = 2400, .pitch = 60}}});
    require(mcpMidiApply.applied && mcpMidiStore.find("mcp-midi-clip")->notes.size() == 1,
            "MCP MIDI edit applies through history");
    auto mcpMidiTranspose = lamusica::mcp_bridge::applyMidiCommand(
        editSession, mcpMidiStore, mcpMidiHistory,
        lamusica::mcp_bridge::MidiEditCommand{lamusica::commands::TransposeMidiClipCommand{
            "mcp-midi-2", "mcp-midi-audit-2", "mcp-midi-clip", 12}});
    require(mcpMidiTranspose.applied &&
                mcpMidiStore.find("mcp-midi-clip")->notes.front().pitch == 72,
            "MCP MIDI transpose edit applies through history");
    const auto mcpMidiUndo =
        lamusica::mcp_bridge::undoLastMidiCommand(editSession, mcpMidiStore, mcpMidiHistory);
    require(mcpMidiUndo.applied && mcpMidiStore.find("mcp-midi-clip")->notes.front().pitch == 60,
            "MCP MIDI undo uses store-backed history");
    const auto mcpMidiRedo =
        lamusica::mcp_bridge::redoLastMidiCommand(editSession, mcpMidiStore, mcpMidiHistory);
    require(mcpMidiRedo.applied && mcpMidiStore.find("mcp-midi-clip")->notes.front().pitch == 72,
            "MCP MIDI redo uses store-backed history");
    lamusica::commands::AutomationLaneStore mcpAutomationStore;
    lamusica::mcp_bridge::AutomationEditHistory mcpAutomationHistory;
    lamusica::session::AutomationLaneData mcpLane{.id = "mcp-auto-lane",
                                                  .targetId = "mcp-track",
                                                  .parameterId = "volumeDb",
                                                  .mode = lamusica::session::AutomationMode::Write,
                                                  .defaultValue = 0.0F};
    auto mcpAutomationPreview = lamusica::mcp_bridge::previewAutomationCommand(
        editSession, mcpAutomationStore,
        lamusica::mcp_bridge::AutomationEditCommand{lamusica::commands::AddAutomationPointCommand{
            "mcp-auto-1", "mcp-auto-audit-1", mcpLane, 256, -3.0F,
            lamusica::session::AutomationCurve::Linear}});
    require(mcpAutomationPreview.validationOk && !mcpAutomationPreview.applied,
            "MCP automation edit preview validates without mutation");
    auto mcpAutomationApply = lamusica::mcp_bridge::applyAutomationCommand(
        editSession, mcpAutomationStore, mcpAutomationHistory,
        lamusica::mcp_bridge::AutomationEditCommand{lamusica::commands::AddAutomationPointCommand{
            "mcp-auto-1", "mcp-auto-audit-1", mcpLane, 256, -3.0F,
            lamusica::session::AutomationCurve::Linear}});
    require(mcpAutomationApply.applied &&
                lamusica::session::evaluateAutomation(*mcpAutomationStore.find("mcp-auto-lane"),
                                                      256) == -3.0F,
            "MCP automation edit applies through history");
    const auto mcpAutomationUndo = lamusica::mcp_bridge::undoLastAutomationCommand(
        editSession, mcpAutomationStore, mcpAutomationHistory);
    require(mcpAutomationUndo.applied && mcpAutomationHistory.canRedo(),
            "MCP automation undo reports redo availability");
    const auto mcpAutomationRedo = lamusica::mcp_bridge::redoLastAutomationCommand(
        editSession, mcpAutomationStore, mcpAutomationHistory);
    require(mcpAutomationRedo.applied &&
                lamusica::session::evaluateAutomation(*mcpAutomationStore.find("mcp-auto-lane"),
                                                      256) == -3.0F,
            "MCP automation redo uses store-backed history");
    lamusica::session::MixerState mcpMixer;
    lamusica::session::addChannel(
        mcpMixer,
        {.id = "mcp-track", .name = "MCP Track", .type = lamusica::session::ChannelType::Audio});
    lamusica::mcp_bridge::MixerEditHistory mcpMixerHistory;
    const auto mcpMixerPreview = lamusica::mcp_bridge::previewMixerCommand(
        editSession, mcpMixer,
        lamusica::mcp_bridge::MixerEditCommand{lamusica::commands::SetChannelMixCommand{
            "mcp-mix-1",
            "mcp-mix-audit-1",
            "mcp-track",
            {.volumeDb = -12.0F, .pan = -0.25F, .muted = true}}});
    require(mcpMixerPreview.validationOk && !mcpMixerPreview.applied,
            "MCP mixer edit preview validates without mutation");
    const auto mcpMixerApply = lamusica::mcp_bridge::applyMixerCommand(
        editSession, mcpMixer, mcpMixerHistory,
        lamusica::mcp_bridge::MixerEditCommand{lamusica::commands::SetChannelMixCommand{
            "mcp-mix-1",
            "mcp-mix-audit-1",
            "mcp-track",
            {.volumeDb = -12.0F, .pan = -0.25F, .muted = true}}});
    require(mcpMixerApply.applied &&
                lamusica::session::findChannel(mcpMixer, "mcp-track")->volumeDb == -12.0F &&
                lamusica::session::findChannel(mcpMixer, "mcp-track")->muted,
            "MCP mixer edit applies through history");
    const auto mcpMixerUndo =
        lamusica::mcp_bridge::undoLastMixerCommand(editSession, mcpMixer, mcpMixerHistory);
    require(mcpMixerUndo.applied &&
                lamusica::session::findChannel(mcpMixer, "mcp-track")->volumeDb == 0.0F &&
                !lamusica::session::findChannel(mcpMixer, "mcp-track")->muted,
            "MCP mixer undo uses store-backed history");
    const auto mcpMixerRedo =
        lamusica::mcp_bridge::redoLastMixerCommand(editSession, mcpMixer, mcpMixerHistory);
    require(mcpMixerRedo.applied &&
                lamusica::session::findChannel(mcpMixer, "mcp-track")->pan == -0.25F,
            "MCP mixer redo uses store-backed history");
    lamusica::commands::PluginInsertChainStore mcpPluginStore;
    lamusica::mcp_bridge::PluginEditHistory mcpPluginHistory;
    const auto mcpPluginPreview = lamusica::mcp_bridge::previewPluginCommand(
        editSession, mcpPluginStore,
        lamusica::mcp_bridge::PluginEditCommand{lamusica::commands::AddPluginInsertCommand{
            "mcp-plugin-1",
            "mcp-plugin-audit-1",
            "mcp-track",
            {.id = "mcp-insert", .pluginIdentifier = "builtin.eq"}}});
    require(mcpPluginPreview.validationOk && !mcpPluginPreview.applied,
            "MCP plugin edit preview validates without mutation");
    const auto mcpPluginApply = lamusica::mcp_bridge::applyPluginCommand(
        editSession, mcpPluginStore, mcpPluginHistory,
        lamusica::mcp_bridge::PluginEditCommand{lamusica::commands::AddPluginInsertCommand{
            "mcp-plugin-1",
            "mcp-plugin-audit-1",
            "mcp-track",
            {.id = "mcp-insert", .pluginIdentifier = "builtin.eq"}}});
    require(mcpPluginApply.applied &&
                mcpPluginStore.find("mcp-track")->inserts.front().id == "mcp-insert",
            "MCP plugin insert applies through history");
    const auto mcpPluginPreset = lamusica::mcp_bridge::applyPluginCommand(
        editSession, mcpPluginStore, mcpPluginHistory,
        lamusica::mcp_bridge::PluginEditCommand{lamusica::commands::ApplyPluginPresetCommand{
            "mcp-plugin-preset",
            "mcp-plugin-audit-preset",
            "mcp-track",
            "mcp-insert",
            {.id = "mcp-preset",
             .name = "MCP Preset",
             .pluginIdentifier = "builtin.eq",
             .parameterValues = {{.parameterId = "gain", .value = 0.7F}}}}});
    require(mcpPluginPreset.applied &&
                lamusica::session::findParameterValue(
                    *lamusica::session::findInsert(*mcpPluginStore.find("mcp-track"), "mcp-insert"),
                    "gain") == 0.7F,
            "MCP plugin preset applies through history");
    const auto mcpPluginUndo =
        lamusica::mcp_bridge::undoLastPluginCommand(editSession, mcpPluginStore, mcpPluginHistory);
    require(mcpPluginUndo.applied && !lamusica::session::findParameterValue(
                                          *lamusica::session::findInsert(
                                              *mcpPluginStore.find("mcp-track"), "mcp-insert"),
                                          "gain")
                                          .has_value(),
            "MCP plugin undo restores insert state");
    const auto mcpPluginRedo =
        lamusica::mcp_bridge::redoLastPluginCommand(editSession, mcpPluginStore, mcpPluginHistory);
    require(mcpPluginRedo.applied &&
                lamusica::session::findParameterValue(
                    *lamusica::session::findInsert(*mcpPluginStore.find("mcp-track"), "mcp-insert"),
                    "gain") == 0.7F,
            "MCP plugin redo reapplies insert state");
    const auto mcpPluginRemovePreview = lamusica::mcp_bridge::previewPluginCommand(
        editSession, mcpPluginStore,
        lamusica::mcp_bridge::PluginEditCommand{lamusica::commands::RemovePluginInsertCommand{
            "mcp-plugin-remove", "mcp-plugin-audit-remove", "mcp-track", "mcp-insert"}});
    require(mcpPluginRemovePreview.confirmationRequired &&
                !mcpPluginRemovePreview.confirmationToken.empty(),
            "MCP plugin remove preview requires confirmation");
    const auto deniedMcpPluginRemove = lamusica::mcp_bridge::applyPluginCommand(
        editSession, mcpPluginStore, mcpPluginHistory,
        lamusica::mcp_bridge::PluginEditCommand{lamusica::commands::RemovePluginInsertCommand{
            "mcp-plugin-remove", "mcp-plugin-audit-remove", "mcp-track", "mcp-insert"}});
    require(!deniedMcpPluginRemove.applied && !mcpPluginStore.find("mcp-track")->inserts.empty(),
            "MCP plugin remove requires confirmation before mutation");
    const auto mcpPluginRemove = lamusica::mcp_bridge::applyPluginCommand(
        editSession, mcpPluginStore, mcpPluginHistory,
        lamusica::mcp_bridge::PluginEditCommand{lamusica::commands::RemovePluginInsertCommand{
            "mcp-plugin-remove", "mcp-plugin-audit-remove", "mcp-track", "mcp-insert"}},
        {.confirmationToken = mcpPluginRemovePreview.confirmationToken});
    require(mcpPluginRemove.applied && mcpPluginStore.find("mcp-track")->inserts.empty(),
            "MCP plugin remove applies through history");

    lamusica::mcp_bridge::RenderJobQueue renderQueue;
    const auto deniedRender =
        renderQueue.enqueueTestTone(editSession, "render-denied", "/tmp/denied.wav");
    require(deniedRender.status == lamusica::mcp_bridge::RenderJobStatus::Failed,
            "MCP render requires render capability");
    require(!renderQueue.cancel("render-denied") &&
                renderQueue.find("render-denied")->status ==
                    lamusica::mcp_bridge::RenderJobStatus::Failed,
            "MCP render queue does not cancel failed jobs");
    editSession.attachProject(
        "fixtures/empty.Project.lamusica",
        {lamusica::mcp_bridge::Capability::ReadOnly, lamusica::mcp_bridge::Capability::Render});
    const auto mcpRenderPath = std::filesystem::temp_directory_path() / "lamusica-mcp-render.wav";
    std::filesystem::remove(mcpRenderPath);
    const auto renderJob = renderQueue.enqueueTestTone(editSession, "render-1", mcpRenderPath);
    require(renderJob.status == lamusica::mcp_bridge::RenderJobStatus::Completed,
            "MCP render job completes");
    const auto duplicateRenderJob =
        renderQueue.enqueueAnalyzeWav(editSession, "render-1", mcpRenderPath);
    require(duplicateRenderJob.status == lamusica::mcp_bridge::RenderJobStatus::Failed &&
                duplicateRenderJob.message == "Render job id already exists" &&
                renderQueue.find("render-1")->status ==
                    lamusica::mcp_bridge::RenderJobStatus::Completed,
            "MCP render queue rejects duplicate job ids without replacing existing jobs");
    require(std::filesystem::exists(mcpRenderPath), "MCP render job writes output file");
    require(lamusica::mcp_bridge::renderJobJson(renderJob).find("\"status\":\"completed\"") !=
                std::string::npos,
            "MCP render job serializes status");
    require(lamusica::mcp_bridge::renderJobJson(renderJob).find("\"type\":\"wav_analysis\"") !=
                std::string::npos,
            "MCP test tone render includes result manifest");
    require(renderJob.resultManifestJson.find("\"explicitExport\":true") != std::string::npos,
            "MCP generated test tone is marked as an explicit export");
    const auto analysisJob = renderQueue.enqueueAnalyzeWav(editSession, "analyze-1", mcpRenderPath);
    require(analysisJob.status == lamusica::mcp_bridge::RenderJobStatus::Completed &&
                analysisJob.resultManifestJson.find("\"peak\"") != std::string::npos,
            "MCP render queue analyzes WAV outputs");
    require(analysisJob.resultManifestJson.find("\"lufsEstimate\"") != std::string::npos &&
                analysisJob.resultManifestJson.find("\"waveform\"") != std::string::npos &&
                analysisJob.resultManifestJson.find("\"bucketCount\"") != std::string::npos,
            "MCP render queue returns loudness and waveform analysis");
    require(analysisJob.resultManifestJson.find("\"explicitExport\":false") != std::string::npos,
            "MCP analysis of existing WAV is not marked as a generated export");
    const auto mcpBouncePath = std::filesystem::temp_directory_path() / "lamusica-mcp-bounce.wav";
    std::filesystem::remove(mcpBouncePath);
    const lamusica::audio::AudioGraph mcpBounceGraph{
        .nodes = {{.id = "osc",
                   .kind = lamusica::audio::GraphNodeKind::Sine,
                   .frequencyHz = 220.0,
                   .gain = 0.25F,
                   .noteSequence = {}},
                  {.id = "master",
                   .kind = lamusica::audio::GraphNodeKind::Output,
                   .noteSequence = {}}},
        .connections = {{.sourceNodeId = "osc", .destinationNodeId = "master", .gain = 1.0F}},
        .outputNodeId = "master"};
    const auto bounceJob = renderQueue.enqueueGraphBounce(editSession, "bounce-1", mcpBounceGraph,
                                                          {.outputPath = mcpBouncePath,
                                                           .startSample = 0,
                                                           .frames = 128,
                                                           .sampleRate = 48000.0,
                                                           .channels = 2,
                                                           .normalizePeak = true,
                                                           .normalizeTargetPeak = 0.5F});
    require(bounceJob.status == lamusica::mcp_bridge::RenderJobStatus::Completed &&
                bounceJob.resultManifestJson.find("\"type\":\"graph_bounce\"") != std::string::npos,
            "MCP render queue bounces selected graph range");
    require(bounceJob.resultManifestJson.find("\"explicitExport\":true") != std::string::npos,
            "MCP graph bounce is marked as an explicit export");
    require(std::abs(lamusica::audio::peakAbsoluteSample(
                         lamusica::audio::readPcm16Wav(mcpBouncePath).audio) -
                     0.5F) < 0.001F,
            "MCP graph bounce applies normalization");
    const auto refusedOverwrite =
        renderQueue.enqueueGraphBounce(editSession, "bounce-overwrite-denied", mcpBounceGraph,
                                       {.outputPath = mcpBouncePath,
                                        .startSample = 0,
                                        .frames = 32,
                                        .sampleRate = 48000.0,
                                        .channels = 2});
    require(refusedOverwrite.status == lamusica::mcp_bridge::RenderJobStatus::Failed &&
                !refusedOverwrite.confirmationToken.empty(),
            "MCP render refuses overwrite without confirmation token");
    const auto confirmedOverwrite = renderQueue.enqueueGraphBounce(
        editSession, "bounce-overwrite-confirmed", mcpBounceGraph,
        {.outputPath = mcpBouncePath,
         .startSample = 0,
         .frames = 32,
         .sampleRate = 48000.0,
         .channels = 2},
        {.allowOverwrite = true,
         .confirmationToken = lamusica::mcp_bridge::renderConfirmationToken(mcpBouncePath)});
    require(confirmedOverwrite.status == lamusica::mcp_bridge::RenderJobStatus::Completed,
            "MCP render overwrites with confirmation token");
    const auto mcpNormalizePath =
        std::filesystem::temp_directory_path() / "lamusica-mcp-normalized.wav";
    const auto mcpReversePath = std::filesystem::temp_directory_path() / "lamusica-mcp-reverse.wav";
    std::filesystem::remove(mcpNormalizePath);
    std::filesystem::remove(mcpReversePath);
    const auto normalizeJob =
        renderQueue.enqueueTransformWav(editSession, "normalize-1", mcpBouncePath, mcpNormalizePath,
                                        lamusica::mcp_bridge::AudioFileTransform::Normalize);
    require(normalizeJob.status == lamusica::mcp_bridge::RenderJobStatus::Completed &&
                normalizeJob.resultManifestJson.find("\"operation\":\"normalize\"") !=
                    std::string::npos &&
                normalizeJob.resultManifestJson.find("\"explicitExport\":true") !=
                    std::string::npos,
            "MCP render queue normalizes WAV as explicit export");
    require(lamusica::audio::peakAbsoluteSample(
                lamusica::audio::readPcm16Wav(mcpNormalizePath).audio) > 0.9F,
            "MCP normalize transform raises output peak");
    const auto reverseJob =
        renderQueue.enqueueTransformWav(editSession, "reverse-1", mcpNormalizePath, mcpReversePath,
                                        lamusica::mcp_bridge::AudioFileTransform::Reverse);
    require(reverseJob.status == lamusica::mcp_bridge::RenderJobStatus::Completed &&
                reverseJob.resultManifestJson.find("\"operation\":\"reverse\"") !=
                    std::string::npos,
            "MCP render queue reverses WAV as explicit export");
    const auto normalizedAudio = lamusica::audio::readPcm16Wav(mcpNormalizePath).audio;
    const auto reversedAudio = lamusica::audio::readPcm16Wav(mcpReversePath).audio;
    require(std::abs(normalizedAudio.interleavedSamples.front() -
                     reversedAudio.interleavedSamples[reversedAudio.channels *
                                                      (reversedAudio.frames - 1U)]) < 0.001F,
            "MCP reverse transform flips frame order");
    const auto refusedSourceOverwrite = renderQueue.enqueueTransformWav(
        editSession, "reverse-overwrite-denied", mcpReversePath, mcpReversePath,
        lamusica::mcp_bridge::AudioFileTransform::Reverse);
    require(refusedSourceOverwrite.status == lamusica::mcp_bridge::RenderJobStatus::Failed &&
                !refusedSourceOverwrite.confirmationToken.empty(),
            "MCP transform refuses source overwrite without confirmation token");
    const auto confirmedSourceOverwrite = renderQueue.enqueueTransformWav(
        editSession, "reverse-overwrite-confirmed", mcpReversePath, mcpReversePath,
        lamusica::mcp_bridge::AudioFileTransform::Reverse,
        {.allowOverwrite = true,
         .confirmationToken = lamusica::mcp_bridge::renderConfirmationToken(mcpReversePath)});
    require(confirmedSourceOverwrite.status == lamusica::mcp_bridge::RenderJobStatus::Completed,
            "MCP transform overwrites source media with confirmation token");
    lamusica::session::ProjectManifest bounceInPlaceManifest;
    bounceInPlaceManifest.tracks.push_back({.id = "mcp-bip-track",
                                            .name = "Bounce Track",
                                            .type = lamusica::session::TrackType::Audio});
    const auto mcpBounceInPlacePath =
        std::filesystem::temp_directory_path() / "lamusica-mcp-bounce-in-place.wav";
    std::filesystem::remove(mcpBounceInPlacePath);
    const auto deniedBounceInPlace = renderQueue.enqueueBounceInPlace(
        editSession, "bip-denied", bounceInPlaceManifest, mcpBounceGraph,
        {.outputPath = mcpBounceInPlacePath,
         .startSample = 0,
         .frames = 64,
         .sampleRate = 48000.0,
         .channels = 2},
        {.assetId = "bip-asset",
         .assetRelativePath = "Audio/bip.wav",
         .clipId = "bip-clip",
         .trackId = "mcp-bip-track"});
    require(deniedBounceInPlace.status == lamusica::mcp_bridge::RenderJobStatus::Failed &&
                bounceInPlaceManifest.assets.empty() && bounceInPlaceManifest.clips.empty(),
            "MCP bounce-in-place requires asset creation capability");
    editSession.attachProject("fixtures/empty.Project.lamusica",
                              {lamusica::mcp_bridge::Capability::ReadOnly,
                               lamusica::mcp_bridge::Capability::Render,
                               lamusica::mcp_bridge::Capability::ImportExport});
    const auto bounceInPlaceJob = renderQueue.enqueueBounceInPlace(
        editSession, "bip-1", bounceInPlaceManifest, mcpBounceGraph,
        {.outputPath = mcpBounceInPlacePath,
         .startSample = 0,
         .frames = 64,
         .sampleRate = 48000.0,
         .channels = 2},
        {.assetId = "bip-asset",
         .assetRelativePath = "Audio/bip.wav",
         .clipId = "bip-clip",
         .trackId = "mcp-bip-track",
         .clipStartSample = 12000});
    require(bounceInPlaceJob.status == lamusica::mcp_bridge::RenderJobStatus::Completed &&
                bounceInPlaceManifest.assets.size() == 1 && bounceInPlaceManifest.clips.size() == 1,
            "MCP bounce-in-place registers generated asset and clip");
    require(bounceInPlaceManifest.clips.front().assetId == "bip-asset" &&
                bounceInPlaceManifest.clips.front().startSample == 12000,
            "MCP bounce-in-place places clip on requested track");
    require(bounceInPlaceJob.resultManifestJson.find("\"assetRegistered\":true") !=
                std::string::npos,
            "MCP bounce-in-place manifest records asset registration");
    const auto duplicateBounceInPlace = renderQueue.enqueueBounceInPlace(
        editSession, "bip-duplicate", bounceInPlaceManifest, mcpBounceGraph,
        {.outputPath = mcpBounceInPlacePath,
         .startSample = 0,
         .frames = 64,
         .sampleRate = 48000.0,
         .channels = 2},
        {.assetId = "bip-asset",
         .assetRelativePath = "Audio/bip.wav",
         .clipId = "bip-clip-2",
         .trackId = "mcp-bip-track"});
    require(duplicateBounceInPlace.status == lamusica::mcp_bridge::RenderJobStatus::Failed,
            "MCP bounce-in-place rejects duplicate asset ids");
    lamusica::session::ProjectManifest freezeManifest;
    freezeManifest.tracks.push_back({.id = "mcp-freeze-track",
                                     .name = "Freeze Track",
                                     .type = lamusica::session::TrackType::Audio});
    freezeManifest.clips.push_back({.id = "freeze-source-clip",
                                    .trackId = "mcp-freeze-track",
                                    .type = lamusica::session::ClipType::Audio,
                                    .startSample = 0,
                                    .lengthSamples = 64});
    const auto mcpFreezePath = std::filesystem::temp_directory_path() / "lamusica-mcp-freeze.wav";
    std::filesystem::remove(mcpFreezePath);
    editSession.attachProject(
        "fixtures/empty.Project.lamusica",
        {lamusica::mcp_bridge::Capability::ReadOnly, lamusica::mcp_bridge::Capability::Render});
    const auto deniedFreeze =
        renderQueue.enqueueFreezeTrack(editSession, "freeze-denied", freezeManifest, mcpBounceGraph,
                                       {.outputPath = mcpFreezePath,
                                        .startSample = 0,
                                        .frames = 64,
                                        .sampleRate = 48000.0,
                                        .channels = 2},
                                       {.trackId = "mcp-freeze-track",
                                        .assetId = "freeze-asset",
                                        .assetRelativePath = "Audio/freeze.wav",
                                        .clipId = "freeze-clip"});
    require(deniedFreeze.status == lamusica::mcp_bridge::RenderJobStatus::Failed &&
                freezeManifest.assets.empty() && freezeManifest.clips.size() == 1,
            "MCP freeze requires asset creation capability");
    editSession.attachProject("fixtures/empty.Project.lamusica",
                              {lamusica::mcp_bridge::Capability::ReadOnly,
                               lamusica::mcp_bridge::Capability::Render,
                               lamusica::mcp_bridge::Capability::ImportExport});
    const auto freezeJob =
        renderQueue.enqueueFreezeTrack(editSession, "freeze-1", freezeManifest, mcpBounceGraph,
                                       {.outputPath = mcpFreezePath,
                                        .startSample = 0,
                                        .frames = 64,
                                        .sampleRate = 48000.0,
                                        .channels = 2},
                                       {.trackId = "mcp-freeze-track",
                                        .assetId = "freeze-asset",
                                        .assetRelativePath = "Audio/freeze.wav",
                                        .clipId = "freeze-clip",
                                        .clipStartSample = 0});
    require(freezeJob.status == lamusica::mcp_bridge::RenderJobStatus::Completed &&
                freezeManifest.assets.size() == 1 && freezeManifest.clips.size() == 2,
            "MCP freeze registers generated asset and clip");
    require(freezeManifest.clips.front().muted &&
                freezeManifest.clips.back().assetId == "freeze-asset",
            "MCP freeze mutes source clips and places frozen render");
    require(freezeJob.resultManifestJson.find("\"type\":\"freeze_track\"") != std::string::npos &&
                freezeJob.resultManifestJson.find(
                    "\"mutedSourceClips\":[\"freeze-source-clip\"]") != std::string::npos,
            "MCP freeze manifest records muted source clips");
    const auto duplicateFreeze = renderQueue.enqueueFreezeTrack(
        editSession, "freeze-duplicate", freezeManifest, mcpBounceGraph,
        {.outputPath = mcpFreezePath,
         .startSample = 0,
         .frames = 64,
         .sampleRate = 48000.0,
         .channels = 2},
        {.trackId = "mcp-freeze-track",
         .assetId = "freeze-asset",
         .assetRelativePath = "Audio/freeze.wav",
         .clipId = "freeze-clip-2"});
    require(duplicateFreeze.status == lamusica::mcp_bridge::RenderJobStatus::Failed,
            "MCP freeze rejects duplicate asset ids");
    const auto longRender =
        renderQueue.enqueueLongRender(editSession, "long-render", mcpBouncePath, 0.42F);
    require(longRender.status == lamusica::mcp_bridge::RenderJobStatus::Running &&
                longRender.progress > 0.4F && longRender.progress < 0.5F,
            "MCP long render reports bounded progress");
    require(renderQueue.cancel("long-render") &&
                renderQueue.find("long-render")->status ==
                    lamusica::mcp_bridge::RenderJobStatus::Cancelled,
            "MCP long render cancels cleanly");
    renderQueue.enqueuePending("pending-render", mcpBouncePath, "waiting");
    require(renderQueue.cancel("pending-render"), "MCP render queue cancels pending render");
    require(renderQueue.find("pending-render")->status ==
                lamusica::mcp_bridge::RenderJobStatus::Cancelled,
            "MCP render cancellation updates job status");
    std::filesystem::remove(mcpReversePath);
    std::filesystem::remove(mcpNormalizePath);
    std::filesystem::remove(mcpFreezePath);
    std::filesystem::remove(mcpBounceInPlacePath);
    std::filesystem::remove(mcpBouncePath);
    std::filesystem::remove(mcpRenderPath);

    lamusica::session::MidiClipData workflowMidi{.clipId = "workflow-midi"};
    workflowMidi.notes.push_back(
        {.id = "wf-note", .startSample = 0, .lengthSamples = 12000, .pitch = 60});
    lamusica::mcp_bridge::DaemonSession orchestrationSession;
    orchestrationSession.attachProject("fixtures/empty.Project.lamusica",
                                       {lamusica::mcp_bridge::Capability::ReadOnly});
    const auto deniedWorkflowPlan =
        lamusica::mcp_bridge::createHarmonizeMidiPlan(orchestrationSession, workflowMidi, 7, 123);
    require(!deniedWorkflowPlan.allowed && deniedWorkflowPlan.plan.steps.empty(),
            "workflow plan creation requires orchestration capability");
    orchestrationSession.attachProject("fixtures/empty.Project.lamusica",
                                       {lamusica::mcp_bridge::Capability::ReadOnly,
                                        lamusica::mcp_bridge::Capability::Orchestration});
    const auto allowedWorkflowPlan =
        lamusica::mcp_bridge::createHarmonizeMidiPlan(orchestrationSession, workflowMidi, 7, 123);
    require(allowedWorkflowPlan.allowed && allowedWorkflowPlan.plan.steps.size() == 1,
            "workflow plan creation allows scoped orchestration capability");
    auto harmonyPlan = lamusica::mcp_bridge::createHarmonizeMidiPlan(workflowMidi, 7, 123);
    require(harmonyPlan.steps.size() == 1, "workflow harmonize creates one step per note");
    require(harmonyPlan.steps.front().commandPreview.find("67") != std::string::npos,
            "workflow harmonize previews interval");
    lamusica::mcp_bridge::approveStep(harmonyPlan, "harmonize-note-0");
    require(lamusica::mcp_bridge::workflowPlanJson(harmonyPlan).find("\"status\":\"approved\"") !=
                std::string::npos,
            "workflow plan records approval");
    lamusica::mcp_bridge::markStepApplied(harmonyPlan, "harmonize-note-0");
    require(lamusica::mcp_bridge::workflowPlanJson(harmonyPlan).find("\"status\":\"applied\"") !=
                std::string::npos,
            "workflow plan records partial application");
    workflowMidi.notes.push_back(
        {.id = "wf-high", .startSample = 12000, .lengthSamples = 12000, .pitch = 126});
    const auto rejectedHarmony =
        lamusica::mcp_bridge::createHarmonizeMidiPlan(workflowMidi, 7, 123);
    require(!rejectedHarmony.steps.back().validationOk,
            "workflow harmonize validates generated note pitch");
    const lamusica::session::PatternClip workflowPattern{
        .id = "workflow-pattern", .name = "Workflow Pattern", .seed = 10};
    const auto drumPlan = lamusica::mcp_bridge::createDrumVariationPlan(workflowPattern, 5);
    require(drumPlan.seed == workflowPattern.seed + 5 && drumPlan.steps.size() == 1 &&
                drumPlan.steps.front().commandName == "duplicate_pattern_variation" &&
                drumPlan.steps.front().validationOk,
            "workflow drum variation is deterministic and command-validated");
    auto rejectedPlan = drumPlan;
    lamusica::mcp_bridge::rejectStep(rejectedPlan, "duplicate-pattern");
    require(lamusica::mcp_bridge::workflowPlanJson(rejectedPlan).find("\"status\":\"rejected\"") !=
                std::string::npos,
            "workflow plan records rejected step");
    lamusica::session::ProjectManifest arrangeManifest;
    arrangeManifest.markers.push_back({.id = "intro", .name = "Intro", .samplePosition = 0});
    const auto arrangePlan = lamusica::mcp_bridge::createArrangeSectionsPlan(
        arrangeManifest,
        {{.id = "verse", .name = "Verse", .samplePosition = 48000},
         {.id = "intro", .name = "Duplicate Intro", .samplePosition = 96000}},
        55);
    require(arrangePlan.steps.size() == 2 && arrangePlan.steps.front().validationOk &&
                arrangePlan.steps.front().commandName == "add_marker" &&
                !arrangePlan.steps.back().validationOk,
            "workflow arrange sections resolves marker edits through command validation");
    lamusica::session::ProjectManifest labelManifest;
    labelManifest.tracks.push_back(
        {.id = "verse-track", .name = "Track 1", .type = lamusica::session::TrackType::Audio});
    const auto labelPlan = lamusica::mcp_bridge::createSongStructureLabelPlan(
        labelManifest, {{"verse-track", "Verse"}, {"missing-track", "Bridge"}}, 44);
    require(labelPlan.steps.size() == 2 && labelPlan.steps.front().validationOk &&
                !labelPlan.steps.back().validationOk,
            "workflow label plan resolves through command validation");
    lamusica::session::MixerState workflowMixer;
    lamusica::session::addChannel(
        workflowMixer,
        {.id = "mix-a", .name = "Mix A", .type = lamusica::session::ChannelType::Audio});
    const auto mixPlanA = lamusica::mcp_bridge::createMixPreparationPlan(workflowMixer, 99);
    const auto mixPlanB = lamusica::mcp_bridge::createMixPreparationPlan(workflowMixer, 99);
    require(mixPlanA.steps.size() == 1 && mixPlanA.steps.front().validationOk &&
                mixPlanA.steps.front().commandPreview == mixPlanB.steps.front().commandPreview,
            "workflow mix preparation is deterministic and command-validated");
    require(lamusica::mcp_bridge::workflowPlanJson(mixPlanA) ==
                lamusica::mcp_bridge::workflowPlanJson(mixPlanB),
            "workflow output is reproducible for fixed inputs and seed");
    auto reviewPlan = rejectedHarmony;
    lamusica::mcp_bridge::reviewWorkflowPlan(
        reviewPlan, {.approvedStepIds = {"harmonize-note-0", "harmonize-note-1"},
                     .rejectedStepIds = {"missing-step"}});
    std::vector<std::string> appliedWorkflowSteps;
    const auto workflowApplySummary = lamusica::mcp_bridge::applyApprovedWorkflowPlanSteps(
        reviewPlan, [&appliedWorkflowSteps](const lamusica::mcp_bridge::WorkflowStep& step) {
            appliedWorkflowSteps.push_back(step.id);
            return true;
        });
    require(workflowApplySummary.appliedCount == 1 && workflowApplySummary.invalidCount == 1 &&
                appliedWorkflowSteps.size() == 1 &&
                appliedWorkflowSteps.front() == "harmonize-note-0",
            "workflow application only applies approved command-validated steps");
    require(lamusica::mcp_bridge::workflowPlanApplicationSummaryJson(workflowApplySummary)
                    .find("\"invalidStepIds\":[\"harmonize-note-1\"]") != std::string::npos,
            "workflow application summary reports invalid approved steps");
    auto reviewAllPlan = drumPlan;
    lamusica::mcp_bridge::reviewWorkflowPlan(reviewAllPlan, {.approveAllValid = true});
    require(std::ranges::all_of(reviewAllPlan.steps,
                                [](const auto& step) {
                                    return step.status ==
                                           lamusica::mcp_bridge::WorkflowStepStatus::Approved;
                                }),
            "workflow review can approve all valid steps for human-confirmed plans");
    lamusica::mcp_bridge::WorkflowTemplateLibrary templateLibrary;
    require(templateLibrary.addTemplate({.id = "mix-prep",
                                         .name = "Mix Prep",
                                         .description = "Prepare mix gain staging",
                                         .workflowType = "mix_preparation"}),
            "workflow template library stores template");
    require(!templateLibrary.addTemplate({.id = "mix-prep",
                                          .name = "Duplicate",
                                          .description = "Duplicate id",
                                          .workflowType = "mix_preparation"}),
            "workflow template library rejects duplicate id");
    require(templateLibrary.find("mix-prep") != nullptr,
            "workflow template library finds stored template");
    require(!templateLibrary.addTemplate({.id = "invalid-template",
                                          .name = "",
                                          .description = "Missing name",
                                          .workflowType = "mix_preparation"}),
            "workflow template library rejects incomplete templates");
    const auto templateJson = lamusica::mcp_bridge::workflowTemplateLibraryJson(templateLibrary);
    require(templateJson.find("\"workflowType\":\"mix_preparation\"") != std::string::npos,
            "workflow template library serializes template records");
    const auto parsedTemplateLibrary =
        lamusica::mcp_bridge::parseWorkflowTemplateLibrary(templateJson);
    require(parsedTemplateLibrary.find("mix-prep") != nullptr &&
                parsedTemplateLibrary.templates().size() == 1,
            "workflow template library parses serialized templates");
    const auto templateLibraryPath =
        std::filesystem::temp_directory_path() / "lamusica-workflow-templates.json";
    lamusica::mcp_bridge::saveWorkflowTemplateLibrary(templateLibrary, templateLibraryPath);
    const auto loadedTemplateLibrary =
        lamusica::mcp_bridge::loadWorkflowTemplateLibrary(templateLibraryPath);
    require(loadedTemplateLibrary.find("mix-prep") != nullptr,
            "workflow template library saves and loads from user library file");
    std::filesystem::remove(templateLibraryPath);

    const auto stressFixture =
        lamusica::session::makeStressProjectFixture({.tracks = 4,
                                                     .clipsPerTrack = 8,
                                                     .markers = 3,
                                                     .pluginsPerTrack = 2,
                                                     .automationLanesPerTrack = 2,
                                                     .automationPointsPerLane = 5,
                                                     .midiNotesPerMidiClip = 4,
                                                     .assets = 5,
                                                     .mcpAuditEntries = 6});
    const auto& stress = stressFixture.manifest;
    require(stress.tracks.size() == 4, "stress project creates tracks");
    require(stress.clips.size() == 32, "stress project creates clips");
    require(stress.markers.size() == 3, "stress project creates markers");
    require(stress.plugins.size() == 8, "stress project creates plugin references");
    require(stress.automation.size() == 8, "stress project creates automation lanes");
    require(stress.automation.front().regions.size() == 1 &&
                stress.automation.front().regions.front().points.size() == 5,
            "stress project creates automation point data");
    require(stress.assets.size() == 5, "stress project creates asset records");
    require(stress.mcpAuditLog.size() == 6, "stress project creates MCP activity records");
    require(!stressFixture.midiClips.empty() && stressFixture.midiClips.front().notes.size() == 4,
            "stress project fixture creates MIDI note data");
    require(lamusica::session::estimateStressProjectMemoryBytes(stressFixture) > 0U &&
                lamusica::session::estimateStressProjectDiskBytes(stressFixture) > 0U,
            "stress project estimates memory and disk footprints");
    const std::array realtimeOps{std::string{"process"}, std::string{"file_io"},
                                 std::string{"lock"}, std::string{"mcp_work"}};
    const auto realtimeReport = lamusica::session::validateRealtimeCallbackPolicy(realtimeOps);
    require(!realtimeReport.noFileIo && !realtimeReport.lockFree && !realtimeReport.noMcpWork,
            "realtime policy detects forbidden operations");
    const lamusica::audio::AudioGraph realtimeAuditGraph{
        .nodes = {{.id = "audit-osc",
                   .kind = lamusica::audio::GraphNodeKind::Sine,
                   .frequencyHz = 220.0,
                   .gain = 0.1F,
                   .noteSequence = {}},
                  {.id = "audit-master",
                   .kind = lamusica::audio::GraphNodeKind::Output,
                   .noteSequence = {}}},
        .connections = {{.sourceNodeId = "audit-osc",
                         .destinationNodeId = "audit-master",
                         .gain = 1.0F}},
        .outputNodeId = "audit-master"};
    const auto callbackAudit = lamusica::session::auditRealtimeGraphCallback(
        realtimeAuditGraph, {.sampleRate = 48000.0, .maxBlockSize = 128, .outputChannels = 2}, 128);
    require(callbackAudit.callbackCompleted && callbackAudit.transportAdvanced &&
                callbackAudit.policy.violations.empty() && callbackAudit.withinBlockDeadline,
            "realtime callback instrumentation proves bounded graph callback safety");
    require(lamusica::session::thresholdsArePositive({}),
            "benchmark thresholds are positive by default");
    const auto machineContext = lamusica::session::currentMachineContext();
    require(!machineContext.cpuModel.empty() && machineContext.logicalCores > 0U &&
                machineContext.memoryMegabytes > 0U && !machineContext.operatingSystem.empty() &&
                !machineContext.compiler.empty(),
            "benchmark machine context records CPU memory OS and compiler");
    const auto passingBenchmark = lamusica::session::evaluateBenchmarkResult(
        {.saveLoadMilliseconds = 10.0, .queryMilliseconds = 2.0, .renderRealtimeFactor = 0.5}, {});
    require(passingBenchmark.passed, "benchmark report passes within thresholds");
    const auto failingBenchmark =
        lamusica::session::evaluateBenchmarkResult({.startupMilliseconds = 500.0,
                                                    .pluginScanMilliseconds = 500.0,
                                                    .cpuWorkMilliseconds = 200.0,
                                                    .editMilliseconds = 200.0,
                                                    .saveLoadMilliseconds = 1000.0,
                                                    .queryMilliseconds = 200.0,
                                                    .mcpQueryMilliseconds = 200.0,
                                                    .realtimeCallbackMilliseconds = 20.0,
                                                    .realtimeCallbackSafe = false,
                                                    .renderRealtimeFactor = 2.0,
                                                    .estimatedMemoryBytes = 512U * 1024U * 1024U,
                                                    .estimatedDiskBytes = 128U * 1024U * 1024U},
                                                   {});
    require(!failingBenchmark.passed && failingBenchmark.regressions.size() == 11,
            "benchmark report catches performance regressions");
    const auto measuredBenchmark = lamusica::session::runStressBenchmark(
        {.stressSpec = {.tracks = 3,
                        .clipsPerTrack = 4,
                        .markers = 2,
                        .pluginsPerTrack = 1,
                        .automationLanesPerTrack = 1,
                        .midiNotesPerMidiClip = 3,
                        .assets = 4,
                        .mcpAuditEntries = 2},
         .thresholds = {.maxSaveLoadMilliseconds = 5000.0,
                        .maxQueryMilliseconds = 5000.0,
                        .maxMcpQueryMilliseconds = 5000.0,
                        .maxRealtimeCallbackMilliseconds = 5000.0,
                        .maxRenderRealtimeFactor = 1000.0},
         .renderFrames = 64});
    require(measuredBenchmark.passed && measuredBenchmark.result.saveLoadMilliseconds >= 0.0 &&
                measuredBenchmark.result.startupMilliseconds >= 0.0 &&
                measuredBenchmark.result.pluginScanMilliseconds >= 0.0 &&
                measuredBenchmark.result.cpuWorkMilliseconds >= 0.0 &&
                measuredBenchmark.result.editMilliseconds >= 0.0 &&
                measuredBenchmark.result.queryMilliseconds >= 0.0 &&
                measuredBenchmark.result.mcpQueryMilliseconds >= 0.0 &&
                measuredBenchmark.result.realtimeCallbackMilliseconds >= 0.0 &&
                measuredBenchmark.result.realtimeCallbackSafe &&
                measuredBenchmark.result.renderRealtimeFactor >= 0.0 &&
                measuredBenchmark.result.estimatedMemoryBytes > 0U &&
                measuredBenchmark.result.estimatedDiskBytes > 0U,
            "stress benchmark runner measures fixed CPU memory disk and render operations");
    const auto tightBenchmark = lamusica::session::runStressBenchmark(
        {.stressSpec = {.tracks = 2, .clipsPerTrack = 2, .assets = 2},
         .thresholds = {.maxSaveLoadMilliseconds = 0.000001,
                        .maxQueryMilliseconds = 0.000001,
                        .maxRenderRealtimeFactor = 0.000001},
         .renderFrames = 64});
    require(!tightBenchmark.passed && !tightBenchmark.regressions.empty(),
            "stress benchmark runner reports threshold failures");

    const auto projectPath =
        std::filesystem::temp_directory_path() / "lamusica-bootstrap-test.Project.lamusica";
    std::filesystem::remove_all(projectPath);

    auto document = lamusica::session::ProjectDocument::createEmpty(projectPath, "Lifecycle");
    require(document.isOpen(), "created document is open");
    require(
        std::filesystem::exists(projectPath / lamusica::session::ProjectDocument::manifestFileName),
        "created document writes manifest");
    require(!std::filesystem::exists(
                projectPath /
                (std::string{lamusica::session::ProjectDocument::manifestFileName} + ".tmp")),
            "project document save does not leave a temporary manifest");

    const auto reopened = lamusica::session::ProjectDocument::open(projectPath);
    require(reopened.project().name() == "Lifecycle", "project document reopens saved name");
    require(reopened.manifest().schemaVersion == lamusica::session::currentProjectSchemaVersion,
            "project document reopens schema version");
    bool rejectedProjectOverwrite = false;
    try {
        (void)lamusica::session::ProjectDocument::createEmpty(projectPath, "Overwrite");
    } catch (const std::exception&) {
        rejectedProjectOverwrite = true;
    }
    require(rejectedProjectOverwrite,
            "project document creation refuses to overwrite an existing project bundle");

    document.close();
    require(!document.isOpen(), "document closes");

    std::filesystem::remove_all(projectPath);

    lamusica::session::ProjectManifest manifest;
    manifest.name = "Round Trip";
    manifest.tracks.push_back(
        {.id = "track-1", .name = "Audio 1", .type = lamusica::session::TrackType::Audio});
    manifest.tracks.push_back(
        {.id = "master", .name = "Master", .type = lamusica::session::TrackType::Master});
    manifest.assets.push_back(
        {.id = "asset-1", .relativePath = "Audio Files/tone.wav", .mediaType = "audio/wav"});
    manifest.markers.push_back({.id = "marker-1", .name = "Verse", .samplePosition = 48000});
    manifest.clips.push_back({.id = "clip-round-trip",
                              .trackId = "track-1",
                              .type = lamusica::session::ClipType::Audio,
                              .startSample = 960,
                              .lengthSamples = 4800,
                              .sourceOffsetSamples = 120,
                              .fadeInSamples = 24,
                              .fadeOutSamples = 48,
                              .gainDb = -3.0F,
                              .muted = true,
                              .reversed = true,
                              .assetId = "asset-1"});
    manifest.routing.push_back({.sourceTrackId = "track-1", .destinationTrackId = "master"});
    manifest.plugins.push_back(
        {.id = "plugin-1", .trackId = "track-1", .format = "AU", .identifier = "example.eq"});
    manifest.automation.push_back(
        {.id = "automation-1",
         .targetKind = lamusica::session::AutomationTargetKind::Plugin,
         .targetId = "plugin-1",
         .parameterId = "gain",
         .mode = lamusica::session::AutomationMode::Touch,
         .defaultValue = 0.25F,
         .regions = {{.startSample = 0,
                      .endSample = 96000,
                      .points = {{.samplePosition = 0,
                                  .value = 0.25F,
                                  .curveToNext = lamusica::session::AutomationCurve::Linear},
                                 {.samplePosition = 96000,
                                  .value = 0.75F,
                                  .curveToNext = lamusica::session::AutomationCurve::Step}}}}});
    manifest.mcpAuditLog.push_back(
        {.id = "audit-1", .toolName = "query_project_summary", .capability = "read_project"});

    const auto manifestJson = lamusica::session::serializeProjectManifest(manifest);
    const auto parsedManifest = lamusica::session::parseProjectManifest(manifestJson);
    require(parsedManifest.name == "Round Trip", "project manifest name round trip");
    require(parsedManifest.schemaVersion == lamusica::session::currentProjectSchemaVersion,
            "project manifest schema version round trip");
    require(parsedManifest.markers.size() == 1 &&
                parsedManifest.markers.front().samplePosition == 48000,
            "project manifest markers round trip");
    require(parsedManifest.assets.size() == 1 &&
                parsedManifest.assets.front().relativePath == "Audio Files/tone.wav",
            "project manifest assets round trip");
    require(parsedManifest.tracks.size() == 2 &&
                parsedManifest.tracks.front().type == lamusica::session::TrackType::Audio,
            "project manifest tracks round trip");
    require(parsedManifest.clips.size() == 1 && parsedManifest.clips.front().muted &&
                parsedManifest.clips.front().reversed &&
                parsedManifest.clips.front().assetId == "asset-1",
            "project manifest clips round trip");
    require(parsedManifest.routing.size() == 1 &&
                parsedManifest.routing.front().destinationTrackId == "master",
            "project manifest routing round trip");
    require(parsedManifest.plugins.size() == 1 &&
                parsedManifest.plugins.front().identifier == "example.eq",
            "project manifest plugins round trip");
    require(parsedManifest.automation.size() == 1 &&
                parsedManifest.automation.front().targetKind ==
                    lamusica::session::AutomationTargetKind::Plugin &&
                parsedManifest.automation.front().parameterId == "gain" &&
                parsedManifest.automation.front().mode ==
                    lamusica::session::AutomationMode::Touch &&
                parsedManifest.automation.front().regions.front().points.back().value == 0.75F,
            "project manifest automation data round trip");
    require(parsedManifest.mcpAuditLog.size() == 1 &&
                parsedManifest.mcpAuditLog.front().capability == "read_project",
            "project manifest MCP audit log round trip");
    bool rejectedFractionalSchema = false;
    try {
        (void)lamusica::session::parseProjectManifest(
            "{\"schemaVersion\": 1.5, \"name\": \"Bad\"}");
    } catch (const std::exception&) {
        rejectedFractionalSchema = true;
    }
    require(rejectedFractionalSchema, "project manifest rejects fractional integer fields");
    bool rejectedMissingTopLevelArray = false;
    try {
        (void)lamusica::session::parseProjectManifest("{\"schemaVersion\": 1, \"name\": \"Bad\"}");
    } catch (const std::exception&) {
        rejectedMissingTopLevelArray = true;
    }
    require(rejectedMissingTopLevelArray, "project manifest rejects missing v1 top-level arrays");
    bool rejectedUnknownTopLevelField = false;
    try {
        (void)lamusica::session::parseProjectManifest(
            "{\"schemaVersion\": 1, \"name\": \"Bad\", \"tempoMap\": [], "
            "\"timeSignatures\": [], \"markers\": [], \"assets\": [], \"tracks\": [], "
            "\"clips\": [], \"midiClips\": [], \"routing\": [], \"plugins\": [], "
            "\"automation\": [], \"mcpAuditLog\": [], \"unexpected\": true}");
    } catch (const std::exception&) {
        rejectedUnknownTopLevelField = true;
    }
    require(rejectedUnknownTopLevelField,
            "project manifest rejects unknown top-level schema fields");
    bool rejectedInvalidTempo = false;
    try {
        auto invalidTempo = manifest;
        invalidTempo.tempoMap.front().bpm = 0.0;
        (void)lamusica::session::serializeProjectManifest(invalidTempo);
    } catch (const std::exception&) {
        rejectedInvalidTempo = true;
    }
    require(rejectedInvalidTempo, "project manifest rejects invalid tempo events");
    bool rejectedDuplicateMarker = false;
    try {
        auto duplicateMarker = manifest;
        duplicateMarker.markers.push_back(duplicateMarker.markers.front());
        (void)lamusica::session::serializeProjectManifest(duplicateMarker);
    } catch (const std::exception&) {
        rejectedDuplicateMarker = true;
    }
    require(rejectedDuplicateMarker, "project manifest rejects duplicate marker ids");
    bool rejectedMissingClipField = false;
    try {
        (void)lamusica::session::parseProjectManifest(
            "{\"schemaVersion\": 1, \"name\": \"Bad\", \"tempoMap\": [], "
            "\"timeSignatures\": [], \"markers\": [], \"assets\": [], \"tracks\": [], "
            "\"clips\": [{\"id\": \"clip\"}], \"midiClips\": [], \"routing\": [], "
            "\"plugins\": [], \"automation\": [], \"mcpAuditLog\": []}");
    } catch (const std::exception&) {
        rejectedMissingClipField = true;
    }
    require(rejectedMissingClipField, "project manifest reports missing required clip fields");
    bool rejectedMissingRouteTrack = false;
    try {
        (void)lamusica::session::parseProjectManifest(
            "{\"schemaVersion\": 1, \"name\": \"Bad\", \"tempoMap\": [], "
            "\"timeSignatures\": [], \"markers\": [], \"assets\": [], "
            "\"tracks\": [{\"id\": \"track\", \"name\": \"Track\", \"type\": \"audio\"}], "
            "\"clips\": [], \"midiClips\": [], "
            "\"routing\": [{\"sourceTrackId\": \"track\", \"destinationTrackId\": \"missing\"}], "
            "\"plugins\": [], \"automation\": [], \"mcpAuditLog\": []}");
    } catch (const std::exception&) {
        rejectedMissingRouteTrack = true;
    }
    require(rejectedMissingRouteTrack, "project manifest rejects missing routing track refs");
    const auto migratedManifest =
        lamusica::session::parseProjectManifest("{\"schemaVersion\": 0, \"projectName\": \"Old\"}");
    require(migratedManifest.schemaVersion == lamusica::session::currentProjectSchemaVersion,
            "project manifest migrates legacy schema version");
    require(migratedManifest.name == "Old", "project manifest migrates legacy project name");
    require(migratedManifest.tempoMap.size() == 1 && migratedManifest.timeSignatures.size() == 1,
            "project manifest migration creates default musical metadata");
    const auto starterManifest =
        lamusica::session::makeFirstTrackStarterManifest("Starter Session");
    require(lamusica::session::isFirstTrackStarterManifest(starterManifest),
            "session creates reusable first-track starter manifest");
    require(starterManifest.tracks.size() == 3 && starterManifest.clips.size() == 2 &&
                starterManifest.routing.size() == 2 && starterManifest.plugins.size() == 3 &&
                starterManifest.automation.size() == 3 && starterManifest.midiClips.size() == 1,
            "first-track starter includes tracks clips routing plugins automation and MIDI refs");
    require(lamusica::session::arrangementEndSample(starterManifest) == 96000,
            "first-track starter reports arrangement end sample");
    require(lamusica::session::renderableArrangementFrames(starterManifest) == 96000,
            "first-track starter reports renderable frame count");
    const auto starterReadiness = lamusica::session::inspectFirstTrackReadiness(starterManifest);
    require(
        starterReadiness.starterStructureReady && starterReadiness.firstTrackEditable &&
            starterReadiness.renderable && starterReadiness.renderFrames == 96000 &&
            starterReadiness.pluginCount == 3 && starterReadiness.automationLaneCount == 3 &&
            starterReadiness.midiClipReferenceCount == 1 &&
            starterReadiness.starterMidiNoteCount == 8 &&
            starterReadiness.starterBassTransposeSemitones == 0 && starterReadiness.loopReady &&
            starterReadiness.loopStartSample == 0 && starterReadiness.loopEndSample == 96000 &&
            starterReadiness.missingRequirements.empty() &&
            lamusica::session::firstTrackStarterRequirementIds().size() == 19,
        "first-track readiness reports structure renderability plugins automation MIDI and loop");
    auto incompleteStarterManifest = starterManifest;
    incompleteStarterManifest.routing.clear();
    incompleteStarterManifest.plugins.erase(incompleteStarterManifest.plugins.begin());
    incompleteStarterManifest.loopEnabled = false;
    const auto incompleteStarterReadiness =
        lamusica::session::inspectFirstTrackReadiness(incompleteStarterManifest);
    require(!incompleteStarterReadiness.starterStructureReady &&
                !incompleteStarterReadiness.loopReady &&
                std::ranges::find(incompleteStarterReadiness.missingRequirements,
                                  "route.drums-master") !=
                    incompleteStarterReadiness.missingRequirements.end() &&
                std::ranges::find(incompleteStarterReadiness.missingRequirements,
                                  "route.bass-master") !=
                    incompleteStarterReadiness.missingRequirements.end() &&
                std::ranges::find(incompleteStarterReadiness.missingRequirements,
                                  "plugin.drum-starter-sampler") !=
                    incompleteStarterReadiness.missingRequirements.end() &&
                std::ranges::find(incompleteStarterReadiness.missingRequirements, "loop.intro") !=
                    incompleteStarterReadiness.missingRequirements.end(),
            "first-track readiness reports actionable missing starter requirements");
    auto looplessStarterManifest = starterManifest;
    looplessStarterManifest.loopEnabled = false;
    looplessStarterManifest.loopStartSample = 0;
    looplessStarterManifest.loopEndSample = 0;
    const auto looplessStarterReadiness =
        lamusica::session::inspectFirstTrackReadiness(looplessStarterManifest);
    require(!looplessStarterReadiness.starterStructureReady &&
                looplessStarterReadiness.firstTrackEditable &&
                looplessStarterReadiness.renderable && !looplessStarterReadiness.loopReady &&
                looplessStarterReadiness.missingRequirements.size() == 1 &&
                looplessStarterReadiness.missingRequirements.front() == "loop.intro",
            "first-track readiness reports loopless starter projects are editable but not "
            "package-ready");
    const auto starterBassMidi = lamusica::session::makeFirstTrackStarterBassMidi();
    require(starterBassMidi.clipId == "bass-pattern" && starterBassMidi.notes.size() == 8 &&
                starterBassMidi.notes.front().pitch == 36 &&
                starterBassMidi.notes.front().startSample == 0 &&
                starterBassMidi.notes.back().startSample == 90000 &&
                std::ranges::any_of(starterBassMidi.metadata,
                                    [](const lamusica::session::MidiMetadata& metadata) {
                                        return metadata.key == "dataId" &&
                                               metadata.value == "starter-bass-midi";
                                    }),
            "first-track starter includes concrete reusable bass MIDI content");
    require(lamusica::session::makeFirstTrackStarterMidiClips().front().notes.size() == 8,
            "first-track starter exposes all reusable MIDI clips");
    const auto starterGraph = lamusica::session::compileProjectAudioGraph(
        starterManifest, lamusica::session::MixerState{});
    const auto starterBassNode =
        std::ranges::find_if(starterGraph.nodes, [](const lamusica::audio::GraphNode& node) {
            return node.id == "clip:bass-pattern";
        });
    require(starterBassNode != starterGraph.nodes.end() &&
                starterBassNode->noteSequence.size() == 8 &&
                starterBassNode->noteSequence.front().startSample == 0 &&
                starterBassNode->noteSequence.back().startSample == 90000,
            "first-track starter bass MIDI compiles into timed graph notes");
    auto transposedStarterManifest = starterManifest;
    transposedStarterManifest.midiClips.front().transposeSemitones = 12;
    const auto transposedReadiness =
        lamusica::session::inspectFirstTrackReadiness(transposedStarterManifest);
    const auto transposedStarterGraph = lamusica::session::compileProjectAudioGraph(
        transposedStarterManifest, lamusica::session::MixerState{});
    const auto transposedBassNode = std::ranges::find_if(
        transposedStarterGraph.nodes,
        [](const lamusica::audio::GraphNode& node) { return node.id == "clip:bass-pattern"; });
    require(transposedReadiness.starterBassTransposeSemitones == 12 &&
                transposedBassNode != transposedStarterGraph.nodes.end() &&
                transposedBassNode->noteSequence.size() == 8 &&
                std::abs((transposedBassNode->noteSequence.front().frequencyHz /
                          starterBassNode->noteSequence.front().frequencyHz) -
                         2.0) < 0.001,
            "first-track starter bass transpose persists into compiled graph notes");
    const auto starterSummary = lamusica::session::summarizeFirstTrackArrangement(starterManifest);
    require(
        starterSummary.tempoBpm == 120.0 && starterSummary.timeSignatureNumerator == 4 &&
            starterSummary.timeSignatureDenominator == 4 && starterSummary.sectionCount == 2 &&
            starterSummary.audioTrackCount == 1 && starterSummary.midiTrackCount == 1 &&
            starterSummary.masterTrackCount == 1 && starterSummary.firstSectionName == "Intro" &&
            starterSummary.finalSectionName == "Verse" && starterSummary.firstSectionSample == 0 &&
            starterSummary.finalSectionSample == 96000,
        "first-track arrangement summary reports tempo meter sections and track roles");

    const auto exampleDocument = lamusica::session::ProjectDocument::open(
        "fixtures/examples/generated-tone.Project.lamusica");
    require(exampleDocument.manifest().tracks.size() == 2, "project document opens example tracks");
    require(exampleDocument.manifest().clips.size() == 1, "project document opens example clips");
    require(exampleDocument.manifest().routing.size() == 1,
            "project document opens example routing");
    const auto tutorialDocument =
        lamusica::session::ProjectDocument::open("fixtures/tutorials/first-song.Project.lamusica");
    require(tutorialDocument.manifest().markers.size() == 2 &&
                tutorialDocument.manifest().tracks.size() == 3 &&
                tutorialDocument.manifest().clips.size() == 2 &&
                tutorialDocument.manifest().plugins.size() == 3 &&
                tutorialDocument.manifest().automation.size() == 3 &&
                tutorialDocument.manifest().midiClips.size() == 1 &&
                tutorialDocument.manifest().midiClips.front().dataId == "starter-bass-midi" &&
                lamusica::session::isFirstTrackStarterManifest(tutorialDocument.manifest()),
            "project document opens first-track-ready tutorial fixture");
    lamusica::session::ApplicationSession appSession;
    bool rejectedProjectVerificationWithoutOpenProject = false;
    try {
        (void)appSession.verifyFirstTrackProject();
    } catch (const std::exception&) {
        rejectedProjectVerificationWithoutOpenProject = true;
    }
    require(rejectedProjectVerificationWithoutOpenProject,
            "application shell rejects first-track project verification without an open project");
    const auto appShellProject =
        std::filesystem::temp_directory_path() / "lamusica-app-shell.Project.lamusica";
    std::filesystem::remove_all(appShellProject);
    appSession.createProject(appShellProject, "App Shell");
    const auto unreadyFirstTrackVerification = appSession.verifyFirstTrackProject();
    require(appSession.status().hasOpenProject && appSession.status().projectName == "App Shell",
            "application shell session creates an open project");
    require(!appSession.status().firstTrackReady && appSession.status().trackCount == 0 &&
                !appSession.status().firstTrackEditable &&
                !unreadyFirstTrackVerification.firstTrackReady &&
                !unreadyFirstTrackVerification.firstTrackEditable &&
                !unreadyFirstTrackVerification.starterStructureReady &&
                !unreadyFirstTrackVerification.renderable &&
                !unreadyFirstTrackVerification.loopEnabled &&
                unreadyFirstTrackVerification.renderFrames == 0 &&
                unreadyFirstTrackVerification.trackCount == 0 &&
                unreadyFirstTrackVerification.clipCount == 0 &&
                unreadyFirstTrackVerification.missingRequirements.size() ==
                    lamusica::session::firstTrackStarterRequirementIds().size(),
            "application shell reports empty project is not first-track ready");
    appSession.saveProject();
    appSession.closeProject();
    require(!appSession.status().hasOpenProject, "application shell session closes project");
    appSession.openProject(appShellProject);
    require(appSession.status().projectPath == appShellProject &&
                appSession.recentProjects().front() == appShellProject,
            "application shell session reopens and tracks recent project");
    require(!std::filesystem::exists(
                appShellProject /
                (std::string{lamusica::session::ProjectDocument::manifestFileName} + ".tmp")),
            "application shell save does not leave a temporary project manifest");
    appSession.closeProject();
    require(appSession.recoverLastProject(appShellProject),
            "application shell startup recovers last project");
    std::filesystem::remove_all(appShellProject);
    require(!appSession.recoverLastProject(appShellProject),
            "application shell startup ignores missing recovery project");
    const auto appFirstTrackProject =
        std::filesystem::temp_directory_path() / "lamusica-app-first-track.Project.lamusica";
    std::filesystem::remove_all(appFirstTrackProject);
    appSession.createFirstTrackProject(appFirstTrackProject, "App First Track");
    const auto readyFirstTrackVerification = appSession.verifyFirstTrackProject();
    require(
        appSession.status().firstTrackReady && appSession.status().starterStructureReady &&
            appSession.status().firstTrackEditable && appSession.status().renderable &&
            appSession.status().trackCount == 3 && appSession.status().clipCount == 2 &&
            appSession.status().pluginCount == 3 && appSession.status().automationLaneCount == 3 &&
            appSession.status().starterMidiNoteCount == 8 &&
            appSession.status().starterBassTransposeSemitones == 0 &&
            appSession.status().drumClipGainDb == -9.0F &&
            appSession.status().bassClipGainDb == 0.0F && appSession.status().loopEnabled &&
            appSession.status().loopStartSample == 0 &&
            appSession.status().loopEndSample == 96000 && appSession.status().loopFrames == 96000 &&
            appSession.status().renderFrames == 96000 && appSession.status().tempoBpm == 120.0 &&
            appSession.status().timeSignatureNumerator == 4 &&
            appSession.status().timeSignatureDenominator == 4 &&
            appSession.status().firstSectionName == "Intro" &&
            appSession.status().finalSectionName == "Verse" &&
            readyFirstTrackVerification.projectPath == appFirstTrackProject &&
            readyFirstTrackVerification.projectName == "App First Track" &&
            readyFirstTrackVerification.firstTrackReady &&
            readyFirstTrackVerification.firstTrackEditable &&
            readyFirstTrackVerification.starterStructureReady &&
            readyFirstTrackVerification.renderable && readyFirstTrackVerification.loopEnabled &&
            readyFirstTrackVerification.loopFrames == 96000 &&
            readyFirstTrackVerification.renderFrames == 96000 &&
            readyFirstTrackVerification.trackCount == 3 &&
            readyFirstTrackVerification.clipCount == 2 &&
            readyFirstTrackVerification.pluginCount == 3 &&
            readyFirstTrackVerification.automationLaneCount == 3 &&
            readyFirstTrackVerification.starterMidiNoteCount == 8 &&
            readyFirstTrackVerification.missingRequirements.empty(),
        "application shell status reports first-track readiness");
    const auto appFirstTrackMix =
        std::filesystem::temp_directory_path() / "lamusica-app-first-track.wav";
    std::filesystem::remove(appFirstTrackMix);
    const auto appFirstTrackBounce = appSession.exportCurrentMix(appFirstTrackMix);
    require(appFirstTrackBounce.outputPath == appFirstTrackMix &&
                appFirstTrackBounce.frames == appSession.status().renderFrames &&
                appFirstTrackBounce.peakAfterNormalization > 0.0F &&
                appSession.status().lastMixExportPath == appFirstTrackMix &&
                appSession.status().lastMixExportFrames == appFirstTrackBounce.frames &&
                appSession.status().lastMixExportPeak == appFirstTrackBounce.peakAfterNormalization,
            "application shell exports the current first-track mix");
    std::filesystem::remove(appFirstTrackMix);
    appSession.transposeFirstTrackBass(12);
    require(appSession.status().starterBassTransposeSemitones == 12 &&
                appSession.status().firstTrackReady,
            "application shell transposes first-track bass MIDI");
    appSession.openProject(appFirstTrackProject);
    require(appSession.status().starterBassTransposeSemitones == 12,
            "application shell persists first-track bass transpose");
    appSession.setClipGain("drum-loop", -18.0F);
    appSession.setClipGain("bass-pattern", -3.0F);
    require(appSession.status().drumClipGainDb == -18.0F &&
                appSession.status().bassClipGainDb == -3.0F && appSession.status().canUndo &&
                appSession.status().undoDepth >= 2,
            "application shell edits first-track clip gains");
    require(appSession.undoLastEdit() && appSession.status().drumClipGainDb == -18.0F &&
                appSession.status().bassClipGainDb == 0.0F && appSession.status().canRedo,
            "application shell undoes the latest first-track edit");
    require(appSession.redoLastEdit() && appSession.status().drumClipGainDb == -18.0F &&
                appSession.status().bassClipGainDb == -3.0F && appSession.status().canUndo,
            "application shell redoes the latest first-track edit");
    appSession.setFirstTrackTrackMix("drums", -9.0F, -0.25F, false, false);
    const auto appTrackMix = appSession.firstTrackTrackMix();
    const auto appDrumMix = std::ranges::find_if(
        appTrackMix, [](const lamusica::session::FirstTrackTrackMixSummary& mix) {
            return mix.trackId == "drums";
        });
    require(appDrumMix != appTrackMix.end() && appDrumMix->trackName == "Generated Drums" &&
                appDrumMix->volumeDb == -9.0F && appDrumMix->pan == -0.25F && !appDrumMix->muted &&
                !appDrumMix->solo && appSession.status().canUndo,
            "application shell edits first-track track mix");
    const auto undoTrackMixSucceeded = appSession.undoLastEdit();
    const auto undoneTrackMix = appSession.firstTrackTrackMix();
    const auto undoneDrumMix = std::ranges::find_if(
        undoneTrackMix, [](const lamusica::session::FirstTrackTrackMixSummary& mix) {
            return mix.trackId == "drums";
        });
    require(undoTrackMixSucceeded && undoneDrumMix != undoneTrackMix.end() &&
                undoneDrumMix->volumeDb == 0.0F && appSession.status().canRedo,
            "application shell undoes first-track track mix edits");
    const auto redoTrackMixSucceeded = appSession.redoLastEdit();
    const auto redoneTrackMix = appSession.firstTrackTrackMix();
    const auto redoneDrumMix = std::ranges::find_if(
        redoneTrackMix, [](const lamusica::session::FirstTrackTrackMixSummary& mix) {
            return mix.trackId == "drums";
        });
    require(redoTrackMixSucceeded && redoneDrumMix != redoneTrackMix.end() &&
                redoneDrumMix->volumeDb == -9.0F,
            "application shell redoes first-track track mix edits");
    const auto mixedFirstTrackGraph = lamusica::session::compileProjectAudioGraph(
        appSession.currentDocument()->manifest(), lamusica::session::MixerState{});
    const auto mixedDrumNode = std::ranges::find_if(
        mixedFirstTrackGraph.nodes,
        [](const lamusica::audio::GraphNode& node) { return node.id == "clip:drum-loop"; });
    const auto mixedBassNode = std::ranges::find_if(
        mixedFirstTrackGraph.nodes,
        [](const lamusica::audio::GraphNode& node) { return node.id == "clip:bass-pattern"; });
    const auto mixedDrumTrackNode = std::ranges::find_if(
        mixedFirstTrackGraph.nodes,
        [](const lamusica::audio::GraphNode& node) { return node.id == "track:drums"; });
    require(
        mixedDrumNode != mixedFirstTrackGraph.nodes.end() &&
            mixedBassNode != mixedFirstTrackGraph.nodes.end() &&
            mixedDrumTrackNode != mixedFirstTrackGraph.nodes.end() &&
            std::abs(mixedDrumNode->gain - lamusica::session::dbToLinearGain(-18.0F)) < 0.0001F &&
            std::abs(mixedBassNode->gain - lamusica::session::dbToLinearGain(-3.0F)) < 0.0001F &&
            std::abs(mixedDrumTrackNode->gain - lamusica::session::dbToLinearGain(-9.0F)) <
                0.0001F &&
            mixedDrumTrackNode->pan == -0.25F,
        "application shell first-track clip and track gains affect compiled graph");
    appSession.openProject(appFirstTrackProject);
    const auto persistedTrackMix = appSession.firstTrackTrackMix();
    const auto persistedDrumMix = std::ranges::find_if(
        persistedTrackMix, [](const lamusica::session::FirstTrackTrackMixSummary& mix) {
            return mix.trackId == "drums";
        });
    require(appSession.status().drumClipGainDb == -18.0F &&
                appSession.status().bassClipGainDb == -3.0F &&
                persistedDrumMix != persistedTrackMix.end() && persistedDrumMix->volumeDb == -9.0F,
            "application shell persists first-track clip and track gains");
    appSession.clearLoopRegion();
    require(!appSession.status().loopEnabled && appSession.status().loopFrames == 0,
            "application shell clears first-track loop region");
    appSession.setFirstTrackLoopToIntro();
    require(appSession.status().loopEnabled && appSession.status().loopStartSample == 0 &&
                appSession.status().loopEndSample == 96000 &&
                appSession.status().loopFrames == 96000,
            "application shell restores first-track intro loop region");
    const auto appFirstTrackLoop =
        std::filesystem::temp_directory_path() / "lamusica-app-first-track-loop.wav";
    std::filesystem::remove(appFirstTrackLoop);
    const auto appFirstTrackLoopBounce = appSession.exportCurrentLoop(appFirstTrackLoop);
    require(appFirstTrackLoopBounce.outputPath == appFirstTrackLoop &&
                appFirstTrackLoopBounce.frames == appSession.status().loopFrames &&
                appFirstTrackLoopBounce.peakAfterNormalization > 0.0F,
            "application shell exports the current first-track loop");
    std::filesystem::remove(appFirstTrackLoop);
    appSession.openProject(appFirstTrackProject);
    require(appSession.status().loopEnabled && appSession.status().loopFrames == 96000,
            "application shell persists first-track loop region");
    appSession.seek(95936);
    appSession.play();
    require(appSession.status().transportPlaying && appSession.status().playheadSample == 95936,
            "application shell starts first-track transport from requested playhead");
    const auto auditionBlock = appSession.auditionCurrentMixBlock(128);
    require(auditionBlock.frames == 128 && auditionBlock.channels == 2 &&
                lamusica::audio::peakAbsoluteSample(auditionBlock) > 0.0F &&
                appSession.status().playheadSample == 64,
            "application shell auditions first-track mix and wraps through loop");
    appSession.stop();
    require(!appSession.status().transportPlaying && appSession.status().playheadSample == 64,
            "application shell stops first-track transport without losing playhead");
    appSession.extendFirstTrackArrangementToVerse();
    require(appSession.status().firstTrackReady && appSession.status().renderFrames == 192000 &&
                appSession.status().loopEnabled && appSession.status().loopFrames == 96000,
            "application shell extends first-track starter into a verse arrangement");
    const auto extendedFirstTrackGraph = lamusica::session::compileProjectAudioGraph(
        appSession.currentDocument()->manifest(), lamusica::session::MixerState{});
    const auto extendedBassNode = std::ranges::find_if(
        extendedFirstTrackGraph.nodes,
        [](const lamusica::audio::GraphNode& node) { return node.id == "clip:bass-pattern"; });
    require(extendedBassNode != extendedFirstTrackGraph.nodes.end() &&
                extendedBassNode->noteSequence.size() == 16 &&
                extendedBassNode->noteSequence.back().startSample == 186000,
            "application shell first-track verse extension repeats starter bass notes");
    const auto duplicatedBass =
        appSession.duplicateClip("bass-pattern", "bass-pattern-copy", 96000);
    const auto duplicatedBassGraph = lamusica::session::compileProjectAudioGraph(
        appSession.currentDocument()->manifest(), lamusica::session::MixerState{});
    const auto duplicatedBassNode =
        std::ranges::find_if(duplicatedBassGraph.nodes, [](const lamusica::audio::GraphNode& node) {
            return node.id == "clip:bass-pattern-copy";
        });
    require(duplicatedBass.clipId == "bass-pattern-copy" && duplicatedBass.trackId == "bass" &&
                duplicatedBassNode != duplicatedBassGraph.nodes.end() &&
                duplicatedBassNode->startSample == 96000 &&
                duplicatedBassNode->noteSequence.size() == 16 && appSession.status().canUndo,
            "application shell duplicates first-track MIDI clips with playable note references");
    const auto removedBassDuplicate = appSession.removeClip("bass-pattern-copy");
    const auto removedBassDuplicateGraph = lamusica::session::compileProjectAudioGraph(
        appSession.currentDocument()->manifest(), lamusica::session::MixerState{});
    require(removedBassDuplicate.clipId == "bass-pattern-copy" &&
                removedBassDuplicate.removedMidiReferenceCount == 1 &&
                std::ranges::none_of(removedBassDuplicateGraph.nodes,
                                     [](const lamusica::audio::GraphNode& node) {
                                         return node.id == "clip:bass-pattern-copy";
                                     }) &&
                std::ranges::none_of(appSession.currentDocument()->manifest().midiClips,
                                     [](const lamusica::session::MidiClipReference& reference) {
                                         return reference.clipId == "bass-pattern-copy";
                                     }),
            "application shell removes duplicated first-track MIDI clips and references");
    require(appSession.undoLastEdit(), "application shell undoes first-track MIDI clip removal");
    require(appSession.undoLastEdit(), "application shell undoes first-track MIDI clip duplicate");
    appSession.openProject(appFirstTrackProject);
    require(appSession.status().renderFrames == 192000 && appSession.status().firstTrackReady,
            "application shell persists first-track verse extension");
    const auto appFirstTrackStems =
        std::filesystem::temp_directory_path() / "lamusica-app-first-track-stems";
    std::filesystem::remove_all(appFirstTrackStems);
    const auto appStemExports = appSession.exportCurrentStems(appFirstTrackStems);
    require(appStemExports.size() == 2 &&
                std::ranges::all_of(appStemExports,
                                    [&appSession](const lamusica::session::StemExportResult& stem) {
                                        return stem.bounce.frames ==
                                                   appSession.status().renderFrames &&
                                               stem.bounce.peakAfterNormalization > 0.0F &&
                                               std::filesystem::exists(stem.bounce.outputPath);
                                    }) &&
                appSession.status().lastStemExportDirectory == appFirstTrackStems &&
                appSession.status().lastStemExportCount == 2 &&
                appSession.status().lastStemExportFrames == 192000,
            "application shell exports first-track drums and bass stems");
    std::filesystem::remove_all(appFirstTrackStems);
    lamusica::session::FirstTrackRecordingOptions punchOptions;
    punchOptions.countInBars = 2;
    punchOptions.punchInSample = 48000;
    punchOptions.punchOutSample = 96000;
    const auto punchPlan = appSession.prepareFirstTrackRecording(punchOptions);
    require(punchPlan.timelineStartSample == 48000 && punchPlan.recordFrames == 48000 &&
                punchPlan.countInBars == 2 && punchPlan.countInSamples == 192000 &&
                punchPlan.prerollStartSample == 0 && punchPlan.punchEnabled &&
                punchPlan.punchInSample == 48000 && punchPlan.punchOutSample == 96000,
            "application shell plans first-track punch recording with count-in");
    lamusica::session::FirstTrackRecordingOptions takeOptions;
    takeOptions.frames = 48000;
    takeOptions.startSample = 96000;
    const auto appRecording = appSession.recordFirstTrackTake(takeOptions);
    require(appRecording.trackId == "recorded-takes" && appRecording.committed.frames == 48000 &&
                appRecording.committed.channels == 2 &&
                std::filesystem::exists(appRecording.committed.path) &&
                appRecording.timelineStartSample == 96000 && appRecording.countInSamples == 96000 &&
                appRecording.prerollStartSample == 0 && !appRecording.punchEnabled &&
                appSession.status().lastRecordingPath == appRecording.committed.path &&
                appSession.status().lastRecordingFrames == 48000 &&
                appSession.status().lastRecordingStartSample == 96000 &&
                appSession.status().lastRecordingCountInSamples == 96000 &&
                appSession.status().lastRecordingPrerollStartSample == 0 &&
                !appSession.status().lastRecordingPunchEnabled &&
                appSession.status().lastRecordingAssetId == appRecording.assetId &&
                appSession.status().lastRecordingClipId == appRecording.clipId &&
                appSession.status().recordedTakeCount == 1 && appSession.status().trackCount == 4 &&
                appSession.status().clipCount == 3,
            "application shell records a first-track audio take into the project");
    appSession.openProject(appFirstTrackProject);
    require(appSession.status().recordedTakeCount == 1 && appSession.status().trackCount == 4 &&
                appSession.status().clipCount == 3 &&
                std::ranges::any_of(appSession.currentDocument()->manifest().assets,
                                    [&appRecording](const lamusica::session::Asset& asset) {
                                        return asset.id == appRecording.assetId &&
                                               asset.mediaType == "audio/wav";
                                    }) &&
                std::ranges::any_of(appSession.currentDocument()->manifest().clips,
                                    [&appRecording](const lamusica::session::Clip& clip) {
                                        return clip.id == appRecording.clipId &&
                                               clip.trackId == "recorded-takes" &&
                                               clip.assetId == appRecording.assetId &&
                                               clip.startSample == 96000;
                                    }),
            "application shell persists the first-track recorded take");
    const auto recordedTakeGraph = lamusica::session::compileProjectAudioGraph(
        appSession.currentDocument()->manifest(), lamusica::session::MixerState{},
        {.projectRoot = appFirstTrackProject});
    const auto recordedTakeNode = std::ranges::find_if(
        recordedTakeGraph.nodes, [&appRecording](const lamusica::audio::GraphNode& node) {
            return node.id == "clip:" + appRecording.clipId;
        });
    require(recordedTakeNode != recordedTakeGraph.nodes.end() &&
                recordedTakeNode->kind == lamusica::audio::GraphNodeKind::Sample &&
                recordedTakeNode->startSample == 96000 && recordedTakeNode->sampleFrames == 48000 &&
                recordedTakeNode->sampleChannels == 2 && !recordedTakeNode->samples.empty(),
            "application shell renders first-track recorded takes from project audio assets");
    appSession.setClipFades(appRecording.clipId, 1024, 2048);
    const auto fadedTakeGraph = lamusica::session::compileProjectAudioGraph(
        appSession.currentDocument()->manifest(), lamusica::session::MixerState{},
        {.projectRoot = appFirstTrackProject});
    const auto fadedTakeNode = std::ranges::find_if(
        fadedTakeGraph.nodes, [&appRecording](const lamusica::audio::GraphNode& node) {
            return node.id == "clip:" + appRecording.clipId;
        });
    require(fadedTakeNode != fadedTakeGraph.nodes.end() && fadedTakeNode->fadeInSamples == 1024 &&
                fadedTakeNode->fadeOutSamples == 2048 && appSession.status().canUndo,
            "application shell edits first-track take fades nondestructively");
    require(appSession.undoLastEdit(), "application shell undoes first-track take fade edits");
    appSession.setClipReversed(appRecording.clipId, true);
    const auto reversedTakeGraph = lamusica::session::compileProjectAudioGraph(
        appSession.currentDocument()->manifest(), lamusica::session::MixerState{},
        {.projectRoot = appFirstTrackProject});
    const auto reversedTakeNode = std::ranges::find_if(
        reversedTakeGraph.nodes, [&appRecording](const lamusica::audio::GraphNode& node) {
            return node.id == "clip:" + appRecording.clipId;
        });
    require(reversedTakeNode != reversedTakeGraph.nodes.end() && reversedTakeNode->reversed &&
                appSession.status().canUndo,
            "application shell reverses first-track takes nondestructively");
    require(appSession.undoLastEdit(), "application shell undoes first-track take reverse edits");
    const auto recordedTakes = appSession.recordedFirstTrackTakes();
    const auto lastTakeFadeRoute = appSession.routeMenuCommand("project.softenLastTakeFades");
    const auto lastTakeMuteRoute = appSession.routeMenuCommand("project.toggleLastTakeMute");
    const auto lastTakeReverseRoute = appSession.routeMenuCommand("project.toggleLastTakeReverse");
    const auto lastTakeTrimRoute = appSession.routeMenuCommand("project.trimLastTakeToLoop");
    const auto lastTakeDuplicateRoute =
        appSession.routeMenuCommand("project.duplicateLastTakeAtPlayhead");
    const auto lastTakeRemoveRoute = appSession.routeMenuCommand("project.removeLastTake");
    const auto recordedTrackMixRoute = appSession.routeMenuCommand("project.recordedTrackVolumeUp");
    require(recordedTakes.size() == 1 && recordedTakes.front().clipId == appRecording.clipId &&
                recordedTakes.front().assetId == appRecording.assetId &&
                recordedTakes.front().startSample == 96000 &&
                recordedTakes.front().frames == 48000 &&
                recordedTakes.front().fadeInSamples == 128 &&
                recordedTakes.front().fadeOutSamples == 128 && !recordedTakes.front().reversed &&
                !recordedTakes.front().muted && recordedTakes.front().mediaAvailable &&
                lastTakeFadeRoute.handled && lastTakeFadeRoute.enabled &&
                lastTakeMuteRoute.handled && lastTakeMuteRoute.enabled &&
                lastTakeReverseRoute.handled && lastTakeReverseRoute.enabled &&
                lastTakeTrimRoute.handled && lastTakeTrimRoute.enabled &&
                lastTakeDuplicateRoute.handled && lastTakeDuplicateRoute.enabled &&
                lastTakeRemoveRoute.handled && lastTakeRemoveRoute.enabled &&
                recordedTrackMixRoute.handled && recordedTrackMixRoute.enabled,
            "application shell summarizes and routes focused first-track recorded take edits");
    appSession.setClipTiming(appRecording.clipId, 120000, 24000, 12000);
    const auto retimedTakeGraph = lamusica::session::compileProjectAudioGraph(
        appSession.currentDocument()->manifest(), lamusica::session::MixerState{},
        {.projectRoot = appFirstTrackProject});
    const auto retimedTakeNode = std::ranges::find_if(
        retimedTakeGraph.nodes, [&appRecording](const lamusica::audio::GraphNode& node) {
            return node.id == "clip:" + appRecording.clipId;
        });
    require(retimedTakeNode != retimedTakeGraph.nodes.end() &&
                retimedTakeNode->startSample == 120000 && retimedTakeNode->lengthSamples == 24000 &&
                retimedTakeNode->sourceOffsetSamples == 12000 && appSession.status().canUndo,
            "application shell retimes first-track takes nondestructively");
    require(appSession.undoLastEdit(), "application shell undoes first-track take timing edits");
    const auto duplicatedTake =
        appSession.duplicateClip(appRecording.clipId, "recorded-take-1-copy", 144000);
    require(duplicatedTake.clipId == "recorded-take-1-copy" &&
                duplicatedTake.assetId == appRecording.assetId &&
                duplicatedTake.startSample == 144000 && appSession.status().clipCount == 4,
            "application shell duplicates first-track recorded takes nondestructively");
    const auto removedTakeDuplicate = appSession.removeClip("recorded-take-1-copy");
    require(
        removedTakeDuplicate.clipId == "recorded-take-1-copy" &&
            removedTakeDuplicate.assetId == appRecording.assetId &&
            appSession.status().clipCount == 3 && appSession.status().recordedTakeCount == 1,
        "application shell removes duplicated first-track recorded takes without deleting media");
    require(appSession.undoLastEdit() && appSession.status().clipCount == 4,
            "application shell undo restores first-track recorded take removal");
    require(appSession.undoLastEdit() && appSession.status().clipCount == 3,
            "application shell undo removes first-track recorded take duplicate");
    appSession.setFirstTrackTakeMuted(appRecording.clipId, true);
    require(appSession.status().mutedRecordedTakeCount == 1 && appSession.status().canUndo,
            "application shell mutes first-track recorded takes nondestructively");
    const auto mutedTakeGraph = lamusica::session::compileProjectAudioGraph(
        appSession.currentDocument()->manifest(), lamusica::session::MixerState{},
        {.projectRoot = appFirstTrackProject});
    require(std::ranges::none_of(mutedTakeGraph.nodes,
                                 [&appRecording](const lamusica::audio::GraphNode& node) {
                                     return node.id == "clip:" + appRecording.clipId;
                                 }),
            "application shell excludes muted first-track takes from the render graph");
    require(appSession.undoLastEdit() && appSession.status().mutedRecordedTakeCount == 0,
            "application shell undo restores first-track take mute state");
    const auto appImportSource =
        std::filesystem::temp_directory_path() / "lamusica-app-first-track-import-source.wav";
    lamusica::audio::AudioEngine appImportEngine{{}};
    auto appImportAudio = appImportEngine.renderSineOffline(24000, 523.25, 0.35F);
    lamusica::audio::writePcm16Wav(appImportSource, appImportAudio, 48000.0);
    const auto appImport = appSession.importAudioFileToFirstTrack(appImportSource, 24000);
    require(appImport.trackId == "imported-audio" && appImport.frames == 24000 &&
                appImport.channels == 2 && std::filesystem::exists(appImport.copiedPath) &&
                appSession.status().lastImportPath == appImport.copiedPath &&
                appSession.status().lastImportFrames == 24000 &&
                appSession.status().lastImportAssetId == appImport.assetId &&
                appSession.status().lastImportClipId == appImport.clipId &&
                appSession.status().importedAudioClipCount == 1 &&
                appSession.status().trackCount == 5 && appSession.status().clipCount == 4,
            "application shell imports an audio file into the first-track project");
    appSession.openProject(appFirstTrackProject);
    appSession.setClipTiming(appImport.clipId, 36000, 12000, 6000);
    const auto importedAudioGraph = lamusica::session::compileProjectAudioGraph(
        appSession.currentDocument()->manifest(), lamusica::session::MixerState{},
        {.projectRoot = appFirstTrackProject});
    const auto importedAudioNode = std::ranges::find_if(
        importedAudioGraph.nodes, [&appImport](const lamusica::audio::GraphNode& node) {
            return node.id == "clip:" + appImport.clipId;
        });
    require(importedAudioNode != importedAudioGraph.nodes.end() &&
                importedAudioNode->kind == lamusica::audio::GraphNodeKind::Sample &&
                importedAudioNode->startSample == 36000 &&
                importedAudioNode->lengthSamples == 12000 &&
                importedAudioNode->sourceOffsetSamples == 6000 &&
                importedAudioNode->sampleFrames == 24000 &&
                importedAudioNode->sampleChannels == 2 && !importedAudioNode->samples.empty(),
            "application shell renders retimed first-track imported audio from project assets");
    appSession.setClipMuted(appImport.clipId, true);
    const auto mutedImportGraph = lamusica::session::compileProjectAudioGraph(
        appSession.currentDocument()->manifest(), lamusica::session::MixerState{},
        {.projectRoot = appFirstTrackProject});
    require(std::ranges::none_of(mutedImportGraph.nodes,
                                 [&appImport](const lamusica::audio::GraphNode& node) {
                                     return node.id == "clip:" + appImport.clipId;
                                 }) &&
                appSession.status().canUndo,
            "application shell mutes imported first-track clips nondestructively");
    require(appSession.undoLastEdit(), "application shell undoes imported first-track clip mute");
    appSession.setClipReversed(appImport.clipId, true);
    const auto duplicatedImport =
        appSession.duplicateClip(appImport.clipId, "imported-audio-copy", 72000);
    const auto duplicatedImportGraph = lamusica::session::compileProjectAudioGraph(
        appSession.currentDocument()->manifest(), lamusica::session::MixerState{},
        {.projectRoot = appFirstTrackProject});
    const auto duplicatedImportNode = std::ranges::find_if(
        duplicatedImportGraph.nodes, [](const lamusica::audio::GraphNode& node) {
            return node.id == "clip:imported-audio-copy";
        });
    require(duplicatedImport.clipId == "imported-audio-copy" &&
                duplicatedImport.assetId == appImport.assetId &&
                duplicatedImportNode != duplicatedImportGraph.nodes.end() &&
                duplicatedImportNode->kind == lamusica::audio::GraphNodeKind::Sample &&
                duplicatedImportNode->startSample == 72000 &&
                duplicatedImportNode->lengthSamples == 12000 &&
                duplicatedImportNode->sourceOffsetSamples == 6000 && duplicatedImportNode->reversed,
            "application shell duplicates retimed and reversed first-track imported audio");
    const auto firstTrackClips = appSession.firstTrackClips();
    const auto lastImportFadeRoute = appSession.routeMenuCommand("project.softenLastImportFades");
    const auto lastImportMuteRoute = appSession.routeMenuCommand("project.toggleLastImportMute");
    const auto lastImportReverseRoute =
        appSession.routeMenuCommand("project.toggleLastImportReverse");
    const auto lastImportTrimRoute = appSession.routeMenuCommand("project.trimLastImportToLoop");
    const auto lastImportDuplicateRoute =
        appSession.routeMenuCommand("project.duplicateLastImportAtPlayhead");
    const auto lastImportRemoveRoute = appSession.routeMenuCommand("project.removeLastImport");
    const auto importedTrackMixRoute = appSession.routeMenuCommand("project.importedTrackVolumeUp");
    const auto listedDrumClip = std::ranges::find_if(
        firstTrackClips, [](const lamusica::session::FirstTrackClipSummary& clip) {
            return clip.clipId == "drum-loop";
        });
    const auto listedBassClip = std::ranges::find_if(
        firstTrackClips, [](const lamusica::session::FirstTrackClipSummary& clip) {
            return clip.clipId == "bass-pattern";
        });
    const auto listedRecordedClip = std::ranges::find_if(
        firstTrackClips, [&appRecording](const lamusica::session::FirstTrackClipSummary& clip) {
            return clip.clipId == appRecording.clipId;
        });
    const auto listedImportedClip = std::ranges::find_if(
        firstTrackClips, [&appImport](const lamusica::session::FirstTrackClipSummary& clip) {
            return clip.clipId == appImport.clipId;
        });
    const auto listedDuplicatedImport = std::ranges::find_if(
        firstTrackClips, [](const lamusica::session::FirstTrackClipSummary& clip) {
            return clip.clipId == "imported-audio-copy";
        });
    require(firstTrackClips.size() == 5 && listedDrumClip != firstTrackClips.end() &&
                listedDrumClip->type == lamusica::session::ClipType::Audio &&
                listedDrumClip->trackId == "drums" && listedBassClip != firstTrackClips.end() &&
                listedBassClip->type == lamusica::session::ClipType::Midi &&
                listedBassClip->trackId == "bass" && listedRecordedClip != firstTrackClips.end() &&
                listedRecordedClip->assetBacked && listedRecordedClip->mediaAvailable &&
                listedRecordedClip->fadeInSamples == 128 &&
                listedRecordedClip->fadeOutSamples == 128 &&
                listedImportedClip != firstTrackClips.end() && listedImportedClip->assetBacked &&
                listedImportedClip->mediaAvailable && listedImportedClip->startSample == 36000 &&
                listedImportedClip->lengthSamples == 12000 &&
                listedImportedClip->sourceOffsetSamples == 6000 && listedImportedClip->reversed &&
                listedDuplicatedImport != firstTrackClips.end() &&
                listedDuplicatedImport->assetId == appImport.assetId &&
                listedDuplicatedImport->startSample == 72000 &&
                listedDuplicatedImport->lengthSamples == 12000 &&
                listedDuplicatedImport->sourceOffsetSamples == 6000 &&
                listedDuplicatedImport->reversed && lastImportFadeRoute.handled &&
                lastImportFadeRoute.enabled && lastImportMuteRoute.handled &&
                lastImportMuteRoute.enabled && lastImportReverseRoute.handled &&
                lastImportReverseRoute.enabled && lastImportTrimRoute.handled &&
                lastImportTrimRoute.enabled && lastImportDuplicateRoute.handled &&
                lastImportDuplicateRoute.enabled && lastImportRemoveRoute.handled &&
                lastImportRemoveRoute.enabled && importedTrackMixRoute.handled &&
                importedTrackMixRoute.enabled,
            "application shell lists and routes focused first-track imported clip edits");
    bool rejectedAssetGraphWithoutProjectRoot = false;
    try {
        (void)lamusica::session::compileProjectAudioGraph(appSession.currentDocument()->manifest(),
                                                          lamusica::session::MixerState{});
    } catch (const std::exception&) {
        rejectedAssetGraphWithoutProjectRoot = true;
    }
    const auto appCustomExportPath =
        std::filesystem::temp_directory_path() / "lamusica-app-first-track-custom-export.wav";
    std::filesystem::remove(appCustomExportPath);
    const auto appCustomExport =
        appSession.exportCurrentMix({.outputPath = appCustomExportPath,
                                     .startSample = 24000,
                                     .frames = 24000,
                                     .sampleRate = 48000.0,
                                     .channels = 2,
                                     .bitDepth = lamusica::audio::ExportBitDepth::Pcm16,
                                     .ditherMode = lamusica::audio::DitherMode::Triangular,
                                     .normalizePeak = true,
                                     .normalizeTargetPeak = 0.98F});
    require(rejectedAssetGraphWithoutProjectRoot && appCustomExport.frames == 24000 &&
                std::filesystem::exists(appCustomExportPath) &&
                appSession.status().lastMixExportPath == appCustomExportPath,
            "application shell custom export resolves imported audio from the open project root");
    std::filesystem::remove(appCustomExportPath);
    const auto appFirstTrackPackage =
        std::filesystem::temp_directory_path() / "lamusica-app-first-track-package";
    std::filesystem::remove_all(appFirstTrackPackage);
    const auto appPackageExport = appSession.exportFirstTrackPackage(appFirstTrackPackage);
    const auto appPackageVerification = appSession.verifyFirstTrackPackage(appFirstTrackPackage);
    std::ifstream appPackageManifestFile{appPackageExport.manifestPath};
    const std::string appPackageManifestJson{std::istreambuf_iterator<char>{appPackageManifestFile},
                                             std::istreambuf_iterator<char>{}};
    require(
        appPackageExport.outputDirectory == appFirstTrackPackage &&
            appPackageVerification.packageDirectory == appFirstTrackPackage &&
            appPackageVerification.manifestPath == appPackageExport.manifestPath &&
            appPackageVerification.projectName == "App First Track" &&
            appPackageVerification.renderFrames == 192000 &&
            appPackageVerification.loopFrames == appSession.status().loopFrames &&
            appPackageVerification.stemCount == 4 && appPackageVerification.trackCount == 5 &&
            appPackageVerification.clipCount == 5 &&
            appPackageVerification.projectSnapshotPath ==
                appFirstTrackPackage / "project" / "project.json" &&
            appPackageVerification.projectAssetCount == 2 &&
            appPackageVerification.recordedTakeCount == 1 &&
            appPackageVerification.importedAudioClipCount == 2 &&
            appPackageVerification.projectSnapshotVerified &&
            appPackageExport.manifestPath == appFirstTrackPackage / "first-track-package.json" &&
            appPackageExport.mix.frames == 192000 &&
            appPackageExport.loop.frames == appSession.status().loopFrames &&
            appPackageExport.mix.peakAfterNormalization > 0.0F &&
            appPackageExport.loop.peakAfterNormalization > 0.0F &&
            std::filesystem::exists(appPackageExport.manifestPath) &&
            std::filesystem::exists(appFirstTrackPackage / "project" / "project.json") &&
            std::filesystem::exists(appFirstTrackPackage / "project" / "Assets" /
                                    (appRecording.assetId + ".wav")) &&
            std::filesystem::exists(appFirstTrackPackage / "project" / "Assets" /
                                    (appImport.assetId + ".wav")) &&
            !std::filesystem::exists(appFirstTrackPackage / "project" / "project.json.tmp") &&
            !std::filesystem::exists(appFirstTrackPackage / "first-track-package.json.tmp") &&
            appPackageManifestJson.find("\"kind\": \"lamusica.firstTrackPackage\"") !=
                std::string::npos &&
            appPackageManifestJson.find("\"projectName\": \"App First Track\"") !=
                std::string::npos &&
            appPackageManifestJson.find("\"projectSnapshotPath\": \"project/project.json\"") !=
                std::string::npos &&
            appPackageManifestJson.find("\"projectAssetCount\": 2") != std::string::npos &&
            appPackageManifestJson.find("\"projectAssets\": [") != std::string::npos &&
            appPackageManifestJson.find("\"id\": \"" + appRecording.assetId + "\"") !=
                std::string::npos &&
            appPackageManifestJson.find("\"id\": \"" + appImport.assetId + "\"") !=
                std::string::npos &&
            appPackageManifestJson.find("\"path\": \"first-track-mix.wav\"") != std::string::npos &&
            appPackageManifestJson.find("\"path\": \"first-track-intro-loop.wav\"") !=
                std::string::npos &&
            appPackageManifestJson.find("\"path\": \"stems/drums.wav\"") != std::string::npos &&
            appPackageManifestJson.find("\"checksum\": \"fnv1a64:") != std::string::npos &&
            appPackageManifestJson.find("\"recordedTakeCount\": 1") != std::string::npos &&
            appPackageManifestJson.find("\"importedAudioClipCount\": 2") != std::string::npos &&
            appPackageManifestJson.find("\"trackId\": \"imported-audio\"") != std::string::npos &&
            std::filesystem::exists(appPackageExport.mix.outputPath) &&
            std::filesystem::exists(appPackageExport.loop.outputPath) &&
            appPackageExport.stems.size() == 4 &&
            std::ranges::all_of(appPackageExport.stems,
                                [&appSession](const lamusica::session::StemExportResult& stem) {
                                    return stem.bounce.frames == appSession.status().renderFrames &&
                                           stem.bounce.peakAfterNormalization > 0.0F &&
                                           std::filesystem::exists(stem.bounce.outputPath);
                                }) &&
            appSession.status().lastPackageExportDirectory == appFirstTrackPackage &&
            appSession.status().lastPackageManifestPath == appPackageExport.manifestPath &&
            appSession.status().lastPackageVerified &&
            appSession.status().lastPackageVerifiedDirectory == appFirstTrackPackage &&
            appSession.status().lastPackageMixFrames == 192000 &&
            appSession.status().lastPackageLoopFrames == appSession.status().loopFrames &&
            appSession.status().lastPackageStemCount == 4,
        "application shell exports a complete first-track handoff package with a manifest");
    const auto movedAppFirstTrackPackage =
        std::filesystem::temp_directory_path() / "lamusica-app-first-track-package-moved";
    std::filesystem::remove_all(movedAppFirstTrackPackage);
    std::filesystem::copy(appFirstTrackPackage, movedAppFirstTrackPackage,
                          std::filesystem::copy_options::recursive);
    const auto inconsistentAppFirstTrackPackage =
        std::filesystem::temp_directory_path() / "lamusica-app-first-track-package-inconsistent";
    std::filesystem::remove_all(inconsistentAppFirstTrackPackage);
    std::filesystem::copy(appFirstTrackPackage, inconsistentAppFirstTrackPackage,
                          std::filesystem::copy_options::recursive);
    const auto unsafeAppFirstTrackPackage =
        std::filesystem::temp_directory_path() / "lamusica-app-first-track-package-unsafe";
    std::filesystem::remove_all(unsafeAppFirstTrackPackage);
    std::filesystem::copy(appFirstTrackPackage, unsafeAppFirstTrackPackage,
                          std::filesystem::copy_options::recursive);
    const auto missingSnapshotAssetPackage =
        std::filesystem::temp_directory_path() /
        "lamusica-app-first-track-package-missing-snapshot-asset";
    std::filesystem::remove_all(missingSnapshotAssetPackage);
    std::filesystem::copy(appFirstTrackPackage, missingSnapshotAssetPackage,
                          std::filesystem::copy_options::recursive);
    const auto inconsistentRecordedCountPackage =
        std::filesystem::temp_directory_path() /
        "lamusica-app-first-track-package-inconsistent-recorded-count";
    std::filesystem::remove_all(inconsistentRecordedCountPackage);
    std::filesystem::copy(appFirstTrackPackage, inconsistentRecordedCountPackage,
                          std::filesystem::copy_options::recursive);
    const auto inconsistentImportCountPackage =
        std::filesystem::temp_directory_path() /
        "lamusica-app-first-track-package-inconsistent-import-count";
    std::filesystem::remove_all(inconsistentImportCountPackage);
    std::filesystem::copy(appFirstTrackPackage, inconsistentImportCountPackage,
                          std::filesystem::copy_options::recursive);
    const auto tamperedSnapshotAssetPackage =
        std::filesystem::temp_directory_path() /
        "lamusica-app-first-track-package-tampered-snapshot-asset";
    std::filesystem::remove_all(tamperedSnapshotAssetPackage);
    std::filesystem::copy(appFirstTrackPackage, tamperedSnapshotAssetPackage,
                          std::filesystem::copy_options::recursive);
    const auto tamperedAppFirstTrackPackage =
        std::filesystem::temp_directory_path() / "lamusica-app-first-track-package-tampered";
    std::filesystem::remove_all(tamperedAppFirstTrackPackage);
    std::filesystem::copy(appFirstTrackPackage, tamperedAppFirstTrackPackage,
                          std::filesystem::copy_options::recursive);
    std::filesystem::remove_all(appFirstTrackPackage);
    const auto movedAppPackageVerification =
        appSession.verifyFirstTrackPackage(movedAppFirstTrackPackage);
    require(movedAppPackageVerification.packageDirectory == movedAppFirstTrackPackage &&
                movedAppPackageVerification.projectName == "App First Track" &&
                movedAppPackageVerification.renderFrames == 192000 &&
                movedAppPackageVerification.loopFrames == appSession.status().loopFrames &&
                movedAppPackageVerification.stemCount == 4 &&
                movedAppPackageVerification.trackCount == 5 &&
                movedAppPackageVerification.clipCount == 5 &&
                movedAppPackageVerification.projectSnapshotVerified &&
                movedAppPackageVerification.projectAssetCount == 2 &&
                movedAppPackageVerification.recordedTakeCount == 1 &&
                movedAppPackageVerification.importedAudioClipCount == 2 &&
                appSession.status().lastPackageVerifiedDirectory == movedAppFirstTrackPackage,
            "application shell verifies a first-track package after moving the handoff folder");
    std::filesystem::remove_all(movedAppFirstTrackPackage);

    writeTextFile(inconsistentAppFirstTrackPackage / "first-track-package.json",
                  replaceFirst(appPackageManifestJson, "\"renderFrames\": 192000",
                               "\"renderFrames\": 191999"));
    bool rejectedInconsistentPackage = false;
    try {
        (void)appSession.verifyFirstTrackPackage(inconsistentAppFirstTrackPackage);
    } catch (const std::exception&) {
        rejectedInconsistentPackage = true;
    }
    require(rejectedInconsistentPackage,
            "application shell rejects first-track packages with inconsistent render frames");
    std::filesystem::remove_all(inconsistentAppFirstTrackPackage);

    writeTextFile(unsafeAppFirstTrackPackage / "first-track-package.json",
                  replaceFirst(appPackageManifestJson, "\"path\": \"first-track-mix.wav\"",
                               "\"path\": \"../first-track-mix.wav\""));
    bool rejectedUnsafePackage = false;
    try {
        (void)appSession.verifyFirstTrackPackage(unsafeAppFirstTrackPackage);
    } catch (const std::exception&) {
        rejectedUnsafePackage = true;
    }
    require(rejectedUnsafePackage,
            "application shell rejects first-track packages with unsafe relative audio paths");
    std::filesystem::remove_all(unsafeAppFirstTrackPackage);

    std::filesystem::remove(missingSnapshotAssetPackage / "project" / "Assets" /
                            (appImport.assetId + ".wav"));
    bool rejectedMissingSnapshotAsset = false;
    try {
        (void)appSession.verifyFirstTrackPackage(missingSnapshotAssetPackage);
    } catch (const std::exception&) {
        rejectedMissingSnapshotAsset = true;
    }
    require(rejectedMissingSnapshotAsset,
            "application shell rejects first-track packages with missing project snapshot assets");
    std::filesystem::remove_all(missingSnapshotAssetPackage);

    writeTextFile(inconsistentRecordedCountPackage / "first-track-package.json",
                  replaceFirst(appPackageManifestJson, "\"recordedTakeCount\": 1",
                               "\"recordedTakeCount\": 0"));
    bool rejectedRecordedCountMismatch = false;
    try {
        (void)appSession.verifyFirstTrackPackage(inconsistentRecordedCountPackage);
    } catch (const std::exception&) {
        rejectedRecordedCountMismatch = true;
    }
    require(rejectedRecordedCountMismatch,
            "application shell rejects first-track packages with mismatched recorded take counts");
    std::filesystem::remove_all(inconsistentRecordedCountPackage);

    writeTextFile(inconsistentImportCountPackage / "first-track-package.json",
                  replaceFirst(appPackageManifestJson, "\"importedAudioClipCount\": 2",
                               "\"importedAudioClipCount\": 0"));
    bool rejectedImportCountMismatch = false;
    try {
        (void)appSession.verifyFirstTrackPackage(inconsistentImportCountPackage);
    } catch (const std::exception&) {
        rejectedImportCountMismatch = true;
    }
    require(rejectedImportCountMismatch,
            "application shell rejects first-track packages with mismatched imported audio counts");
    std::filesystem::remove_all(inconsistentImportCountPackage);

    auto tamperedSnapshotAsset = lamusica::audio::readPcm16Wav(
        tamperedSnapshotAssetPackage / "project" / "Assets" / (appImport.assetId + ".wav"));
    require(!tamperedSnapshotAsset.audio.interleavedSamples.empty(),
            "first-track package test snapshot asset contains samples");
    tamperedSnapshotAsset.audio.interleavedSamples.front() =
        tamperedSnapshotAsset.audio.interleavedSamples.front() == 0.25F ? 0.125F : 0.25F;
    lamusica::audio::writePcm16Wav(tamperedSnapshotAssetPackage / "project" / "Assets" /
                                       (appImport.assetId + ".wav"),
                                   tamperedSnapshotAsset.audio, tamperedSnapshotAsset.sampleRate);
    bool rejectedTamperedSnapshotAssetPackage = false;
    try {
        (void)appSession.verifyFirstTrackPackage(tamperedSnapshotAssetPackage);
    } catch (const std::exception&) {
        rejectedTamperedSnapshotAssetPackage = true;
    }
    require(
        rejectedTamperedSnapshotAssetPackage,
        "application shell rejects first-track packages with tampered snapshot asset checksums");
    std::filesystem::remove_all(tamperedSnapshotAssetPackage);

    auto tamperedMix =
        lamusica::audio::readPcm16Wav(tamperedAppFirstTrackPackage / "first-track-mix.wav");
    require(!tamperedMix.audio.interleavedSamples.empty(),
            "first-track package test mix contains samples");
    tamperedMix.audio.interleavedSamples.front() =
        tamperedMix.audio.interleavedSamples.front() == 0.25F ? 0.125F : 0.25F;
    lamusica::audio::writePcm16Wav(tamperedAppFirstTrackPackage / "first-track-mix.wav",
                                   tamperedMix.audio, tamperedMix.sampleRate);
    bool rejectedTamperedPackage = false;
    try {
        (void)appSession.verifyFirstTrackPackage(tamperedAppFirstTrackPackage);
    } catch (const std::exception&) {
        rejectedTamperedPackage = true;
    }
    require(rejectedTamperedPackage,
            "application shell rejects first-track packages with tampered audio checksums");
    std::filesystem::remove_all(tamperedAppFirstTrackPackage);

    std::filesystem::remove(appImport.copiedPath);
    appSession.openProject(appFirstTrackProject);
    const auto missingMediaVerification = appSession.verifyFirstTrackProject();
    const auto missingMediaRecordRoute =
        appSession.routeMenuCommand("project.recordFirstTrackTake");
    const auto missingMediaPackageRoute =
        appSession.routeMenuCommand("project.exportFirstTrackPackage");
    const auto missingMediaMixRoute = appSession.routeMenuCommand("project.exportMix");
    const auto missingMediaLoopRoute = appSession.routeMenuCommand("project.exportLoop");
    const auto missingMediaStemRoute = appSession.routeMenuCommand("project.exportStems");
    const auto missingMediaRelinkRoute =
        appSession.routeMenuCommand("project.relinkFirstTrackMedia");
    bool rejectedMissingMediaMixExport = false;
    try {
        (void)appSession.exportCurrentMix(std::filesystem::temp_directory_path() /
                                          "lamusica-missing-media-mix.wav");
    } catch (const std::exception&) {
        rejectedMissingMediaMixExport = true;
    }
    require(!appSession.status().mediaReady && !appSession.status().firstTrackReady &&
                !appSession.status().firstTrackEditable && !missingMediaVerification.mediaReady &&
                !missingMediaVerification.firstTrackReady &&
                !missingMediaVerification.firstTrackEditable &&
                missingMediaVerification.mediaError.find("Could not open WAV input file") !=
                    std::string::npos &&
                appSession.status().missingMediaAssetId == appImport.assetId &&
                !missingMediaRecordRoute.enabled && !missingMediaPackageRoute.enabled &&
                !missingMediaMixRoute.enabled && !missingMediaLoopRoute.enabled &&
                !missingMediaStemRoute.enabled && missingMediaRelinkRoute.enabled &&
                rejectedMissingMediaMixExport,
            "application shell reports missing first-track media before record or export");
    const auto relinkedMedia =
        appSession.relinkFirstTrackAudioAsset(appImport.assetId, appImportSource);
    const auto relinkedMediaVerification = appSession.verifyFirstTrackProject();
    const auto relinkedMediaPackageRoute =
        appSession.routeMenuCommand("project.exportFirstTrackPackage");
    const auto relinkedMediaRelinkRoute =
        appSession.routeMenuCommand("project.relinkFirstTrackMedia");
    require(relinkedMedia.assetId == appImport.assetId && relinkedMedia.frames == 24000 &&
                relinkedMedia.channels == 2 && std::filesystem::exists(relinkedMedia.copiedPath) &&
                relinkedMedia.mediaReady && appSession.status().mediaReady &&
                appSession.status().firstTrackReady && appSession.status().firstTrackEditable &&
                appSession.status().missingMediaAssetId.empty() &&
                relinkedMediaVerification.mediaReady && relinkedMediaVerification.firstTrackReady &&
                relinkedMediaVerification.firstTrackEditable && relinkedMediaPackageRoute.enabled &&
                !relinkedMediaRelinkRoute.enabled,
            "application shell relinks missing first-track media and restores readiness");

    std::filesystem::remove(appImportSource);
    std::filesystem::remove_all(appFirstTrackPackage);
    std::filesystem::remove_all(appFirstTrackProject);
    lamusica::session::ApplicationPreferences appPreferences{
        .audioDeviceId = "built-in-output",
        .enabledMidiInputIds = {"keyboard"},
        .pluginSearchPaths = {"/Library/Audio/Plug-Ins/VST3"},
        .mcpEnabled = true,
        .allowMcpProjectMutation = true,
        .keyboardShortcuts = {{.command = "transport.play", .keyEquivalent = "space"}},
        .allowUserFolderScanning = true,
        .shareDiagnostics = false,
        .diagnosticsConsent = lamusica::session::DiagnosticsConsent::Declined};
    appSession.setPreferences(appPreferences);
    require(appSession.preferences().audioDeviceId == "built-in-output" &&
                appSession.preferences().enabledMidiInputIds.front() == "keyboard" &&
                appSession.preferences().pluginSearchPaths.front().find("VST3") !=
                    std::string::npos &&
                appSession.preferences().mcpEnabled &&
                appSession.preferences().allowMcpProjectMutation &&
                appSession.preferences().allowUserFolderScanning,
            "application preferences store audio MIDI plugin MCP and privacy settings");
    appSession.setKeyboardShortcut("transport.play", "p");
    require(appSession.preferences().keyboardShortcuts.size() == 1 &&
                appSession.preferences().keyboardShortcuts.front().keyEquivalent == "p",
            "application preferences update keyboard shortcuts by command");
    bool rejectedUndecidedDiagnostics = false;
    try {
        auto preferences = appSession.preferences();
        preferences.diagnosticsConsent = lamusica::session::DiagnosticsConsent::Undecided;
        preferences.shareDiagnostics = true;
        appSession.setPreferences(preferences);
    } catch (const std::exception&) {
        rejectedUndecidedDiagnostics = true;
    }
    bool rejectedDeclinedTelemetry = false;
    try {
        auto preferences = appSession.preferences();
        preferences.diagnosticsConsent = lamusica::session::DiagnosticsConsent::Declined;
        preferences.shareDiagnostics = false;
        preferences.telemetryEnabled = true;
        appSession.setPreferences(preferences);
    } catch (const std::exception&) {
        rejectedDeclinedTelemetry = true;
    }
    bool rejectedPlaintextDiagnosticsEndpoint = false;
    try {
        auto preferences = appSession.preferences();
        preferences.diagnosticsConsent = lamusica::session::DiagnosticsConsent::Granted;
        preferences.shareDiagnostics = true;
        preferences.telemetryEnabled = false;
        preferences.diagnosticsEndpoint = "http://example.test/crash";
        appSession.setPreferences(preferences);
    } catch (const std::exception&) {
        rejectedPlaintextDiagnosticsEndpoint = true;
    }
    auto grantedDiagnostics = appSession.preferences();
    grantedDiagnostics.diagnosticsConsent = lamusica::session::DiagnosticsConsent::Granted;
    grantedDiagnostics.shareDiagnostics = true;
    grantedDiagnostics.telemetryEnabled = false;
    grantedDiagnostics.diagnosticsEndpoint = "LAMUSICA_DIAGNOSTICS_ENDPOINT";
    appSession.setPreferences(grantedDiagnostics);
    const auto diagnosticsPayload = lamusica::session::makeDiagnosticsPayload(
        {.applicationName = "LaMusica",
         .version = "0.1.0",
         .gitCommit = "test",
         .osVersion = "macOS",
         .signalNumber = 11,
         .rawBacktrace =
             "frame /Users/alex/Projects/Secret Song.Project.lamusica/Audio/Take.wav\n"
             "project Secret Song\nsignal=11\n",
         .projectName = "Secret Song"});
    require(rejectedUndecidedDiagnostics && rejectedDeclinedTelemetry &&
                rejectedPlaintextDiagnosticsEndpoint &&
                lamusica::session::diagnosticsUploadPermitted(
                    appSession.preferences().diagnosticsConsent,
                    appSession.preferences().shareDiagnostics) &&
                diagnosticsPayload.json.find("/Users/alex") == std::string::npos &&
                diagnosticsPayload.json.find("Secret Song") == std::string::npos &&
                diagnosticsPayload.json.find(".Project.lamusica") == std::string::npos &&
                diagnosticsPayload.json.find("<path>") != std::string::npos,
            "diagnostics consent gates upload and scrubbed payload removes project PII");
    const auto deniedDiagnosticsUpload = lamusica::session::makeDiagnosticsUploadRequest(
        lamusica::session::DiagnosticsConsent::Declined, true,
        "https://diagnostics.lamusica.dev/v1/crash", {}, {.applicationName = "LaMusica",
                                                          .version = "0.1.0",
                                                          .gitCommit = "test",
                                                          .osVersion = "macOS",
                                                          .signalNumber = 11,
                                                          .rawBacktrace =
                                                              "/Users/alex/private.Project."
                                                              "lamusica/project.json",
                                                          .projectName = "private"});
    require(!deniedDiagnosticsUpload.permitted && deniedDiagnosticsUpload.endpoint.empty() &&
                deniedDiagnosticsUpload.payload.json.empty(),
            "diagnostics upload preparation must not resolve endpoint or payload without consent");
    const auto relativeBundleScrubbed = lamusica::session::scrubDiagnosticsText(
        "frame Secret Relative.Project.lamusica/Audio/take.wav\n"
        "frame Users/alex/Library/Logs/LaMusica.crashlog\n");
    require(relativeBundleScrubbed.find(".Project.lamusica") == std::string::npos &&
                relativeBundleScrubbed.find("Users/alex") == std::string::npos &&
                relativeBundleScrubbed.find("<path>") != std::string::npos,
            "diagnostics scrubber removes relative project bundles and username paths");
    const auto appShellRoutingProject =
        std::filesystem::temp_directory_path() / "lamusica-app-shell-routing.Project.lamusica";
    std::filesystem::remove_all(appShellRoutingProject);
    appSession.createProject(appShellRoutingProject, "App Routing");
    require(appSession.focusedPanel() == lamusica::session::ApplicationPanel::Timeline,
            "application shell defaults keyboard focus to timeline");
    appSession.focusPanel(lamusica::session::ApplicationPanel::Mixer);
    const auto mixerCutRoute = appSession.routeMenuCommand("edit.cut");
    require(mixerCutRoute.handled && mixerCutRoute.enabled &&
                mixerCutRoute.panel == lamusica::session::ApplicationPanel::Mixer,
            "application shell routes edit menu commands to focused primary panel");
    const auto browserRoute = appSession.routeMenuCommand("view.browser");
    require(browserRoute.handled && browserRoute.enabled &&
                appSession.focusedPanel() == lamusica::session::ApplicationPanel::Browser,
            "application shell view commands update focused primary panel");
    const auto playRoute = appSession.routeMenuCommand("transport.play");
    require(playRoute.handled && playRoute.enabled &&
                appSession.focusedPanel() == lamusica::session::ApplicationPanel::Transport,
            "application shell transport commands route to transport panel");
    const auto unreadyImportAudioRoute = appSession.routeMenuCommand("project.importAudio");
    const auto unreadyPackageRoute = appSession.routeMenuCommand("project.exportFirstTrackPackage");
    const auto unreadyVerifyRoute = appSession.routeMenuCommand("project.verifyFirstTrackProject");
    require(unreadyVerifyRoute.handled && unreadyVerifyRoute.enabled &&
                unreadyImportAudioRoute.handled && !unreadyImportAudioRoute.enabled &&
                unreadyPackageRoute.handled && !unreadyPackageRoute.enabled,
            "application shell disables first-track creation commands until project is ready");
    appSession.closeProject();
    std::filesystem::remove_all(appShellRoutingProject);
    const auto nonStarterIntroProject =
        std::filesystem::temp_directory_path() / "lamusica-non-starter-intro.Project.lamusica";
    std::filesystem::remove_all(nonStarterIntroProject);
    lamusica::session::ProjectManifest nonStarterIntroManifest;
    nonStarterIntroManifest.name = "Non Starter Intro";
    nonStarterIntroManifest.markers = {
        {.id = "intro-marker", .name = "Intro", .samplePosition = 0}};
    nonStarterIntroManifest.tracks = {
        {.id = "audio-track", .name = "Audio", .type = lamusica::session::TrackType::Audio}};
    nonStarterIntroManifest.clips = {{.id = "audio-clip",
                                      .trackId = "audio-track",
                                      .type = lamusica::session::ClipType::Audio,
                                      .lengthSamples = 48000}};
    (void)lamusica::session::ProjectDocument::create(nonStarterIntroProject,
                                                     nonStarterIntroManifest);
    appSession.openProject(nonStarterIntroProject);
    const auto nonStarterLoopIntroRoute = appSession.routeMenuCommand("project.loopIntro");
    bool rejectedNonStarterIntroLoop = false;
    try {
        appSession.setFirstTrackLoopToIntro();
    } catch (const std::exception&) {
        rejectedNonStarterIntroLoop = true;
    }
    require(nonStarterLoopIntroRoute.handled && !nonStarterLoopIntroRoute.enabled &&
                rejectedNonStarterIntroLoop,
            "application shell disables first-track intro loop repair for non-starter projects");
    appSession.closeProject();
    std::filesystem::remove_all(nonStarterIntroProject);
    appSession.createFirstTrackProject(appShellRoutingProject, "App Routing");
    const auto exportMixRoute = appSession.routeMenuCommand("project.exportMix");
    const auto exportLoopRoute = appSession.routeMenuCommand("project.exportLoop");
    const auto exportStemsRoute = appSession.routeMenuCommand("project.exportStems");
    const auto importAudioRoute = appSession.routeMenuCommand("project.importAudio");
    const auto recordRoute = appSession.routeMenuCommand("project.recordFirstTrackTake");
    const auto packageRoute = appSession.routeMenuCommand("project.exportFirstTrackPackage");
    const auto verifyPackageRoute = appSession.routeMenuCommand("project.verifyFirstTrackPackage");
    const auto loopIntroRoute = appSession.routeMenuCommand("project.loopIntro");
    const auto arrangeRoute = appSession.routeMenuCommand("project.extendIntroToVerse");
    const auto transposeRoute = appSession.routeMenuCommand("project.transposeBassUpOctave");
    const auto mixRoute = appSession.routeMenuCommand("project.drumsGainDown");
    const auto emptyRecordedTrackRoute =
        appSession.routeMenuCommand("project.recordedTrackVolumeUp");
    const auto emptyImportedTrackRoute =
        appSession.routeMenuCommand("project.importedTrackVolumeUp");
    const auto emptyLastTakeRoute = appSession.routeMenuCommand("project.toggleLastTakeMute");
    const auto emptyLastImportRoute = appSession.routeMenuCommand("project.toggleLastImportMute");
    const auto initialUndoRoute = appSession.routeMenuCommand("edit.undo");
    const auto initialRedoRoute = appSession.routeMenuCommand("edit.redo");
    require(exportMixRoute.handled && exportMixRoute.enabled && exportLoopRoute.handled &&
                exportLoopRoute.enabled && exportStemsRoute.handled && exportStemsRoute.enabled &&
                importAudioRoute.handled && importAudioRoute.enabled && recordRoute.handled &&
                recordRoute.enabled && packageRoute.handled && packageRoute.enabled &&
                verifyPackageRoute.handled && verifyPackageRoute.enabled &&
                loopIntroRoute.handled && loopIntroRoute.enabled && arrangeRoute.handled &&
                arrangeRoute.enabled && transposeRoute.handled && transposeRoute.enabled &&
                mixRoute.handled && mixRoute.enabled && emptyRecordedTrackRoute.handled &&
                !emptyRecordedTrackRoute.enabled && emptyImportedTrackRoute.handled &&
                !emptyImportedTrackRoute.enabled && emptyLastTakeRoute.handled &&
                !emptyLastTakeRoute.enabled && emptyLastImportRoute.handled &&
                !emptyLastImportRoute.enabled && initialUndoRoute.handled &&
                !initialUndoRoute.enabled && initialRedoRoute.handled && !initialRedoRoute.enabled,
            "application shell routes first-track project commands when the starter is ready");
    appSession.clearLoopRegion();
    const auto clearedLoopExportRoute = appSession.routeMenuCommand("project.exportLoop");
    const auto clearedPackageRoute = appSession.routeMenuCommand("project.exportFirstTrackPackage");
    const auto restoreLoopRoute = appSession.routeMenuCommand("project.loopIntro");
    const auto clearedImportAudioRoute = appSession.routeMenuCommand("project.importAudio");
    const auto clearedRecordRoute = appSession.routeMenuCommand("project.recordFirstTrackTake");
    const auto clearedArrangeRoute = appSession.routeMenuCommand("project.extendIntroToVerse");
    const auto clearedTransposeRoute = appSession.routeMenuCommand("project.transposeBassUpOctave");
    const auto clearedMixRoute = appSession.routeMenuCommand("project.drumsGainDown");
    const auto clearedUndoRoute = appSession.routeMenuCommand("edit.undo");
    require(!clearedLoopExportRoute.enabled && !clearedPackageRoute.enabled &&
                restoreLoopRoute.enabled && clearedImportAudioRoute.enabled &&
                clearedRecordRoute.enabled && clearedArrangeRoute.enabled &&
                clearedTransposeRoute.enabled && clearedMixRoute.enabled &&
                clearedUndoRoute.handled && clearedUndoRoute.enabled,
            "application shell disables loop-dependent exports while keeping first-track edits "
            "available");
    const auto looplessRecording = appSession.recordFirstTrackTake(12000);
    require(looplessRecording.committed.frames == 12000 &&
                appSession.status().recordedTakeCount == 1 && !appSession.status().loopEnabled &&
                appSession.status().firstTrackEditable && !appSession.status().firstTrackReady,
            "application shell records first-track takes while the loop is cleared");
    appSession.setFirstTrackLoopToIntro();
    bool rejectedUnsafeMcpPreferences = false;
    try {
        appSession.setPreferences({.mcpEnabled = false, .allowMcpProjectMutation = true});
    } catch (const std::exception&) {
        rejectedUnsafeMcpPreferences = true;
    }
    require(rejectedUnsafeMcpPreferences,
            "application preferences reject MCP mutation when MCP is disabled");
    appSession.closeProject();
    require(!appSession.routeMenuCommand("project.save").enabled,
            "application shell disables project menu commands without an open project");
    std::filesystem::remove_all(appShellRoutingProject);

    manifest.clips.push_back({.id = "clip-a",
                              .trackId = "track-1",
                              .type = lamusica::session::ClipType::Audio,
                              .startSample = 48100,
                              .lengthSamples = 12000});
    lamusica::session::SnapSettings snap{.enabled = true,
                                         .sampleGrid = 0,
                                         .beatGridSamples = 24000,
                                         .snapToClips = true,
                                         .snapToMarkers = true,
                                         .snapToTransients = true,
                                         .transientSamples = {36200}};
    require(lamusica::session::snapSample(47900, snap, manifest) == 48000,
            "timeline snap prefers nearby marker");
    require(lamusica::session::snapSample(36180, snap, manifest) == 36200,
            "timeline snap can target transient candidates");
    const auto clips = lamusica::session::clipsOnTrack(manifest, "track-1");
    require(clips.size() == 2 &&
                std::ranges::any_of(
                    clips, [](const lamusica::session::Clip& clip) { return clip.id == "clip-a"; }),
            "timeline filters clips by track");
    lamusica::session::TimelineOrganization timelineOrganization;
    lamusica::session::addColorLabel(timelineOrganization,
                                     {.id = "blue", .name = "Blue", .color = "#3366ff"});
    lamusica::session::addTrackFolder(timelineOrganization, manifest,
                                      {.id = "folder-rhythm",
                                       .name = "Rhythm",
                                       .trackIds = {"track-1", "master"},
                                       .colorLabelId = "blue",
                                       .collapsed = true});
    const auto folderTracks =
        lamusica::session::tracksInFolder(manifest, timelineOrganization, "folder-rhythm");
    require(folderTracks.size() == 2 && folderTracks.front().id == "track-1",
            "timeline track folders preserve explicit track order");
    lamusica::session::addArrangerSection(timelineOrganization,
                                          {.id = "section-verse",
                                           .name = "Verse",
                                           .range = {.startSample = 24000, .endSample = 96000},
                                           .colorLabelId = "blue"});
    const auto visibleSections = lamusica::session::sectionsIntersecting(
        timelineOrganization, {.startSample = 48000, .endSample = 72000});
    require(visibleSections.size() == 1 && visibleSections.front().id == "section-verse",
            "timeline arranger sections intersect visible ranges");
    lamusica::session::TimelineViewState timelineView;
    timelineView.visibleRange = {.startSample = 0, .endSample = 96000};
    lamusica::session::setTimelinePlayhead(timelineView, -120);
    require(timelineView.playheadSample == 0, "timeline playhead clamps to valid samples");
    lamusica::session::setTimelineLoopRegion(timelineView,
                                             lamusica::session::TimelineRange{24000, 72000});
    require(timelineView.loopRegion.has_value() && timelineView.loopRegion->length() == 48000,
            "timeline loop region stores non-empty sample range");
    const auto normalizedTimelineRange = lamusica::session::normalizedRange(96000, 24000);
    require(normalizedTimelineRange.startSample == 24000 &&
                normalizedTimelineRange.endSample == 96000,
            "timeline range selections normalize drag direction");
    lamusica::session::setTimelineSelection(timelineView,
                                            {.trackIds = {"track-1"},
                                             .clipIds = {"clip-round-trip", "clip-a"},
                                             .range = normalizedTimelineRange},
                                            manifest);
    require(!timelineView.selection.empty() && timelineView.selection.clipIds.size() == 2,
            "timeline selection accepts existing tracks, clips, and ranges");
    bool rejectedMissingSelectionClip = false;
    try {
        lamusica::session::setTimelineSelection(timelineView, {.clipIds = {"missing-clip"}},
                                                manifest);
    } catch (const std::exception&) {
        rejectedMissingSelectionClip = true;
    }
    require(rejectedMissingSelectionClip, "timeline selection rejects missing clips");
    lamusica::session::zoomTimelineAroundSample(timelineView, 2.0, 48000);
    require(timelineView.visibleRange.length() == 48000 &&
                timelineView.visibleRange.contains(48000),
            "timeline zoom keeps anchor sample visible with stable range dimensions");
    lamusica::session::setTimelinePlayhead(timelineView, 48000);
    const auto timelineLayout = lamusica::session::buildTimelineLayout(
        manifest, timelineView,
        {.viewportWidth = 1000.0, .headerWidth = 200.0, .rulerHeight = 24.0, .trackHeight = 40.0});
    require(timelineLayout.trackHeaders.size() == manifest.tracks.size() &&
                timelineLayout.lanes.size() == manifest.tracks.size() &&
                timelineLayout.trackHeaders.front().bounds.width == 200.0 &&
                timelineLayout.lanes.front().bounds.height == 40.0,
            "timeline layout creates stable track headers and lanes");
    require(std::ranges::any_of(timelineLayout.clips,
                                [](const lamusica::session::TimelineClipLayout& clip) {
                                    return clip.clipId == "clip-a" && clip.selected &&
                                           clip.bounds.width > 1.0;
                                }),
            "timeline layout positions visible selected clips");
    require(timelineLayout.loopRegion.has_value() && timelineLayout.loopRegion->x == 200.0 &&
                timelineLayout.loopRegion->width == 800.0,
            "timeline layout maps visible loop region to content bounds");
    require(std::ranges::any_of(timelineLayout.markers,
                                [](const lamusica::session::TimelineMarkerLayout& marker) {
                                    return marker.markerId == "marker-1";
                                }) &&
                std::ranges::any_of(timelineLayout.rulerTicks,
                                    [](const lamusica::session::TimelineRulerTick& tick) {
                                        return tick.samplePosition == 48000;
                                    }),
            "timeline layout includes markers and ruler ticks");
    require(timelineLayout.playheadX > 200.0 && timelineLayout.playheadX < 1000.0,
            "timeline layout maps playhead inside visible content");
    bool rejectedInvalidLoopRegion = false;
    try {
        lamusica::session::setTimelineLoopRegion(timelineView,
                                                 lamusica::session::TimelineRange{72000, 72000});
    } catch (const std::exception&) {
        rejectedInvalidLoopRegion = true;
    }
    require(rejectedInvalidLoopRegion, "timeline loop region rejects empty ranges");
    bool rejectedMissingFolderTrack = false;
    try {
        lamusica::session::addTrackFolder(
            timelineOrganization, manifest,
            {.id = "folder-bad", .name = "Bad", .trackIds = {"missing-track"}});
    } catch (const std::exception&) {
        rejectedMissingFolderTrack = true;
    }
    require(rejectedMissingFolderTrack, "timeline folders reject missing track references");

    lamusica::session::MidiDeviceConfiguration midiDevices;
    lamusica::session::mergeMidiDevice(
        midiDevices, {.id = "keyboard",
                      .name = "Keyboard",
                      .direction = lamusica::session::MidiDeviceDirection::Input});
    lamusica::session::mergeMidiDevice(
        midiDevices, {.id = "synth",
                      .name = "Synth",
                      .direction = lamusica::session::MidiDeviceDirection::Output});
    lamusica::session::setMidiInputEnabled(midiDevices, "keyboard", true);
    require(lamusica::session::enabledMidiInputs(midiDevices).size() == 1,
            "MIDI device config enables input devices");
    lamusica::session::setMidiOutputRoute(
        midiDevices, {.trackId = "track-1", .deviceId = "synth", .channel = 2});
    require(lamusica::session::findMidiOutputRoute(midiDevices, "track-1")->channel == 2,
            "MIDI device config stores output routes");
    lamusica::session::setMidiClockOptions(
        midiDevices, {.mode = lamusica::session::MidiClockMode::Send, .deviceId = "synth"});
    require(midiDevices.clock.mode == lamusica::session::MidiClockMode::Send,
            "MIDI device config stores clock options");
    bool rejectedMidiInputDirection = false;
    try {
        lamusica::session::setMidiInputEnabled(midiDevices, "synth", true);
    } catch (const std::exception&) {
        rejectedMidiInputDirection = true;
    }
    require(rejectedMidiInputDirection, "MIDI device config rejects non-input enablement");
    bool rejectedMidiClockDirection = false;
    try {
        lamusica::session::setMidiClockOptions(
            midiDevices, {.mode = lamusica::session::MidiClockMode::Receive, .deviceId = "synth"});
    } catch (const std::exception&) {
        rejectedMidiClockDirection = true;
    }
    require(rejectedMidiClockDirection, "MIDI device config rejects wrong clock direction");

    lamusica::session::MidiClipData midiClip{.clipId = "midi-clip-1"};
    midiClip.notes.push_back(
        {.id = "note-b", .startSample = 19000, .lengthSamples = 1000, .pitch = 64, .velocity = 80});
    midiClip.notes.push_back(
        {.id = "note-a", .startSample = 1000, .lengthSamples = 1000, .pitch = 60, .velocity = 100});
    lamusica::session::quantizeNotes(midiClip, {.gridSamples = 24000, .strength = 1.0F});
    require(midiClip.notes.front().startSample == 24000, "MIDI quantize moves note to grid");
    lamusica::session::transposeNotes(midiClip, 12);
    require(midiClip.notes.front().pitch == 76, "MIDI transpose changes pitch");
    lamusica::session::transformVelocity(midiClip, {.add = -10, .scale = 0.5F});
    require(midiClip.notes.front().velocity == 30, "MIDI velocity transform clamps and scales");
    auto humanizedA = midiClip;
    auto humanizedB = midiClip;
    lamusica::session::humanizeNotes(
        humanizedA, {.maxTimingOffsetSamples = 120, .maxVelocityOffset = 8, .seed = 42});
    lamusica::session::humanizeNotes(
        humanizedB, {.maxTimingOffsetSamples = 120, .maxVelocityOffset = 8, .seed = 42});
    require(humanizedA.notes.front().startSample == humanizedB.notes.front().startSample &&
                humanizedA.notes.front().velocity == humanizedB.notes.front().velocity,
            "MIDI humanize is deterministic for a seed");
    require(std::abs(humanizedA.notes.front().startSample - midiClip.notes.front().startSample) <=
                    120 &&
                std::abs(static_cast<int>(humanizedA.notes.front().velocity) -
                         static_cast<int>(midiClip.notes.front().velocity)) <= 8,
            "MIDI humanize respects timing and velocity bounds");
    lamusica::session::setNoteLengths(midiClip, 4800);
    require(midiClip.notes.front().lengthSamples == 4800, "MIDI note length transform applies");
    const auto musicalTiming = lamusica::session::midiNoteToMusicalTiming(
        {.id = "musical-note", .startSample = 24000, .lengthSamples = 12000},
        {.sampleRate = 48000.0, .tempoBpm = 120.0});
    require(musicalTiming.startPpq == 1.0 && musicalTiming.lengthPpq == 0.5,
            "MIDI note timing converts samples to PPQ");
    const auto sampleTimedNote = lamusica::session::midiNoteFromMusicalTiming(
        {.id = "sample-note", .pitch = 67}, {.startPpq = 2.0, .lengthPpq = 0.25},
        {.sampleRate = 44100.0, .tempoBpm = 60.0});
    require(sampleTimedNote.startSample == 88200 && sampleTimedNote.lengthSamples == 11025,
            "MIDI note timing converts PPQ to sample positions");
    const auto orderedNotes = lamusica::session::notesInPlaybackOrder(midiClip);
    require(orderedNotes.front().id == "note-a", "MIDI playback order sorts by sample");
    midiClip.controlChanges.push_back(
        {.samplePosition = 1200, .controller = 1, .value = 64, .channel = 1});
    midiClip.pitchBends.push_back({.samplePosition = 1300, .value = 2048, .channel = 1});
    midiClip.aftertouch.push_back(
        {.samplePosition = 1400, .pressure = 88, .channel = 1, .pitch = 72, .polyphonic = true});
    midiClip.programChanges.push_back({.samplePosition = 900, .program = 12, .channel = 1});
    midiClip.metadata.push_back({.key = "source", .value = "fixture"});
    const auto playbackEvents = lamusica::session::midiEventsInPlaybackOrder(midiClip);
    require(std::ranges::is_sorted(playbackEvents, {},
                                   &lamusica::session::MidiPlaybackEvent::samplePosition),
            "MIDI playback events are sorted by sample");
    require(std::ranges::any_of(playbackEvents,
                                [](const lamusica::session::MidiPlaybackEvent& event) {
                                    return event.type ==
                                               lamusica::session::MidiEventType::ProgramChange &&
                                           event.samplePosition == 900 && event.data1 == 12;
                                }),
            "MIDI playback events include program changes");
    require(std::ranges::any_of(playbackEvents,
                                [](const lamusica::session::MidiPlaybackEvent& event) {
                                    return event.type ==
                                               lamusica::session::MidiEventType::Aftertouch &&
                                           event.data1 == 72 && event.data2 == 88;
                                }),
            "MIDI playback events include aftertouch");
    const auto rangedMidiEvents = lamusica::session::midiEventsInSampleRange(midiClip, 1000, 1400);
    require(!rangedMidiEvents.empty() &&
                std::ranges::all_of(rangedMidiEvents,
                                    [](const lamusica::session::MidiPlaybackEvent& event) {
                                        return event.samplePosition >= 1000 &&
                                               event.samplePosition < 1400;
                                    }) &&
                std::ranges::none_of(rangedMidiEvents,
                                     [](const lamusica::session::MidiPlaybackEvent& event) {
                                         return event.type ==
                                                lamusica::session::MidiEventType::ProgramChange;
                                     }),
            "MIDI playback range filters events to the transport render block");
    bool rejectedBadMidiPlaybackRange = false;
    try {
        (void)lamusica::session::midiEventsInSampleRange(midiClip, 2000, 1000);
    } catch (const std::exception&) {
        rejectedBadMidiPlaybackRange = true;
    }
    require(rejectedBadMidiPlaybackRange, "MIDI playback range rejects unordered ranges");
    const auto ccLaneEvents = lamusica::session::controllerLaneEvents(
        midiClip, {.type = lamusica::session::ControllerLaneType::ControlChange, .controller = 1});
    require(ccLaneEvents.size() == 1 && ccLaneEvents.front().value == 64,
            "piano roll controller lane filters CC events");
    const auto aftertouchLaneEvents = lamusica::session::controllerLaneEvents(
        midiClip, {.type = lamusica::session::ControllerLaneType::Aftertouch});
    require(aftertouchLaneEvents.size() == 1 && aftertouchLaneEvents.front().controller == 72,
            "piano roll aftertouch lane renders independently from notes");
    lamusica::session::MidiClipData recordedMidi{.clipId = "recorded-midi"};
    recordedMidi.notes.push_back(
        {.id = "existing", .startSample = 1000, .lengthSamples = 5000, .pitch = 60});
    lamusica::session::commitMidiRecording(recordedMidi,
                                           {.mode = lamusica::session::MidiRecordingMode::Overdub,
                                            .notes = {{.id = "overdub",
                                                       .noteOnSample = 1234,
                                                       .noteOffSample = 2234,
                                                       .pitch = 64,
                                                       .velocity = 90}}});
    require(recordedMidi.notes.size() == 2 &&
                std::ranges::any_of(recordedMidi.notes,
                                    [](const lamusica::session::MidiNote& note) {
                                        return note.id == "overdub" && note.startSample == 1234 &&
                                               note.lengthSamples == 1000;
                                    }),
            "MIDI overdub recording preserves sample timing");
    lamusica::session::commitMidiRecording(recordedMidi,
                                           {.mode = lamusica::session::MidiRecordingMode::Replace,
                                            .replaceStartSample = 0,
                                            .replaceEndSample = 3000,
                                            .quantizeOnCommit = true,
                                            .quantize = {.gridSamples = 1000, .strength = 1.0F},
                                            .notes = {{.id = "replace",
                                                       .noteOnSample = 1499,
                                                       .noteOffSample = 2499,
                                                       .pitch = 67,
                                                       .velocity = 100}}});
    require(recordedMidi.notes.size() == 1 && recordedMidi.notes.front().id == "replace" &&
                recordedMidi.notes.front().startSample == 1000,
            "MIDI replace recording removes overlapped notes and quantizes committed timing");
    bool rejectedBadMidiReplace = false;
    try {
        lamusica::session::commitMidiRecording(
            recordedMidi, {.mode = lamusica::session::MidiRecordingMode::Replace,
                           .replaceStartSample = 100,
                           .replaceEndSample = 100,
                           .notes = {{.id = "bad", .noteOnSample = 100, .noteOffSample = 200}}});
    } catch (const std::exception&) {
        rejectedBadMidiReplace = true;
    }
    require(rejectedBadMidiReplace, "MIDI replace recording rejects empty ranges");

    lamusica::commands::MidiClipStore midiStore;
    lamusica::commands::AddMidiNoteCommand addMidiNote{
        "cmd-midi-1",
        "audit-midi-1",
        "midi-command-clip",
        {.id = "note-command-1", .startSample = 12001, .lengthSamples = 2400, .pitch = 67}};
    require(addMidiNote.validate(midiStore).ok, "add MIDI note command validates");
    require(addMidiNote.apply(midiStore).ok, "add MIDI note command applies");
    require(midiStore.find("midi-command-clip")->notes.size() == 1,
            "MIDI note command mutates store");
    lamusica::commands::QuantizeMidiClipCommand quantizeMidi{
        "cmd-midi-2",
        "audit-midi-2",
        "midi-command-clip",
        {.gridSamples = 24000, .strength = 1.0F}};
    require(quantizeMidi.apply(midiStore).ok, "MIDI quantize command applies");
    require(midiStore.find("midi-command-clip")->notes.front().startSample == 24000,
            "MIDI quantize command mutates note timing");
    require(quantizeMidi.undo(midiStore).ok, "MIDI quantize command undoes");
    require(midiStore.find("midi-command-clip")->notes.front().startSample == 12001,
            "MIDI quantize undo restores note timing");
    lamusica::commands::TransposeMidiClipCommand transposeMidi{"cmd-midi-2a", "audit-midi-2a",
                                                               "midi-command-clip", 5};
    require(transposeMidi.apply(midiStore).ok, "MIDI transpose command applies");
    require(midiStore.find("midi-command-clip")->notes.front().pitch == 72,
            "MIDI transpose command mutates pitch");
    require(transposeMidi.undo(midiStore).ok, "MIDI transpose command undoes");
    require(midiStore.find("midi-command-clip")->notes.front().pitch == 67,
            "MIDI transpose undo restores pitch");
    lamusica::session::MidiNote editedNote = midiStore.find("midi-command-clip")->notes.front();
    editedNote.startSample = 48000;
    editedNote.lengthSamples = 9600;
    editedNote.velocity = 64;
    editedNote.muted = true;
    lamusica::commands::EditMidiNoteCommand editMidiNote{
        "cmd-midi-3", "audit-midi-3", "midi-command-clip", "note-command-1", editedNote};
    require(editMidiNote.apply(midiStore).ok, "MIDI note edit command applies");
    require(midiStore.find("midi-command-clip")->notes.front().muted, "MIDI note edit sets mute");
    require(midiStore.find("midi-command-clip")->notes.front().velocity == 64,
            "MIDI note edit sets velocity");
    require(editMidiNote.undo(midiStore).ok, "MIDI note edit command undoes");
    require(!midiStore.find("midi-command-clip")->notes.front().muted,
            "MIDI note edit undo restores mute");
    lamusica::commands::SplitMidiNoteCommand splitMidiNote{"cmd-midi-3b",          "audit-midi-3b",
                                                           "midi-command-clip",    "note-command-1",
                                                           "note-command-1-right", 13201};
    require(splitMidiNote.apply(midiStore).ok, "MIDI note split command applies");
    require(midiStore.find("midi-command-clip")->notes.size() == 2 &&
                midiStore.find("midi-command-clip")->notes.front().lengthSamples == 1200 &&
                midiStore.find("midi-command-clip")->notes.back().startSample == 13201,
            "MIDI note split command creates right-hand note");
    require(splitMidiNote.undo(midiStore).ok, "MIDI note split command undoes");
    require(midiStore.find("midi-command-clip")->notes.size() == 1 &&
                midiStore.find("midi-command-clip")->notes.front().lengthSamples == 2400,
            "MIDI note split undo restores original note length");
    lamusica::commands::AddMidiControlChangeCommand addMidiControl{
        "cmd-midi-3c",
        "audit-midi-3c",
        "midi-command-clip",
        {.samplePosition = 12200, .controller = 74, .value = 96, .channel = 1}};
    require(addMidiControl.apply(midiStore).ok, "MIDI CC command applies");
    require(midiStore.find("midi-command-clip")->controlChanges.size() == 1 &&
                midiStore.find("midi-command-clip")->notes.size() == 1,
            "MIDI CC command edits controller lane without corrupting notes");
    require(addMidiControl.undo(midiStore).ok, "MIDI CC command undoes");
    require(midiStore.find("midi-command-clip")->controlChanges.empty() &&
                midiStore.find("midi-command-clip")->notes.size() == 1,
            "MIDI CC undo restores controller lane independently");
    lamusica::commands::AddMidiPitchBendCommand addMidiPitchBend{
        "cmd-midi-3d",
        "audit-midi-3d",
        "midi-command-clip",
        {.samplePosition = 12300, .value = -2048, .channel = 1}};
    require(addMidiPitchBend.apply(midiStore).ok, "MIDI pitch bend command applies");
    require(midiStore.find("midi-command-clip")->pitchBends.size() == 1 &&
                midiStore.find("midi-command-clip")->notes.size() == 1,
            "MIDI pitch bend command edits controller lane without corrupting notes");
    require(addMidiPitchBend.undo(midiStore).ok, "MIDI pitch bend command undoes");
    require(midiStore.find("midi-command-clip")->pitchBends.empty() &&
                midiStore.find("midi-command-clip")->notes.size() == 1,
            "MIDI pitch bend undo restores controller lane independently");
    lamusica::commands::AddMidiAftertouchCommand addMidiAftertouch{
        "cmd-midi-3e",
        "audit-midi-3e",
        "midi-command-clip",
        {.samplePosition = 12400, .pressure = 92, .channel = 1, .pitch = 67, .polyphonic = true}};
    require(addMidiAftertouch.apply(midiStore).ok, "MIDI aftertouch command applies");
    require(midiStore.find("midi-command-clip")->aftertouch.size() == 1 &&
                midiStore.find("midi-command-clip")->notes.size() == 1,
            "MIDI aftertouch command edits controller lane without corrupting notes");
    require(addMidiAftertouch.undo(midiStore).ok, "MIDI aftertouch command undoes");
    require(midiStore.find("midi-command-clip")->aftertouch.empty() &&
                midiStore.find("midi-command-clip")->notes.size() == 1,
            "MIDI aftertouch undo restores controller lane independently");
    lamusica::commands::AddMidiProgramChangeCommand addMidiProgram{
        "cmd-midi-3f",
        "audit-midi-3f",
        "midi-command-clip",
        {.samplePosition = 12500, .program = 12, .channel = 1}};
    require(addMidiProgram.apply(midiStore).ok, "MIDI program change command applies");
    require(midiStore.find("midi-command-clip")->programChanges.size() == 1 &&
                midiStore.find("midi-command-clip")->notes.size() == 1,
            "MIDI program change command edits event list without corrupting notes");
    require(addMidiProgram.undo(midiStore).ok, "MIDI program change command undoes");
    require(midiStore.find("midi-command-clip")->programChanges.empty() &&
                midiStore.find("midi-command-clip")->notes.size() == 1,
            "MIDI program change undo restores event list independently");
    require(addMidiNote.undo(midiStore).ok, "add MIDI note command undoes");
    require(midiStore.find("midi-command-clip")->notes.empty(), "MIDI add undo removes note");

    lamusica::commands::MidiClipStore midiTransformStore;
    auto& midiTransformClip = midiTransformStore.getOrCreate("midi-transform-clip");
    midiTransformClip.notes.push_back(
        {.id = "vel-a", .startSample = 1000, .lengthSamples = 1200, .pitch = 60, .velocity = 80});
    midiTransformClip.notes.push_back(
        {.id = "vel-b", .startSample = 4000, .lengthSamples = 1000, .pitch = 64, .velocity = 100});
    lamusica::commands::TransformMidiVelocityCommand velocityMidi{
        "cmd-midi-4", "audit-midi-4", "midi-transform-clip", {.add = 10, .scale = 0.5F}};
    require(velocityMidi.apply(midiTransformStore).ok, "MIDI velocity command applies");
    require(midiTransformStore.find("midi-transform-clip")->notes.front().velocity == 50,
            "MIDI velocity command mutates note velocities");
    require(velocityMidi.undo(midiTransformStore).ok, "MIDI velocity command undoes");
    require(midiTransformStore.find("midi-transform-clip")->notes.front().velocity == 80,
            "MIDI velocity undo restores note velocities");
    lamusica::commands::HumanizeMidiClipCommand humanizeMidi{
        "cmd-midi-5",
        "audit-midi-5",
        "midi-transform-clip",
        {.maxTimingOffsetSamples = 24, .maxVelocityOffset = 4, .seed = 7}};
    auto expectedHumanized = *midiTransformStore.find("midi-transform-clip");
    lamusica::session::humanizeNotes(
        expectedHumanized, {.maxTimingOffsetSamples = 24, .maxVelocityOffset = 4, .seed = 7});
    require(humanizeMidi.apply(midiTransformStore).ok, "MIDI humanize command applies");
    require(midiTransformStore.find("midi-transform-clip")->notes.front().startSample ==
                    expectedHumanized.notes.front().startSample &&
                midiTransformStore.find("midi-transform-clip")->notes.front().velocity ==
                    expectedHumanized.notes.front().velocity,
            "MIDI humanize command is deterministic for a seed");
    require(humanizeMidi.undo(midiTransformStore).ok, "MIDI humanize command undoes");
    lamusica::commands::SetMidiNoteLengthsCommand lengthMidi{"cmd-midi-6", "audit-midi-6",
                                                             "midi-transform-clip", 2400};
    require(lengthMidi.apply(midiTransformStore).ok, "MIDI length command applies");
    require(midiTransformStore.find("midi-transform-clip")->notes.front().lengthSamples == 2400,
            "MIDI length command mutates note lengths");
    require(lengthMidi.undo(midiTransformStore).ok, "MIDI length command undoes");
    require(midiTransformStore.find("midi-transform-clip")->notes.front().lengthSamples == 1200,
            "MIDI length undo restores note lengths");
    lamusica::commands::LegatoMidiClipCommand legatoMidi{"cmd-midi-7", "audit-midi-7",
                                                         "midi-transform-clip"};
    require(legatoMidi.apply(midiTransformStore).ok, "MIDI legato command applies");
    require(midiTransformStore.find("midi-transform-clip")->notes.front().lengthSamples == 3000,
            "MIDI legato command extends notes to the next note");
    require(legatoMidi.undo(midiTransformStore).ok, "MIDI legato command undoes");
    require(midiTransformStore.find("midi-transform-clip")->notes.front().lengthSamples == 1200,
            "MIDI legato undo restores previous clip state");

    lamusica::commands::AutomationLaneStore automationStore;
    lamusica::session::AutomationLaneData commandAutomationLane{
        .id = "automation-command-lane",
        .targetId = "track-command-1",
        .parameterId = "volumeDb",
        .mode = lamusica::session::AutomationMode::Write,
        .defaultValue = 0.0F};
    lamusica::commands::AddAutomationPointCommand addAutomationPoint{
        "cmd-auto-1", "audit-auto-1", commandAutomationLane,
        128,          -6.0F,          lamusica::session::AutomationCurve::Linear};
    require(addAutomationPoint.apply(automationStore).ok, "automation point command applies");
    require(lamusica::session::evaluateAutomation(*automationStore.find("automation-command-lane"),
                                                  128) == -6.0F,
            "automation point command mutates lane");
    require(addAutomationPoint.undo(automationStore).ok, "automation point command undoes");
    require(automationStore.find("automation-command-lane")->regions.empty(),
            "automation point undo restores empty lane");
    lamusica::commands::CaptureAutomationWriteCommand captureAutomation{
        "cmd-auto-2",
        "audit-auto-2",
        commandAutomationLane,
        {{.samplePosition = 200, .value = -3.0F, .touched = true},
         {.samplePosition = 201, .value = -2.0F, .touched = true}}};
    require(captureAutomation.apply(automationStore).ok, "automation write command applies");
    require(automationStore.find("automation-command-lane")->regions.front().points.size() == 2,
            "automation write command captures batch points");
    require(captureAutomation.capturedBatch().laneId == "automation-command-lane" &&
                captureAutomation.capturedBatch().points.size() == 2,
            "automation write command exposes captured undoable batch");
    require(captureAutomation.undo(automationStore).ok, "automation write command undoes");
    require(automationStore.find("automation-command-lane")->regions.empty(),
            "automation write undo restores previous lane");

    lamusica::session::MidiClipData pianoRollClip{.clipId = "piano-roll"};
    pianoRollClip.notes.push_back(
        {.id = "pr-1", .startSample = 0, .lengthSamples = 12000, .pitch = 60});
    pianoRollClip.notes.push_back(
        {.id = "pr-2", .startSample = 24000, .lengthSamples = 12000, .pitch = 64});
    const auto pitches = lamusica::session::usedPitches(pianoRollClip);
    require(pitches.size() == 2 && pitches.front() == 60 && pitches.back() == 64,
            "piano roll used pitches sorted");
    const auto visibleNotes = lamusica::session::notesInRange(
        pianoRollClip, {.startSample = 0, .endSample = 13000, .lowPitch = 48, .highPitch = 72});
    require(visibleNotes.size() == 1 && visibleNotes.front().id == "pr-1",
            "piano roll filters notes by range");
    require(lamusica::session::pitchName(60) == "C4", "piano roll pitch names middle C");
    require(lamusica::session::drumNoteName(36, {{.pitch = 36, .name = "Kick"}}) == "Kick" &&
                lamusica::session::drumNoteName(38, {{.pitch = 36, .name = "Kick"}}) == "D2",
            "piano roll drum note naming overrides chromatic fallback per pitch");
    pianoRollClip.notes.push_back(
        {.id = "pr-3", .startSample = 0, .lengthSamples = 12000, .pitch = 64});
    pianoRollClip.notes.push_back(
        {.id = "pr-4", .startSample = 0, .lengthSamples = 12000, .pitch = 67});
    const auto foldedRange = lamusica::session::foldedPitchRange(
        pianoRollClip, {.startSample = 0, .endSample = 48000, .lowPitch = 0, .highPitch = 127});
    require(foldedRange.lowPitch == 60 && foldedRange.highPitch == 67,
            "piano roll fold mode narrows to used pitches");
    const auto cMajorHighlights =
        lamusica::session::scaleHighlights(0, lamusica::session::ScaleKind::Major, 60, 71);
    require(cMajorHighlights.size() == 12 && cMajorHighlights.front().root &&
                !cMajorHighlights[1].inScale,
            "piano roll scale highlighting marks roots and out-of-scale pitches");
    const auto chords = lamusica::session::chordLabels(pianoRollClip);
    require(chords.size() == 1 && chords.front().name == "C major",
            "piano roll chord labels detect simple triads");
    lamusica::session::PianoRollViewState pianoRollView;
    const auto selectedPianoNotes = lamusica::session::selectNotesInRange(
        pianoRollClip, {.startSample = 0, .endSample = 12000, .lowPitch = 60, .highPitch = 67});
    lamusica::session::setPianoRollSelection(pianoRollView, selectedPianoNotes, pianoRollClip);
    require(pianoRollView.selection.noteIds.size() == 3 &&
                pianoRollView.selection.range.has_value(),
            "piano roll lasso selection stores existing note ids and range");
    bool rejectedMissingPianoRollNote = false;
    try {
        lamusica::session::setPianoRollSelection(pianoRollView, {.noteIds = {"missing-note"}},
                                                 pianoRollClip);
    } catch (const std::exception&) {
        rejectedMissingPianoRollNote = true;
    }
    require(rejectedMissingPianoRollNote, "piano roll selection rejects missing notes");
    lamusica::session::setControllerLaneVisible(
        pianoRollView, lamusica::session::ControllerLaneType::ControlChange, 74, true);
    require(pianoRollView.controllerLanes.size() == 2 &&
                pianoRollView.controllerLanes.back().controller == 74,
            "piano roll controller lane visibility can add independent lanes");
    lamusica::session::setControllerLaneVisible(
        pianoRollView, lamusica::session::ControllerLaneType::ControlChange, 74, false);
    require(!pianoRollView.controllerLanes.back().visible &&
                pianoRollView.selection.noteIds.size() == 3,
            "piano roll controller lane visibility changes do not corrupt note selection");
    lamusica::session::AutomationLaneData pianoRollAutomation{
        .id = "piano-auto-volume",
        .targetKind = lamusica::session::AutomationTargetKind::Mixer,
        .targetId = "piano-track",
        .parameterId = "volumeDb"};
    lamusica::session::addAutomationPoint(pianoRollAutomation, 120, -6.0F);
    lamusica::session::addAutomationPoint(pianoRollAutomation, 240, -3.0F);
    const auto automationControllerEvents = lamusica::session::automationLinkedControllerLaneEvents(
        {std::array{pianoRollAutomation}},
        {.type = lamusica::session::ControllerLaneType::Automation,
         .automationTargetKind = lamusica::session::AutomationTargetKind::Mixer,
         .automationTargetId = "piano-track",
         .automationParameterId = "volumeDb"});
    require(automationControllerEvents.size() == 2 &&
                automationControllerEvents.front().automationValue == -6.0F &&
                automationControllerEvents.back().sourceId == "piano-auto-volume" &&
                pianoRollView.selection.noteIds.size() == 3,
            "piano roll automation-linked controller lane renders independently from notes");
    lamusica::session::MidiClipData ghostClip{.clipId = "ghost"};
    ghostClip.notes.push_back(
        {.id = "ghost-1", .startSample = 6000, .lengthSamples = 12000, .pitch = 72});
    const auto ghosts = lamusica::session::ghostNotesInRange(
        {pianoRollClip, ghostClip}, "piano-roll",
        {.startSample = 0, .endSample = 24000, .lowPitch = 60, .highPitch = 80});
    require(ghosts.size() == 1 && ghosts.front().id == "ghost-1",
            "piano roll ghost notes exclude the active clip");
    const auto eventItems = lamusica::session::eventListItems(pianoRollClip);
    require(eventItems.front().id == "pr-1" && eventItems.front().pitch == 60,
            "piano roll event list exposes precise note values");
    const auto audition = lamusica::session::auditionForNote(pianoRollClip.notes.front(), 3600);
    require(audition.pitch == 60 && audition.lengthSamples == 3600,
            "piano roll audition event follows selected note");
    lamusica::session::PianoRollLayoutOptions pianoRollLayoutOptions{
        .visibleRange = {.startSample = 0, .endSample = 48000, .lowPitch = 60, .highPitch = 72},
        .contentWidth = 480.0F,
        .keyboardWidth = 80.0F,
        .noteRowHeight = 10.0F,
        .controllerLaneHeight = 48.0F,
        .majorGridSamples = 24000,
        .minorGridSamples = 12000};
    const auto pianoRollLayout = lamusica::session::buildPianoRollLayout(
        pianoRollClip, pianoRollView, pianoRollLayoutOptions);
    require(pianoRollLayout.noteArea.x == 80.0F && pianoRollLayout.keyboardArea.width == 80.0F &&
                pianoRollLayout.keys.size() == 13 && pianoRollLayout.gridLines.size() == 5,
            "piano roll layout builds stable grid and keyboard geometry");
    require(std::ranges::any_of(pianoRollLayout.notes,
                                [](const lamusica::session::PianoRollNoteRect& note) {
                                    return note.noteId == "pr-1" && note.bounds.x == 80.0F &&
                                           note.bounds.y == 120.0F && note.selected;
                                }),
            "piano roll layout exposes selected note drawing rectangles");
    require(pianoRollLayout.controllerLanes.size() == 1 &&
                pianoRollLayout.controllerLanes.front().bounds.y == pianoRollLayout.noteArea.height,
            "piano roll layout gives controller lanes independent bounds");
    const auto draftedPianoNote =
        lamusica::session::noteDraftFromGridDrag(pianoRollLayoutOptions, 200.0F, 320.0F, 20.0F);
    require(draftedPianoNote.startSample == 12000 && draftedPianoNote.lengthSamples == 12000 &&
                draftedPianoNote.pitch == 70,
            "piano roll note drawing maps pointer drag to sample and pitch values");
    lamusica::session::MidiClipData densePianoRollClip{.clipId = "dense-piano-roll"};
    densePianoRollClip.notes.reserve(4096);
    for (int index = 0; index < 4096; ++index) {
        densePianoRollClip.notes.push_back({.id = "dense-" + std::to_string(index),
                                            .startSample = static_cast<std::int64_t>(index) * 120,
                                            .lengthSamples = 60,
                                            .pitch = static_cast<std::uint8_t>(36 + index % 48),
                                            .velocity = 80});
    }
    const auto densePianoRollLayout =
        lamusica::session::buildPianoRollLayout(densePianoRollClip, {},
                                                {.visibleRange = {.startSample = 12000,
                                                                  .endSample = 24000,
                                                                  .lowPitch = 48,
                                                                  .highPitch = 72},
                                                 .contentWidth = 600.0F,
                                                 .keyboardWidth = 64.0F,
                                                 .noteRowHeight = 8.0F,
                                                 .majorGridSamples = 6000,
                                                 .minorGridSamples = 1000});
    require(!densePianoRollLayout.notes.empty() && densePianoRollLayout.notes.size() < 4096 &&
                std::ranges::all_of(densePianoRollLayout.notes,
                                    [](const lamusica::session::PianoRollNoteRect& note) {
                                        return note.bounds.x >= 64.0F && note.bounds.x <= 664.0F &&
                                               note.pitch >= 48 && note.pitch <= 72;
                                    }),
            "piano roll dense clips render only visible note rectangles");

    lamusica::session::DrumMachinePreset drumPreset{
        .id = "kit-1",
        .name = "Kit 1",
        .license = "CC0-1.0",
        .licenseUrl = "https://creativecommons.org/publicdomain/zero/1.0/",
        .bundledAssetsIncluded = true,
        .pads = {
            {.id = "kick",
             .name = "Kick",
             .midiNote = 36,
             .color = "#ff6600",
             .outputRoute = "drum-bus",
             .gainDb = -3.0F,
             .sampleStart = 4,
             .sampleEnd = 2400,
             .velocityLayers = {{.minVelocity = 1, .maxVelocity = 127, .assetId = "kick-wav"}}},
            {.id = "closed-hat",
             .name = "Closed Hat",
             .midiNote = 42,
             .chokeGroup = 1,
             .outputRoute = "hat-bus",
             .velocityLayers = {{.minVelocity = 1, .maxVelocity = 127, .assetId = "ch.wav"}}},
            {.id = "open-hat",
             .name = "Open Hat",
             .midiNote = 46,
             .chokeGroup = 1,
             .outputRoute = "hat-bus",
             .velocityLayers = {{.minVelocity = 1, .maxVelocity = 127, .assetId = "oh.wav"}}}}};
    require(lamusica::session::findPadForMidiNote(drumPreset, 36)->id == "kick",
            "drum machine finds pad by MIDI note");
    require(lamusica::session::selectLayerAsset(drumPreset.pads.front(), 100) == "kick-wav",
            "drum machine selects velocity layer");
    drumPreset.pads.front().velocityLayers = {
        {.minVelocity = 1, .maxVelocity = 63, .assetId = "kick-soft.wav"},
        {.minVelocity = 64, .maxVelocity = 127, .assetId = "kick-hard.wav"}};
    lamusica::session::assignAssetToPad(
        drumPreset, "kick", {.minVelocity = 64, .maxVelocity = 127, .assetId = "kick-drop.wav"});
    require(lamusica::session::findPadById(drumPreset, "kick")->velocityLayers.size() == 2 &&
                lamusica::session::selectLayerAsset(drumPreset.pads.front(), 120) ==
                    "kick-drop.wav",
            "drum machine pad browser assignment replaces matching velocity layer");
    lamusica::session::assignAssetToPad(
        drumPreset, "kick", {.minVelocity = 100, .maxVelocity = 127, .assetId = "kick-accent.wav"});
    require(lamusica::session::findPadById(drumPreset, "kick")->velocityLayers.size() == 3,
            "drum machine pad browser assignment adds new velocity layer");
    bool rejectedMissingPadAssignment = false;
    try {
        lamusica::session::assignAssetToPad(
            drumPreset, "missing-pad",
            {.minVelocity = 1, .maxVelocity = 127, .assetId = "missing.wav"});
    } catch (const std::exception&) {
        rejectedMissingPadAssignment = true;
    }
    require(rejectedMissingPadAssignment,
            "drum machine pad browser assignment rejects missing pads");
    const auto drumTriggers = lamusica::session::renderDrumTriggers(
        drumPreset, std::vector<std::pair<std::int64_t, std::uint8_t>>{{0, 42}, {2400, 46}});
    require(drumTriggers.size() == 2, "drum machine renders triggers");
    require(drumTriggers.front().chokedPrevious, "drum machine marks previous choke");
    require(drumTriggers.back().outputRoute == "hat-bus", "drum machine preserves pad routing");
    const auto velocityTriggers = lamusica::session::renderDrumTriggers(
        drumPreset, std::vector<lamusica::session::DrumPadEvent>{
                        {.samplePosition = 0, .midiNote = 36, .velocity = 48},
                        {.samplePosition = 2400, .midiNote = 36, .velocity = 120}});
    require(velocityTriggers.size() == 2 && velocityTriggers.front().assetId == "kick-soft.wav" &&
                velocityTriggers.back().assetId == "kick-drop.wav" &&
                velocityTriggers.back().velocity == 120,
            "drum machine trigger rendering selects velocity layers from MIDI velocity");
    lamusica::audio::RenderedAudio drumSource{
        .channels = 1,
        .frames = 8,
        .interleavedSamples = {0.0F, 0.25F, 0.5F, 0.75F, 1.0F, 0.75F, 0.5F, 0.25F}};
    auto playablePad = drumPreset.pads.front();
    playablePad.sampleStart = 1;
    playablePad.sampleEnd = 7;
    playablePad.attackSamples = 2;
    playablePad.releaseSamples = 2;
    playablePad.gainDb = 0.0F;
    const auto renderedPad = lamusica::session::renderDrumPadSample(playablePad, drumSource, 127);
    require(renderedPad.frames == 6 && renderedPad.interleavedSamples.front() == 0.0F &&
                renderedPad.interleavedSamples[2] > 0.4F &&
                renderedPad.interleavedSamples.back() == 0.0F,
            "drum machine sample playback applies range and envelope");
    playablePad.reversed = true;
    playablePad.attackSamples = 0;
    playablePad.releaseSamples = 0;
    const auto reversedPad = lamusica::session::renderDrumPadSample(playablePad, drumSource, 127);
    require(reversedPad.interleavedSamples.front() == 0.5F &&
                reversedPad.interleavedSamples.back() == 0.25F,
            "drum machine sample playback reverses selected range");
    playablePad.reversed = false;
    playablePad.pitchSemitones = 12.0F;
    const auto pitchedPad = lamusica::session::renderDrumPadSample(playablePad, drumSource, 127);
    require(pitchedPad.frames == 3, "drum machine sample playback applies pitch ratio");
    playablePad.pitchSemitones = 0.0F;
    playablePad.lowPassCoefficient = 0.25F;
    const auto filteredPad = lamusica::session::renderDrumPadSample(playablePad, drumSource, 127);
    require(filteredPad.interleavedSamples[2] < renderedPad.interleavedSamples[2],
            "drum machine sample playback applies simple low-pass filtering");
    auto routedDrumPreset = drumPreset;
    routedDrumPreset.pads[0].sampleStart = 0;
    routedDrumPreset.pads[0].sampleEnd = 0;
    routedDrumPreset.pads[1].sampleStart = 0;
    routedDrumPreset.pads[1].sampleEnd = 0;
    routedDrumPreset.pads[2].sampleStart = 0;
    routedDrumPreset.pads[2].sampleEnd = 0;
    const auto routedDrums = lamusica::session::renderDrumMachineRoutes(
        routedDrumPreset,
        {{.samplePosition = 0, .midiNote = 42, .velocity = 127},
         {.samplePosition = 2, .midiNote = 46, .velocity = 127},
         {.samplePosition = 4, .midiNote = 36, .velocity = 127}},
        {{.assetId = "ch.wav", .audio = drumSource},
         {.assetId = "oh.wav", .audio = drumSource},
         {.assetId = "kick-drop.wav", .audio = drumSource}},
        8, 1);
    const auto hatRoute =
        std::ranges::find_if(routedDrums, [](const lamusica::session::DrumRouteRender& route) {
            return route.outputRoute == "hat-bus";
        });
    const auto kickRoute =
        std::ranges::find_if(routedDrums, [](const lamusica::session::DrumRouteRender& route) {
            return route.outputRoute == "drum-bus";
        });
    require(hatRoute != routedDrums.end() && kickRoute != routedDrums.end(),
            "drum machine route render creates per-pad output buses");
    require(hatRoute->audio.interleavedSamples[3] > 0.0F &&
                hatRoute->audio.interleavedSamples[6] > 0.0F &&
                kickRoute->audio.interleavedSamples[5] > 0.0F,
            "drum machine route render mixes triggered samples to their routes");
    require(hatRoute->audio.interleavedSamples[2] < 0.51F,
            "drum machine route render truncates choked voices");
    const auto serializedDrumPreset = lamusica::session::serializeDrumMachinePreset(drumPreset);
    const auto parsedDrumPreset = lamusica::session::parseDrumMachinePreset(serializedDrumPreset);
    require(parsedDrumPreset.name == "Kit 1" && parsedDrumPreset.license == "CC0-1.0" &&
                parsedDrumPreset.pads.front().color == "#ff6600" &&
                parsedDrumPreset.pads.front().gainDb == -3.0F &&
                parsedDrumPreset.pads.front().velocityLayers.size() == 3 &&
                parsedDrumPreset.pads.front().velocityLayers[1].assetId == "kick-drop.wav",
            "drum machine preset serializes and parses portable pad metadata");
    const auto drumAssetReferences =
        lamusica::session::collectDrumPresetAssetReferences(parsedDrumPreset);
    require(std::ranges::any_of(drumAssetReferences,
                                [](const lamusica::session::DrumPresetAssetReference& reference) {
                                    return reference.assetId == "kick-drop.wav" &&
                                           std::ranges::contains(reference.padIds, "kick");
                                }),
            "drum machine preset collection enumerates pad asset dependencies");
    const auto portableDrumPreset = lamusica::session::makePortableDrumMachinePreset(
        parsedDrumPreset,
        {std::array{
            lamusica::session::DrumPresetAssetMapping{.sourceAssetId = "kick-soft.wav",
                                                      .collectedAssetId = "Samples/kick-soft.wav"},
            lamusica::session::DrumPresetAssetMapping{.sourceAssetId = "kick-drop.wav",
                                                      .collectedAssetId = "Samples/kick-drop.wav"},
            lamusica::session::DrumPresetAssetMapping{
                .sourceAssetId = "kick-accent.wav", .collectedAssetId = "Samples/kick-accent.wav"},
            lamusica::session::DrumPresetAssetMapping{.sourceAssetId = "ch.wav",
                                                      .collectedAssetId = "Samples/ch.wav"},
            lamusica::session::DrumPresetAssetMapping{.sourceAssetId = "oh.wav",
                                                      .collectedAssetId = "Samples/oh.wav"}}});
    require(lamusica::session::selectLayerAsset(portableDrumPreset.pads.front(), 120) ==
                "Samples/kick-drop.wav",
            "drum machine portable preset remaps velocity layers to collected assets");
    bool rejectedIncompleteDrumCollection = false;
    try {
        (void)lamusica::session::makePortableDrumMachinePreset(
            parsedDrumPreset,
            {std::array{lamusica::session::DrumPresetAssetMapping{
                .sourceAssetId = "kick-soft.wav", .collectedAssetId = "Samples/kick-soft.wav"}}});
    } catch (const std::exception&) {
        rejectedIncompleteDrumCollection = true;
    }
    require(rejectedIncompleteDrumCollection,
            "drum machine portable preset rejects incomplete asset collection mappings");
    require(lamusica::session::hasClearDrumPresetRedistributionRights(parsedDrumPreset),
            "drum machine preset records clear bundled asset rights");
    const auto placeholderKit = lamusica::session::parseDrumMachinePreset(
        "{\"id\":\"basic-starter-kit\",\"name\":\"Basic Starter Kit\","
        "\"license\":\"placeholder-assets-not-included\",\"bundledAssetsIncluded\":false,"
        "\"pads\":[]}");
    require(lamusica::session::hasClearDrumPresetRedistributionRights(placeholderKit),
            "drum machine starter kit placeholder documents that no bundled assets ship");

    lamusica::session::PatternClip pattern{.id = "pattern-1",
                                           .name = "Pattern 1",
                                           .lengthSteps = 4,
                                           .stepLengthSamples = 6000,
                                           .swing = 0.25F,
                                           .seed = 7,
                                           .lanes = {{.id = "lane-kick",
                                                      .name = "Kick",
                                                      .defaultPitch = 36,
                                                      .lengthSteps = 4,
                                                      .steps = {{.enabled = true,
                                                                 .pitch = 36,
                                                                 .velocity = 100,
                                                                 .probability = 1.0F,
                                                                 .ratchets = 1},
                                                                {},
                                                                {.enabled = true,
                                                                 .pitch = 36,
                                                                 .velocity = 100,
                                                                 .probability = 1.0F,
                                                                 .ratchets = 2,
                                                                 .accent = true},
                                                                {}}},
                                                     {.id = "lane-hat",
                                                      .name = "Hat",
                                                      .defaultPitch = 42,
                                                      .lengthSteps = 4,
                                                      .steps = {{},
                                                                {.enabled = true,
                                                                 .pitch = 42,
                                                                 .velocity = 70,
                                                                 .probability = 1.0F,
                                                                 .ratchets = 1,
                                                                 .slide = true},
                                                                {},
                                                                {}}}}};
    const auto patternMidi = lamusica::session::patternToMidi(pattern, "pattern-midi");
    require(patternMidi.notes.size() == 4, "pattern converts enabled steps and ratchets to MIDI");
    require(patternMidi.notes[1].startSample == 7500, "pattern applies swing to odd steps");
    require(patternMidi.notes[2].velocity == 116, "pattern accent raises velocity");
    require(std::ranges::any_of(patternMidi.metadata,
                                [](const lamusica::session::MidiMetadata& metadata) {
                                    return metadata.key == "slide:lane-hat-1-0" &&
                                           metadata.value == "true";
                                }),
            "pattern conversion preserves slide step metadata");
    const auto placedPatternMidi = lamusica::session::patternClipToMidi(
        {.pattern = pattern, .timelineStartSample = 48000}, "placed-pattern-midi");
    require(placedPatternMidi.notes.front().startSample ==
                patternMidi.notes.front().startSample + 48000,
            "pattern clip placement offsets notes on the arrangement timeline");
    const auto patternPlaybackWindow = lamusica::session::patternPlaybackEventsInRange(
        {.pattern = pattern, .timelineStartSample = 48000}, 54000, 66000);
    require(!patternPlaybackWindow.empty() &&
                std::ranges::all_of(patternPlaybackWindow,
                                    [](const lamusica::session::MidiPlaybackEvent& event) {
                                        return event.samplePosition >= 0 &&
                                               event.samplePosition < 12000;
                                    }) &&
                std::ranges::any_of(patternPlaybackWindow,
                                    [](const lamusica::session::MidiPlaybackEvent& event) {
                                        return event.type ==
                                                   lamusica::session::MidiEventType::NoteOn &&
                                               event.samplePosition == 1500 &&
                                               event.sourceId == "lane-hat-1-0";
                                    }),
            "pattern playback range emits sample-aligned buffer-relative events");
    bool rejectedBadPatternPlaybackRange = false;
    try {
        (void)lamusica::session::patternPlaybackEventsInRange(
            {.pattern = pattern, .timelineStartSample = 48000}, 66000, 54000);
    } catch (const std::exception&) {
        rejectedBadPatternPlaybackRange = true;
    }
    require(rejectedBadPatternPlaybackRange, "pattern playback range rejects unordered windows");
    lamusica::session::PatternClip polymeterPattern{
        .id = "polymeter",
        .name = "Polymeter",
        .lengthSteps = 4,
        .stepLengthSamples = 1000,
        .lanes = {
            {.id = "short-lane",
             .name = "Short Lane",
             .defaultPitch = 36,
             .lengthSteps = 2,
             .steps = {{.enabled = true, .pitch = 36, .velocity = 100}, {.enabled = false}}}}};
    const auto polymeterMidi = lamusica::session::patternToMidi(polymeterPattern, "polymeter-midi");
    require(polymeterMidi.notes.size() == 2 && polymeterMidi.notes[0].startSample == 0 &&
                polymeterMidi.notes[1].startSample == 2000,
            "pattern per-lane length cycles shorter lanes across the pattern");
    lamusica::session::PatternClip tiedPattern{
        .id = "tied-pattern",
        .name = "Tied Pattern",
        .lengthSteps = 4,
        .stepLengthSamples = 1000,
        .lanes = {{.id = "tied-lane",
                   .name = "Tied Lane",
                   .defaultPitch = 60,
                   .lengthSteps = 4,
                   .steps = {{.enabled = true, .pitch = 60, .velocity = 100, .tie = true},
                             {.enabled = true, .pitch = 60, .velocity = 100, .tie = true},
                             {.enabled = true, .pitch = 60, .velocity = 100},
                             {}}}}};
    const auto tiedMidi = lamusica::session::patternToMidi(tiedPattern, "tied-midi");
    require(tiedMidi.notes.size() == 1 && tiedMidi.notes.front().lengthSamples == 3000,
            "pattern tied steps sustain without retriggering continuation steps");
    const auto roundTripPattern =
        lamusica::session::midiToPattern(patternMidi, "round-trip-pattern", "Round Trip", 6000, 4);
    const auto roundTripMidi =
        lamusica::session::patternToMidi(roundTripPattern, "round-trip-midi");
    require(roundTripMidi.notes.size() == patternMidi.notes.size() &&
                roundTripMidi.notes.front().pitch == patternMidi.notes.front().pitch &&
                roundTripMidi.notes.front().startSample == patternMidi.notes.front().startSample,
            "pattern MIDI conversion round trips expected musical data");
    require(lamusica::session::probabilityHit(0.5F, 123, 0, 0, 0) ==
                lamusica::session::probabilityHit(0.5F, 123, 0, 0, 0),
            "pattern probability is deterministic");
    const auto variation =
        lamusica::session::duplicatePatternVariation(pattern, "pattern-variation", "Variation", 10);
    require(variation.id == "pattern-variation" && variation.seed == 17,
            "pattern variation duplicates with seed offset");
    const auto chainMidi =
        lamusica::session::patternChainToMidi({.patterns = {pattern, variation}}, "chain-midi");
    require(chainMidi.notes.size() == 8, "pattern chain concatenates MIDI notes");
    require(chainMidi.notes[4].startSample >= 24000, "pattern chain offsets later patterns");
    lamusica::commands::PatternClipStore patternStore;
    lamusica::commands::AddPatternClipCommand addPatternCommand{"cmd-pattern-1", "audit-pattern-1",
                                                                pattern};
    require(addPatternCommand.apply(patternStore).ok, "pattern add command applies");
    require(patternStore.find("pattern-1") != nullptr, "pattern add command stores pattern");
    lamusica::commands::DuplicatePatternVariationCommand duplicatePatternCommand{
        "cmd-pattern-2",     "audit-pattern-2",
        "pattern-1",         "pattern-command-variation",
        "Command Variation", 23};
    require(duplicatePatternCommand.apply(patternStore).ok, "pattern variation command applies");
    require(patternStore.find("pattern-command-variation")->seed == pattern.seed + 23,
            "pattern variation command duplicates with deterministic seed offset");
    lamusica::commands::EditPatternStepCommand editPatternStep{
        "cmd-pattern-3",
        "audit-pattern-3",
        "pattern-1",
        "lane-kick",
        1,
        {.enabled = true, .pitch = 38, .velocity = 90, .probability = 1.0F, .ratchets = 1}};
    require(editPatternStep.apply(patternStore).ok, "pattern step edit command applies");
    require(patternStore.find("pattern-1")->lanes.front().steps[1].enabled &&
                patternStore.find("pattern-1")->lanes.front().steps[1].pitch == 38,
            "pattern step edit command mutates step grid");
    require(editPatternStep.undo(patternStore).ok, "pattern step edit command undoes");
    require(!patternStore.find("pattern-1")->lanes.front().steps[1].enabled,
            "pattern step edit undo restores previous step");
    require(duplicatePatternCommand.undo(patternStore).ok, "pattern variation command undoes");
    require(patternStore.find("pattern-command-variation") == nullptr,
            "pattern variation undo removes duplicate");
    require(addPatternCommand.undo(patternStore).ok, "pattern add command undoes");
    require(patternStore.find("pattern-1") == nullptr, "pattern add undo removes pattern");

    lamusica::session::PluginScanCache pluginCache;
    lamusica::session::mergeScanResult(
        pluginCache,
        {.description = {.identifier = "builtin.eq",
                         .name = "EQ",
                         .vendor = "LaMusica",
                         .format = lamusica::session::PluginFormat::BuiltIn,
                         .parameters = {{.id = "gain", .name = "Gain", .defaultValue = 0.0F}}},
         .valid = true});
    lamusica::session::mergeScanResult(
        pluginCache,
        {.description = {.identifier = "builtin.synth",
                         .name = "Synth",
                         .vendor = "LaMusica",
                         .format = lamusica::session::PluginFormat::BuiltIn,
                         .instrument = true,
                         .parameters = {{.id = "cutoff", .name = "Cutoff", .defaultValue = 0.5F}}},
         .valid = true});
    require(lamusica::session::findPlugin(pluginCache, "builtin.eq").has_value(),
            "plugin cache finds valid plugin");
    require(lamusica::session::stableParameterAddress("builtin.eq", "gain") == "builtin.eq::gain",
            "plugin parameter address is stable");
    const auto discoveredParameters = lamusica::session::discoverPluginParameters(
        *lamusica::session::findPlugin(pluginCache, "builtin.eq"));
    require(discoveredParameters.size() == 1 &&
                discoveredParameters.front().automationAddress == "builtin.eq::gain" &&
                discoveredParameters.front().defaultValue == 0.0F,
            "plugin parameter discovery exposes stable automation addresses");
    bool rejectedIncompletePluginParameter = false;
    try {
        (void)lamusica::session::discoverPluginParameters(
            {.identifier = "bad.parameters",
             .parameters = {{.id = "", .name = "Missing Id", .defaultValue = 0.0F}}});
    } catch (const std::exception&) {
        rejectedIncompletePluginParameter = true;
    }
    require(rejectedIncompletePluginParameter,
            "plugin parameter discovery rejects incomplete parameter metadata");
    lamusica::session::PluginInstrumentRack instrumentRack;
    lamusica::session::assignInstrumentSlot(
        instrumentRack, pluginCache, "instrument-track",
        {.id = "synth-slot", .pluginIdentifier = "builtin.synth"});
    require(lamusica::session::findInstrumentSlot(instrumentRack, "instrument-track")
                    ->instrument.pluginIdentifier == "builtin.synth",
            "plugin instrument slot accepts discovered instrument plugin");
    bool rejectedEffectInstrumentSlot = false;
    try {
        lamusica::session::assignInstrumentSlot(
            instrumentRack, pluginCache, "effect-track",
            {.id = "effect-slot", .pluginIdentifier = "builtin.eq"});
    } catch (const std::exception&) {
        rejectedEffectInstrumentSlot = true;
    }
    require(rejectedEffectInstrumentSlot,
            "plugin instrument slot rejects effect plugins as instruments");
    const auto reloadedInstrumentRack = lamusica::session::parsePluginInstrumentRack(
        lamusica::session::serializePluginInstrumentRack(instrumentRack));
    require(lamusica::session::findInstrumentSlot(reloadedInstrumentRack, "instrument-track")
                    ->instrument.id == "synth-slot",
            "plugin instrument slot state saves and reloads");
    lamusica::session::PluginEditorState editorState;
    lamusica::session::openPluginEditor(editorState, {.trackId = "instrument-track",
                                                      .insertId = "synth-slot",
                                                      .pinned = true,
                                                      .x = 120,
                                                      .y = 80,
                                                      .width = 900,
                                                      .height = 620});
    require(
        lamusica::session::findEditorWindow(editorState, "instrument-track", "synth-slot")->pinned,
        "plugin editor window state tracks opened editors");
    lamusica::session::closePluginEditor(editorState, "instrument-track", "synth-slot");
    const auto reloadedEditorState = lamusica::session::parsePluginEditorState(
        lamusica::session::serializePluginEditorState(editorState));
    const auto* reloadedEditor =
        lamusica::session::findEditorWindow(reloadedEditorState, "instrument-track", "synth-slot");
    require(reloadedEditor != nullptr && !reloadedEditor->open && reloadedEditor->width == 900,
            "plugin editor window state saves closed geometry");
    lamusica::session::blacklistPlugin(pluginCache, "bad.plugin", "scan failed");
    require(lamusica::session::isBlacklisted(pluginCache, "bad.plugin"),
            "plugin blacklist records bad plugin");
    require(!lamusica::session::findPlugin(pluginCache, "bad.plugin").has_value(),
            "plugin blacklist hides bad plugin");
    const auto macPluginFormats =
        lamusica::session::pluginFormatSupport({.macOS = true,
                                                .audioUnitRuntimeAvailable = true,
                                                .vst3SdkAvailable = false,
                                                .vst3LicenseAccepted = false});
    require(std::ranges::any_of(macPluginFormats,
                                [](const lamusica::session::PluginFormatSupport& support) {
                                    return support.format ==
                                               lamusica::session::PluginFormat::AudioUnit &&
                                           support.available;
                                }) &&
                std::ranges::any_of(macPluginFormats,
                                    [](const lamusica::session::PluginFormatSupport& support) {
                                        return support.format ==
                                                   lamusica::session::PluginFormat::Vst3 &&
                                               !support.available &&
                                               support.reason == "vst3_sdk_not_available";
                                    }),
            "plugin host exposes AU on macOS and gates VST3 on SDK availability");
    const auto licensedVst3Formats =
        lamusica::session::pluginFormatSupport({.macOS = true,
                                                .audioUnitRuntimeAvailable = true,
                                                .vst3SdkAvailable = true,
                                                .vst3LicenseAccepted = true});
    require(std::ranges::any_of(licensedVst3Formats,
                                [](const lamusica::session::PluginFormatSupport& support) {
                                    return support.format ==
                                               lamusica::session::PluginFormat::Vst3 &&
                                           support.available;
                                }),
            "plugin host enables VST3 only after SDK and license checks pass");
    const auto nonMacPluginFormats =
        lamusica::session::pluginFormatSupport({.macOS = false,
                                                .audioUnitRuntimeAvailable = true,
                                                .vst3SdkAvailable = true,
                                                .vst3LicenseAccepted = false});
    require(std::ranges::any_of(nonMacPluginFormats,
                                [](const lamusica::session::PluginFormatSupport& support) {
                                    return support.format ==
                                               lamusica::session::PluginFormat::AudioUnit &&
                                           !support.available &&
                                           support.reason == "audio_unit_requires_macos";
                                }),
            "plugin host disables AU formats outside macOS");
    const std::array pluginScanCandidates{
        lamusica::session::PluginScanCandidate{
            .description = {.identifier = "safe.plugin",
                            .name = "Safe",
                            .vendor = "LaMusica",
                            .format = lamusica::session::PluginFormat::BuiltIn},
            .outcome = lamusica::session::PluginScanOutcome::Valid},
        lamusica::session::PluginScanCandidate{
            .description = {.identifier = "crashy.plugin",
                            .name = "Crashy",
                            .vendor = "Unknown",
                            .format = lamusica::session::PluginFormat::Vst3},
            .outcome = lamusica::session::PluginScanOutcome::Crashed},
        lamusica::session::PluginScanCandidate{
            .description = {.identifier = "slow.plugin",
                            .name = "Slow",
                            .vendor = "Unknown",
                            .format = lamusica::session::PluginFormat::AudioUnit},
            .outcome = lamusica::session::PluginScanOutcome::TimedOut}};
    const auto pluginScanReport = lamusica::session::scanPluginCandidates(
        pluginCache, pluginScanCandidates, {.timeoutMilliseconds = 250});
    require(pluginScanReport.appLaunchSafe &&
                lamusica::session::findPlugin(pluginCache, "safe.plugin").has_value(),
            "plugin scan report keeps app launch safe while accepting valid plugins");
    require(lamusica::session::isBlacklisted(pluginCache, "crashy.plugin") &&
                lamusica::session::isBlacklisted(pluginCache, "slow.plugin"),
            "plugin scan blacklists crashed and timed-out plugins");
    const auto skippedScan = lamusica::session::scanPluginCandidates(
        pluginCache, {std::array{lamusica::session::PluginScanCandidate{
                         .description = {.identifier = "crashy.plugin"},
                         .outcome = lamusica::session::PluginScanOutcome::Valid}}});
    require(skippedScan.skippedBlacklisted.size() == 1 &&
                skippedScan.skippedBlacklisted.front() == "crashy.plugin",
            "plugin scan skips previously blacklisted plugins");
    lamusica::session::allowPluginRescan(pluginCache, "crashy.plugin");
    const auto rescanReport = lamusica::session::scanPluginCandidates(
        pluginCache, {std::array{lamusica::session::PluginScanCandidate{
                         .description = {.identifier = "crashy.plugin",
                                         .name = "Recovered",
                                         .vendor = "Unknown",
                                         .format = lamusica::session::PluginFormat::Vst3},
                         .outcome = lamusica::session::PluginScanOutcome::Valid}}});
    require(rescanReport.scanned.size() == 1 &&
                lamusica::session::findPlugin(pluginCache, "crashy.plugin").has_value(),
            "plugin rescan can explicitly clear blacklist and accept recovered plugins");
    const auto malformedPluginScan = lamusica::session::scanPluginCandidates(
        pluginCache, {std::array{lamusica::session::PluginScanCandidate{
                         .description = {.identifier = ""},
                         .outcome = lamusica::session::PluginScanOutcome::Valid}}});
    require(!malformedPluginScan.appLaunchSafe,
            "plugin scan reports malformed candidates as unsafe scanner input");
    lamusica::session::PluginInsertChain insertChain{.trackId = "track-1"};
    lamusica::session::addInsert(insertChain, {.id = "insert-1", .pluginIdentifier = "builtin.eq"});
    lamusica::session::addInsert(insertChain,
                                 {.id = "insert-2", .pluginIdentifier = "builtin.comp"});
    lamusica::session::moveInsert(insertChain, "insert-2", 0);
    require(insertChain.inserts.front().id == "insert-2", "plugin insert move reorders chain");
    lamusica::session::removeInsert(insertChain, "insert-2");
    require(insertChain.inserts.size() == 1 && insertChain.inserts.front().id == "insert-1",
            "plugin insert remove updates chain");
    lamusica::session::setParameterValue(*lamusica::session::findInsert(insertChain, "insert-1"),
                                         "gain", 0.25F);
    lamusica::session::setParameterValue(*lamusica::session::findInsert(insertChain, "insert-1"),
                                         "gain", 0.5F);
    require(lamusica::session::findParameterValue(insertChain.inserts.front(), "gain") == 0.5F,
            "plugin insert parameter values upsert by id");
    insertChain.inserts.front().bypassed = true;
    const auto serializedPluginChain = lamusica::session::serializePluginInsertChain(insertChain);
    const auto reloadedPluginChain =
        lamusica::session::parsePluginInsertChain(serializedPluginChain);
    require(reloadedPluginChain.trackId == "track-1" && reloadedPluginChain.inserts.size() == 1 &&
                reloadedPluginChain.inserts.front().bypassed &&
                lamusica::session::findParameterValue(reloadedPluginChain.inserts.front(),
                                                      "gain") == 0.5F,
            "plugin insert chain state saves and reloads");
    require(lamusica::session::stableParameterAddress(
                reloadedPluginChain.inserts.front().pluginIdentifier, "gain") == "builtin.eq::gain",
            "plugin parameter address remains stable after reload");
    lamusica::session::PluginPreset eqPreset{
        .id = "eq-bright",
        .name = "Bright",
        .pluginIdentifier = "builtin.eq",
        .parameterValues = {{.parameterId = "gain", .value = 0.75F}}};
    const auto reloadedPreset =
        lamusica::session::parsePluginPreset(lamusica::session::serializePluginPreset(eqPreset));
    auto presetInsert = reloadedPluginChain.inserts.front();
    lamusica::session::applyPreset(presetInsert, reloadedPreset);
    require(lamusica::session::findParameterValue(presetInsert, "gain") == 0.75F,
            "plugin preset applies parameter values");
    bool rejectedMismatchedPreset = false;
    try {
        lamusica::session::applyPreset(
            presetInsert, {.id = "wrong", .name = "Wrong", .pluginIdentifier = "builtin.comp"});
    } catch (const std::exception&) {
        rejectedMismatchedPreset = true;
    }
    require(rejectedMismatchedPreset, "plugin preset rejects mismatched plugin identifier");

    lamusica::commands::PluginInsertChainStore pluginCommandStore;
    lamusica::commands::AddPluginInsertCommand addPluginInsert{
        "plugin-cmd-add",
        "plugin-audit-add",
        "track-1",
        {.id = "cmd-insert-1",
         .pluginIdentifier = "builtin.eq",
         .parameterValues = {{.parameterId = "gain", .value = 0.1F}}}};
    require(addPluginInsert.validate(pluginCommandStore).ok, "plugin insert command validates");
    require(addPluginInsert.apply(pluginCommandStore).ok, "plugin insert command applies");
    require(pluginCommandStore.find("track-1")->inserts.size() == 1,
            "plugin insert command mutates store");
    lamusica::commands::AddPluginInsertCommand addSecondPluginInsert{
        "plugin-cmd-add-2",
        "plugin-audit-add-2",
        "track-1",
        {.id = "cmd-insert-2", .pluginIdentifier = "builtin.comp"}};
    require(addSecondPluginInsert.apply(pluginCommandStore).ok,
            "second plugin insert command applies");
    lamusica::commands::MovePluginInsertCommand movePluginInsert{
        "plugin-cmd-move", "plugin-audit-move", "track-1", "cmd-insert-2", 0};
    require(movePluginInsert.apply(pluginCommandStore).ok, "plugin insert move command applies");
    require(pluginCommandStore.find("track-1")->inserts.front().id == "cmd-insert-2",
            "plugin insert move command reorders chain");
    require(movePluginInsert.undo(pluginCommandStore).ok, "plugin insert move command undoes");
    require(pluginCommandStore.find("track-1")->inserts.back().id == "cmd-insert-2",
            "plugin insert move undo restores order");
    lamusica::commands::RemovePluginInsertCommand removePluginInsert{
        "plugin-cmd-remove", "plugin-audit-remove", "track-1", "cmd-insert-2"};
    require(removePluginInsert.apply(pluginCommandStore).ok,
            "plugin insert remove command applies");
    require(pluginCommandStore.find("track-1")->inserts.size() == 1,
            "plugin insert remove command mutates store");
    require(removePluginInsert.undo(pluginCommandStore).ok, "plugin insert remove command undoes");
    require(pluginCommandStore.find("track-1")->inserts.back().id == "cmd-insert-2",
            "plugin insert remove undo restores insert");
    lamusica::commands::ApplyPluginPresetCommand applyPluginPreset{
        "plugin-cmd-preset",
        "plugin-audit-preset",
        "track-1",
        "cmd-insert-1",
        {.id = "eq-command-preset",
         .name = "Command Preset",
         .pluginIdentifier = "builtin.eq",
         .parameterValues = {{.parameterId = "gain", .value = 0.9F}}}};
    require(applyPluginPreset.apply(pluginCommandStore).ok, "plugin preset command applies");
    require(lamusica::session::findParameterValue(
                *lamusica::session::findInsert(*pluginCommandStore.find("track-1"), "cmd-insert-1"),
                "gain") == 0.9F,
            "plugin preset command changes parameter values");
    require(applyPluginPreset.undo(pluginCommandStore).ok, "plugin preset command undoes");
    require(lamusica::session::findParameterValue(
                *lamusica::session::findInsert(*pluginCommandStore.find("track-1"), "cmd-insert-1"),
                "gain") == 0.1F,
            "plugin preset undo restores parameter values");
    lamusica::commands::ApplyPluginPresetCommand wrongPluginPreset{
        "plugin-cmd-preset-bad",
        "plugin-audit-preset-bad",
        "track-1",
        "cmd-insert-1",
        {.id = "wrong-command-preset",
         .name = "Wrong Command Preset",
         .pluginIdentifier = "builtin.comp"}};
    require(!wrongPluginPreset.validate(pluginCommandStore).ok,
            "plugin preset command rejects mismatched plugin identifier");
    require(addPluginInsert.undo(pluginCommandStore).ok, "plugin insert add command undoes");
    require(lamusica::session::findInsert(*pluginCommandStore.find("track-1"), "cmd-insert-1") ==
                nullptr,
            "plugin insert add undo removes insert");

    lamusica::session::MixerState mixer;
    lamusica::session::addChannel(
        mixer, {.id = "audio-1", .name = "Audio 1", .type = lamusica::session::ChannelType::Audio});
    lamusica::session::addChannel(
        mixer, {.id = "bus-1", .name = "Bus 1", .type = lamusica::session::ChannelType::Group});
    lamusica::session::addChannel(
        mixer, {.id = "master", .name = "Master", .type = lamusica::session::ChannelType::Master});
    lamusica::session::addRoute(mixer,
                                {.sourceChannelId = "audio-1", .destinationChannelId = "bus-1"});
    lamusica::session::addRoute(mixer,
                                {.sourceChannelId = "bus-1", .destinationChannelId = "master"});
    require(lamusica::session::validateRouting(mixer), "mixer routing validates acyclic graph");
    lamusica::session::ProjectManifest mixerGraphManifest;
    mixerGraphManifest.tracks.push_back(
        {.id = "audio-1", .name = "Audio 1", .type = lamusica::session::TrackType::Audio});
    mixerGraphManifest.tracks.push_back(
        {.id = "bus-1", .name = "Bus 1", .type = lamusica::session::TrackType::Group});
    mixerGraphManifest.tracks.push_back(
        {.id = "master", .name = "Master", .type = lamusica::session::TrackType::Master});
    mixerGraphManifest.clips.push_back({.id = "mixer-clip",
                                        .trackId = "audio-1",
                                        .type = lamusica::session::ClipType::Audio,
                                        .startSample = 0,
                                        .lengthSamples = 48000});
    const auto mixerGraphUpdate =
        lamusica::session::prepareMixerGraphUpdate(mixerGraphManifest, mixer);
    require(mixerGraphUpdate.ready && mixerGraphUpdate.validationError.empty() &&
                !mixerGraphUpdate.graph.nodes.empty(),
            "mixer graph update plan compiles replacement graph off the realtime path");
    lamusica::session::MixerState sendCycleMixer;
    lamusica::session::addChannel(
        sendCycleMixer,
        {.id = "send-a",
         .name = "Send A",
         .type = lamusica::session::ChannelType::Audio,
         .sends = {{.id = "send-a-b", .destinationChannelId = "send-b", .gainDb = 0.0F}}});
    lamusica::session::addChannel(
        sendCycleMixer,
        {.id = "send-b",
         .name = "Send B",
         .type = lamusica::session::ChannelType::Return,
         .sends = {{.id = "send-b-a", .destinationChannelId = "send-a", .gainDb = 0.0F}}});
    require(!lamusica::session::validateRouting(sendCycleMixer),
            "mixer routing rejects send feedback cycle");
    const auto refusedMixerGraphUpdate =
        lamusica::session::prepareMixerGraphUpdate(mixerGraphManifest, sendCycleMixer);
    require(!refusedMixerGraphUpdate.ready &&
                refusedMixerGraphUpdate.validationError.find("feedback cycle") != std::string::npos,
            "mixer graph update plan refuses unsafe feedback before publishing");
    lamusica::session::MixerState sendEditMixer;
    lamusica::session::addChannel(sendEditMixer, {.id = "send-source",
                                                  .name = "Send Source",
                                                  .type = lamusica::session::ChannelType::Audio});
    lamusica::session::addChannel(sendEditMixer, {.id = "send-return",
                                                  .name = "Send Return",
                                                  .type = lamusica::session::ChannelType::Return});
    lamusica::session::addSend(
        sendEditMixer, "send-source",
        {.id = "send-edit-1", .destinationChannelId = "send-return", .gainDb = -6.0F});
    require(sendEditMixer.channels.front().sends.size() == 1 &&
                lamusica::session::validateRouting(sendEditMixer),
            "mixer send edit validates and publishes safe sends");
    const auto sendEditMatrix = lamusica::session::buildRoutingMatrix(sendEditMixer);
    const auto sendSourceToReturn =
        std::ranges::find_if(sendEditMatrix, [](const lamusica::session::RoutingMatrixCell& cell) {
            return cell.sourceChannelId == "send-source" &&
                   cell.destinationChannelId == "send-return";
        });
    const auto sendReturnToSource =
        std::ranges::find_if(sendEditMatrix, [](const lamusica::session::RoutingMatrixCell& cell) {
            return cell.sourceChannelId == "send-return" &&
                   cell.destinationChannelId == "send-source";
        });
    require(sendSourceToReturn != sendEditMatrix.end() && sendSourceToReturn->routeAllowed &&
                !sendSourceToReturn->sendAllowed && sendSourceToReturn->sidechainAllowed,
            "mixer routing matrix previews route send and sidechain availability");
    require(sendReturnToSource != sendEditMatrix.end() && !sendReturnToSource->routeAllowed &&
                !sendReturnToSource->sendAllowed && sendReturnToSource->wouldCreateFeedback,
            "mixer routing matrix flags feedback before mutation");
    bool rejectedDuplicateSend = false;
    try {
        lamusica::session::addSend(
            sendEditMixer, "send-source",
            {.id = "send-edit-1", .destinationChannelId = "send-return", .gainDb = -12.0F});
    } catch (const std::exception&) {
        rejectedDuplicateSend = true;
    }
    require(rejectedDuplicateSend && sendEditMixer.channels.front().sends.size() == 1,
            "mixer send edit rejects duplicate send ids without mutation");
    bool rejectedSendCycle = false;
    try {
        lamusica::session::addSend(
            sendEditMixer, "send-return",
            {.id = "send-cycle", .destinationChannelId = "send-source", .gainDb = 0.0F});
    } catch (const std::exception&) {
        rejectedSendCycle = true;
    }
    require(rejectedSendCycle && sendEditMixer.channels.back().sends.empty(),
            "mixer send edit rejects feedback before publishing");
    lamusica::session::addSidechainRoute(mixer, {.id = "kick-ducks-bus",
                                                 .sourceChannelId = "audio-1",
                                                 .destinationChannelId = "bus-1",
                                                 .targetInsertId = "bus-compressor"});
    require(mixer.sidechains.size() == 1 && lamusica::session::validateRouting(mixer),
            "mixer sidechain route validates without creating an audio feedback edge");
    bool rejectedDuplicateSidechain = false;
    try {
        lamusica::session::addSidechainRoute(mixer, {.id = "kick-ducks-bus",
                                                     .sourceChannelId = "audio-1",
                                                     .destinationChannelId = "bus-1",
                                                     .targetInsertId = "bus-compressor"});
    } catch (const std::exception&) {
        rejectedDuplicateSidechain = true;
    }
    require(rejectedDuplicateSidechain && mixer.sidechains.size() == 1,
            "mixer sidechain route rejects duplicate ids without mutation");
    bool rejectedCycle = false;
    try {
        lamusica::session::addRoute(
            mixer, {.sourceChannelId = "master", .destinationChannelId = "audio-1"});
    } catch (const std::exception&) {
        rejectedCycle = true;
    }
    require(rejectedCycle, "mixer routing rejects feedback cycle");
    const auto meter =
        lamusica::session::measureInterleaved({std::array{0.0F, 0.5F, -1.0F, 0.25F}}, 2);
    require(meter.peak == 1.0F, "mixer meter reads peak");
    require(meter.rms > 0.0F && meter.lufs < 0.0F, "mixer meter reads RMS and LUFS estimate");
    require(meter.clipped, "mixer meter detects clipping");
    lamusica::session::MeterState meterState;
    lamusica::session::updateMeter(meterState, {std::array{0.0F, 0.25F}}, 2);
    lamusica::session::updateMeter(meterState, {std::array{0.0F, 0.1F}}, 2);
    require(meterState.reading.heldPeak == 0.25F, "mixer meter holds peak");
    lamusica::session::resetMeter(meterState);
    require(meterState.reading.heldPeak == 0.0F, "mixer meter reset clears peak hold");
    lamusica::session::addFaderGroup(mixer, {.id = "drums",
                                             .name = "Drums",
                                             .channelIds = {"audio-1", "bus-1"},
                                             .linkVolume = true,
                                             .linkMute = true});
    lamusica::session::applyFaderGroupVolumeDelta(mixer, "drums", -3.0F);
    require(lamusica::session::findChannel(mixer, "audio-1")->volumeDb == -3.0F &&
                lamusica::session::findChannel(mixer, "bus-1")->volumeDb == -3.0F,
            "mixer fader group applies linked volume delta");
    lamusica::session::applyFaderGroupMute(mixer, "drums", true);
    require(lamusica::session::findChannel(mixer, "audio-1")->muted &&
                lamusica::session::findChannel(mixer, "bus-1")->muted,
            "mixer fader group applies linked mute");
    lamusica::session::addSend(
        mixer, "audio-1",
        {.id = "parallel-bus", .destinationChannelId = "master", .gainDb = -12.0F});
    const auto serializedMixer = lamusica::session::serializeMixerState(mixer);
    const auto parsedMixer = lamusica::session::parseMixerState(serializedMixer);
    require(parsedMixer.channels.size() == 3 && parsedMixer.routing.size() == 2 &&
                parsedMixer.sidechains.size() == 1 && parsedMixer.faderGroups.size() == 1 &&
                lamusica::session::validateRouting(parsedMixer),
            "mixer state saves, reloads, and keeps valid routing and sidechains");
    require(lamusica::session::findChannel(parsedMixer, "audio-1")->sends.front().gainDb ==
                    -12.0F &&
                lamusica::session::findChannel(parsedMixer, "audio-1")->muted,
            "mixer state reload preserves sends and channel strip flags");
    const auto serializedHardwareOutputMixer =
        lamusica::session::serializeMixerState(hardwareOutputMixer);
    const auto parsedHardwareOutputMixer =
        lamusica::session::parseMixerState(serializedHardwareOutputMixer);
    require(lamusica::session::findChannel(parsedHardwareOutputMixer, "output-1-2")->type ==
                    lamusica::session::ChannelType::HardwareOutput &&
                parsedHardwareOutputMixer.routing.size() == 1 &&
                lamusica::session::validateRouting(parsedHardwareOutputMixer),
            "mixer state reload preserves hardware output routing");
    lamusica::commands::SetChannelMixCommand mixCommand{
        "cmd-mix-1",
        "audit-mix-1",
        "audio-1",
        {.volumeDb = -9.0F, .pan = 0.5F, .muted = false, .solo = true}};
    require(mixCommand.apply(mixer).ok, "mixer channel mix command applies");
    require(lamusica::session::findChannel(mixer, "audio-1")->volumeDb == -9.0F &&
                lamusica::session::findChannel(mixer, "audio-1")->solo,
            "mixer channel mix command mutates channel");
    require(mixCommand.undo(mixer).ok, "mixer channel mix command undoes");
    require(lamusica::session::findChannel(mixer, "audio-1")->volumeDb == -3.0F &&
                !lamusica::session::findChannel(mixer, "audio-1")->solo,
            "mixer channel mix undo restores channel");

    lamusica::session::AutomationLaneData automation{
        .id = "auto-1",
        .targetKind = lamusica::session::AutomationTargetKind::Plugin,
        .targetId = "builtin.eq",
        .parameterId = "gain",
        .mode = lamusica::session::AutomationMode::Read,
        .defaultValue = 0.25F};
    lamusica::session::addAutomationPoint(automation, 0, 0.0F);
    lamusica::session::addAutomationPoint(automation, 100, 1.0F);
    require(lamusica::session::evaluateAutomation(automation, 50) == 0.5F,
            "automation linearly interpolates");
    bool rejectedNegativeAutomationPoint = false;
    try {
        lamusica::session::addAutomationPoint(automation, -1, 0.0F);
    } catch (const std::exception&) {
        rejectedNegativeAutomationPoint = true;
    }
    require(rejectedNegativeAutomationPoint,
            "automation rejects negative point positions before mutation");
    const auto automationBlock = lamusica::session::evaluateAutomationBlock(automation, 48, 4);
    require(automationBlock.front() == lamusica::session::evaluateAutomation(automation, 48) &&
                automationBlock.back() == lamusica::session::evaluateAutomation(automation, 51),
            "automation block evaluation is sample deterministic");
    const auto rangedAutomationPoints =
        lamusica::session::automationPointsInRange(automation, 50, 150);
    require(rangedAutomationPoints.size() == 1 &&
                rangedAutomationPoints.front().samplePosition == 100,
            "automation lane range query returns visible timeline points");
    bool rejectedBadAutomationPointRange = false;
    try {
        (void)lamusica::session::automationPointsInRange(automation, 150, 50);
    } catch (const std::exception&) {
        rejectedBadAutomationPointRange = true;
    }
    require(rejectedBadAutomationPointRange,
            "automation lane range query rejects unordered ranges");
    const auto fullAutomationBlock = lamusica::session::evaluateAutomationBlock(automation, 0, 128);
    std::vector<float> chunkedAutomationBlock;
    for (std::int64_t start = 0; start < 128;) {
        const auto frames = static_cast<std::uint32_t>(std::min<std::int64_t>(17, 128 - start));
        const auto chunk = lamusica::session::evaluateAutomationBlock(automation, start, frames);
        chunkedAutomationBlock.insert(chunkedAutomationBlock.end(), chunk.begin(), chunk.end());
        start += frames;
    }
    require(chunkedAutomationBlock == fullAutomationBlock,
            "automation playback is deterministic across buffer sizes");
    lamusica::session::addAutomationPoint(automation, 200, 0.25F,
                                          lamusica::session::AutomationCurve::Step);
    require(lamusica::session::evaluateAutomation(automation, 250) == 0.25F,
            "automation holds final value");
    automation.mode = lamusica::session::AutomationMode::Off;
    require(lamusica::session::evaluateAutomation(automation, 50) == 0.25F,
            "automation off returns default");
    automation.mode = lamusica::session::AutomationMode::Write;
    const auto writeBatch = lamusica::session::captureAutomationWrite(
        automation, {std::array{lamusica::session::AutomationWriteSample{300, 0.75F, true},
                                lamusica::session::AutomationWriteSample{301, 0.8F, true}}});
    require(writeBatch.points.size() == 2 &&
                lamusica::session::evaluateAutomation(automation, 300) == 0.75F,
            "automation write mode captures command batch points");
    automation.mode = lamusica::session::AutomationMode::Touch;
    const auto touchBatch = lamusica::session::captureAutomationWrite(
        automation, {std::array{lamusica::session::AutomationWriteSample{400, 0.1F, false},
                                lamusica::session::AutomationWriteSample{401, 0.2F, true}}});
    require(touchBatch.points.size() == 1 && touchBatch.points.front().samplePosition == 401,
            "automation touch mode writes only touched samples");
    automation.mode = lamusica::session::AutomationMode::Latch;
    const auto latchBatch = lamusica::session::captureAutomationWrite(
        automation, {std::array{lamusica::session::AutomationWriteSample{500, 0.3F, false},
                                lamusica::session::AutomationWriteSample{501, 0.4F, true},
                                lamusica::session::AutomationWriteSample{502, 0.9F, false}}});
    require(latchBatch.points.size() == 2 && latchBatch.points.back().value == 0.4F,
            "automation latch mode holds touched value");
    automation.mode = lamusica::session::AutomationMode::Trim;
    const auto trimBase = lamusica::session::evaluateAutomation(automation, 600);
    const auto trimBatch = lamusica::session::captureAutomationWrite(
        automation, {std::array{lamusica::session::AutomationWriteSample{600, 0.1F, true}}});
    require(trimBatch.points.size() == 1 &&
                lamusica::session::evaluateAutomation(automation, 600) == trimBase + 0.1F,
            "automation trim mode offsets existing value");
    require(lamusica::session::effectiveAutomationValue(automation, 600, 2.0F) ==
                2.0F + lamusica::session::evaluateAutomation(automation, 600),
            "automation trim playback offsets current parameter value");
    automation.mode = lamusica::session::AutomationMode::Off;
    require(lamusica::session::effectiveAutomationValue(automation, 600, 2.0F) == 2.0F,
            "automation off playback preserves current parameter value");
    automation.mode = lamusica::session::AutomationMode::Read;
    const std::array selectionLanes{automation};
    const auto* selectedAutomation = lamusica::session::selectAutomationLane(
        selectionLanes, {.targetKind = lamusica::session::AutomationTargetKind::Plugin,
                         .targetId = "builtin.eq",
                         .parameterId = "gain"});
    require(selectedAutomation != nullptr && selectedAutomation->id == "auto-1",
            "automation lane selection resolves target parameter");
    const auto* mismatchedAutomation = lamusica::session::selectAutomationLane(
        selectionLanes, {.targetKind = lamusica::session::AutomationTargetKind::Mixer,
                         .targetId = "builtin.eq",
                         .parameterId = "gain"});
    require(mismatchedAutomation == nullptr, "automation lane selection honors target kind");
    lamusica::session::MixerState automationMixer;
    lamusica::session::addChannel(automationMixer, {.id = "auto-track",
                                                    .name = "Automation Track",
                                                    .type = lamusica::session::ChannelType::Audio});
    lamusica::session::AutomationLaneData volumeAutomation{
        .id = "auto-volume",
        .targetId = "auto-track",
        .parameterId = "volumeDb",
        .mode = lamusica::session::AutomationMode::Read,
        .defaultValue = 0.0F};
    lamusica::session::addAutomationPoint(volumeAutomation, 0, -12.0F);
    lamusica::session::addAutomationPoint(volumeAutomation, 100, -6.0F);
    lamusica::session::AutomationLaneData panAutomation{.id = "auto-pan",
                                                        .targetId = "auto-track",
                                                        .parameterId = "pan",
                                                        .mode =
                                                            lamusica::session::AutomationMode::Read,
                                                        .defaultValue = 0.0F};
    lamusica::session::addAutomationPoint(panAutomation, 0, 2.0F);
    lamusica::session::addAutomationPoint(panAutomation, 100, 2.0F);
    lamusica::session::AutomationLaneData muteAutomation{
        .id = "auto-mute",
        .targetId = "auto-track",
        .parameterId = "mute",
        .mode = lamusica::session::AutomationMode::Read,
        .defaultValue = 0.0F};
    lamusica::session::addAutomationPoint(muteAutomation, 0, 1.0F);
    lamusica::session::addAutomationPoint(muteAutomation, 100, 1.0F);
    const std::array automationLanes{volumeAutomation, panAutomation, muteAutomation};
    lamusica::session::applyAutomationBlockToMixer(automationMixer, automationLanes, 50);
    require(lamusica::session::findChannel(automationMixer, "auto-track")->volumeDb == -9.0F,
            "automation applies interpolated mixer volume");
    require(lamusica::session::findChannel(automationMixer, "auto-track")->pan == 1.0F,
            "automation clamps and applies mixer pan");
    require(lamusica::session::findChannel(automationMixer, "auto-track")->muted,
            "automation applies boolean mixer parameters");
    lamusica::session::AutomationLaneData offVolumeAutomation = volumeAutomation;
    offVolumeAutomation.mode = lamusica::session::AutomationMode::Off;
    lamusica::session::findChannel(automationMixer, "auto-track")->volumeDb = -18.0F;
    lamusica::session::applyAutomationToMixer(automationMixer, offVolumeAutomation, 50);
    require(lamusica::session::findChannel(automationMixer, "auto-track")->volumeDb == -18.0F,
            "automation off mode does not overwrite mixer playback values");
    const auto mixerAutomationBindings =
        lamusica::session::automationBindingsForMixer(automationMixer);
    require(std::ranges::any_of(mixerAutomationBindings,
                                [](const lamusica::session::AutomationParameterBinding& binding) {
                                    return binding.targetId == "auto-track" &&
                                           binding.parameterId == "volumeDb" &&
                                           binding.minimumValue < binding.maximumValue;
                                }),
            "automation exposes mixer-linked parameter bindings");
    bool rejectedAutomationTarget = false;
    try {
        lamusica::session::applyAutomationToMixer(automationMixer,
                                                  {.id = "bad-auto",
                                                   .targetId = "auto-track",
                                                   .parameterId = "unsupported",
                                                   .mode = lamusica::session::AutomationMode::Read,
                                                   .defaultValue = 0.0F},
                                                  0);
    } catch (const std::exception&) {
        rejectedAutomationTarget = true;
    }
    require(rejectedAutomationTarget, "automation rejects unsupported mixer parameter");
    lamusica::session::PluginInsertChain automationPluginChain{.trackId = "auto-track"};
    lamusica::session::addInsert(automationPluginChain,
                                 {.id = "auto-insert", .pluginIdentifier = "builtin.eq"});
    lamusica::session::AutomationLaneData pluginAutomation{
        .id = "auto-plugin-gain",
        .targetKind = lamusica::session::AutomationTargetKind::Plugin,
        .targetId = "auto-insert",
        .parameterId = "gain",
        .mode = lamusica::session::AutomationMode::Read,
        .defaultValue = 0.0F};
    lamusica::session::addAutomationPoint(pluginAutomation, 0, 0.0F);
    lamusica::session::addAutomationPoint(pluginAutomation, 100, 1.0F);
    lamusica::session::applyAutomationToPluginChain(automationPluginChain, pluginAutomation, 25);
    require(lamusica::session::findParameterValue(automationPluginChain.inserts.front(), "gain") ==
                0.25F,
            "automation applies interpolated plugin parameter value");
    const auto pluginAutomationBindings = lamusica::session::automationBindingsForPluginChain(
        automationPluginChain, pluginCache, lamusica::session::AutomationTargetKind::Plugin);
    require(pluginAutomationBindings.size() == 1 &&
                pluginAutomationBindings.front().targetId == "auto-insert" &&
                pluginAutomationBindings.front().parameterId == "gain" &&
                pluginAutomationBindings.front().defaultValue == 0.25F,
            "automation exposes plugin parameter bindings with current values");
    lamusica::session::AutomationLaneData instrumentAutomation = pluginAutomation;
    instrumentAutomation.id = "auto-instrument-gain";
    instrumentAutomation.targetKind = lamusica::session::AutomationTargetKind::Instrument;
    instrumentAutomation.mode = lamusica::session::AutomationMode::Trim;
    lamusica::session::applyAutomationToInstrumentChain(automationPluginChain, instrumentAutomation,
                                                        25);
    require(lamusica::session::findParameterValue(automationPluginChain.inserts.front(), "gain") ==
                0.5F,
            "automation applies instrument parameter trim through plugin insert chain");
    bool rejectedPluginAutomationTarget = false;
    try {
        lamusica::session::applyAutomationToPluginChain(
            automationPluginChain,
            {.id = "bad-plugin-auto",
             .targetKind = lamusica::session::AutomationTargetKind::Plugin,
             .targetId = "missing-insert",
             .parameterId = "gain",
             .mode = lamusica::session::AutomationMode::Read,
             .defaultValue = 0.0F},
            0);
    } catch (const std::exception&) {
        rejectedPluginAutomationTarget = true;
    }
    require(rejectedPluginAutomationTarget, "automation rejects missing plugin insert target");
    lamusica::session::Clip automationClip{.id = "auto-clip",
                                           .trackId = "auto-track",
                                           .type = lamusica::session::ClipType::Audio,
                                           .startSample = 0,
                                           .lengthSamples = 1000,
                                           .gainDb = -3.0F};
    lamusica::session::AutomationLaneData clipGainAutomation{
        .id = "auto-clip-gain",
        .targetKind = lamusica::session::AutomationTargetKind::Clip,
        .targetId = "auto-clip",
        .parameterId = "gainDb",
        .mode = lamusica::session::AutomationMode::Read,
        .defaultValue = 0.0F};
    lamusica::session::addAutomationPoint(clipGainAutomation, 0, -9.0F);
    lamusica::session::addAutomationPoint(clipGainAutomation, 100, -3.0F);
    lamusica::session::AutomationLaneData clipMuteAutomation{
        .id = "auto-clip-mute",
        .targetKind = lamusica::session::AutomationTargetKind::Clip,
        .targetId = "auto-clip",
        .parameterId = "muted",
        .mode = lamusica::session::AutomationMode::Read,
        .defaultValue = 0.0F};
    lamusica::session::addAutomationPoint(clipMuteAutomation, 0, 1.0F);
    lamusica::session::addAutomationPoint(clipMuteAutomation, 100, 1.0F);
    const std::array clipAutomationLanes{clipGainAutomation, clipMuteAutomation};
    lamusica::session::applyAutomationBlockToClips({&automationClip, 1}, clipAutomationLanes, 50);
    require(automationClip.gainDb == -6.0F && automationClip.muted,
            "automation applies clip gain and boolean parameters");
    const auto clipAutomationBindings =
        lamusica::session::automationBindingsForClip(automationClip);
    require(std::ranges::any_of(clipAutomationBindings,
                                [](const lamusica::session::AutomationParameterBinding& binding) {
                                    return binding.targetKind ==
                                               lamusica::session::AutomationTargetKind::Clip &&
                                           binding.parameterId == "sourceOffsetSamples";
                                }),
            "automation exposes clip parameter bindings for timeline lanes");

    lamusica::session::WarpState warp{
        .clipId = "clip-warp",
        .enabled = true,
        .sourceTempoBpm = 120.0,
        .targetTempoBpm = 60.0,
        .pitchShiftSemitones = 2.0F,
        .markers = {{.id = "w1", .sourceSample = 0, .timelineSample = 0},
                    {.id = "w2", .sourceSample = 48000, .timelineSample = 96000}}};
    lamusica::session::validateWarpState(warp);
    require(lamusica::session::conformSampleToTempo(48000, 120.0, 60.0) == 96000,
            "warp tempo conform maps sample duration");
    require(lamusica::session::mapSourceToTimeline(warp, 24000) == 48000,
            "warp markers interpolate source to timeline");
    const auto retargetedWarp = lamusica::session::retargetWarpTempo(warp, 120.0);
    require(retargetedWarp.clipId == warp.clipId &&
                retargetedWarp.markers.front().id == warp.markers.front().id &&
                retargetedWarp.pitchShiftSemitones == warp.pitchShiftSemitones &&
                retargetedWarp.quality == warp.quality &&
                retargetedWarp.markers.back().timelineSample == 48000,
            "warp tempo retarget preserves edit metadata while stretching markers");
    const auto transients =
        lamusica::session::detectTransients(std::array{0.0F, 0.1F, 0.9F, 0.92F, -0.2F}, 0.5F);
    require(transients.size() == 2, "warp transient detector finds threshold crossings");
    const auto conformedWarp = lamusica::session::conformWarpToTempo(
        {.clipId = "conform-warp",
         .enabled = false,
         .sourceTempoBpm = 120.0,
         .targetTempoBpm = 120.0,
         .pitchShiftSemitones = -2.0F,
         .quality = lamusica::session::StretchQuality::High},
        {std::array{lamusica::session::Transient{.sourceSample = 12000, .strength = 0.8F},
                    lamusica::session::Transient{.sourceSample = 24000, .strength = 0.7F}}},
        48000, 60.0);
    require(conformedWarp.enabled && conformedWarp.targetTempoBpm == 60.0 &&
                conformedWarp.pitchShiftSemitones == -2.0F &&
                conformedWarp.quality == lamusica::session::StretchQuality::High &&
                conformedWarp.markers.size() == 4 &&
                conformedWarp.markers[1].sourceSample == 12000 &&
                conformedWarp.markers[1].timelineSample == 24000 &&
                conformedWarp.markers.back().timelineSample == 96000,
            "warp tempo conform seeds editable transient markers without losing metadata");
    require(lamusica::session::makeWarpCacheKey(conformedWarp) !=
                lamusica::session::makeWarpCacheKey(retargetedWarp),
            "warp tempo conform changes render-cache identity with marker metadata");
    const auto slices = lamusica::session::makeBeatSlices(warp, transients, 48000);
    require(slices.size() == 2 && slices.front().sourceStartSample == 2,
            "warp beat slicing creates transient slices");
    const auto groove = lamusica::session::extractGroove("groove-1", transients, 3);
    require(groove.points.size() == 2 && groove.points.back().offsetSamples == 1,
            "warp groove extraction stores offsets from grid");
    lamusica::session::WarpState groovedWarp{
        .clipId = "grooved-warp",
        .enabled = true,
        .sourceTempoBpm = 120.0,
        .targetTempoBpm = 120.0,
        .markers = {{.id = "g1", .sourceSample = 0, .timelineSample = 0},
                    {.id = "g2", .sourceSample = 1000, .timelineSample = 6}}};
    lamusica::session::applyGrooveToWarpMarkers(
        groovedWarp,
        {.id = "apply-groove",
         .points = {{.gridSample = 0, .offsetSamples = 2, .strength = 1.0F},
                    {.gridSample = 6, .offsetSamples = -2, .strength = 1.0F}}},
        1.0F);
    require(groovedWarp.markers.front().timelineSample == 2 &&
                groovedWarp.markers.back().timelineSample == 4,
            "warp groove application moves markers onto extracted groove offsets");
    lamusica::session::WarpState quantizedWarp{
        .clipId = "quantized-warp",
        .enabled = true,
        .sourceTempoBpm = 120.0,
        .targetTempoBpm = 120.0,
        .markers = {{.id = "q1", .sourceSample = 0, .timelineSample = 120},
                    {.id = "q2", .sourceSample = 1000, .timelineSample = 880}}};
    lamusica::session::quantizeWarpMarkers(quantizedWarp, 500, 1.0F);
    require(quantizedWarp.markers.front().timelineSample == 0 &&
                quantizedWarp.markers.back().timelineSample == 1000,
            "warp marker quantize moves markers to grid");
    require(lamusica::session::quantizeSampleToGrid(750, 1000, 0.5F) == 875,
            "warp quantize strength moves partially toward grid");
    lamusica::commands::WarpStateStore warpCommandStore;
    lamusica::commands::AddWarpMarkerCommand addWarpMarker{
        "warp-cmd-add",
        "warp-audit-add",
        {.clipId = "cmd-warp", .enabled = true, .sourceTempoBpm = 120.0, .targetTempoBpm = 120.0},
        {.id = "cmd-w1", .sourceSample = 100, .timelineSample = 120}};
    require(addWarpMarker.apply(warpCommandStore).ok, "warp marker add command applies");
    require(warpCommandStore.find("cmd-warp")->markers.size() == 1,
            "warp marker add command mutates store");
    lamusica::commands::MoveWarpMarkerCommand moveWarpMarker{
        "warp-cmd-move", "warp-audit-move", "cmd-warp", "cmd-w1", 100, 260};
    require(moveWarpMarker.apply(warpCommandStore).ok, "warp marker move command applies");
    require(warpCommandStore.find("cmd-warp")->markers.front().timelineSample == 260,
            "warp marker move command changes marker");
    require(moveWarpMarker.undo(warpCommandStore).ok, "warp marker move command undoes");
    require(warpCommandStore.find("cmd-warp")->markers.front().timelineSample == 120,
            "warp marker move undo restores marker");
    lamusica::commands::QuantizeWarpMarkersCommand quantizeWarpMarkers{
        "warp-cmd-quantize", "warp-audit-quantize", "cmd-warp", 500, 1.0F};
    require(quantizeWarpMarkers.apply(warpCommandStore).ok, "warp marker quantize command applies");
    require(warpCommandStore.find("cmd-warp")->markers.front().timelineSample == 0,
            "warp marker quantize command changes marker");
    require(quantizeWarpMarkers.undo(warpCommandStore).ok, "warp marker quantize command undoes");
    require(warpCommandStore.find("cmd-warp")->markers.front().timelineSample == 120,
            "warp marker quantize undo restores marker");
    lamusica::commands::ApplyWarpGrooveCommand applyWarpGroove{
        "warp-cmd-groove",
        "warp-audit-groove",
        "cmd-warp",
        {.id = "cmd-groove",
         .points = {{.gridSample = 0, .offsetSamples = 25, .strength = 1.0F},
                    {.gridSample = 500, .offsetSamples = -25, .strength = 1.0F}}},
        1.0F};
    require(applyWarpGroove.apply(warpCommandStore).ok, "warp groove command applies");
    require(warpCommandStore.find("cmd-warp")->markers.front().timelineSample == 25,
            "warp groove command changes marker timing");
    require(applyWarpGroove.undo(warpCommandStore).ok, "warp groove command undoes");
    require(warpCommandStore.find("cmd-warp")->markers.front().timelineSample == 120,
            "warp groove undo restores marker timing");
    lamusica::commands::RemoveWarpMarkerCommand removeWarpMarker{
        "warp-cmd-remove", "warp-audit-remove", "cmd-warp", "cmd-w1"};
    require(removeWarpMarker.apply(warpCommandStore).ok, "warp marker remove command applies");
    require(warpCommandStore.find("cmd-warp")->markers.empty(),
            "warp marker remove command mutates store");
    require(removeWarpMarker.undo(warpCommandStore).ok, "warp marker remove command undoes");
    require(warpCommandStore.find("cmd-warp")->markers.front().id == "cmd-w1",
            "warp marker remove undo restores marker");
    require(addWarpMarker.undo(warpCommandStore).ok, "warp marker add command undoes");
    require(warpCommandStore.find("cmd-warp")->markers.empty(),
            "warp marker add undo removes marker");
    std::vector<lamusica::session::RenderCacheEntry> renderCache;
    lamusica::session::upsertRenderCacheEntry(
        renderCache, {.clipId = "clip-warp",
                      .cacheKey = lamusica::session::makeWarpCacheKey(warp),
                      .relativePath = "Cache/clip-warp.wav",
                      .valid = true});
    require(renderCache.size() == 1 && renderCache.front().valid, "warp render cache upserts");
    const auto cachedWarpPlan = lamusica::session::makeWarpRenderPlan(warp, renderCache, 0, 48000,
                                                                      "Cache/new-warp-render.wav");
    require(cachedWarpPlan.cacheHit && cachedWarpPlan.relativePath == "Cache/clip-warp.wav",
            "warp render plan reuses valid cache entry");
    require(cachedWarpPlan.timelineEndSample == 96000 && cachedWarpPlan.stretchRatio == 2.0,
            "warp render plan maps source range to timeline range");
    require(std::abs(cachedWarpPlan.pitchRatio - lamusica::session::pitchShiftRatio(2.0F)) <
                0.000001,
            "warp render plan records pitch shift ratio");
    const auto previewWarpPlan =
        lamusica::session::makeWarpRenderPlan(warp, renderCache, 0, 48000, "Cache/preview.wav");
    require(lamusica::session::warpRenderPlansAgree(cachedWarpPlan, previewWarpPlan, 0),
            "warp live preview and offline render plans agree within tolerance");
    const lamusica::audio::RenderedAudio warpSource{
        .channels = 1, .frames = 4, .interleavedSamples = {0.0F, 1.0F, 0.0F, -1.0F}};
    const lamusica::session::WarpState renderWarp{
        .clipId = "clip-render-warp",
        .enabled = true,
        .sourceTempoBpm = 120.0,
        .targetTempoBpm = 60.0,
        .markers = {{.id = "rw1", .sourceSample = 0, .timelineSample = 0},
                    {.id = "rw2", .sourceSample = 4, .timelineSample = 8}}};
    const auto offlineRenderPlan =
        lamusica::session::makeWarpRenderPlan(renderWarp, {}, 0, 4, "Cache/render.wav");
    const auto offlineWarped = lamusica::session::renderWarpedAudio(warpSource, offlineRenderPlan);
    const auto previewWarped = lamusica::session::renderWarpPreview(warpSource, offlineRenderPlan);
    require(offlineWarped.frames == 8 &&
                std::abs(offlineWarped.interleavedSamples[1] - 1.0F) < 0.00001F &&
                std::abs(offlineWarped.interleavedSamples[4] + 1.0F) < 0.00001F,
            "warp audio render time-stretches source samples deterministically");
    require(offlineWarped.interleavedSamples == previewWarped.interleavedSamples,
            "warp audio render matches live preview samples");
    auto pitchedRenderWarp = renderWarp;
    pitchedRenderWarp.pitchShiftSemitones = 12.0F;
    const auto pitchedRenderPlan =
        lamusica::session::makeWarpRenderPlan(pitchedRenderWarp, {}, 0, 4, "Cache/pitch.wav");
    const auto pitchedWarped = lamusica::session::renderWarpedAudio(warpSource, pitchedRenderPlan);
    require(pitchedWarped.frames == offlineWarped.frames &&
                pitchedWarped.interleavedSamples != offlineWarped.interleavedSamples,
            "warp audio render applies pitch-shift ratio during resampling");
    lamusica::session::invalidateRenderCache(renderCache, "clip-warp");
    require(!renderCache.front().valid, "warp render cache invalidates by clip");
    const auto uncachedWarpPlan = lamusica::session::makeWarpRenderPlan(
        warp, renderCache, 0, 48000, "Cache/new-warp-render.wav");
    require(!uncachedWarpPlan.cacheHit &&
                uncachedWarpPlan.relativePath == "Cache/new-warp-render.wav",
            "warp render plan falls back to requested output path on cache miss");
    bool rejectedEmptyWarpPlan = false;
    try {
        (void)lamusica::session::makeWarpRenderPlan(warp, renderCache, 100, 100, "Cache/bad.wav");
    } catch (const std::exception&) {
        rejectedEmptyWarpPlan = true;
    }
    require(rejectedEmptyWarpPlan, "warp render plan rejects empty source range");

    const auto assetRoot = std::filesystem::temp_directory_path() / "lamusica-assets-test";
    std::filesystem::remove_all(assetRoot);
    std::filesystem::create_directories(assetRoot / "Audio");
    {
        std::ofstream assetFile{assetRoot / "Audio" / "kick.wav"};
        assetFile << "fixture";
    }
    lamusica::session::AssetCatalog catalog{
        .projectRoot = assetRoot,
        .assets = {{.id = "kick",
                    .relativePath = "Audio/kick.wav",
                    .kind = lamusica::session::AssetKind::Audio,
                    .tags = {"drum", "kick"}},
                   {.id = "missing",
                    .relativePath = "Audio/missing.wav",
                    .kind = lamusica::session::AssetKind::Audio}}};
    lamusica::session::markMissingAssets(catalog);
    require(!lamusica::session::findAsset(catalog, "kick")->missing,
            "asset catalog keeps found asset");
    require(lamusica::session::findAsset(catalog, "missing")->missing,
            "asset catalog marks missing asset");
    lamusica::session::relinkAsset(catalog, "missing", "Audio/kick.wav");
    require(!lamusica::session::findAsset(catalog, "missing")->missing,
            "asset relink repairs missing asset");
    lamusica::session::upsertAnalysis(
        catalog, {.assetId = "kick", .durationSamples = 48000, .tempoBpm = 120.0});
    require(catalog.analyses.size() == 1, "asset analysis upserts");
    require(lamusica::session::searchAssets(catalog, "drum").size() == 1,
            "asset search matches tags");
    lamusica::session::findAsset(catalog, "kick")->favorite = true;
    require(lamusica::session::favoriteAssets(catalog).size() == 1,
            "asset catalog filters favorites");
    const auto preview = lamusica::session::makeAssetPreview(catalog, "kick");
    require(preview.available && preview.analysis.has_value() &&
                preview.analysis->durationSamples == 48000,
            "asset preview resolves availability and analysis");
    lamusica::audio::RenderedAudio analyzedAudio{
        .channels = 1,
        .frames = 8,
        .interleavedSamples = {0.0F, 0.01F, 0.02F, 0.8F, -0.5F, 0.1F, 0.0F, -0.2F}};
    const auto analysisResult =
        lamusica::session::analyzeAudioAsset("kick", analyzedAudio, 48000.0, 4);
    require(analysisResult.analysis.durationSamples == 8 && analysisResult.analysis.channels == 1 &&
                analysisResult.analysis.peakAmplitude == 0.8F,
            "asset media analysis measures duration channels and peak");
    require(analysisResult.analysis.sampleRate == 48000.0 &&
                analysisResult.waveform.sampleRate == 48000.0,
            "asset media analysis tags analysis and waveform with sample rate");
    require(analysisResult.analysis.rmsAmplitude > 0.0F &&
                analysisResult.analysis.loudnessLufs < 0.0F,
            "asset media analysis estimates RMS and loudness");
    require(analysisResult.analysis.transientSamples.size() == 1 &&
                analysisResult.analysis.transientSamples.front() == 3,
            "asset media analysis detects transient onsets");
    require(analysisResult.waveform.buckets.size() == 2 &&
                analysisResult.waveform.buckets.front().maxSample == 0.8F &&
                analysisResult.waveform.buckets.back().minSample == -0.5F,
            "asset media analysis builds waveform buckets");
    lamusica::audio::RenderedAudio tempoAudio{
        .channels = 1, .frames = 48001, .interleavedSamples = std::vector<float>(48001)};
    tempoAudio.interleavedSamples[0] = 1.0F;
    tempoAudio.interleavedSamples[24000] = 1.0F;
    tempoAudio.interleavedSamples[48000] = 1.0F;
    const auto tempoAnalysis =
        lamusica::session::analyzeAudioAsset("tempo", tempoAudio, 48000.0, 12000);
    require(std::abs(tempoAnalysis.analysis.tempoBpm - 120.0) < 0.001 &&
                tempoAnalysis.analysis.transientSamples.size() == 3,
            "asset media analysis estimates tempo from transient spacing");
    lamusica::audio::RenderedAudio keyAudio{
        .channels = 1, .frames = 4800, .interleavedSamples = std::vector<float>(4800)};
    for (std::uint32_t frame = 0; frame < keyAudio.frames; ++frame) {
        keyAudio.interleavedSamples[frame] = static_cast<float>(
            std::sin(2.0 * std::numbers::pi * 440.0 * static_cast<double>(frame) / 48000.0));
    }
    const auto keyAnalysis = lamusica::session::analyzeAudioAsset("key-a", keyAudio, 48000.0, 512);
    require(keyAnalysis.analysis.musicalKey == "A major",
            "asset media analysis estimates musical key from pitch content");
    const auto mediaAnalysisJob =
        lamusica::session::scheduleMediaAnalysis(catalog, "analysis-1", "kick");
    require(mediaAnalysisJob.status == lamusica::session::MediaAnalysisJobStatus::Pending &&
                catalog.analysisJobs.size() == 1 && catalog.analyses.size() == 1,
            "asset media analysis scheduling does not block by computing inline");
    lamusica::session::upsertAnalysis(catalog, analysisResult.analysis);
    lamusica::session::upsertWaveform(catalog, analysisResult.waveform);
    require(lamusica::session::findWaveform(catalog, "kick")->valid,
            "asset waveform overview upserts as valid cache");
    require(lamusica::session::waveformOverviewMatchesSampleRate(
                *lamusica::session::findWaveform(catalog, "kick"), 48000.0),
            "asset waveform cache matches its computed sample rate");
    lamusica::session::invalidateAnalysisCachesForSampleRate(catalog, 44100.0);
    require(lamusica::session::findAnalysis(catalog, "kick") == nullptr &&
                !lamusica::session::waveformOverviewMatchesSampleRate(
                    *lamusica::session::findWaveform(catalog, "kick"), 44100.0),
            "project sample-rate change invalidates stale analysis and waveform cache");
    lamusica::session::upsertAnalysis(catalog, analysisResult.analysis);
    lamusica::session::upsertWaveform(catalog, analysisResult.waveform);
    const auto completedAnalysis =
        lamusica::session::analyzeAudioAsset("kick", analyzedAudio, 48000.0, 4);
    lamusica::session::completeMediaAnalysis(catalog, "analysis-1", completedAnalysis);
    require(catalog.analysisJobs.front().status ==
                    lamusica::session::MediaAnalysisJobStatus::Completed &&
                catalog.analysisJobs.front().result.has_value(),
            "asset media analysis completion stores result off the realtime path");
    lamusica::session::relinkAsset(catalog, "kick", "Audio/kick.wav");
    require(lamusica::session::findAnalysis(catalog, "kick") == nullptr,
            "asset relink invalidates stale analysis");
    require(!lamusica::session::findWaveform(catalog, "kick")->valid,
            "asset relink invalidates waveform cache");
    const auto importPlan =
        lamusica::session::planAssetImport(catalog, assetRoot / "Audio" / "snare.wav", "snare",
                                           lamusica::session::AssetKind::Audio, {"drum", "snare"});
    require(importPlan.copyIntoProject && importPlan.record.relativePath == "Assets/snare.wav",
            "asset import plan collects media into project assets folder");
    lamusica::session::addAsset(catalog, importPlan.record);
    require(lamusica::session::findAsset(catalog, "snare")->missing,
            "asset import record tracks missing destination until copied");
    const auto collisionImportPlan =
        lamusica::session::planAssetImport(catalog, assetRoot / "Audio" / "snare.wav", "snare-alt",
                                           lamusica::session::AssetKind::Audio);
    require(collisionImportPlan.record.relativePath == "Assets/snare-2.wav",
            "asset import plan avoids collected filename collisions");
    const auto importSourcePath = assetRoot / "Audio" / "clap.wav";
    lamusica::audio::writePcm16Wav(importSourcePath, analyzedAudio, 48000.0);
    require(
        lamusica::session::isSupportedAudioImportExtension(importSourcePath) &&
            !lamusica::session::isSupportedAudioImportExtension(assetRoot / "Audio" / "clap.mp3"),
        "audio import declares currently decodable extensions");
    const auto importedAudio =
        lamusica::session::importAudioAsset(catalog, {.sourcePath = importSourcePath,
                                                      .assetId = "clap",
                                                      .tags = {"drum", "clap"},
                                                      .samplesPerWaveformBucket = 4});
    require(importedAudio.plan.copyIntoProject &&
                importedAudio.plan.record.relativePath == "Assets/clap.wav" &&
                std::filesystem::exists(assetRoot / "Assets" / "clap.wav"),
            "audio import copies supported media into project assets");
    require(lamusica::session::findAsset(catalog, "clap") != nullptr &&
                !lamusica::session::findAsset(catalog, "clap")->missing,
            "audio import registers available asset record");
    require(lamusica::session::findAnalysis(catalog, "clap")->durationSamples == 8 &&
                lamusica::session::findWaveform(catalog, "clap")->buckets.size() == 2,
            "audio import stores decoded analysis and waveform overview");
    const auto mixedRateSourcePath = assetRoot / "Audio" / "mixed-rate.wav";
    lamusica::audio::RenderedAudio mixedRateAudio{
        .channels = 1, .frames = 4410, .interleavedSamples = std::vector<float>(4410)};
    for (std::uint32_t frame = 0; frame < mixedRateAudio.frames; ++frame) {
        mixedRateAudio.interleavedSamples[frame] = static_cast<float>(
            0.5 * std::sin(2.0 * std::numbers::pi * 440.0 * static_cast<double>(frame) /
                           44100.0));
    }
    lamusica::audio::writePcm16Wav(mixedRateSourcePath, mixedRateAudio, 44100.0);
    const auto mixedRateImport = lamusica::session::importAudioAsset(
        catalog, {.sourcePath = mixedRateSourcePath,
                  .assetId = "mixed-rate",
                  .projectSampleRate = 48000.0,
                  .importPolicy = lamusica::session::AudioAssetImportPolicy::ResampleToProjectRate,
                  .samplesPerWaveformBucket = 480});
    const auto mixedRateDestination =
        lamusica::audio::readPcm16Wav(assetRoot / mixedRateImport.plan.record.relativePath);
    require(mixedRateImport.plan.record.sourceSampleRate == 44100.0 &&
                mixedRateImport.plan.record.resampledToProjectRate &&
                mixedRateDestination.sampleRate == 48000.0 &&
                mixedRateDestination.audio.frames == 4800 &&
                mixedRateImport.analysis.analysis.sampleRate == 48000.0 &&
                mixedRateImport.analysis.analysis.durationSamples == 4800,
            "audio import can resample mixed-rate assets to the project rate");
    bool rejectedUnsupportedImport = false;
    try {
        (void)lamusica::session::importAudioAsset(
            catalog, {.sourcePath = assetRoot / "Audio" / "clap.mp3", .assetId = "bad-import"});
    } catch (const std::exception&) {
        rejectedUnsupportedImport = true;
    }
    require(rejectedUnsupportedImport, "audio import rejects unsupported formats before register");
    lamusica::session::grantUserFolder(
        catalog, {.id = "samples", .absolutePath = assetRoot / "Audio", .recursive = true});
    require(
        lamusica::session::isPathInsideGrantedUserFolder(catalog, assetRoot / "Audio" / "kick.wav"),
        "asset browser recognizes explicitly granted user folder paths");
    require(!lamusica::session::isPathInsideGrantedUserFolder(catalog,
                                                              assetRoot / "Private" / "kick.wav"),
            "asset browser rejects ungranted user folder paths");
    const auto scanPlan = lamusica::session::planUserFolderScan(catalog, "samples");
    require(scanPlan.absoluteRoot ==
                    std::filesystem::absolute(assetRoot / "Audio").lexically_normal() &&
                scanPlan.recursive,
            "asset browser only plans scans for explicitly granted user folders");
    bool rejectedUngrantedScan = false;
    try {
        (void)lamusica::session::planUserFolderScan(catalog, "private");
    } catch (const std::exception&) {
        rejectedUngrantedScan = true;
    }
    require(rejectedUngrantedScan, "asset browser rejects scans without a user folder grant");
    lamusica::session::recordRecentBrowserItem(
        catalog, {.id = "kick",
                  .kind = lamusica::session::RecentBrowserItemKind::Asset,
                  .path = "Audio/kick.wav",
                  .lastUsedUnixSeconds = 10});
    lamusica::session::recordRecentBrowserItem(
        catalog, {.id = "snare",
                  .kind = lamusica::session::RecentBrowserItemKind::Asset,
                  .path = "Assets/snare.wav",
                  .lastUsedUnixSeconds = 20});
    lamusica::session::recordRecentBrowserItem(
        catalog, {.id = "kick",
                  .kind = lamusica::session::RecentBrowserItemKind::Asset,
                  .path = "Audio/kick.wav",
                  .lastUsedUnixSeconds = 30});
    const auto recentAssets = lamusica::session::recentBrowserItems(
        catalog, lamusica::session::RecentBrowserItemKind::Asset);
    require(recentAssets.size() == 2 && recentAssets.front().id == "kick",
            "asset browser recent items dedupe and sort by recency");
    lamusica::session::addAsset(catalog, {.id = "kit",
                                          .relativePath = "Audio/kick.wav",
                                          .kind = lamusica::session::AssetKind::DrumKit});
    lamusica::session::addAsset(catalog, {.id = "preset",
                                          .relativePath = "Audio/kick.wav",
                                          .kind = lamusica::session::AssetKind::Preset});
    lamusica::session::addAsset(catalog, {.id = "template",
                                          .relativePath = "Audio/kick.wav",
                                          .kind = lamusica::session::AssetKind::Template});
    lamusica::session::recordRecentBrowserItem(
        catalog, {.id = "template",
                  .kind = lamusica::session::RecentBrowserItemKind::Template,
                  .path = "Audio/kick.wav",
                  .lastUsedUnixSeconds = 40});
    const auto browserSections = lamusica::session::buildBrowserSections(catalog);
    auto browserSection = [&browserSections](lamusica::session::BrowserSectionKind kind)
        -> const lamusica::session::BrowserSection& {
        const auto found = std::ranges::find_if(
            browserSections, [kind](const auto& section) { return section.kind == kind; });
        require(found != browserSections.end(), "asset browser exposes required section");
        return *found;
    };
    require(browserSection(lamusica::session::BrowserSectionKind::ProjectMedia).items.size() >= 2,
            "asset browser sections include project media assets");
    require(browserSection(lamusica::session::BrowserSectionKind::UserFolders).items.size() == 1,
            "asset browser sections include granted user folders");
    require(browserSection(lamusica::session::BrowserSectionKind::PluginPresets).items.size() == 1,
            "asset browser sections include plugin presets");
    require(browserSection(lamusica::session::BrowserSectionKind::DrumKits).items.size() == 1,
            "asset browser sections include drum kits");
    require(browserSection(lamusica::session::BrowserSectionKind::Templates).items.size() == 1,
            "asset browser sections include templates");
    const auto& recentSection =
        browserSection(lamusica::session::BrowserSectionKind::RecentFiles).items;
    require(!recentSection.empty() && recentSection.front().id == "template" &&
                recentSection.front().assetKind == lamusica::session::AssetKind::Template,
            "asset browser sections include recent files with resolved asset metadata");
    const auto timelineDrop = lamusica::session::planBrowserDrop(
        catalog, {.assetId = "kick",
                  .destination = lamusica::session::BrowserDropDestination::Timeline,
                  .targetId = "track-1",
                  .timelineSample = 48000});
    require(timelineDrop.createsProjectClip && timelineDrop.timelineSample == 48000,
            "asset browser plans timeline drops as project clips");
    const auto drumDrop = lamusica::session::planBrowserDrop(
        catalog, {.assetId = "kick",
                  .destination = lamusica::session::BrowserDropDestination::DrumPad,
                  .targetId = "pad-1"});
    require(drumDrop.assignsToInstrument, "asset browser plans drum pad sample assignment");
    const auto pluginDrop = lamusica::session::planBrowserDrop(
        catalog, {.assetId = "preset",
                  .destination = lamusica::session::BrowserDropDestination::PluginArea,
                  .targetId = "plugin-1"});
    require(pluginDrop.opensPluginArea, "asset browser plans plugin preset drops");
    bool rejectedBadDrop = false;
    try {
        (void)lamusica::session::planBrowserDrop(
            catalog, {.assetId = "preset",
                      .destination = lamusica::session::BrowserDropDestination::DrumPad,
                      .targetId = "pad-1"});
    } catch (const std::exception&) {
        rejectedBadDrop = true;
    }
    require(rejectedBadDrop, "asset browser rejects incompatible drop destinations");
    bool rejectedDuplicateAsset = false;
    try {
        (void)lamusica::session::planAssetImport(catalog, assetRoot / "Audio" / "kick.wav", "kick",
                                                 lamusica::session::AssetKind::Audio);
    } catch (const std::exception&) {
        rejectedDuplicateAsset = true;
    }
    require(rejectedDuplicateAsset, "asset import plan rejects duplicate ids");
    require(lamusica::session::collectedAssetPath(catalog, catalog.assets.front()).filename() ==
                "kick.wav",
            "asset collect path preserves filename");
    std::filesystem::remove_all(assetRoot);

    const auto fixture =
        lamusica::session::ProjectDocument::open("fixtures/empty.Project.lamusica");
    require(fixture.project().name() == "Empty Fixture", "golden fixture opens");

    const auto sampleRateProjectPath =
        std::filesystem::temp_directory_path() / "lamusica-project-sample-rate.Project.lamusica";
    std::filesystem::remove_all(sampleRateProjectPath);
    auto sampleRateManifest =
        lamusica::session::makeFirstTrackStarterManifest("Sample Rate Fixture");
    sampleRateManifest.projectSampleRate = 44100.0;
    auto sampleRateDocument =
        lamusica::session::ProjectDocument::create(sampleRateProjectPath, sampleRateManifest);
    sampleRateDocument.close();
    lamusica::session::ApplicationSession sampleRateSession;
    sampleRateSession.openProject(sampleRateProjectPath);
    const auto sampleRateMixPath =
        std::filesystem::temp_directory_path() / "lamusica-project-sample-rate.wav";
    std::filesystem::remove(sampleRateMixPath);
    const auto sampleRateMix = sampleRateSession.exportCurrentMix(sampleRateMixPath);
    const auto sampleRateWav = lamusica::audio::readPcm16Wav(sampleRateMixPath);
    require(sampleRateMix.frames == sampleRateSession.status().renderFrames,
            "project-rate export reports arrangement frames");
    require(sampleRateWav.sampleRate == 44100.0,
            "ApplicationSession default mix export uses project sample rate");
    const auto directSampleRateMixPath =
        std::filesystem::temp_directory_path() / "lamusica-project-sample-rate-direct.wav";
    std::filesystem::remove(directSampleRateMixPath);
    const auto directSampleRateMix = lamusica::session::exportProjectMixToWav(
        sampleRateManifest, {}, {.outputPath = directSampleRateMixPath,
                                 .frames = sampleRateMix.frames,
                                 .channels = 2});
    const auto directSampleRateWav = lamusica::audio::readPcm16Wav(directSampleRateMixPath);
    require(directSampleRateMix.frames == sampleRateMix.frames &&
                directSampleRateWav.sampleRate == 44100.0,
            "direct project export defaults to manifest sample rate");
    const auto sampleRatePackagePath =
        std::filesystem::temp_directory_path() / "lamusica-project-sample-rate-package";
    std::filesystem::remove_all(sampleRatePackagePath);
    const auto sampleRatePackage = sampleRateSession.exportFirstTrackPackage(sampleRatePackagePath);
    const auto sampleRatePackageVerification =
        sampleRateSession.verifyFirstTrackPackage(sampleRatePackagePath);
    const auto sampleRatePackageManifest =
        readTextFile(sampleRatePackagePath / "first-track-package.json");
    const auto sampleRatePackageMix = lamusica::audio::readPcm16Wav(sampleRatePackage.mix.outputPath);
    require(sampleRatePackageVerification.sampleRate == 44100 &&
                sampleRatePackageMix.sampleRate == 44100.0 &&
                sampleRatePackageManifest.find("\"sampleRate\": 44100") != std::string::npos,
            "first-track package preserves and verifies project sample rate");
    std::filesystem::remove_all(sampleRatePackagePath);
    std::filesystem::remove(directSampleRateMixPath);
    std::filesystem::remove(sampleRateMixPath);
    std::filesystem::remove_all(sampleRateProjectPath);

    lamusica::audio::AudioEngine renderEngine{{.sampleRate = 48000.0, .maxBlockSize = 128}};
    renderEngine.setTempo(120.0);
    require(renderEngine.ppqToSamples(1.0) == 24000, "PPQ to samples at 120 BPM");
    require(renderEngine.samplesToPpq(24000) == 1.0, "samples to PPQ at 120 BPM");
    renderEngine.setTimeSignature({.numerator = 6, .denominator = 8});
    require(renderEngine.barBeatToSamples({.bar = 2, .beat = 1}) == 72000,
            "bar beat clock converts 6/8 bar boundary to samples");
    require(renderEngine.barBeatToSamples({.bar = 2, .beat = 3, .ppqOffset = 0.25}) == 102000,
            "bar beat clock converts sub-beat PPQ offsets to samples");
    const auto barBeat = renderEngine.samplesToBarBeat(102000);
    require(barBeat.bar == 2 && barBeat.beat == 3 && std::abs(barBeat.ppqOffset - 0.25) < 0.0001,
            "sample clock converts samples to bar beat position");
    lamusica::audio::AudioEngine slowClock{{.sampleRate = 44100.0, .maxBlockSize = 128}};
    slowClock.setTempo(60.0);
    require(slowClock.ppqToSamples(1.0) == 44100,
            "PPQ to sample conversion follows engine sample rate");

    auto silence = renderEngine.renderSilenceOffline(64);
    require(silence.channels == 2, "silence channel count");
    require(silence.frames == 64, "silence frame count");
    require(std::ranges::all_of(silence.interleavedSamples,
                                [](float sample) { return sample == 0.0F; }),
            "silence render contains only zeroes");

    renderEngine.seekSamples(0);
    auto sine = renderEngine.renderSineOffline(64, 1000.0, 0.5F);
    require(sine.interleavedSamples.size() == 128, "sine render sample count");
    require(
        std::ranges::any_of(sine.interleavedSamples, [](float sample) { return sample != 0.0F; }),
        "sine render contains signal");

    renderEngine.seekSamples(0);
    auto metronome = renderEngine.renderMetronomeOffline(64);
    require(metronome.interleavedSamples.front() == 0.9F, "metronome starts with downbeat");

    const auto wavPath = std::filesystem::temp_directory_path() / "lamusica-test-tone.wav";
    std::filesystem::remove(wavPath);
    lamusica::audio::writePcm16Wav(wavPath, renderEngine.renderSineOffline(128, 440.0, 0.25F),
                                   renderEngine.config().sampleRate);
    require(std::filesystem::exists(wavPath), "WAV export writes file");
    require(std::filesystem::file_size(wavPath) > 44, "WAV export writes audio data");
    const auto importedWav = lamusica::audio::readPcm16Wav(wavPath);
    require(importedWav.sampleRate == renderEngine.config().sampleRate,
            "WAV import reads sample rate");
    require(importedWav.bitsPerSample == 16, "WAV import reads PCM16 format");
    require(importedWav.audio.channels == renderEngine.config().outputChannels,
            "WAV import reads channel count");
    require(importedWav.audio.frames == 128, "WAV import reads frame count");
    require(std::ranges::any_of(importedWav.audio.interleavedSamples,
                                [](float sample) { return sample != 0.0F; }),
            "WAV import reads audio data");
    std::filesystem::remove(wavPath);

    const auto bouncePath = std::filesystem::temp_directory_path() / "lamusica-bounce.wav";
    std::filesystem::remove(bouncePath);
    const lamusica::audio::AudioGraph bounceGraph{
        .nodes = {{.id = "osc",
                   .kind = lamusica::audio::GraphNodeKind::Sine,
                   .frequencyHz = 440.0,
                   .gain = 0.25F,
                   .noteSequence = {}},
                  {.id = "master",
                   .kind = lamusica::audio::GraphNodeKind::Output,
                   .noteSequence = {}}},
        .connections = {{.sourceNodeId = "osc", .destinationNodeId = "master", .gain = 1.0F}},
        .outputNodeId = "master"};
    const auto bounceResult =
        lamusica::audio::bounceGraphToWav(bounceGraph, {.outputPath = bouncePath,
                                                        .startSample = 0,
                                                        .frames = 128,
                                                        .sampleRate = 48000.0,
                                                        .channels = 2,
                                                        .normalizePeak = true,
                                                        .normalizeTargetPeak = 0.5F});
    require(std::filesystem::exists(bouncePath), "bounce export writes WAV");
    require(bounceResult.frames == 128, "bounce export reports frame count");
    require(bounceResult.peakAfterNormalization > bounceResult.peakBeforeNormalization,
            "bounce export applies peak normalization");
    require(bounceResult.peakAfterDither == bounceResult.peakAfterNormalization,
            "bounce export reports explicit post-dither peak");
    const auto importedBounce = lamusica::audio::readPcm16Wav(bouncePath);
    require(importedBounce.audio.frames == 128, "bounce export WAV imports with expected frames");
    require(std::abs(lamusica::audio::peakAbsoluteSample(importedBounce.audio) - 0.5F) < 0.001F,
            "bounce export normalized WAV peak matches target");
    std::filesystem::remove(bouncePath);

    const auto recordingPath =
        std::filesystem::temp_directory_path() / "lamusica-recording-commit.wav";
    const auto recordingTempPath =
        std::filesystem::temp_directory_path() / "lamusica-recording-commit.tmp.wav";
    std::filesystem::remove(recordingPath);
    std::filesystem::remove(recordingTempPath);
    lamusica::audio::RecordingSession recording{{.finalPath = recordingPath,
                                                 .temporaryPath = recordingTempPath,
                                                 .sampleRate = 48000.0,
                                                 .channels = 2,
                                                 .timelineStartSample = 1024,
                                                 .measuredInputLatencySamples = 128}};
    const std::array<float, 8> recordingBlock{0.0F, 0.1F, 0.2F, 0.3F, 0.4F, 0.5F, 0.6F, 0.7F};
    recording.appendInterleaved(recordingBlock);
    require(recording.frames() == 4, "recording session counts appended frames");
    const auto committedRecording = recording.commit();
    require(!recording.active(), "recording session deactivates after commit");
    require(committedRecording.timelineStartSample == 896,
            "recording session applies measured input latency");
    require(std::filesystem::exists(recordingPath), "recording session commits final WAV");
    require(!std::filesystem::exists(recordingTempPath),
            "recording session removes temporary file after commit");
    const auto importedRecording = lamusica::audio::readPcm16Wav(recordingPath);
    require(importedRecording.audio.frames == 4, "recording session writes captured frames");
    std::filesystem::remove(recordingPath);

    const auto recoveryPath =
        std::filesystem::temp_directory_path() / "lamusica-recording-recovered.wav";
    const auto recoveryTempPath =
        std::filesystem::temp_directory_path() / "lamusica-recording-recovered.tmp.wav";
    std::filesystem::remove(recoveryPath);
    std::filesystem::remove(recoveryTempPath);
    const lamusica::audio::RecordingOptions recoveryOptions{.finalPath = recoveryPath,
                                                            .temporaryPath = recoveryTempPath,
                                                            .sampleRate = 48000.0,
                                                            .channels = 2,
                                                            .timelineStartSample = 2048,
                                                            .measuredInputLatencySamples = 256};
    lamusica::audio::writePcm16Wav(
        recoveryTempPath,
        {.channels = 2, .frames = 2, .interleavedSamples = {0.1F, 0.2F, 0.3F, 0.4F}},
        recoveryOptions.sampleRate);
    require(lamusica::audio::hasInterruptedRecording(recoveryOptions),
            "recording recovery detects interrupted temporary file");
    const auto recoveredRecording = lamusica::audio::recoverInterruptedRecording(
        recoveryOptions, lamusica::audio::RecordingRecoveryAction::Recover);
    require(recoveredRecording.recovered && !recoveredRecording.discarded,
            "recording recovery promotes interrupted temporary file");
    require(recoveredRecording.committed.timelineStartSample == 1792 &&
                recoveredRecording.committed.frames == 2,
            "recording recovery preserves latency-aligned timeline metadata");
    require(std::filesystem::exists(recoveryPath) && !std::filesystem::exists(recoveryTempPath),
            "recording recovery leaves final file and removes temporary file");
    std::filesystem::remove(recoveryPath);

    lamusica::audio::writePcm16Wav(
        recoveryTempPath,
        {.channels = 2, .frames = 2, .interleavedSamples = {0.1F, 0.2F, 0.3F, 0.4F}},
        recoveryOptions.sampleRate);
    const auto discardedInterruptedRecording = lamusica::audio::recoverInterruptedRecording(
        recoveryOptions, lamusica::audio::RecordingRecoveryAction::Discard);
    require(discardedInterruptedRecording.discarded && !std::filesystem::exists(recoveryTempPath) &&
                !std::filesystem::exists(recoveryPath),
            "recording recovery can discard interrupted temporary file");

    const auto discardedRecordingPath =
        std::filesystem::temp_directory_path() / "lamusica-recording-discard.wav";
    const auto discardedRecordingTempPath =
        std::filesystem::temp_directory_path() / "lamusica-recording-discard.tmp.wav";
    std::filesystem::remove(discardedRecordingPath);
    std::filesystem::remove(discardedRecordingTempPath);
    lamusica::audio::RecordingSession discardedRecording{
        {.finalPath = discardedRecordingPath,
         .temporaryPath = discardedRecordingTempPath,
         .sampleRate = 48000.0,
         .channels = 2,
         .timelineStartSample = 0,
         .measuredInputLatencySamples = 0}};
    discardedRecording.appendInterleaved(recordingBlock);
    discardedRecording.discard();
    require(!discardedRecording.active(), "recording session deactivates after discard");
    require(!std::filesystem::exists(discardedRecordingPath),
            "discarded recording does not create final file");

    const std::vector<float> latencyReference{0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 0.0F};
    const std::vector<float> latencyRecorded{0.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F};
    const auto latencyMeasurement =
        lamusica::audio::measureRecordingLatency(latencyReference, latencyRecorded, 0.75F);
    require(latencyMeasurement.valid && latencyMeasurement.measuredInputLatencySamples == 3 &&
                latencyMeasurement.referencePeak == 1.0F && latencyMeasurement.recordedPeak == 1.0F,
            "recording latency measurement derives latency from impulse fixtures");
    const std::vector<float> earlyRecorded{1.0F, 0.0F, 0.0F, 0.0F};
    const auto invalidLatency =
        lamusica::audio::measureRecordingLatency(latencyReference, earlyRecorded, 0.75F);
    require(!invalidLatency.valid,
            "recording latency measurement rejects recorded impulses before reference impulses");

    const auto recordingPlan = lamusica::audio::makeRecordingPlan(
        {.trackId = "record-track",
         .transportStartSample = 1000,
         .punchInSample = 2000,
         .punchOutSample = 3000,
         .preRollSamples = 256,
         .countInSamples = 128,
         .measuredInputLatencySamples = latencyMeasurement.measuredInputLatencySamples,
         .punchEnabled = true,
         .inputMonitoringEnabled = true});
    require(recordingPlan.captureStartSample == 1616,
            "recording plan includes pre-roll and count-in before punch");
    require(recordingPlan.clipStartSample == 1997,
            "recording plan uses measured latency to align punch clip start");
    require(recordingPlan.punchOutSample == 3000, "recording plan preserves punch out");
    require(recordingPlan.inputMonitoringEnabled, "recording plan carries input monitoring intent");
    bool rejectedBadPunch = false;
    try {
        (void)lamusica::audio::makeRecordingPlan({.trackId = "record-track",
                                                  .punchInSample = 100,
                                                  .punchOutSample = 100,
                                                  .punchEnabled = true});
    } catch (const std::exception&) {
        rejectedBadPunch = true;
    }
    require(rejectedBadPunch, "recording plan rejects empty punch range");
    bool rejectedNegativePunch = false;
    try {
        (void)lamusica::audio::makeRecordingPlan({.trackId = "record-track",
                                                  .punchInSample = -1,
                                                  .punchOutSample = 100,
                                                  .punchEnabled = true});
    } catch (const std::exception&) {
        rejectedNegativePunch = true;
    }
    require(rejectedNegativePunch, "recording plan rejects negative punch positions");

    lamusica::audio::TakeLane takeLane{.trackId = "record-track"};
    lamusica::audio::addTake(takeLane, {.id = "take-1",
                                        .trackId = "record-track",
                                        .path = "take-1.wav",
                                        .timelineStartSample = recordingPlan.clipStartSample,
                                        .frames = 512,
                                        .channels = 2});
    lamusica::audio::addTake(takeLane, {.id = "take-2",
                                        .trackId = "record-track",
                                        .path = "take-2.wav",
                                        .timelineStartSample = recordingPlan.clipStartSample,
                                        .frames = 512,
                                        .channels = 2});
    require(takeLane.takes.front().active && takeLane.takes.back().takeNumber == 2,
            "recording take lane numbers takes and activates first take");
    bool rejectedEmptyTake = false;
    try {
        lamusica::audio::addTake(takeLane, {.id = "empty-take",
                                            .trackId = "record-track",
                                            .path = "empty.wav",
                                            .timelineStartSample = recordingPlan.clipStartSample,
                                            .frames = 0,
                                            .channels = 2});
    } catch (const std::exception&) {
        rejectedEmptyTake = true;
    }
    require(rejectedEmptyTake && takeLane.takes.size() == 2,
            "recording take lane rejects empty takes without mutation");
    lamusica::audio::activateTake(takeLane, "take-2");
    require(!takeLane.takes.front().active && takeLane.takes.back().active,
            "recording take lane switches active take");

    return 0;
}
