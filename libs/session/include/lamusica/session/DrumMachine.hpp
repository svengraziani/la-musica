#pragma once

#include "lamusica/audio/AudioEngine.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace lamusica::session {

struct VelocityLayer {
    std::uint8_t minVelocity{1};
    std::uint8_t maxVelocity{127};
    std::string assetId;
};

struct DrumPad {
    std::string id;
    std::string name;
    std::uint8_t midiNote{36};
    std::string color{"#808080"};
    std::optional<std::uint8_t> chokeGroup;
    bool muted{false};
    bool solo{false};
    std::string outputRoute{"master"};
    float gainDb{0.0F};
    float pitchSemitones{0.0F};
    bool reversed{false};
    std::int64_t sampleStart{0};
    std::int64_t sampleEnd{0};
    std::int64_t attackSamples{0};
    std::int64_t releaseSamples{0};
    float lowPassCoefficient{1.0F};
    std::vector<VelocityLayer> velocityLayers;
};

struct DrumMachinePreset {
    std::string id;
    std::string name;
    std::string license{"unknown"};
    std::string licenseUrl;
    bool bundledAssetsIncluded{false};
    std::vector<DrumPad> pads;
};

struct DrumTrigger {
    std::string padId;
    std::string assetId;
    std::string outputRoute;
    std::int64_t samplePosition{0};
    std::uint8_t velocity{100};
    bool chokedPrevious{false};
};

struct DrumPadEvent {
    std::int64_t samplePosition{0};
    std::uint8_t midiNote{36};
    std::uint8_t velocity{100};
};

[[nodiscard]] const DrumPad* findPadForMidiNote(const DrumMachinePreset& preset,
                                                std::uint8_t midiNote);
[[nodiscard]] std::string selectLayerAsset(const DrumPad& pad, std::uint8_t velocity);
[[nodiscard]] std::vector<DrumTrigger>
renderDrumTriggers(const DrumMachinePreset& preset,
                   const std::vector<std::pair<std::int64_t, std::uint8_t>>& events);
[[nodiscard]] std::vector<DrumTrigger> renderDrumTriggers(const DrumMachinePreset& preset,
                                                          const std::vector<DrumPadEvent>& events);
[[nodiscard]] audio::RenderedAudio
renderDrumPadSample(const DrumPad& pad, const audio::RenderedAudio& source, std::uint8_t velocity);
[[nodiscard]] std::string serializeDrumMachinePreset(const DrumMachinePreset& preset);
[[nodiscard]] DrumMachinePreset parseDrumMachinePreset(std::string_view json);
[[nodiscard]] bool hasClearDrumPresetRedistributionRights(const DrumMachinePreset& preset);

} // namespace lamusica::session
