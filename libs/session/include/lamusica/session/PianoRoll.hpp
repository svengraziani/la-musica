#pragma once

#include "lamusica/session/Midi.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace lamusica::session {

struct PianoRollRange {
    std::int64_t startSample{0};
    std::int64_t endSample{48000 * 4};
    std::uint8_t lowPitch{0};
    std::uint8_t highPitch{127};
};

struct PianoRollSelection {
    std::vector<std::string> noteIds;
    std::optional<PianoRollRange> range;
};

enum class ControllerLaneType {
    Velocity,
    ControlChange,
    PitchBend,
    Aftertouch,
};

enum class ScaleKind {
    Major,
    NaturalMinor,
    Chromatic,
};

struct ControllerLane {
    ControllerLaneType type{ControllerLaneType::Velocity};
    std::uint8_t controller{0};
    bool visible{true};
};

struct ControllerLaneEvent {
    std::int64_t samplePosition{0};
    std::uint8_t controller{0};
    std::int16_t value{0};
    std::uint8_t channel{1};
    std::string sourceId;
};

struct ScaleHighlight {
    std::uint8_t pitch{0};
    bool inScale{false};
    bool root{false};
};

struct ChordLabel {
    std::int64_t samplePosition{0};
    std::string name;
    std::vector<std::uint8_t> pitches;
};

struct PianoRollEventListItem {
    std::string id;
    std::int64_t startSample{0};
    std::int64_t lengthSamples{0};
    std::uint8_t pitch{60};
    std::uint8_t velocity{100};
    std::uint8_t channel{1};
    bool muted{false};
};

struct PianoRollAuditionEvent {
    std::uint8_t pitch{60};
    std::uint8_t velocity{100};
    std::uint8_t channel{1};
    std::int64_t lengthSamples{2400};
};

struct PianoRollViewState {
    PianoRollRange visibleRange;
    PianoRollSelection selection;
    std::vector<ControllerLane> controllerLanes{{.type = ControllerLaneType::Velocity}};
    bool foldToUsedPitches{false};
    bool showGhostNotes{true};
};

[[nodiscard]] bool pianoRollSelectionReferencesExistingNotes(const PianoRollSelection& selection,
                                                             const MidiClipData& clip) noexcept;
void setPianoRollSelection(PianoRollViewState& viewState, PianoRollSelection selection,
                           const MidiClipData& clip);
[[nodiscard]] PianoRollSelection selectNotesInRange(const MidiClipData& clip, PianoRollRange range);
void setControllerLaneVisible(PianoRollViewState& viewState, ControllerLaneType type,
                              std::uint8_t controller, bool visible);
[[nodiscard]] std::vector<std::uint8_t> usedPitches(const MidiClipData& clip);
[[nodiscard]] PianoRollRange foldedPitchRange(const MidiClipData& clip,
                                              PianoRollRange fallbackRange);
[[nodiscard]] std::vector<MidiNote> notesInRange(const MidiClipData& clip, PianoRollRange range);
[[nodiscard]] std::vector<ControllerLaneEvent> controllerLaneEvents(const MidiClipData& clip,
                                                                    ControllerLane lane);
[[nodiscard]] std::vector<ScaleHighlight> scaleHighlights(std::uint8_t rootPitchClass,
                                                          ScaleKind scale, std::uint8_t lowPitch,
                                                          std::uint8_t highPitch);
[[nodiscard]] std::vector<ChordLabel> chordLabels(const MidiClipData& clip,
                                                  std::int64_t toleranceSamples = 0);
[[nodiscard]] std::vector<MidiNote> ghostNotesInRange(const std::vector<MidiClipData>& clips,
                                                      std::string_view activeClipId,
                                                      PianoRollRange range);
[[nodiscard]] std::vector<PianoRollEventListItem> eventListItems(const MidiClipData& clip);
[[nodiscard]] PianoRollAuditionEvent auditionForNote(const MidiNote& note,
                                                     std::int64_t lengthSamples = 2400);
[[nodiscard]] bool noteIntersectsRange(const MidiNote& note, PianoRollRange range) noexcept;
[[nodiscard]] std::string pitchName(std::uint8_t pitch);

} // namespace lamusica::session
