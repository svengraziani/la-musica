#include "lamusica/session/GraphCompiler.hpp"

#include "lamusica/audio/WavFile.hpp"
#include "lamusica/session/StarterProject.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <optional>
#include <set>
#include <stdexcept>
#include <utility>
#include <vector>

namespace lamusica::session {

float dbToLinearGain(float db) noexcept;

namespace {

std::string trackNodeId(std::string_view trackId) {
    return "track:" + std::string{trackId};
}

std::string channelNodeId(std::string_view channelId) {
    return "channel:" + std::string{channelId};
}

std::string mixerNodeId(std::string_view channelId, const std::set<std::string>& knownTrackIds) {
    return knownTrackIds.contains(std::string{channelId}) ? trackNodeId(channelId)
                                                          : channelNodeId(channelId);
}

std::string clipNodeId(std::string_view clipId) {
    return "clip:" + std::string{clipId};
}

std::string compSegmentNodeId(std::string_view clipId, std::size_t segmentIndex) {
    return clipNodeId(clipId) + "#seg" + std::to_string(segmentIndex);
}

std::int64_t compBoundaryCrossfadeSamples(const ClipCompSegment& left,
                                          const ClipCompSegment& right) noexcept {
    constexpr std::int64_t defaultCrossfadeSamples = 64;
    return std::max<std::int64_t>(
        0, std::min({defaultCrossfadeSamples, left.lengthSamples, right.lengthSamples}));
}

bool hasNode(const audio::AudioGraph& graph, std::string_view nodeId) {
    return std::ranges::any_of(
        graph.nodes, [nodeId](const audio::GraphNode& node) { return node.id == nodeId; });
}

audio::GraphNode* findNode(audio::AudioGraph& graph, std::string_view nodeId) {
    const auto found = std::ranges::find_if(
        graph.nodes, [nodeId](const audio::GraphNode& node) { return node.id == nodeId; });
    return found == graph.nodes.end() ? nullptr : &*found;
}

audio::GraphNodeKind kindForTrack(TrackType type) noexcept {
    return type == TrackType::Master ? audio::GraphNodeKind::Output : audio::GraphNodeKind::Bus;
}

audio::GraphNodeKind kindForChannel(ChannelType type) noexcept {
    return type == ChannelType::Master || type == ChannelType::HardwareOutput
               ? audio::GraphNodeKind::Output
               : audio::GraphNodeKind::Bus;
}

double clipFixtureFrequency(std::string_view clipId) {
    std::uint32_t hash = 2166136261U;
    for (const char character : clipId) {
        hash ^= static_cast<std::uint8_t>(character);
        hash *= 16777619U;
    }
    return 110.0 + static_cast<double>(hash % 660U);
}

double midiPitchFrequency(std::uint8_t pitch) {
    return 440.0 * std::pow(2.0, (static_cast<double>(pitch) - 69.0) / 12.0);
}

const MidiClipReference* findMidiDataReference(const ProjectManifest& manifest,
                                               std::string_view clipId) {
    const auto found =
        std::ranges::find_if(manifest.midiClips, [&](const MidiClipReference& reference) {
            return reference.clipId == clipId && reference.dataId == "starter-bass-midi";
        });
    return found == manifest.midiClips.end() ? nullptr : &*found;
}

std::vector<audio::GraphNote> starterBassGraphNotes(const ProjectManifest& manifest,
                                                    const Clip& clip) {
    const auto* reference = findMidiDataReference(manifest, clip.id);
    if (reference == nullptr) {
        return {};
    }

    std::vector<audio::GraphNote> notes;
    const auto midi = makeFirstTrackStarterBassMidi();
    constexpr std::int64_t starterPatternLengthSamples = 96000;
    const auto repeats = std::max<std::int64_t>(
        1, (clip.lengthSamples + starterPatternLengthSamples - 1) / starterPatternLengthSamples);
    notes.reserve(midi.notes.size() * static_cast<std::size_t>(repeats));
    for (std::int64_t repeat = 0; repeat < repeats; ++repeat) {
        const auto repeatOffset = repeat * starterPatternLengthSamples;
        for (const auto& note : midi.notes) {
            if (note.muted || repeatOffset + note.startSample >= clip.lengthSamples) {
                continue;
            }
            const auto noteLength = std::min(
                note.lengthSamples, clip.lengthSamples - (repeatOffset + note.startSample));
            notes.push_back(
                {.startSample = clip.startSample + repeatOffset + note.startSample,
                 .lengthSamples = noteLength,
                 .frequencyHz = midiPitchFrequency(static_cast<std::uint8_t>(std::clamp(
                     static_cast<int>(note.pitch) + reference->transposeSemitones, 0, 127))),
                 .velocity = static_cast<float>(note.velocity) / 127.0F});
        }
    }
    return notes;
}

void ensureNode(audio::AudioGraph& graph, audio::GraphNode node) {
    if (!hasNode(graph, node.id)) {
        graph.nodes.push_back(std::move(node));
    }
}

bool hasSoloChannel(const MixerState& mixer) {
    return std::ranges::any_of(mixer.channels,
                               [](const ChannelStrip& channel) { return channel.solo; });
}

bool hasSoloTrackMix(const ProjectManifest& manifest) {
    return std::ranges::any_of(manifest.trackMix,
                               [](const TrackMixState& mix) { return mix.solo; });
}

const ChannelStrip* findMixerChannel(const MixerState& mixer, std::string_view channelId) {
    const auto found =
        std::ranges::find_if(mixer.channels, [channelId](const ChannelStrip& channel) {
            return channel.id == channelId;
        });
    return found == mixer.channels.end() ? nullptr : &*found;
}

const TrackMixState* findTrackMix(const ProjectManifest& manifest, std::string_view trackId) {
    const auto found = std::ranges::find_if(
        manifest.trackMix, [trackId](const TrackMixState& mix) { return mix.trackId == trackId; });
    return found == manifest.trackMix.end() ? nullptr : &*found;
}

float channelGain(const ChannelStrip& channel, bool soloMode) noexcept {
    const bool mutedBySolo = soloMode && !channel.solo && channel.type != ChannelType::Master &&
                             channel.type != ChannelType::HardwareOutput;
    if (channel.muted || mutedBySolo) {
        return 0.0F;
    }
    const auto linearGain = dbToLinearGain(channel.volumeDb);
    return channel.phaseInverted ? -linearGain : linearGain;
}

float trackMixGain(const TrackMixState& mix, TrackType type, bool soloMode) noexcept {
    const bool mutedBySolo = soloMode && !mix.solo && type != TrackType::Master;
    if (mix.muted || mutedBySolo) {
        return 0.0F;
    }
    return dbToLinearGain(mix.volumeDb);
}

audio::GraphNode makeGraphNode(std::string id, audio::GraphNodeKind kind, float gain = 1.0F,
                               float pan = 0.0F) {
    audio::GraphNode node;
    node.id = std::move(id);
    node.kind = kind;
    node.gain = gain;
    node.pan = pan;
    return node;
}

const Asset* findAsset(const ProjectManifest& manifest, std::string_view assetId) {
    const auto found = std::ranges::find_if(
        manifest.assets, [assetId](const Asset& asset) { return asset.id == assetId; });
    return found == manifest.assets.end() ? nullptr : &*found;
}

const ClipTakeLane* findTakeLane(const ProjectManifest& manifest, std::string_view clipId) {
    const auto found = std::ranges::find_if(
        manifest.takeLanes,
        [clipId](const ClipTakeLane& takeLane) { return takeLane.clipId == clipId; });
    return found == manifest.takeLanes.end() ? nullptr : &*found;
}

const ClipComp* findComp(const ProjectManifest& manifest, std::string_view clipId) {
    const auto found = std::ranges::find_if(
        manifest.comps, [clipId](const ClipComp& comp) { return comp.clipId == clipId; });
    return found == manifest.comps.end() || found->segments.empty() ? nullptr : &*found;
}

const ClipTake* findTake(const ClipTakeLane& takeLane, std::string_view takeId) {
    const auto found = std::ranges::find_if(
        takeLane.takes, [takeId](const ClipTake& take) { return take.id == takeId; });
    return found == takeLane.takes.end() ? nullptr : &*found;
}

const WarpState* findWarpState(const GraphCompileOptions& options, std::string_view clipId) {
    const auto found = std::ranges::find_if(
        options.warpStates, [clipId](const WarpState& warp) { return warp.clipId == clipId; });
    return found == options.warpStates.end() ? nullptr : &*found;
}

bool safeRelativeCachePath(const std::filesystem::path& path) {
    if (path.empty() || path.is_absolute()) {
        return false;
    }
    for (const auto& part : path) {
        if (part == "..") {
            return false;
        }
    }
    return true;
}

std::optional<audio::WavAudioData> readWarpCacheWav(const GraphCompileOptions& options,
                                                    const WarpRenderPlan& plan,
                                                    double expectedSampleRate,
                                                    std::uint32_t expectedChannels) {
    const std::filesystem::path relativePath{plan.relativePath};
    if (!plan.cacheHit || options.projectRoot.empty() || !safeRelativeCachePath(relativePath)) {
        return std::nullopt;
    }
    const auto cachePath = options.projectRoot / relativePath;
    if (!std::filesystem::exists(cachePath)) {
        return std::nullopt;
    }
    auto cached = audio::readPcm16Wav(cachePath);
    const auto expectedFrames = plan.timelineEndSample - plan.timelineStartSample;
    if (expectedFrames <= 0 || cached.audio.frames != static_cast<std::uint32_t>(expectedFrames) ||
        cached.audio.channels != expectedChannels ||
        std::abs(cached.sampleRate - expectedSampleRate) > 0.000001) {
        return std::nullopt;
    }
    return cached;
}

audio::WavAudioData readClipAssetWav(const ProjectManifest& manifest,
                                     const GraphCompileOptions& options,
                                     std::string_view assetId) {
    const auto* asset = findAsset(manifest, assetId);
    if (asset == nullptr) {
        throw std::runtime_error("Audio clip references missing asset: " + std::string{assetId});
    }
    return audio::readPcm16Wav(options.projectRoot / asset->relativePath);
}

audio::GraphNode makeClipNode(const ProjectManifest& manifest, const Clip& clip,
                              const GraphCompileOptions& options) {
    audio::GraphNode node;
    node.id = clipNodeId(clip.id);
    node.kind = audio::GraphNodeKind::Sine;
    node.frequencyHz = clipFixtureFrequency(clip.id);
    node.gain = dbToLinearGain(clip.gainDb);
    node.startSample = clip.startSample;
    node.lengthSamples = clip.lengthSamples;
    node.sourceOffsetSamples = clip.sourceOffsetSamples;
    node.fadeInSamples = clip.fadeInSamples;
    node.fadeOutSamples = clip.fadeOutSamples;
    node.reversed = clip.reversed;
    node.noteSequence = clip.type == ClipType::Midi ? starterBassGraphNotes(manifest, clip)
                                                    : std::vector<audio::GraphNote>{};

    if (clip.type != ClipType::Audio || clip.assetId.empty()) {
        return node;
    }
    if (options.projectRoot.empty()) {
        if (options.synthesizeAssetBackedClipsWithoutProjectRoot) {
            return node;
        }
        throw std::runtime_error("Audio clip requires a project root to resolve asset: " +
                                 clip.assetId);
    }

    const auto wav = readClipAssetWav(manifest, options, clip.assetId);
    node.kind = audio::GraphNodeKind::Sample;
    node.sampleChannels = wav.audio.channels;
    node.sampleFrames = wav.audio.frames;
    node.sampleRate = wav.sampleRate;
    node.samples = wav.audio.interleavedSamples;

    const auto* warp = findWarpState(options, clip.id);
    if (warp != nullptr && warp->enabled) {
        const auto sourceStart = std::max<std::int64_t>(0, clip.sourceOffsetSamples);
        const auto sourceEnd =
            std::min<std::int64_t>(static_cast<std::int64_t>(wav.audio.frames),
                                   sourceStart + clip.lengthSamples);
        const auto plan = makeWarpRenderPlan(*warp, options.warpRenderCache, sourceStart,
                                             sourceEnd, "Cache/" + clip.id + "-warp.wav");
        const auto cached =
            readWarpCacheWav(options, plan, manifest.projectSampleRate, wav.audio.channels);
        const auto rendered =
            cached.has_value() ? cached->audio : renderWarpedAudio(wav.audio, plan);
        node.sourceOffsetSamples = 0;
        node.lengthSamples = rendered.frames;
        node.sampleChannels = rendered.channels;
        node.sampleFrames = rendered.frames;
        node.sampleRate = cached.has_value() ? cached->sampleRate : manifest.projectSampleRate;
        node.samples = std::move(rendered.interleavedSamples);
    }
    return node;
}

std::optional<std::vector<audio::GraphNode>>
makeCompSegmentNodes(const ProjectManifest& manifest, const Clip& clip,
                     const GraphCompileOptions& options) {
    struct SegmentSource {
        const ClipCompSegment* segment{nullptr};
        const ClipTake* take{nullptr};
        audio::WavAudioData wav;
    };

    if (clip.type != ClipType::Audio) {
        return std::nullopt;
    }
    const auto* takeLane = findTakeLane(manifest, clip.id);
    const auto* comp = findComp(manifest, clip.id);
    if (takeLane == nullptr || comp == nullptr) {
        return std::nullopt;
    }
    if (comp->segments.size() < 2U) {
        return std::nullopt;
    }
    if (options.projectRoot.empty()) {
        if (options.synthesizeAssetBackedClipsWithoutProjectRoot) {
            return std::nullopt;
        }
        throw std::runtime_error("Comped audio clip requires a project root to resolve assets: " +
                                 clip.id);
    }

    constexpr std::int64_t internalCrossfadeSamples = 64;
    std::vector<SegmentSource> segmentSources;
    segmentSources.reserve(comp->segments.size());
    for (const auto& segment : comp->segments) {
        const auto* take = findTake(*takeLane, segment.takeId);
        if (take == nullptr) {
            throw std::runtime_error("Clip comp references missing take id: " + segment.takeId);
        }
        const auto assetId = take->assetId.empty() ? clip.assetId : take->assetId;
        if (assetId.empty()) {
            throw std::runtime_error("Comp take has no source asset: " + take->id);
        }
        auto wav = readClipAssetWav(manifest, options, assetId);
        if (segment.takeSourceOffsetSamples + segment.lengthSamples > wav.audio.frames) {
            throw std::runtime_error("Comp segment exceeds take source asset: " + take->id);
        }
        segmentSources.push_back({.segment = &segment, .take = take, .wav = std::move(wav)});
    }

    std::vector<std::int64_t> boundaryCrossfades(comp->segments.size() - 1U, 0);
    for (std::size_t index = 0; index + 1U < segmentSources.size(); ++index) {
        const auto& left = segmentSources[index];
        const auto& right = segmentSources[index + 1U];
        if (left.take->muted || right.take->muted) {
            continue;
        }
        boundaryCrossfades[index] = std::min(
            internalCrossfadeSamples, compBoundaryCrossfadeSamples(*left.segment, *right.segment));
    }

    std::vector<audio::GraphNode> nodes;
    nodes.reserve(comp->segments.size() + boundaryCrossfades.size());
    for (std::size_t index = 0; index < comp->segments.size(); ++index) {
        const auto& segmentSource = segmentSources[index];
        const auto& segment = *segmentSource.segment;
        const auto& take = *segmentSource.take;
        const auto& wav = segmentSource.wav;
        const auto incomingCrossfade = index == 0U ? 0 : boundaryCrossfades[index - 1U];

        audio::GraphNode node;
        node.id = compSegmentNodeId(clip.id, index);
        node.kind = take.muted ? audio::GraphNodeKind::Silence : audio::GraphNodeKind::Sample;
        node.gain = dbToLinearGain(clip.gainDb);
        node.startSample = clip.startSample + segment.clipStartSample;
        node.lengthSamples = segment.lengthSamples;
        node.sourceOffsetSamples = segment.takeSourceOffsetSamples;
        const bool firstSegment = index == 0U;
        const bool lastSegment = index + 1U == comp->segments.size();
        node.fadeInSamples = firstSegment ? clip.fadeInSamples : incomingCrossfade;
        node.fadeOutSamples = lastSegment ? clip.fadeOutSamples : 0;
        node.reversed = clip.reversed;
        node.sampleChannels = wav.audio.channels;
        node.sampleFrames = wav.audio.frames;
        node.sampleRate = wav.sampleRate;
        node.samples = take.muted ? std::vector<float>{} : wav.audio.interleavedSamples;
        nodes.push_back(std::move(node));

        if (index + 1U == comp->segments.size() || boundaryCrossfades[index] <= 0 ||
            take.muted) {
            continue;
        }

        const auto& nextSegment = *segmentSources[index + 1U].segment;
        audio::GraphNode tailNode;
        tailNode.id = compSegmentNodeId(clip.id, index) + "#tail";
        tailNode.kind = audio::GraphNodeKind::Sample;
        tailNode.gain = dbToLinearGain(clip.gainDb);
        tailNode.startSample = clip.startSample + nextSegment.clipStartSample;
        tailNode.lengthSamples = boundaryCrossfades[index];
        tailNode.sourceOffsetSamples =
            segment.takeSourceOffsetSamples + segment.lengthSamples - boundaryCrossfades[index];
        tailNode.fadeOutSamples = boundaryCrossfades[index];
        tailNode.reversed = clip.reversed;
        tailNode.sampleChannels = wav.audio.channels;
        tailNode.sampleFrames = wav.audio.frames;
        tailNode.sampleRate = wav.sampleRate;
        tailNode.samples = wav.audio.interleavedSamples;
        nodes.push_back(std::move(tailNode));
    }
    return nodes;
}

} // namespace

float dbToLinearGain(float db) noexcept {
    return std::pow(10.0F, db / 20.0F);
}

audio::AudioGraph compileProjectAudioGraph(const ProjectManifest& manifest, const MixerState& mixer,
                                           GraphCompileOptions options) {
    audio::AudioGraph graph;
    graph.nodes.clear();
    graph.connections.clear();
    graph.outputNodeId = "master";

    std::set<std::string> knownTrackIds;
    const auto trackSoloMode = hasSoloTrackMix(manifest);
    for (const auto& track : manifest.tracks) {
        knownTrackIds.insert(track.id);
        float gain = 1.0F;
        float pan = 0.0F;
        if (const auto* mix = findTrackMix(manifest, track.id); mix != nullptr) {
            gain = trackMixGain(*mix, track.type, trackSoloMode);
            pan = mix->pan;
        }
        ensureNode(graph,
                   makeGraphNode(trackNodeId(track.id), kindForTrack(track.type), gain, pan));
        if (track.type == TrackType::Master) {
            graph.outputNodeId = trackNodeId(track.id);
        }
    }

    const auto soloMode = hasSoloChannel(mixer);
    for (const auto& channel : mixer.channels) {
        const auto gain = channelGain(channel, soloMode);
        const auto matchingTrackNode = trackNodeId(channel.id);
        if (knownTrackIds.contains(channel.id)) {
            if (auto* node = findNode(graph, matchingTrackNode); node != nullptr) {
                node->gain = gain;
                node->pan = channel.pan;
                if (channel.type == ChannelType::Master) {
                    graph.outputNodeId = matchingTrackNode;
                }
                continue;
            }
        }
        ensureNode(graph, makeGraphNode(channelNodeId(channel.id), kindForChannel(channel.type),
                                        gain, channel.pan));
        if (channel.type == ChannelType::Master) {
            graph.outputNodeId = channelNodeId(channel.id);
        }
    }

    if (options.synthesizeMissingMaster && !hasNode(graph, graph.outputNodeId)) {
        graph.nodes.push_back(makeGraphNode(graph.outputNodeId, audio::GraphNodeKind::Output));
    }

    for (const auto& clip : manifest.clips) {
        if (clip.muted && !options.includeMutedClips) {
            continue;
        }
        if (!knownTrackIds.contains(clip.trackId)) {
            throw std::runtime_error("Clip references unknown track: " + clip.trackId);
        }
        if (auto compNodes = makeCompSegmentNodes(manifest, clip, options);
            compNodes.has_value()) {
            for (auto& node : *compNodes) {
                const auto nodeId = node.id;
                graph.nodes.push_back(std::move(node));
                graph.connections.push_back({.sourceNodeId = nodeId,
                                             .destinationNodeId = trackNodeId(clip.trackId),
                                             .gain = 1.0F});
            }
        } else {
            const auto clipNode = clipNodeId(clip.id);
            graph.nodes.push_back(makeClipNode(manifest, clip, options));
            graph.connections.push_back({.sourceNodeId = clipNode,
                                         .destinationNodeId = trackNodeId(clip.trackId),
                                         .gain = 1.0F});
        }
    }

    for (const auto& route : manifest.routing) {
        graph.connections.push_back({.sourceNodeId = trackNodeId(route.sourceTrackId),
                                     .destinationNodeId = trackNodeId(route.destinationTrackId),
                                     .gain = 1.0F});
    }

    for (const auto& route : mixer.routing) {
        graph.connections.push_back(
            {.sourceNodeId = mixerNodeId(route.sourceChannelId, knownTrackIds),
             .destinationNodeId = mixerNodeId(route.destinationChannelId, knownTrackIds),
             .gain = 1.0F});
        const auto* destination = findMixerChannel(mixer, route.destinationChannelId);
        if (destination != nullptr && destination->type == ChannelType::HardwareOutput &&
            mixerNodeId(route.sourceChannelId, knownTrackIds) == graph.outputNodeId) {
            graph.outputNodeId = mixerNodeId(route.destinationChannelId, knownTrackIds);
        }
    }

    for (const auto& channel : mixer.channels) {
        for (const auto& send : channel.sends) {
            graph.connections.push_back(
                {.sourceNodeId = mixerNodeId(channel.id, knownTrackIds),
                 .destinationNodeId = mixerNodeId(send.destinationChannelId, knownTrackIds),
                 .gain = dbToLinearGain(send.gainDb),
                 .bypassSourceProcessing = send.preFader});
        }
    }

    for (const auto& track : manifest.tracks) {
        const auto node = trackNodeId(track.id);
        const bool hasOutgoing =
            std::ranges::any_of(graph.connections, [&node](const audio::GraphConnection& c) {
                return c.sourceNodeId == node;
            });
        if (!hasOutgoing && node != graph.outputNodeId) {
            graph.connections.push_back(
                {.sourceNodeId = node, .destinationNodeId = graph.outputNodeId, .gain = 1.0F});
        }
    }

    std::string error;
    if (!audio::validateGraph(graph, &error)) {
        throw std::runtime_error(error);
    }
    return graph;
}

MixerGraphUpdatePlan prepareMixerGraphUpdate(const ProjectManifest& manifest,
                                             const MixerState& mixer, GraphCompileOptions options) {
    MixerGraphUpdatePlan plan;
    std::string routingError;
    if (!validateRouting(mixer, &routingError)) {
        plan.validationError = std::move(routingError);
        return plan;
    }

    try {
        plan.graph = compileProjectAudioGraph(manifest, mixer, options);
        plan.ready = true;
    } catch (const std::exception& exception) {
        plan.validationError = exception.what();
    }
    return plan;
}

} // namespace lamusica::session
