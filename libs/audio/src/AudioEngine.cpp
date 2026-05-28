#include "lamusica/audio/AudioEngine.hpp"

#include "lamusica/audio/AudioGraph.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <stdexcept>
#include <utility>

namespace lamusica::audio {
namespace {

constexpr double pulsesPerQuarter = 1.0;
constexpr std::size_t invalidNodeIndex = std::numeric_limits<std::size_t>::max();

double quarterNotesPerBeat(TimeSignature timeSignature) noexcept {
    if (timeSignature.denominator == 0) {
        return 1.0;
    }
    return 4.0 / static_cast<double>(timeSignature.denominator);
}

double quarterNotesPerBar(TimeSignature timeSignature) noexcept {
    return static_cast<double>(std::max<std::uint32_t>(1, timeSignature.numerator)) *
           quarterNotesPerBeat(timeSignature);
}

void validateConfig(const EngineConfig& config) {
    if (config.sampleRate <= 0.0) {
        throw std::invalid_argument("AudioEngine sample rate must be positive");
    }

    if (config.maxBlockSize == 0) {
        throw std::invalid_argument("AudioEngine max block size must be positive");
    }

    if (config.outputChannels == 0) {
        throw std::invalid_argument("AudioEngine output channels must be positive");
    }
}

std::size_t sampleCount(std::uint32_t frames, std::uint32_t channels) noexcept {
    return static_cast<std::size_t>(frames) * static_cast<std::size_t>(channels);
}

float engineNodeOutputGain(const GraphNode& node) noexcept {
    const auto gain = node.kind == GraphNodeKind::Sine ? 1.0F : node.gain;
    return node.phaseInverted ? -gain : gain;
}

float engineNodeChannelGain(const GraphNode& node, std::uint32_t channel,
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

std::size_t graphNodeIndex(const AudioGraph& graph, std::string_view nodeId) noexcept {
    for (std::size_t index = 0; index < graph.nodes.size(); ++index) {
        if (graph.nodes[index].id == nodeId) {
            return index;
        }
    }
    return invalidNodeIndex;
}

} // namespace

AudioEngine::AudioEngine(EngineConfig config) : config_(config) {
    validateConfig(config_);
    device_.sampleRate = config_.sampleRate;
    device_.bufferSize = config_.maxBlockSize;
    device_.outputChannels = config_.outputChannels;
    realtimeGraphBuffers_.resize(maxRealtimeGraphNodes *
                                 sampleCount(config_.maxBlockSize, config_.outputChannels));
}

const EngineConfig& AudioEngine::config() const noexcept {
    return config_;
}

const AudioDeviceInfo& AudioEngine::device() const noexcept {
    return device_;
}

const TransportState& AudioEngine::transport() const noexcept {
    return transport_;
}

bool RealtimeCommandQueue::push(RealtimeCommand command) noexcept {
    if (full()) {
        return false;
    }

    commands_[writeIndex_] = command;
    writeIndex_ = (writeIndex_ + 1) % commands_.size();
    ++size_;
    return true;
}

std::optional<RealtimeCommand> RealtimeCommandQueue::pop() noexcept {
    if (empty()) {
        return std::nullopt;
    }

    const auto command = commands_[readIndex_];
    readIndex_ = (readIndex_ + 1) % commands_.size();
    --size_;
    return command;
}

bool RealtimeCommandQueue::empty() const noexcept {
    return size_ == 0;
}

bool RealtimeCommandQueue::full() const noexcept {
    return size_ == commands_.size();
}

std::size_t RealtimeCommandQueue::size() const noexcept {
    return size_;
}

void RealtimeCommandQueue::clear() noexcept {
    readIndex_ = 0;
    writeIndex_ = 0;
    size_ = 0;
}

void AudioEngine::selectDevice(AudioDeviceInfo device) {
    if (device.sampleRate <= 0.0 || device.bufferSize == 0 || device.outputChannels == 0) {
        throw std::invalid_argument("Audio device configuration is invalid");
    }

    device_ = std::move(device);
    config_.sampleRate = device_.sampleRate;
    config_.maxBlockSize = device_.bufferSize;
    config_.outputChannels = device_.outputChannels;
    realtimeGraphBuffers_.assign(
        maxRealtimeGraphNodes * sampleCount(config_.maxBlockSize, config_.outputChannels), 0.0F);
}

bool AudioEngine::enqueueCommand(RealtimeCommand command) noexcept {
    return commandQueue_.push(command);
}

void AudioEngine::processRealtimeCommands() noexcept {
    while (auto command = commandQueue_.pop()) {
        applyRealtimeCommand(*command);
    }
}

void AudioEngine::play() noexcept {
    transport_.playing = true;
}

void AudioEngine::stop() noexcept {
    transport_.playing = false;
    transport_.recording = false;
}

void AudioEngine::record(bool enabled) noexcept {
    transport_.recording = enabled;
    if (enabled) {
        transport_.playing = true;
    }
}

void AudioEngine::seekSamples(std::int64_t samplePosition) noexcept {
    transport_.samplePosition = std::max<std::int64_t>(0, samplePosition);
}

void AudioEngine::setTempo(double tempoBpm) noexcept {
    if (tempoBpm > 0.0) {
        transport_.tempoBpm = tempoBpm;
    }
}

void AudioEngine::setTimeSignature(TimeSignature timeSignature) noexcept {
    if (timeSignature.numerator > 0 && timeSignature.denominator > 0) {
        transport_.timeSignature = timeSignature;
    }
}

void AudioEngine::setLoop(bool enabled, std::int64_t startSample, std::int64_t endSample) noexcept {
    transport_.loopEnabled = enabled && startSample >= 0 && endSample > startSample;
    transport_.loopStartSample = transport_.loopEnabled ? startSample : 0;
    transport_.loopEndSample = transport_.loopEnabled ? endSample : 0;
}

void AudioEngine::applyRealtimeCommand(RealtimeCommand command) noexcept {
    switch (command.type) {
    case RealtimeCommandType::Play:
        play();
        break;
    case RealtimeCommandType::Stop:
        stop();
        break;
    case RealtimeCommandType::Record:
        record(command.flag);
        break;
    case RealtimeCommandType::Seek:
        seekSamples(command.sampleA);
        break;
    case RealtimeCommandType::SetTempo:
        setTempo(command.value);
        break;
    case RealtimeCommandType::SetLoop:
        setLoop(command.flag, command.sampleA, command.sampleB);
        break;
    }
}

void AudioEngine::advanceTransport(std::uint32_t frames) noexcept {
    if (!transport_.loopEnabled) {
        transport_.samplePosition += frames;
        return;
    }

    const auto loopLength = transport_.loopEndSample - transport_.loopStartSample;
    if (loopLength <= 0) {
        transport_.samplePosition += frames;
        return;
    }

    auto relativePosition = transport_.samplePosition - transport_.loopStartSample;
    if (relativePosition < 0 || relativePosition >= loopLength) {
        relativePosition = 0;
    }
    const auto advanced = relativePosition + static_cast<std::int64_t>(frames);
    transport_.samplePosition = transport_.loopStartSample + (advanced % loopLength);
}

double AudioEngine::samplesToPpq(std::int64_t samples) const noexcept {
    const double seconds = static_cast<double>(samples) / config_.sampleRate;
    const double quartersPerSecond = transport_.tempoBpm / 60.0;
    return seconds * quartersPerSecond * pulsesPerQuarter;
}

std::int64_t AudioEngine::ppqToSamples(double ppq) const noexcept {
    const double quartersPerSecond = transport_.tempoBpm / 60.0;
    const double seconds = ppq / (quartersPerSecond * pulsesPerQuarter);
    return static_cast<std::int64_t>(std::llround(seconds * config_.sampleRate));
}

BarBeatPosition AudioEngine::samplesToBarBeat(std::int64_t samplePosition) const noexcept {
    const auto sample = std::max<std::int64_t>(0, samplePosition);
    const auto ppqPosition = std::max(0.0, samplesToPpq(sample));
    const auto beatLength = quarterNotesPerBeat(transport_.timeSignature);
    const auto barLength = quarterNotesPerBar(transport_.timeSignature);
    const auto barIndex = static_cast<std::int64_t>(std::floor(ppqPosition / barLength));
    const auto ppqWithinBar = ppqPosition - (static_cast<double>(barIndex) * barLength);
    const auto beatIndex = static_cast<std::uint32_t>(std::floor(ppqWithinBar / beatLength));
    const auto ppqOffset = ppqWithinBar - (static_cast<double>(beatIndex) * beatLength);

    return {.bar = barIndex + 1, .beat = beatIndex + 1, .ppqOffset = std::max(0.0, ppqOffset)};
}

std::int64_t AudioEngine::barBeatToSamples(BarBeatPosition position) const noexcept {
    const auto bar = std::max<std::int64_t>(1, position.bar);
    const auto beat = std::max<std::uint32_t>(1, position.beat);
    const auto ppqOffset = std::max(0.0, position.ppqOffset);
    const auto beatLength = quarterNotesPerBeat(transport_.timeSignature);
    const auto barLength = quarterNotesPerBar(transport_.timeSignature);
    const auto ppqPosition = (static_cast<double>(bar - 1) * barLength) +
                             (static_cast<double>(beat - 1) * beatLength) + ppqOffset;
    return ppqToSamples(ppqPosition);
}

void AudioEngine::renderSilence(std::span<float> interleavedOutput, std::uint32_t frames) noexcept {
    processRealtimeCommands();
    const auto requestedSamples = sampleCount(frames, config_.outputChannels);
    const auto writableSamples = std::min(interleavedOutput.size(), requestedSamples);
    std::fill_n(interleavedOutput.begin(), writableSamples, 0.0F);
    advanceTransport(frames);
}

void AudioEngine::renderSine(std::span<float> interleavedOutput, std::uint32_t frames,
                             double frequencyHz, float amplitude) noexcept {
    processRealtimeCommands();
    const auto requestedSamples = sampleCount(frames, config_.outputChannels);
    const auto writableSamples = std::min(interleavedOutput.size(), requestedSamples);

    for (std::uint32_t frame = 0; frame < frames; ++frame) {
        const auto absoluteSample = transport_.samplePosition + static_cast<std::int64_t>(frame);
        const double phase =
            (static_cast<double>(absoluteSample) * frequencyHz * 2.0 * std::numbers::pi) /
            config_.sampleRate;
        const auto value = static_cast<float>(std::sin(phase) * static_cast<double>(amplitude));

        for (std::uint32_t channel = 0; channel < config_.outputChannels; ++channel) {
            const auto index = sampleCount(frame, config_.outputChannels) + channel;
            if (index < writableSamples) {
                interleavedOutput[index] = value;
            }
        }
    }

    advanceTransport(frames);
}

void AudioEngine::renderMetronome(std::span<float> interleavedOutput,
                                  std::uint32_t frames) noexcept {
    processRealtimeCommands();
    const auto requestedSamples = sampleCount(frames, config_.outputChannels);
    const auto writableSamples = std::min(interleavedOutput.size(), requestedSamples);
    std::fill_n(interleavedOutput.begin(), writableSamples, 0.0F);

    const auto startSample = transport_.samplePosition;
    const auto samplesPerBeat = ppqToSamples(1.0);
    const auto clickLengthSamples = static_cast<std::int64_t>(config_.sampleRate * 0.005);

    for (std::uint32_t frame = 0; frame < frames; ++frame) {
        const auto absoluteSample = startSample + static_cast<std::int64_t>(frame);
        const auto beatOffset = samplesPerBeat > 0 ? absoluteSample % samplesPerBeat : 0;
        if (beatOffset >= clickLengthSamples) {
            continue;
        }

        const float value = beatOffset == 0 ? 0.9F : 0.35F;
        for (std::uint32_t channel = 0; channel < config_.outputChannels; ++channel) {
            const auto index = sampleCount(frame, config_.outputChannels) + channel;
            if (index < writableSamples) {
                interleavedOutput[index] = value;
            }
        }
    }
    advanceTransport(frames);
}

void AudioEngine::renderGraphBlock(const AudioGraph& graph, std::span<float> interleavedOutput,
                                   std::uint32_t frames) {
    processRealtimeCommands();

    if (graph.nodes.empty() || graph.nodes.size() > maxRealtimeGraphNodes) {
        throw std::runtime_error("Realtime graph node count exceeds bounded engine capacity");
    }
    if (graph.connections.size() > maxRealtimeGraphConnections) {
        throw std::runtime_error("Realtime graph connection count exceeds bounded engine capacity");
    }
    if (frames > config_.maxBlockSize) {
        throw std::runtime_error("Realtime graph block exceeds configured max block size");
    }

    const auto totalSamples = sampleCount(frames, config_.outputChannels);
    if (interleavedOutput.size() < totalSamples) {
        throw std::runtime_error("Output buffer is too small for graph render");
    }

    const auto outputNodeIndex = graphNodeIndex(graph, graph.outputNodeId);
    if (outputNodeIndex == invalidNodeIndex) {
        throw std::runtime_error("Output node does not exist: " + graph.outputNodeId);
    }

    std::array<std::size_t, maxRealtimeGraphConnections> sourceIndices{};
    std::array<std::size_t, maxRealtimeGraphConnections> destinationIndices{};
    std::array<std::size_t, maxRealtimeGraphNodes> incoming{};
    for (std::size_t connectionIndex = 0; connectionIndex < graph.connections.size();
         ++connectionIndex) {
        const auto& connection = graph.connections[connectionIndex];
        const auto source = graphNodeIndex(graph, connection.sourceNodeId);
        const auto destination = graphNodeIndex(graph, connection.destinationNodeId);
        if (source == invalidNodeIndex || destination == invalidNodeIndex) {
            throw std::runtime_error("Graph connection references unknown node");
        }
        sourceIndices[connectionIndex] = source;
        destinationIndices[connectionIndex] = destination;
        ++incoming[destination];
    }

    std::array<std::size_t, maxRealtimeGraphNodes> ready{};
    std::array<std::size_t, maxRealtimeGraphNodes> ordered{};
    std::size_t readyCount = 0;
    std::size_t orderedCount = 0;
    for (std::size_t nodeIndex = 0; nodeIndex < graph.nodes.size(); ++nodeIndex) {
        if (incoming[nodeIndex] == 0) {
            ready[readyCount++] = nodeIndex;
        }
    }
    while (readyCount > 0) {
        const auto nodeIndex = ready[--readyCount];
        ordered[orderedCount++] = nodeIndex;
        for (std::size_t connectionIndex = 0; connectionIndex < graph.connections.size();
             ++connectionIndex) {
            if (sourceIndices[connectionIndex] != nodeIndex) {
                continue;
            }
            const auto destination = destinationIndices[connectionIndex];
            --incoming[destination];
            if (incoming[destination] == 0) {
                ready[readyCount++] = destination;
            }
        }
    }
    if (orderedCount != graph.nodes.size()) {
        throw std::runtime_error("Graph contains a cycle");
    }

    const auto graphBufferSamples = sampleCount(config_.maxBlockSize, config_.outputChannels);
    std::fill_n(realtimeGraphBuffers_.begin(), graph.nodes.size() * graphBufferSamples, 0.0F);

    const auto nodeBuffer = [&](std::size_t nodeIndex) {
        return std::span<float>{realtimeGraphBuffers_}.subspan(nodeIndex * graphBufferSamples,
                                                               totalSamples);
    };

    for (std::size_t orderIndex = 0; orderIndex < orderedCount; ++orderIndex) {
        const auto nodeIndex = ordered[orderIndex];
        const auto& node = graph.nodes[nodeIndex];
        auto buffer = nodeBuffer(nodeIndex);

        if (node.kind == GraphNodeKind::Sine) {
            for (std::uint32_t frame = 0; frame < frames; ++frame) {
                const auto absoluteSample =
                    transport_.samplePosition + static_cast<std::int64_t>(frame);
                const auto value =
                    static_cast<float>(std::sin((static_cast<double>(absoluteSample) *
                                                 node.frequencyHz * 2.0 * std::numbers::pi) /
                                                config_.sampleRate) *
                                       node.gain);
                for (std::uint32_t channel = 0; channel < config_.outputChannels; ++channel) {
                    buffer[sampleCount(frame, config_.outputChannels) + channel] += value;
                }
            }
        }

        for (std::size_t connectionIndex = 0; connectionIndex < graph.connections.size();
             ++connectionIndex) {
            if (sourceIndices[connectionIndex] != nodeIndex) {
                continue;
            }
            const auto& connection = graph.connections[connectionIndex];
            auto destination = nodeBuffer(destinationIndices[connectionIndex]);
            const auto gain = connection.bypassSourceProcessing
                                  ? connection.gain
                                  : engineNodeOutputGain(node) * connection.gain;
            for (std::uint32_t frame = 0; frame < frames; ++frame) {
                for (std::uint32_t channel = 0; channel < config_.outputChannels; ++channel) {
                    const auto index = sampleCount(frame, config_.outputChannels) + channel;
                    const auto channelGain =
                        connection.bypassSourceProcessing
                            ? 1.0F
                            : engineNodeChannelGain(node, channel, config_.outputChannels);
                    destination[index] += buffer[index] * gain * channelGain;
                }
            }
        }
    }

    const auto& outputNode = graph.nodes[outputNodeIndex];
    const auto outputBuffer = nodeBuffer(outputNodeIndex);
    const auto outputGain = engineNodeOutputGain(outputNode);
    for (std::uint32_t frame = 0; frame < frames; ++frame) {
        for (std::uint32_t channel = 0; channel < config_.outputChannels; ++channel) {
            const auto index = sampleCount(frame, config_.outputChannels) + channel;
            interleavedOutput[index] =
                outputBuffer[index] * outputGain *
                engineNodeChannelGain(outputNode, channel, config_.outputChannels);
        }
    }

    advanceTransport(frames);
}

RenderedAudio AudioEngine::renderSilenceOffline(std::uint32_t frames) {
    RenderedAudio rendered{config_.outputChannels, frames,
                           std::vector<float>(sampleCount(frames, config_.outputChannels))};
    renderSilence(rendered.interleavedSamples, frames);
    return rendered;
}

RenderedAudio AudioEngine::renderSineOffline(std::uint32_t frames, double frequencyHz,
                                             float amplitude) {
    RenderedAudio rendered{config_.outputChannels, frames,
                           std::vector<float>(sampleCount(frames, config_.outputChannels))};
    renderSine(rendered.interleavedSamples, frames, frequencyHz, amplitude);
    return rendered;
}

RenderedAudio AudioEngine::renderMetronomeOffline(std::uint32_t frames) {
    RenderedAudio rendered{config_.outputChannels, frames,
                           std::vector<float>(sampleCount(frames, config_.outputChannels))};
    renderMetronome(rendered.interleavedSamples, frames);
    return rendered;
}

RenderedAudio AudioEngine::renderGraphOffline(const AudioGraph& graph, std::uint32_t frames) {
    RenderedAudio rendered{config_.outputChannels, frames,
                           std::vector<float>(sampleCount(frames, config_.outputChannels))};
    renderGraphBlock(graph, rendered.interleavedSamples, frames);
    return rendered;
}

} // namespace lamusica::audio
