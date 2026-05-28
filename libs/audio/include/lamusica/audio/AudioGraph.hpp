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
    Sample,
    Bus,
    Output,
};

struct GraphNote {
    std::int64_t startSample{0};
    std::int64_t lengthSamples{0};
    double frequencyHz{440.0};
    float velocity{1.0F};
};

struct GraphNode {
    std::string id;
    GraphNodeKind kind{GraphNodeKind::Silence};
    double frequencyHz{440.0};
    float gain{1.0F};
    float pan{0.0F};
    bool phaseInverted{false};
    std::int64_t startSample{0};
    std::int64_t lengthSamples{0};
    std::int64_t sourceOffsetSamples{0};
    std::int64_t fadeInSamples{0};
    std::int64_t fadeOutSamples{0};
    bool reversed{false};
    std::uint32_t sampleChannels{0};
    std::uint32_t sampleFrames{0};
    std::vector<float> samples;
    std::vector<GraphNote> noteSequence;
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
