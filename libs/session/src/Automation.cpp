#include "lamusica/session/Automation.hpp"

#include "lamusica/session/Mixer.hpp"
#include "lamusica/session/Plugin.hpp"
#include "lamusica/session/ProjectManifest.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string_view>

namespace lamusica::session {
namespace {

bool containsSample(const AutomationRegion& region, std::int64_t samplePosition) {
    return samplePosition >= region.startSample && samplePosition <= region.endSample;
}

void sortRegion(AutomationRegion& region) {
    std::ranges::sort(region.points, {}, &AutomationPoint::samplePosition);
}

bool valueToBool(float value) noexcept {
    return value >= 0.5F;
}

float clampPan(float value) noexcept {
    return std::clamp(value, -1.0F, 1.0F);
}

std::int64_t roundedSampleValue(float value) {
    if (!std::isfinite(value) || value < 0.0F) {
        throw std::runtime_error("Automation sample parameter must be finite and non-negative");
    }
    return static_cast<std::int64_t>(std::llround(value));
}

} // namespace

float evaluateAutomation(const AutomationLaneData& lane, std::int64_t samplePosition) {
    if (lane.mode == AutomationMode::Off) {
        return lane.defaultValue;
    }

    for (const auto& region : lane.regions) {
        if (!containsSample(region, samplePosition) || region.points.empty()) {
            continue;
        }

        if (samplePosition <= region.points.front().samplePosition) {
            return region.points.front().value;
        }

        if (const auto exact = std::ranges::find_if(region.points,
                                                    [samplePosition](const AutomationPoint& point) {
                                                        return point.samplePosition ==
                                                               samplePosition;
                                                    });
            exact != region.points.end()) {
            return exact->value;
        }

        for (std::size_t index = 0; index + 1 < region.points.size(); ++index) {
            const auto& left = region.points[index];
            const auto& right = region.points[index + 1];
            if (samplePosition < left.samplePosition || samplePosition > right.samplePosition) {
                continue;
            }

            if (left.curveToNext == AutomationCurve::Step ||
                right.samplePosition == left.samplePosition) {
                return left.value;
            }

            const auto position = static_cast<float>(samplePosition - left.samplePosition) /
                                  static_cast<float>(right.samplePosition - left.samplePosition);
            return left.value + ((right.value - left.value) * position);
        }

        return region.points.back().value;
    }

    return lane.defaultValue;
}

std::vector<float> evaluateAutomationBlock(const AutomationLaneData& lane, std::int64_t startSample,
                                           std::uint32_t frames) {
    std::vector<float> values;
    values.reserve(frames);
    for (std::uint32_t frame = 0; frame < frames; ++frame) {
        values.push_back(evaluateAutomation(lane, startSample + static_cast<std::int64_t>(frame)));
    }
    return values;
}

float effectiveAutomationValue(const AutomationLaneData& lane, std::int64_t samplePosition,
                               float currentValue) {
    if (lane.mode == AutomationMode::Off) {
        return currentValue;
    }
    if (lane.mode == AutomationMode::Trim) {
        return currentValue + evaluateAutomation(lane, samplePosition);
    }
    return evaluateAutomation(lane, samplePosition);
}

const AutomationLaneData* selectAutomationLane(std::span<const AutomationLaneData> lanes,
                                               const AutomationLaneSelection& selection) {
    if (selection.targetId.empty()) {
        throw std::invalid_argument("Automation selection target id must not be empty");
    }
    if (selection.parameterId.empty()) {
        throw std::invalid_argument("Automation selection parameter id must not be empty");
    }

    const auto lane = std::ranges::find_if(lanes, [&selection](const AutomationLaneData& item) {
        return item.targetId == selection.targetId && item.parameterId == selection.parameterId;
    });
    if (lane == lanes.end()) {
        return nullptr;
    }
    return &*lane;
}

void addAutomationPoint(AutomationLaneData& lane, std::int64_t samplePosition, float value,
                        AutomationCurve curveToNext) {
    if (lane.id.empty()) {
        throw std::invalid_argument("Automation lane id must not be empty");
    }
    if (samplePosition < 0) {
        throw std::invalid_argument("Automation point sample must not be negative");
    }
    if (!std::isfinite(value)) {
        throw std::invalid_argument("Automation point value must be finite");
    }

    if (lane.regions.empty()) {
        lane.regions.push_back({.startSample = samplePosition, .endSample = samplePosition});
    }

    auto& region = lane.regions.front();
    region.startSample = std::min(region.startSample, samplePosition);
    region.endSample = std::max(region.endSample, samplePosition);

    const auto existing =
        std::ranges::find_if(region.points, [samplePosition](const AutomationPoint& point) {
            return point.samplePosition == samplePosition;
        });
    if (existing == region.points.end()) {
        region.points.push_back(
            {.samplePosition = samplePosition, .value = value, .curveToNext = curveToNext});
    } else {
        existing->value = value;
        existing->curveToNext = curveToNext;
    }

    sortRegion(region);
}

AutomationCommandBatch captureAutomationWrite(AutomationLaneData& lane,
                                              std::span<const AutomationWriteSample> samples,
                                              float thinningTolerance) {
    if (lane.mode == AutomationMode::Off || lane.mode == AutomationMode::Read) {
        return {.laneId = lane.id, .mode = lane.mode};
    }
    if (thinningTolerance < 0.0F) {
        throw std::invalid_argument("Automation thinning tolerance must not be negative");
    }

    AutomationCommandBatch batch{.laneId = lane.id, .mode = lane.mode};
    bool latchActive = false;
    float latchedValue = lane.defaultValue;
    for (const auto& sample : samples) {
        if (sample.samplePosition < 0) {
            throw std::invalid_argument("Automation write sample position must not be negative");
        }

        bool shouldWrite = false;
        float value = sample.value;
        switch (lane.mode) {
        case AutomationMode::Off:
        case AutomationMode::Read:
            break;
        case AutomationMode::Write:
            shouldWrite = true;
            break;
        case AutomationMode::Touch:
            shouldWrite = sample.touched;
            break;
        case AutomationMode::Latch:
            latchActive = latchActive || sample.touched;
            if (sample.touched) {
                latchedValue = sample.value;
            }
            shouldWrite = latchActive;
            value = latchedValue;
            break;
        case AutomationMode::Trim:
            shouldWrite = sample.touched;
            value = evaluateAutomation(lane, sample.samplePosition) + sample.value;
            break;
        }

        if (!shouldWrite) {
            continue;
        }
        addAutomationPoint(lane, sample.samplePosition, value);
        batch.points.push_back({.samplePosition = sample.samplePosition, .value = value});
    }

    if (thinningTolerance > 0.0F) {
        thinAutomationPoints(lane, thinningTolerance);
    }
    return batch;
}

void applyAutomationToMixer(MixerState& mixer, const AutomationLaneData& lane,
                            std::int64_t samplePosition) {
    if (lane.targetId.empty()) {
        throw std::runtime_error("Automation lane target id must not be empty");
    }

    auto* channel = findChannel(mixer, lane.targetId);
    if (channel == nullptr) {
        throw std::runtime_error("Automation target channel was not found: " + lane.targetId);
    }

    const auto parameter = std::string_view{lane.parameterId};
    if (parameter == "volumeDb") {
        channel->volumeDb = effectiveAutomationValue(lane, samplePosition, channel->volumeDb);
    } else if (parameter == "pan") {
        channel->pan = clampPan(effectiveAutomationValue(lane, samplePosition, channel->pan));
    } else if (parameter == "mute" || parameter == "muted") {
        channel->muted = valueToBool(
            effectiveAutomationValue(lane, samplePosition, channel->muted ? 1.0F : 0.0F));
    } else if (parameter == "solo") {
        channel->solo = valueToBool(
            effectiveAutomationValue(lane, samplePosition, channel->solo ? 1.0F : 0.0F));
    } else if (parameter == "recordArmed") {
        channel->recordArmed = valueToBool(
            effectiveAutomationValue(lane, samplePosition, channel->recordArmed ? 1.0F : 0.0F));
    } else if (parameter == "inputMonitoring") {
        channel->inputMonitoring = valueToBool(
            effectiveAutomationValue(lane, samplePosition, channel->inputMonitoring ? 1.0F : 0.0F));
    } else if (parameter == "phaseInverted") {
        channel->phaseInverted = valueToBool(
            effectiveAutomationValue(lane, samplePosition, channel->phaseInverted ? 1.0F : 0.0F));
    } else {
        throw std::runtime_error("Unsupported mixer automation parameter: " + lane.parameterId);
    }
}

void applyAutomationBlockToMixer(MixerState& mixer, std::span<const AutomationLaneData> lanes,
                                 std::int64_t samplePosition) {
    for (const auto& lane : lanes) {
        applyAutomationToMixer(mixer, lane, samplePosition);
    }
}

void applyAutomationToPluginChain(PluginInsertChain& chain, const AutomationLaneData& lane,
                                  std::int64_t samplePosition) {
    if (lane.targetId.empty()) {
        throw std::runtime_error("Automation lane target id must not be empty");
    }
    if (lane.parameterId.empty()) {
        throw std::runtime_error("Automation lane parameter id must not be empty");
    }

    auto* insert = findInsert(chain, lane.targetId);
    if (insert == nullptr) {
        throw std::runtime_error("Automation target plugin insert was not found: " + lane.targetId);
    }

    const auto currentValue =
        findParameterValue(*insert, lane.parameterId).value_or(lane.defaultValue);
    setParameterValue(*insert, lane.parameterId,
                      effectiveAutomationValue(lane, samplePosition, currentValue));
}

void applyAutomationBlockToPluginChain(PluginInsertChain& chain,
                                       std::span<const AutomationLaneData> lanes,
                                       std::int64_t samplePosition) {
    for (const auto& lane : lanes) {
        applyAutomationToPluginChain(chain, lane, samplePosition);
    }
}

void applyAutomationToInstrumentChain(PluginInsertChain& chain, const AutomationLaneData& lane,
                                      std::int64_t samplePosition) {
    applyAutomationToPluginChain(chain, lane, samplePosition);
}

void applyAutomationToClip(Clip& clip, const AutomationLaneData& lane,
                           std::int64_t samplePosition) {
    if (lane.targetId.empty()) {
        throw std::runtime_error("Automation lane target id must not be empty");
    }
    if (lane.targetId != clip.id) {
        throw std::runtime_error("Automation target clip was not found: " + lane.targetId);
    }

    const auto parameter = std::string_view{lane.parameterId};
    if (parameter == "gainDb") {
        clip.gainDb = effectiveAutomationValue(lane, samplePosition, clip.gainDb);
    } else if (parameter == "mute" || parameter == "muted") {
        clip.muted =
            valueToBool(effectiveAutomationValue(lane, samplePosition, clip.muted ? 1.0F : 0.0F));
    } else if (parameter == "reversed") {
        clip.reversed = valueToBool(
            effectiveAutomationValue(lane, samplePosition, clip.reversed ? 1.0F : 0.0F));
    } else if (parameter == "fadeInSamples") {
        clip.fadeInSamples = roundedSampleValue(
            effectiveAutomationValue(lane, samplePosition, static_cast<float>(clip.fadeInSamples)));
    } else if (parameter == "fadeOutSamples") {
        clip.fadeOutSamples = roundedSampleValue(effectiveAutomationValue(
            lane, samplePosition, static_cast<float>(clip.fadeOutSamples)));
    } else if (parameter == "sourceOffsetSamples") {
        clip.sourceOffsetSamples = roundedSampleValue(effectiveAutomationValue(
            lane, samplePosition, static_cast<float>(clip.sourceOffsetSamples)));
    } else {
        throw std::runtime_error("Unsupported clip automation parameter: " + lane.parameterId);
    }
}

void applyAutomationBlockToClips(std::span<Clip> clips, std::span<const AutomationLaneData> lanes,
                                 std::int64_t samplePosition) {
    for (const auto& lane : lanes) {
        auto clip = std::ranges::find_if(
            clips, [&lane](const Clip& item) { return item.id == lane.targetId; });
        if (clip == clips.end()) {
            throw std::runtime_error("Automation target clip was not found: " + lane.targetId);
        }
        applyAutomationToClip(*clip, lane, samplePosition);
    }
}

void thinAutomationPoints(AutomationLaneData& lane, float tolerance) {
    for (auto& region : lane.regions) {
        if (region.points.size() <= 2) {
            continue;
        }

        std::vector<AutomationPoint> thinned;
        thinned.push_back(region.points.front());
        for (std::size_t index = 1; index + 1 < region.points.size(); ++index) {
            const auto& previous = thinned.back();
            const auto& current = region.points[index];
            if (std::abs(current.value - previous.value) > tolerance) {
                thinned.push_back(current);
            }
        }
        thinned.push_back(region.points.back());
        region.points = std::move(thinned);
    }
}

} // namespace lamusica::session
