#pragma once

#include "lamusica/session/Midi.hpp"
#include "lamusica/session/ProjectManifest.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace lamusica::session {

struct FirstTrackReadiness {
    bool starterStructureReady{false};
    bool firstTrackEditable{false};
    bool renderable{false};
    std::size_t trackCount{0};
    std::size_t clipCount{0};
    std::size_t markerCount{0};
    std::size_t routingCount{0};
    std::size_t midiClipReferenceCount{0};
    std::size_t starterMidiNoteCount{0};
    int starterBassTransposeSemitones{0};
    std::size_t pluginCount{0};
    std::size_t automationLaneCount{0};
    bool loopReady{false};
    std::int64_t loopStartSample{0};
    std::int64_t loopEndSample{0};
    std::int64_t arrangementEndSample{0};
    std::uint32_t renderFrames{0};
    std::vector<std::string> missingRequirements;
};

struct FirstTrackArrangementSummary {
    double tempoBpm{120.0};
    std::uint32_t timeSignatureNumerator{4};
    std::uint32_t timeSignatureDenominator{4};
    std::size_t sectionCount{0};
    std::size_t audioTrackCount{0};
    std::size_t midiTrackCount{0};
    std::size_t masterTrackCount{0};
    std::string firstSectionName;
    std::string finalSectionName;
    std::int64_t firstSectionSample{0};
    std::int64_t finalSectionSample{0};
};

[[nodiscard]] ProjectManifest makeFirstTrackStarterManifest(std::string name);
[[nodiscard]] MidiClipData makeFirstTrackStarterBassMidi();
[[nodiscard]] std::vector<MidiClipData> makeFirstTrackStarterMidiClips();
[[nodiscard]] bool isFirstTrackStarterManifest(const ProjectManifest& manifest) noexcept;
[[nodiscard]] std::int64_t arrangementEndSample(const ProjectManifest& manifest) noexcept;
[[nodiscard]] std::uint32_t renderableArrangementFrames(const ProjectManifest& manifest);
[[nodiscard]] FirstTrackReadiness inspectFirstTrackReadiness(const ProjectManifest& manifest);
[[nodiscard]] std::vector<std::string_view> firstTrackStarterRequirementIds() noexcept;
[[nodiscard]] FirstTrackArrangementSummary
summarizeFirstTrackArrangement(const ProjectManifest& manifest);

} // namespace lamusica::session
