#pragma once

#include "lamusica/audio/AudioEngine.hpp"

#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

namespace lamusica::audio {

struct RecordingOptions {
    std::filesystem::path finalPath;
    std::filesystem::path temporaryPath;
    double sampleRate{48000.0};
    std::uint32_t channels{2};
    std::int64_t timelineStartSample{0};
    std::int64_t measuredInputLatencySamples{0};
};

struct CommittedRecording {
    std::filesystem::path path;
    std::uint32_t frames{0};
    std::uint32_t channels{0};
    std::int64_t timelineStartSample{0};
    std::int64_t measuredInputLatencySamples{0};
};

enum class RecordingRecoveryAction {
    Recover,
    Discard,
};

struct RecordingRecoveryResult {
    RecordingRecoveryAction action{RecordingRecoveryAction::Discard};
    bool recovered{false};
    bool discarded{false};
    CommittedRecording committed;
};

struct RecordingWorkflowOptions {
    std::string trackId;
    std::int64_t transportStartSample{0};
    std::int64_t punchInSample{0};
    std::int64_t punchOutSample{0};
    std::int64_t preRollSamples{0};
    std::int64_t countInSamples{0};
    std::int64_t measuredInputLatencySamples{0};
    bool punchEnabled{false};
    bool inputMonitoringEnabled{false};
};

struct RecordingLatencyMeasurement {
    std::int64_t measuredInputLatencySamples{0};
    float referencePeak{0.0F};
    float recordedPeak{0.0F};
    bool valid{false};
};

struct RecordingPlan {
    std::string trackId;
    std::int64_t captureStartSample{0};
    std::int64_t punchInSample{0};
    std::int64_t punchOutSample{0};
    std::int64_t clipStartSample{0};
    std::int64_t preRollSamples{0};
    std::int64_t countInSamples{0};
    std::int64_t measuredInputLatencySamples{0};
    bool punchEnabled{false};
    bool inputMonitoringEnabled{false};
};

struct RecordingTake {
    std::string id;
    std::string trackId;
    std::filesystem::path path;
    std::int64_t timelineStartSample{0};
    std::uint32_t frames{0};
    std::uint32_t channels{0};
    std::uint32_t takeNumber{1};
    bool active{false};
};

struct TakeLane {
    std::string trackId;
    std::vector<RecordingTake> takes;
};

[[nodiscard]] RecordingLatencyMeasurement
measureRecordingLatency(std::span<const float> referenceImpulse,
                        std::span<const float> recordedInput, float threshold = 0.5F);
[[nodiscard]] RecordingPlan makeRecordingPlan(const RecordingWorkflowOptions& options);
[[nodiscard]] bool hasInterruptedRecording(const RecordingOptions& options);
[[nodiscard]] RecordingRecoveryResult recoverInterruptedRecording(const RecordingOptions& options,
                                                                  RecordingRecoveryAction action);
void addTake(TakeLane& lane, RecordingTake take);
void activateTake(TakeLane& lane, std::string_view takeId);

class RecordingSession {
  public:
    explicit RecordingSession(RecordingOptions options);
    ~RecordingSession();

    RecordingSession(const RecordingSession&) = delete;
    RecordingSession& operator=(const RecordingSession&) = delete;
    RecordingSession(RecordingSession&&) noexcept = default;
    RecordingSession& operator=(RecordingSession&&) noexcept = default;

    void appendInterleaved(std::span<const float> interleavedInput);
    [[nodiscard]] CommittedRecording commit();
    void discard() noexcept;

    [[nodiscard]] bool active() const noexcept;
    [[nodiscard]] std::uint32_t frames() const noexcept;
    [[nodiscard]] const RecordingOptions& options() const noexcept;

  private:
    RecordingOptions options_;
    RenderedAudio captured_;
    bool active_{true};
};

} // namespace lamusica::audio
