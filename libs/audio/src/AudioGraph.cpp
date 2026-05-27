#include "lamusica/audio/AudioGraph.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <numbers>
#include <set>
#include <stdexcept>

namespace lamusica::audio {
namespace {

std::size_t sampleCount(std::uint32_t frames, std::uint32_t channels) noexcept {
    return static_cast<std::size_t>(frames) * static_cast<std::size_t>(channels);
}

bool nodeExists(const AudioGraph& graph, std::string_view nodeId) {
    return std::ranges::any_of(graph.nodes,
                               [nodeId](const GraphNode& node) { return node.id == nodeId; });
}

const GraphNode* findNode(const AudioGraph& graph, std::string_view nodeId) {
    const auto found = std::ranges::find_if(
        graph.nodes, [nodeId](const GraphNode& node) { return node.id == nodeId; });
    return found == graph.nodes.end() ? nullptr : &*found;
}

float nodeOutputGain(const GraphNode& node) noexcept {
    const auto gain = node.kind == GraphNodeKind::Sine ? 1.0F : node.gain;
    return node.phaseInverted ? -gain : gain;
}

float nodeChannelGain(const GraphNode& node, std::uint32_t channel,
                      std::uint32_t channels) noexcept {
    if (channels < 2U) {
        return 1.0F;
    }
    if (channel == 0U) {
        return node.pan > 0.0F ? 1.0F - node.pan : 1.0F;
    }
    if (channel == 1U) {
        return node.pan < 0.0F ? 1.0F + node.pan : 1.0F;
    }
    return 1.0F;
}

bool visitCycle(std::string_view node, const std::map<std::string, std::vector<std::string>>& graph,
                std::set<std::string>& visiting, std::set<std::string>& visited) {
    const auto id = std::string{node};
    if (visited.contains(id)) {
        return false;
    }
    if (visiting.contains(id)) {
        return true;
    }

    visiting.insert(id);
    if (const auto found = graph.find(id); found != graph.end()) {
        for (const auto& next : found->second) {
            if (visitCycle(next, graph, visiting, visited)) {
                return true;
            }
        }
    }
    visiting.erase(id);
    visited.insert(id);
    return false;
}

} // namespace

bool validateGraph(const AudioGraph& graph, std::string* errorMessage) {
    if (!nodeExists(graph, graph.outputNodeId)) {
        if (errorMessage != nullptr) {
            *errorMessage = "Output node does not exist: " + graph.outputNodeId;
        }
        return false;
    }

    std::set<std::string> ids;
    for (const auto& node : graph.nodes) {
        if (node.id.empty()) {
            if (errorMessage != nullptr) {
                *errorMessage = "Graph node id must not be empty";
            }
            return false;
        }
        if (!ids.insert(node.id).second) {
            if (errorMessage != nullptr) {
                *errorMessage = "Duplicate graph node id: " + node.id;
            }
            return false;
        }
    }

    std::map<std::string, std::vector<std::string>> adjacency;
    for (const auto& connection : graph.connections) {
        if (!nodeExists(graph, connection.sourceNodeId) ||
            !nodeExists(graph, connection.destinationNodeId)) {
            if (errorMessage != nullptr) {
                *errorMessage = "Graph connection references unknown node";
            }
            return false;
        }
        adjacency[connection.sourceNodeId].push_back(connection.destinationNodeId);
    }

    std::set<std::string> visiting;
    std::set<std::string> visited;
    for (const auto& node : graph.nodes) {
        if (visitCycle(node.id, adjacency, visiting, visited)) {
            if (errorMessage != nullptr) {
                *errorMessage = "Graph contains a cycle";
            }
            return false;
        }
    }

    return true;
}

std::vector<std::string> topologicalOrder(const AudioGraph& graph) {
    std::vector<std::string> ordered;
    ordered.reserve(graph.nodes.size());
    std::map<std::string, std::size_t> incoming;
    std::map<std::string, std::vector<std::string>> outgoing;

    for (const auto& node : graph.nodes) {
        incoming[node.id] = 0;
    }
    for (const auto& connection : graph.connections) {
        outgoing[connection.sourceNodeId].push_back(connection.destinationNodeId);
        ++incoming[connection.destinationNodeId];
    }

    std::vector<std::string> ready;
    for (const auto& [node, count] : incoming) {
        if (count == 0) {
            ready.push_back(node);
        }
    }

    while (!ready.empty()) {
        auto node = ready.back();
        ready.pop_back();
        ordered.push_back(node);
        for (const auto& next : outgoing[node]) {
            --incoming[next];
            if (incoming[next] == 0) {
                ready.push_back(next);
            }
        }
    }

    return ordered;
}

void renderGraph(const AudioGraph& graph, const EngineConfig& config, std::int64_t startSample,
                 std::uint32_t frames, std::span<float> interleavedOutput) {
    std::string error;
    if (!validateGraph(graph, &error)) {
        throw std::runtime_error(error);
    }

    const auto totalSamples = sampleCount(frames, config.outputChannels);
    if (interleavedOutput.size() < totalSamples) {
        throw std::runtime_error("Output buffer is too small for graph render");
    }

    std::map<std::string, std::vector<float>> buffers;
    for (const auto& node : graph.nodes) {
        buffers[node.id] = std::vector<float>(totalSamples, 0.0F);
    }

    for (const auto& nodeId : topologicalOrder(graph)) {
        const auto* node = findNode(graph, nodeId);
        auto& buffer = buffers[nodeId];

        if (node->kind == GraphNodeKind::Sine) {
            for (std::uint32_t frame = 0; frame < frames; ++frame) {
                const auto absoluteSample = startSample + static_cast<std::int64_t>(frame);
                const auto value =
                    static_cast<float>(std::sin((static_cast<double>(absoluteSample) *
                                                 node->frequencyHz * 2.0 * std::numbers::pi) /
                                                config.sampleRate) *
                                       node->gain);
                for (std::uint32_t channel = 0; channel < config.outputChannels; ++channel) {
                    buffer[sampleCount(frame, config.outputChannels) + channel] += value;
                }
            }
        }

        for (const auto& connection : graph.connections) {
            if (connection.sourceNodeId != nodeId) {
                continue;
            }
            auto& destination = buffers[connection.destinationNodeId];
            const auto gain = connection.bypassSourceProcessing
                                  ? connection.gain
                                  : nodeOutputGain(*node) * connection.gain;
            for (std::uint32_t frame = 0; frame < frames; ++frame) {
                for (std::uint32_t channel = 0; channel < config.outputChannels; ++channel) {
                    const auto index = sampleCount(frame, config.outputChannels) + channel;
                    const auto channelGain =
                        connection.bypassSourceProcessing
                            ? 1.0F
                            : nodeChannelGain(*node, channel, config.outputChannels);
                    destination[index] += buffer[index] * gain * channelGain;
                }
            }
        }
    }

    const auto* outputNode = findNode(graph, graph.outputNodeId);
    const auto outputGain = outputNode == nullptr ? 1.0F : nodeOutputGain(*outputNode);
    const auto& outputBuffer = buffers[graph.outputNodeId];
    for (std::uint32_t frame = 0; frame < frames; ++frame) {
        for (std::uint32_t channel = 0; channel < config.outputChannels; ++channel) {
            const auto index = sampleCount(frame, config.outputChannels) + channel;
            const auto panGain = outputNode == nullptr
                                     ? 1.0F
                                     : nodeChannelGain(*outputNode, channel, config.outputChannels);
            interleavedOutput[index] = outputBuffer[index] * outputGain * panGain;
        }
    }
}

} // namespace lamusica::audio
