#pragma once

#include "lamusica/session/Automation.hpp"
#include "lamusica/session/Midi.hpp"

#include <cstdint>
#include <optional>
#include <span>
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

struct PianoRollRect {
    float x{0.0F};
    float y{0.0F};
    float width{0.0F};
    float height{0.0F};
};

struct PianoRollLayoutOptions {
    PianoRollRange visibleRange;
    float contentWidth{960.0F};
    float keyboardWidth{72.0F};
    float noteRowHeight{12.0F};
    float controllerLaneHeight{72.0F};
    std::int64_t majorGridSamples{48000};
    std::int64_t minorGridSamples{12000};
    bool foldToUsedPitches{false};
    bool showMutedNotes{true};
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
    Automation,
};

enum class ScaleKind {
    Major,
    NaturalMinor,
    Chromatic,
};

struct ControllerLane {
    ControllerLaneType type{ControllerLaneType::Velocity};
    std::uint8_t controller{0};
    AutomationTargetKind automationTargetKind{AutomationTargetKind::Mixer};
    std::string automationTargetId;
    std::string automationParameterId;
    bool visible{true};
};

struct ControllerLaneEvent {
    std::int64_t samplePosition{0};
    std::uint8_t controller{0};
    std::int16_t value{0};
    float automationValue{0.0F};
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

struct DrumNoteName {
    std::uint8_t pitch{60};
    std::string name;
};

struct PianoRollGridLine {
    std::int64_t samplePosition{0};
    float x{0.0F};
    bool major{false};
};

struct PianoRollKeyboardKey {
    std::uint8_t pitch{60};
    std::string label;
    PianoRollRect bounds;
    bool blackKey{false};
    bool foldedOut{false};
};

struct PianoRollNoteRect {
    std::string noteId;
    PianoRollRect bounds;
    std::uint8_t pitch{60};
    std::uint8_t velocity{100};
    bool muted{false};
    bool selected{false};
};

struct PianoRollControllerLaneLayout {
    ControllerLane lane;
    PianoRollRect bounds;
};

struct PianoRollLayout {
    PianoRollRect noteArea;
    PianoRollRect keyboardArea;
    std::vector<PianoRollGridLine> gridLines;
    std::vector<PianoRollKeyboardKey> keys;
    std::vector<PianoRollNoteRect> notes;
    std::vector<PianoRollControllerLaneLayout> controllerLanes;
};

struct PianoRollNoteDraft {
    std::int64_t startSample{0};
    std::int64_t lengthSamples{0};
    std::uint8_t pitch{60};
    std::uint8_t velocity{100};
    std::uint8_t channel{1};
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
[[nodiscard]] std::vector<ControllerLaneEvent>
automationLinkedControllerLaneEvents(std::span<const AutomationLaneData> automationLanes,
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
[[nodiscard]] PianoRollLayout buildPianoRollLayout(const MidiClipData& clip,
                                                   const PianoRollViewState& viewState,
                                                   PianoRollLayoutOptions options);
[[nodiscard]] PianoRollNoteDraft noteDraftFromGridDrag(PianoRollLayoutOptions options, float startX,
                                                       float endX, float y,
                                                       std::uint8_t velocity = 100,
                                                       std::uint8_t channel = 1);
[[nodiscard]] bool noteIntersectsRange(const MidiNote& note, PianoRollRange range) noexcept;
[[nodiscard]] std::string pitchName(std::uint8_t pitch);
[[nodiscard]] std::string drumNoteName(std::uint8_t pitch, const std::vector<DrumNoteName>& names);

} // namespace lamusica::session
