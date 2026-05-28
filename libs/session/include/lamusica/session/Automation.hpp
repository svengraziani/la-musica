#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace lamusica::session {

struct MixerState;
struct Clip;
struct PluginInsertChain;

enum class AutomationCurve {
    Step,
    Linear,
};

enum class AutomationMode {
    Off,
    Read,
    Write,
    Touch,
    Latch,
    Trim,
};

enum class AutomationTargetKind {
    Mixer,
    Plugin,
    Instrument,
    Clip,
};

struct AutomationPoint {
    std::int64_t samplePosition{0};
    float value{0.0F};
    AutomationCurve curveToNext{AutomationCurve::Linear};
};

struct AutomationRegion {
    std::int64_t startSample{0};
    std::int64_t endSample{0};
    std::vector<AutomationPoint> points;
};

struct AutomationLaneData {
    std::string id;
    AutomationTargetKind targetKind{AutomationTargetKind::Mixer};
    std::string targetId;
    std::string parameterId;
    AutomationMode mode{AutomationMode::Read};
    float defaultValue{0.0F};
    std::vector<AutomationRegion> regions;
};

struct AutomationWriteSample {
    std::int64_t samplePosition{0};
    float value{0.0F};
    bool touched{true};
};

struct AutomationCommandBatch {
    std::string laneId;
    AutomationMode mode{AutomationMode::Read};
    std::vector<AutomationPoint> points;
};

struct AutomationParameterBinding {
    AutomationTargetKind targetKind{AutomationTargetKind::Mixer};
    std::string targetId;
    std::string parameterId;
    std::string displayName;
    float defaultValue{0.0F};
    float minimumValue{0.0F};
    float maximumValue{1.0F};
    bool stepped{false};
};

struct AutomationLaneSelection {
    AutomationTargetKind targetKind{AutomationTargetKind::Mixer};
    std::string targetId;
    std::string parameterId;
};

[[nodiscard]] float evaluateAutomation(const AutomationLaneData& lane, std::int64_t samplePosition);
[[nodiscard]] std::vector<float> evaluateAutomationBlock(const AutomationLaneData& lane,
                                                         std::int64_t startSample,
                                                         std::uint32_t frames);
[[nodiscard]] float effectiveAutomationValue(const AutomationLaneData& lane,
                                             std::int64_t samplePosition, float currentValue);
[[nodiscard]] const AutomationLaneData*
selectAutomationLane(std::span<const AutomationLaneData> lanes,
                     const AutomationLaneSelection& selection);
void addAutomationPoint(AutomationLaneData& lane, std::int64_t samplePosition, float value,
                        AutomationCurve curveToNext = AutomationCurve::Linear);
[[nodiscard]] AutomationCommandBatch
captureAutomationWrite(AutomationLaneData& lane, std::span<const AutomationWriteSample> samples,
                       float thinningTolerance = 0.0F);
void applyAutomationToMixer(MixerState& mixer, const AutomationLaneData& lane,
                            std::int64_t samplePosition);
void applyAutomationBlockToMixer(MixerState& mixer, std::span<const AutomationLaneData> lanes,
                                 std::int64_t samplePosition);
void applyAutomationToPluginChain(PluginInsertChain& chain, const AutomationLaneData& lane,
                                  std::int64_t samplePosition);
void applyAutomationBlockToPluginChain(PluginInsertChain& chain,
                                       std::span<const AutomationLaneData> lanes,
                                       std::int64_t samplePosition);
void applyAutomationToInstrumentChain(PluginInsertChain& chain, const AutomationLaneData& lane,
                                      std::int64_t samplePosition);
void applyAutomationToClip(Clip& clip, const AutomationLaneData& lane, std::int64_t samplePosition);
void applyAutomationBlockToClips(std::span<Clip> clips, std::span<const AutomationLaneData> lanes,
                                 std::int64_t samplePosition);
void thinAutomationPoints(AutomationLaneData& lane, float tolerance);

} // namespace lamusica::session
