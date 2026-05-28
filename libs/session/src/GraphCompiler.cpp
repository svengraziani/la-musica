#include "lamusica/session/GraphCompiler.hpp"

#include <algorithm>
#include <cmath>
#include <set>
#include <stdexcept>

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

void ensureNode(audio::AudioGraph& graph, audio::GraphNode node) {
    if (!hasNode(graph, node.id)) {
        graph.nodes.push_back(std::move(node));
    }
}

bool hasSoloChannel(const MixerState& mixer) {
    return std::ranges::any_of(mixer.channels,
                               [](const ChannelStrip& channel) { return channel.solo; });
}

const ChannelStrip* findMixerChannel(const MixerState& mixer, std::string_view channelId) {
    const auto found =
        std::ranges::find_if(mixer.channels, [channelId](const ChannelStrip& channel) {
            return channel.id == channelId;
        });
    return found == mixer.channels.end() ? nullptr : &*found;
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
    for (const auto& track : manifest.tracks) {
        knownTrackIds.insert(track.id);
        ensureNode(graph, {.id = trackNodeId(track.id), .kind = kindForTrack(track.type)});
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
        ensureNode(graph, {.id = channelNodeId(channel.id),
                           .kind = kindForChannel(channel.type),
                           .gain = gain,
                           .pan = channel.pan});
        if (channel.type == ChannelType::Master) {
            graph.outputNodeId = channelNodeId(channel.id);
        }
    }

    if (options.synthesizeMissingMaster && !hasNode(graph, graph.outputNodeId)) {
        graph.nodes.push_back({.id = graph.outputNodeId, .kind = audio::GraphNodeKind::Output});
    }

    for (const auto& clip : manifest.clips) {
        if (clip.muted && !options.includeMutedClips) {
            continue;
        }
        if (!knownTrackIds.contains(clip.trackId)) {
            throw std::runtime_error("Clip references unknown track: " + clip.trackId);
        }
        const auto clipNode = clipNodeId(clip.id);
        graph.nodes.push_back({.id = clipNode,
                               .kind = audio::GraphNodeKind::Sine,
                               .frequencyHz = clipFixtureFrequency(clip.id),
                               .gain = dbToLinearGain(clip.gainDb)});
        graph.connections.push_back({.sourceNodeId = clipNode,
                                     .destinationNodeId = trackNodeId(clip.trackId),
                                     .gain = 1.0F});
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
