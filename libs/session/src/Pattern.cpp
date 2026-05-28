#include "lamusica/session/Pattern.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <sstream>
#include <stdexcept>

namespace lamusica::session {
namespace {

std::uint32_t mixSeed(std::uint32_t seed, std::uint32_t laneIndex, std::uint32_t stepIndex,
                      std::uint32_t ratchetIndex) noexcept {
    auto value = seed;
    value ^= laneIndex + 0x9e3779b9U + (value << 6U) + (value >> 2U);
    value ^= stepIndex + 0x85ebca6bU + (value << 6U) + (value >> 2U);
    value ^= ratchetIndex + 0xc2b2ae35U + (value << 6U) + (value >> 2U);
    return value;
}

std::int64_t swungStepStart(const PatternClip& pattern, std::uint32_t stepIndex) {
    auto start = static_cast<std::int64_t>(stepIndex) * pattern.stepLengthSamples;
    if (stepIndex % 2U == 1U) {
        start +=
            static_cast<std::int64_t>(std::llround(static_cast<float>(pattern.stepLengthSamples) *
                                                   std::clamp(pattern.swing, -0.5F, 0.5F)));
    }
    return std::max<std::int64_t>(0, start);
}

std::string noteId(std::string_view laneId, std::uint32_t stepIndex, std::uint32_t ratchetIndex) {
    std::ostringstream output;
    output << laneId << "-" << stepIndex << "-" << ratchetIndex;
    return output.str();
}

} // namespace

bool probabilityHit(float probability, std::uint32_t seed, std::uint32_t laneIndex,
                    std::uint32_t stepIndex, std::uint32_t ratchetIndex) noexcept {
    if (probability <= 0.0F) {
        return false;
    }
    if (probability >= 1.0F) {
        return true;
    }

    const auto value = mixSeed(seed, laneIndex, stepIndex, ratchetIndex);
    const auto normalized = static_cast<float>(value % 10000U) / 9999.0F;
    return normalized <= probability;
}

MidiClipData patternToMidi(const PatternClip& pattern, std::string midiClipId) {
    if (pattern.stepLengthSamples <= 0) {
        throw std::runtime_error("Pattern step length must be positive");
    }

    MidiClipData midi{.clipId = std::move(midiClipId)};

    for (std::uint32_t laneIndex = 0; laneIndex < pattern.lanes.size(); ++laneIndex) {
        const auto& lane = pattern.lanes[laneIndex];
        if (lane.lengthSteps == 0U) {
            throw std::runtime_error("Pattern lane length must be positive");
        }
        for (std::uint32_t stepIndex = 0; stepIndex < pattern.lengthSteps; ++stepIndex) {
            const auto laneStepIndex = stepIndex % lane.lengthSteps;
            if (laneStepIndex >= lane.steps.size()) {
                continue;
            }
            const auto& step = lane.steps[laneStepIndex];
            if (!step.enabled) {
                continue;
            }

            const auto ratchets = std::max<std::uint8_t>(1, step.ratchets);
            const auto ratchetLength = pattern.stepLengthSamples / ratchets;
            for (std::uint32_t ratchetIndex = 0; ratchetIndex < ratchets; ++ratchetIndex) {
                if (!probabilityHit(step.probability, pattern.seed, laneIndex, stepIndex,
                                    ratchetIndex)) {
                    continue;
                }

                const auto generatedNoteId = noteId(lane.id, stepIndex, ratchetIndex);
                midi.notes.push_back(
                    {.id = generatedNoteId,
                     .startSample = swungStepStart(pattern, stepIndex) +
                                    static_cast<std::int64_t>(ratchetIndex) * ratchetLength,
                     .lengthSamples = step.tie ? pattern.stepLengthSamples : ratchetLength,
                     .pitch = step.pitch == 0 ? lane.defaultPitch : step.pitch,
                     .velocity = step.accent ? static_cast<std::uint8_t>(std::min<int>(
                                                   127, static_cast<int>(step.velocity) + 16))
                                             : step.velocity});
                if (step.slide) {
                    midi.metadata.push_back({.key = "slide:" + generatedNoteId, .value = "true"});
                }
            }
        }
    }

    std::ranges::sort(midi.notes, [](const MidiNote& left, const MidiNote& right) {
        if (left.startSample != right.startSample) {
            return left.startSample < right.startSample;
        }
        return left.pitch < right.pitch;
    });
    return midi;
}

MidiClipData patternClipToMidi(const PatternClipPlacement& placement, std::string midiClipId) {
    if (placement.timelineStartSample < 0) {
        throw std::runtime_error("Pattern clip timeline start must not be negative");
    }

    auto midi = patternToMidi(placement.pattern, std::move(midiClipId));
    for (auto& note : midi.notes) {
        note.startSample += placement.timelineStartSample;
    }
    return midi;
}

PatternClip midiToPattern(const MidiClipData& midi, std::string patternId, std::string patternName,
                          std::int64_t stepLengthSamples, std::uint32_t lengthSteps) {
    if (patternId.empty() || patternName.empty()) {
        throw std::runtime_error("Pattern id and name must not be empty");
    }
    if (stepLengthSamples <= 0) {
        throw std::runtime_error("Pattern step length must be positive");
    }
    if (lengthSteps == 0U) {
        throw std::runtime_error("Pattern length must be positive");
    }

    PatternClip pattern{.id = std::move(patternId),
                        .name = std::move(patternName),
                        .lengthSteps = lengthSteps,
                        .stepLengthSamples = stepLengthSamples};

    std::map<std::pair<std::uint8_t, std::uint32_t>, std::uint8_t> ratchetsByPitchStep;
    for (const auto& note : midi.notes) {
        if (note.muted || note.startSample < 0) {
            continue;
        }
        const auto stepIndex = static_cast<std::uint32_t>(note.startSample / stepLengthSamples);
        if (stepIndex < lengthSteps) {
            auto& ratchets = ratchetsByPitchStep[{note.pitch, stepIndex}];
            ratchets = static_cast<std::uint8_t>(std::min<int>(127, ratchets + 1));
        }
    }

    for (const auto& note : midi.notes) {
        if (note.muted) {
            continue;
        }
        if (note.startSample < 0 || note.lengthSamples <= 0) {
            throw std::runtime_error("MIDI note timing cannot be converted to a pattern");
        }
        const auto stepIndex = static_cast<std::uint32_t>(note.startSample / stepLengthSamples);
        if (stepIndex >= lengthSteps) {
            continue;
        }

        const auto laneId = "pitch-" + std::to_string(note.pitch);
        auto lane = std::ranges::find_if(
            pattern.lanes, [&laneId](const PatternLane& item) { return item.id == laneId; });
        if (lane == pattern.lanes.end()) {
            pattern.lanes.push_back({.id = laneId,
                                     .name = "Pitch " + std::to_string(note.pitch),
                                     .defaultPitch = note.pitch,
                                     .lengthSteps = lengthSteps,
                                     .steps = std::vector<PatternStep>(lengthSteps)});
            lane = pattern.lanes.end() - 1;
        }

        auto& step = lane->steps[stepIndex];
        step.enabled = true;
        step.pitch = note.pitch;
        step.velocity = note.velocity;
        step.probability = 1.0F;
        step.ratchets = std::max<std::uint8_t>(1, ratchetsByPitchStep[{note.pitch, stepIndex}]);
        step.tie = note.lengthSamples > stepLengthSamples;
    }

    std::ranges::sort(pattern.lanes, {}, &PatternLane::defaultPitch);
    return pattern;
}

PatternClip duplicatePatternVariation(const PatternClip& pattern, std::string newId,
                                      std::string newName, std::uint32_t seedOffset) {
    auto variation = pattern;
    variation.id = std::move(newId);
    variation.name = std::move(newName);
    variation.seed += seedOffset;
    return variation;
}

MidiClipData patternChainToMidi(const PatternChain& chain, std::string midiClipId) {
    MidiClipData combined{.clipId = std::move(midiClipId)};
    std::int64_t offset = 0;

    for (const auto& pattern : chain.patterns) {
        auto midi = patternToMidi(pattern, pattern.id + "-midi");
        for (auto& note : midi.notes) {
            note.startSample += offset;
            combined.notes.push_back(std::move(note));
        }
        offset += static_cast<std::int64_t>(pattern.lengthSteps) * pattern.stepLengthSamples;
    }

    return combined;
}

} // namespace lamusica::session
