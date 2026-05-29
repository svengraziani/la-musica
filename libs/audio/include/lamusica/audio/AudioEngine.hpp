#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace lamusica::audio {

struct AudioGraph;

struct EngineConfig {
    double sampleRate{48000.0};
    std::uint32_t maxBlockSize{512};
    std::uint32_t outputChannels{2};
};

struct AudioDeviceInfo {
    std::string id{"default-output"};
    std::string name{"Default Output"};
    double sampleRate{48000.0};
    std::uint32_t bufferSize{512};
    std::uint32_t inputChannels{0};
    std::uint32_t outputChannels{2};
};

struct TimeSignature {
    std::uint32_t numerator{4};
    std::uint32_t denominator{4};
};

struct BarBeatPosition {
    std::int64_t bar{1};
    std::uint32_t beat{1};
    double ppqOffset{0.0};
};

struct TransportState {
    bool playing{false};
    bool recording{false};
    bool loopEnabled{false};
    std::int64_t samplePosition{0};
    std::int64_t loopStartSample{0};
    std::int64_t loopEndSample{0};
    double tempoBpm{120.0};
    TimeSignature timeSignature{};
};

struct RenderedAudio {
    std::uint32_t channels{0};
    std::uint32_t frames{0};
    std::vector<float> interleavedSamples;
};

enum class RealtimeCommandType {
    Play,
    Stop,
    Record,
    Seek,
    SetTempo,
    SetLoop,
};

struct RealtimeCommand {
    RealtimeCommandType type{RealtimeCommandType::Stop};
    bool flag{false};
    std::int64_t sampleA{0};
    std::int64_t sampleB{0};
    double value{0.0};
};

class RealtimeCommandQueue {
  public:
    [[nodiscard]] bool push(RealtimeCommand command) noexcept;
    [[nodiscard]] std::optional<RealtimeCommand> pop() noexcept;
    [[nodiscard]] bool empty() const noexcept;
    [[nodiscard]] bool full() const noexcept;
    [[nodiscard]] std::size_t size() const noexcept;
    void clear() noexcept;

  private:
    static constexpr std::size_t capacity = 64;
    std::array<RealtimeCommand, capacity> commands_{};
    std::size_t readIndex_{0};
    std::size_t writeIndex_{0};
    std::size_t size_{0};
};

class AudioEngine {
  public:
    explicit AudioEngine(EngineConfig config);

    [[nodiscard]] const EngineConfig& config() const noexcept;
    [[nodiscard]] const AudioDeviceInfo& device() const noexcept;
    [[nodiscard]] const TransportState& transport() const noexcept;

    void selectDevice(AudioDeviceInfo device);
    [[nodiscard]] bool enqueueCommand(RealtimeCommand command) noexcept;
    void processRealtimeCommands() noexcept;

    void play() noexcept;
    void stop() noexcept;
    void record(bool enabled) noexcept;
    void seekSamples(std::int64_t samplePosition) noexcept;
    void setTempo(double tempoBpm) noexcept;
    void setTimeSignature(TimeSignature timeSignature) noexcept;
    void setLoop(bool enabled, std::int64_t startSample, std::int64_t endSample) noexcept;

    [[nodiscard]] double samplesToPpq(std::int64_t samples) const noexcept;
    [[nodiscard]] std::int64_t ppqToSamples(double ppq) const noexcept;
    [[nodiscard]] BarBeatPosition samplesToBarBeat(std::int64_t samplePosition) const noexcept;
    [[nodiscard]] std::int64_t barBeatToSamples(BarBeatPosition position) const noexcept;

    void renderSilence(std::span<float> interleavedOutput, std::uint32_t frames) noexcept;
    void renderSine(std::span<float> interleavedOutput, std::uint32_t frames, double frequencyHz,
                    float amplitude) noexcept;
    void renderMetronome(std::span<float> interleavedOutput, std::uint32_t frames) noexcept;
    void renderGraphBlock(const AudioGraph& graph, std::span<float> interleavedOutput,
                          std::uint32_t frames);
    void renderGraphDeviceBlock(const AudioGraph& graph, std::span<float> interleavedOutput,
                                std::uint32_t deviceFrames);

    [[nodiscard]] RenderedAudio renderSilenceOffline(std::uint32_t frames);
    [[nodiscard]] RenderedAudio renderSineOffline(std::uint32_t frames, double frequencyHz,
                                                  float amplitude);
    [[nodiscard]] RenderedAudio renderMetronomeOffline(std::uint32_t frames);
    [[nodiscard]] RenderedAudio renderGraphOffline(const AudioGraph& graph, std::uint32_t frames);

  private:
    void applyRealtimeCommand(RealtimeCommand command) noexcept;
    void advanceTransport(std::uint32_t frames) noexcept;

    static constexpr std::size_t maxRealtimeGraphNodes = 512;
    static constexpr std::size_t maxRealtimeGraphConnections = 4096;

    EngineConfig config_;
    AudioDeviceInfo device_{};
    TransportState transport_{};
    RealtimeCommandQueue commandQueue_;
    std::vector<float> realtimeGraphBuffers_;
    std::vector<float> deviceRenderBuffer_;
};

} // namespace lamusica::audio
