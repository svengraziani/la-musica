#pragma once

#include "lamusica/audio/AudioEngine.hpp"

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace lamusica::audio {

enum class GraphNodeKind {
    Silence,
    Sine,
    Bus,
    Output,
};

struct GraphNode {
    std::string id;
    GraphNodeKind kind{GraphNodeKind::Silence};
    double frequencyHz{440.0};
    float gain{1.0F};
    float pan{0.0F};
    bool phaseInverted{false};
};

struct GraphConnection {
    std::string sourceNodeId;
    std::string destinationNodeId;
    float gain{1.0F};
    bool bypassSourceProcessing{false};
};

struct AudioGraph {
    std::vector<GraphNode> nodes;
    std::vector<GraphConnection> connections;
    std::string outputNodeId{"master"};
};

[[nodiscard]] bool validateGraph(const AudioGraph& graph, std::string* errorMessage = nullptr);
[[nodiscard]] std::vector<std::string> topologicalOrder(const AudioGraph& graph);
void renderGraph(const AudioGraph& graph, const EngineConfig& config, std::int64_t startSample,
                 std::uint32_t frames, std::span<float> interleavedOutput);

} // namespace lamusica::audio
