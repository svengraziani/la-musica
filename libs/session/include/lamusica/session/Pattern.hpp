#pragma once

#include "lamusica/session/Midi.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace lamusica::session {

struct PatternStep {
    bool enabled{false};
    std::uint8_t pitch{60};
    std::uint8_t velocity{100};
    float probability{1.0F};
    std::uint8_t ratchets{1};
    bool tie{false};
    bool slide{false};
    bool accent{false};
};

struct PatternLane {
    std::string id;
    std::string name;
    std::uint8_t defaultPitch{60};
    std::uint32_t lengthSteps{16};
    std::vector<PatternStep> steps;
};

struct PatternClip {
    std::string id;
    std::string name;
    std::uint32_t lengthSteps{16};
    std::int64_t stepLengthSamples{6000};
    float swing{0.0F};
    std::uint32_t seed{1};
    std::vector<PatternLane> lanes;
};

struct PatternChain {
    std::vector<PatternClip> patterns;
};

struct PatternClipPlacement {
    PatternClip pattern;
    std::int64_t timelineStartSample{0};
};

[[nodiscard]] bool probabilityHit(float probability, std::uint32_t seed, std::uint32_t laneIndex,
                                  std::uint32_t stepIndex, std::uint32_t ratchetIndex) noexcept;
[[nodiscard]] MidiClipData patternToMidi(const PatternClip& pattern, std::string midiClipId);
[[nodiscard]] MidiClipData patternClipToMidi(const PatternClipPlacement& placement,
                                             std::string midiClipId);
[[nodiscard]] std::vector<MidiPlaybackEvent>
patternPlaybackEventsInRange(const PatternClipPlacement& placement, std::int64_t rangeStartSample,
                             std::int64_t rangeEndSample);
[[nodiscard]] PatternClip midiToPattern(const MidiClipData& midi, std::string patternId,
                                        std::string patternName, std::int64_t stepLengthSamples,
                                        std::uint32_t lengthSteps);
[[nodiscard]] PatternClip duplicatePatternVariation(const PatternClip& pattern, std::string newId,
                                                    std::string newName, std::uint32_t seedOffset);
[[nodiscard]] MidiClipData patternChainToMidi(const PatternChain& chain, std::string midiClipId);

} // namespace lamusica::session
