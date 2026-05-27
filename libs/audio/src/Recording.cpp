#include "lamusica/audio/Recording.hpp"

#include "lamusica/audio/WavFile.hpp"

#include <algorithm>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace lamusica::audio {
namespace {

void validateOptions(const RecordingOptions& options) {
    if (options.finalPath.empty()) {
        throw std::invalid_argument("Recording final path must not be empty");
    }
    if (options.temporaryPath.empty()) {
        throw std::invalid_argument("Recording temporary path must not be empty");
    }
    if (options.sampleRate <= 0.0) {
        throw std::invalid_argument("Recording sample rate must be positive");
    }
    if (options.channels == 0U) {
        throw std::invalid_argument("Recording channel count must be positive");
    }
    if (options.timelineStartSample < 0) {
        throw std::invalid_argument("Recording timeline start must not be negative");
    }
    if (options.measuredInputLatencySamples < 0) {
        throw std::invalid_argument("Recording latency must not be negative");
    }
}

std::int64_t latencyAlignedStart(const RecordingOptions& options) noexcept {
    return std::max<std::int64_t>(0, options.timelineStartSample -
                                         options.measuredInputLatencySamples);
}

} // namespace

RecordingPlan makeRecordingPlan(const RecordingWorkflowOptions& options) {
    if (options.trackId.empty()) {
        throw std::invalid_argument("Recording plan track id must not be empty");
    }
    if (options.transportStartSample < 0 || options.preRollSamples < 0 ||
        options.countInSamples < 0 || options.measuredInputLatencySamples < 0 ||
        options.punchInSample < 0 || options.punchOutSample < 0) {
        throw std::invalid_argument("Recording plan sample positions must not be negative");
    }
    if (options.punchEnabled && options.punchOutSample <= options.punchInSample) {
        throw std::invalid_argument("Recording punch range must be non-empty");
    }

    const auto armedStart =
        options.punchEnabled ? options.punchInSample : options.transportStartSample;
    const auto leadIn = options.preRollSamples + options.countInSamples;
    const auto captureStart = std::max<std::int64_t>(0, armedStart - leadIn);
    const auto alignedClipStart =
        std::max<std::int64_t>(0, armedStart - options.measuredInputLatencySamples);

    return {.trackId = options.trackId,
            .captureStartSample = captureStart,
            .punchInSample = options.punchEnabled ? options.punchInSample : armedStart,
            .punchOutSample = options.punchEnabled ? options.punchOutSample : 0,
            .clipStartSample = alignedClipStart,
            .preRollSamples = options.preRollSamples,
            .countInSamples = options.countInSamples,
            .measuredInputLatencySamples = options.measuredInputLatencySamples,
            .punchEnabled = options.punchEnabled};
}

bool hasInterruptedRecording(const RecordingOptions& options) {
    validateOptions(options);
    return std::filesystem::exists(options.temporaryPath);
}

RecordingRecoveryResult recoverInterruptedRecording(const RecordingOptions& options,
                                                    RecordingRecoveryAction action) {
    validateOptions(options);
    if (!std::filesystem::exists(options.temporaryPath)) {
        throw std::runtime_error("Interrupted recording temporary file was not found");
    }

    if (action == RecordingRecoveryAction::Discard) {
        std::filesystem::remove(options.temporaryPath);
        return {.action = action, .discarded = true};
    }

    const auto wav = readPcm16Wav(options.temporaryPath);
    if (options.finalPath.has_parent_path()) {
        std::filesystem::create_directories(options.finalPath.parent_path());
    }
    std::filesystem::rename(options.temporaryPath, options.finalPath);
    return {.action = action,
            .recovered = true,
            .committed = {.path = options.finalPath,
                          .frames = wav.audio.frames,
                          .channels = wav.audio.channels,
                          .timelineStartSample = latencyAlignedStart(options),
                          .measuredInputLatencySamples = options.measuredInputLatencySamples}};
}

void addTake(TakeLane& lane, RecordingTake take) {
    if (take.id.empty()) {
        throw std::invalid_argument("Recording take id must not be empty");
    }
    if (take.trackId.empty()) {
        throw std::invalid_argument("Recording take track id must not be empty");
    }
    if (take.path.empty()) {
        throw std::invalid_argument("Recording take path must not be empty");
    }
    if (take.timelineStartSample < 0) {
        throw std::invalid_argument("Recording take timeline start must not be negative");
    }
    if (take.frames == 0U) {
        throw std::invalid_argument("Recording take frame count must be positive");
    }
    if (take.channels == 0U) {
        throw std::invalid_argument("Recording take channel count must be positive");
    }
    if (!lane.trackId.empty() && lane.trackId != take.trackId) {
        throw std::invalid_argument("Recording take track id does not match lane");
    }
    if (std::ranges::any_of(lane.takes, [&take](const RecordingTake& existing) {
            return existing.id == take.id;
        })) {
        throw std::runtime_error("Recording take id already exists");
    }

    lane.trackId = take.trackId;
    take.takeNumber = static_cast<std::uint32_t>(lane.takes.size() + 1U);
    if (lane.takes.empty()) {
        take.active = true;
    }
    lane.takes.push_back(std::move(take));
}

void activateTake(TakeLane& lane, std::string_view takeId) {
    bool found = false;
    for (auto& take : lane.takes) {
        take.active = take.id == takeId;
        found = found || take.active;
    }
    if (!found) {
        throw std::runtime_error("Recording take was not found");
    }
}

RecordingSession::RecordingSession(RecordingOptions options) : options_(std::move(options)) {
    validateOptions(options_);
    captured_.channels = options_.channels;
}

RecordingSession::~RecordingSession() {
    discard();
}

void RecordingSession::appendInterleaved(std::span<const float> interleavedInput) {
    if (!active_) {
        throw std::runtime_error("Cannot append to inactive recording session");
    }
    if ((interleavedInput.size() % options_.channels) != 0U) {
        throw std::runtime_error("Recording input does not contain complete interleaved frames");
    }

    captured_.interleavedSamples.insert(captured_.interleavedSamples.end(),
                                        interleavedInput.begin(), interleavedInput.end());
    captured_.frames =
        static_cast<std::uint32_t>(captured_.interleavedSamples.size() / options_.channels);
}

CommittedRecording RecordingSession::commit() {
    if (!active_) {
        throw std::runtime_error("Cannot commit inactive recording session");
    }

    writePcm16Wav(options_.temporaryPath, captured_, options_.sampleRate);
    if (options_.finalPath.has_parent_path()) {
        std::filesystem::create_directories(options_.finalPath.parent_path());
    }
    std::filesystem::rename(options_.temporaryPath, options_.finalPath);
    active_ = false;

    return {.path = options_.finalPath,
            .frames = captured_.frames,
            .channels = captured_.channels,
            .timelineStartSample = latencyAlignedStart(options_),
            .measuredInputLatencySamples = options_.measuredInputLatencySamples};
}

void RecordingSession::discard() noexcept {
    if (!active_) {
        return;
    }

    active_ = false;
    std::error_code error;
    std::filesystem::remove(options_.temporaryPath, error);
}

bool RecordingSession::active() const noexcept {
    return active_;
}

std::uint32_t RecordingSession::frames() const noexcept {
    return captured_.frames;
}

const RecordingOptions& RecordingSession::options() const noexcept {
    return options_;
}

} // namespace lamusica::audio
