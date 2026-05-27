#include "lamusica/session/PianoRoll.hpp"

#include <algorithm>
#include <array>
#include <map>
#include <set>
#include <stdexcept>

namespace lamusica::session {
namespace {

std::string pitchClassName(std::uint8_t pitchClass) {
    static constexpr std::array names{"C",  "C#", "D",  "D#", "E",  "F",
                                      "F#", "G",  "G#", "A",  "A#", "B"};
    return names[pitchClass % 12U];
}

} // namespace

bool pianoRollSelectionReferencesExistingNotes(const PianoRollSelection& selection,
                                               const MidiClipData& clip) noexcept {
    for (const auto& noteId : selection.noteIds) {
        if (std::ranges::none_of(clip.notes,
                                 [&noteId](const MidiNote& note) { return note.id == noteId; })) {
            return false;
        }
    }
    if (selection.range.has_value()) {
        const auto& range = *selection.range;
        if (range.startSample < 0 || range.endSample < range.startSample ||
            range.lowPitch > range.highPitch) {
            return false;
        }
    }
    return true;
}

void setPianoRollSelection(PianoRollViewState& viewState, PianoRollSelection selection,
                           const MidiClipData& clip) {
    if (!pianoRollSelectionReferencesExistingNotes(selection, clip)) {
        throw std::runtime_error("Piano roll selection references missing notes or invalid range");
    }
    viewState.selection = std::move(selection);
}

PianoRollSelection selectNotesInRange(const MidiClipData& clip, PianoRollRange range) {
    PianoRollSelection selection{.range = range};
    for (const auto& note : notesInRange(clip, range)) {
        selection.noteIds.push_back(note.id);
    }
    return selection;
}

void setControllerLaneVisible(PianoRollViewState& viewState, ControllerLaneType type,
                              std::uint8_t controller, bool visible) {
    const auto found = std::ranges::find_if(
        viewState.controllerLanes, [type, controller](const ControllerLane& lane) {
            return lane.type == type && lane.controller == controller;
        });
    if (found == viewState.controllerLanes.end()) {
        viewState.controllerLanes.push_back(
            {.type = type, .controller = controller, .visible = visible});
        return;
    }
    found->visible = visible;
}

std::vector<std::uint8_t> usedPitches(const MidiClipData& clip) {
    std::vector<std::uint8_t> pitches;
    pitches.reserve(clip.notes.size());

    for (const auto& note : clip.notes) {
        if (!note.muted && !std::ranges::contains(pitches, note.pitch)) {
            pitches.push_back(note.pitch);
        }
    }

    std::ranges::sort(pitches);
    return pitches;
}

PianoRollRange foldedPitchRange(const MidiClipData& clip, PianoRollRange fallbackRange) {
    const auto pitches = usedPitches(clip);
    if (pitches.empty()) {
        return fallbackRange;
    }
    fallbackRange.lowPitch = pitches.front();
    fallbackRange.highPitch = pitches.back();
    return fallbackRange;
}

bool noteIntersectsRange(const MidiNote& note, PianoRollRange range) noexcept {
    const auto noteEnd = note.startSample + note.lengthSamples;
    const auto timeIntersects = note.startSample < range.endSample && noteEnd > range.startSample;
    const auto pitchIntersects = note.pitch >= range.lowPitch && note.pitch <= range.highPitch;
    return timeIntersects && pitchIntersects;
}

std::vector<MidiNote> notesInRange(const MidiClipData& clip, PianoRollRange range) {
    std::vector<MidiNote> notes;
    std::ranges::copy_if(clip.notes, std::back_inserter(notes), [range](const MidiNote& note) {
        return noteIntersectsRange(note, range);
    });
    std::ranges::sort(notes, {}, &MidiNote::startSample);
    return notes;
}

std::vector<ControllerLaneEvent> controllerLaneEvents(const MidiClipData& clip,
                                                      ControllerLane lane) {
    std::vector<ControllerLaneEvent> events;
    if (!lane.visible) {
        return events;
    }

    switch (lane.type) {
    case ControllerLaneType::Velocity:
        events.reserve(clip.notes.size());
        for (const auto& note : clip.notes) {
            events.push_back({.samplePosition = note.startSample,
                              .value = note.velocity,
                              .channel = note.channel,
                              .sourceId = note.id});
        }
        break;
    case ControllerLaneType::ControlChange:
        for (const auto& change : clip.controlChanges) {
            if (change.controller == lane.controller) {
                events.push_back({.samplePosition = change.samplePosition,
                                  .controller = change.controller,
                                  .value = change.value,
                                  .channel = change.channel});
            }
        }
        break;
    case ControllerLaneType::PitchBend:
        events.reserve(clip.pitchBends.size());
        for (const auto& bend : clip.pitchBends) {
            events.push_back({.samplePosition = bend.samplePosition,
                              .value = bend.value,
                              .channel = bend.channel});
        }
        break;
    case ControllerLaneType::Aftertouch:
        events.reserve(clip.aftertouch.size());
        for (const auto& pressure : clip.aftertouch) {
            events.push_back({.samplePosition = pressure.samplePosition,
                              .controller = pressure.pitch,
                              .value = pressure.pressure,
                              .channel = pressure.channel});
        }
        break;
    }

    std::ranges::sort(events, {}, &ControllerLaneEvent::samplePosition);
    return events;
}

std::vector<ScaleHighlight> scaleHighlights(std::uint8_t rootPitchClass, ScaleKind scale,
                                            std::uint8_t lowPitch, std::uint8_t highPitch) {
    static constexpr std::array major{0, 2, 4, 5, 7, 9, 11};
    static constexpr std::array naturalMinor{0, 2, 3, 5, 7, 8, 10};
    const auto normalizedRoot = static_cast<int>(rootPitchClass % 12U);

    std::vector<ScaleHighlight> highlights;
    for (auto pitch = lowPitch; pitch <= highPitch; ++pitch) {
        const auto pitchClass = static_cast<int>(pitch % 12U);
        const auto interval = (pitchClass - normalizedRoot + 12) % 12;
        bool inScale = true;
        if (scale == ScaleKind::Major) {
            inScale = std::ranges::contains(major, interval);
        } else if (scale == ScaleKind::NaturalMinor) {
            inScale = std::ranges::contains(naturalMinor, interval);
        }
        highlights.push_back(
            {.pitch = pitch, .inScale = inScale, .root = pitchClass == normalizedRoot});
        if (pitch == 127U) {
            break;
        }
    }
    return highlights;
}

std::vector<ChordLabel> chordLabels(const MidiClipData& clip, std::int64_t toleranceSamples) {
    std::map<std::int64_t, std::set<std::uint8_t>> grouped;
    for (const auto& note : clip.notes) {
        if (note.muted) {
            continue;
        }
        const auto groupSample = toleranceSamples <= 0
                                     ? note.startSample
                                     : (note.startSample / toleranceSamples) * toleranceSamples;
        grouped[groupSample].insert(static_cast<std::uint8_t>(note.pitch % 12U));
    }

    std::vector<ChordLabel> labels;
    for (const auto& [sample, pitchClasses] : grouped) {
        if (pitchClasses.size() < 3) {
            continue;
        }
        for (const auto root : pitchClasses) {
            const auto majorThird = static_cast<std::uint8_t>((root + 4U) % 12U);
            const auto minorThird = static_cast<std::uint8_t>((root + 3U) % 12U);
            const auto fifth = static_cast<std::uint8_t>((root + 7U) % 12U);
            if (pitchClasses.contains(majorThird) && pitchClasses.contains(fifth)) {
                labels.push_back(
                    {.samplePosition = sample, .name = pitchClassName(root) + " major"});
                break;
            }
            if (pitchClasses.contains(minorThird) && pitchClasses.contains(fifth)) {
                labels.push_back(
                    {.samplePosition = sample, .name = pitchClassName(root) + " minor"});
                break;
            }
        }
        if (!labels.empty() && labels.back().samplePosition == sample) {
            labels.back().pitches.assign(pitchClasses.begin(), pitchClasses.end());
        }
    }
    return labels;
}

std::vector<MidiNote> ghostNotesInRange(const std::vector<MidiClipData>& clips,
                                        std::string_view activeClipId, PianoRollRange range) {
    std::vector<MidiNote> notes;
    for (const auto& clip : clips) {
        if (clip.clipId == activeClipId) {
            continue;
        }
        auto clipNotes = notesInRange(clip, range);
        notes.insert(notes.end(), clipNotes.begin(), clipNotes.end());
    }
    std::ranges::sort(notes, {}, &MidiNote::startSample);
    return notes;
}

std::vector<PianoRollEventListItem> eventListItems(const MidiClipData& clip) {
    std::vector<PianoRollEventListItem> items;
    items.reserve(clip.notes.size());
    for (const auto& note : clip.notes) {
        items.push_back({.id = note.id,
                         .startSample = note.startSample,
                         .lengthSamples = note.lengthSamples,
                         .pitch = note.pitch,
                         .velocity = note.velocity,
                         .channel = note.channel,
                         .muted = note.muted});
    }
    std::ranges::sort(items, {}, &PianoRollEventListItem::startSample);
    return items;
}

PianoRollAuditionEvent auditionForNote(const MidiNote& note, std::int64_t lengthSamples) {
    return {.pitch = note.pitch,
            .velocity = note.velocity,
            .channel = note.channel,
            .lengthSamples = std::max<std::int64_t>(0, lengthSamples)};
}

std::string pitchName(std::uint8_t pitch) {
    const auto octave = static_cast<int>(pitch / 12U) - 1;
    return pitchClassName(pitch) + std::to_string(octave);
}

} // namespace lamusica::session
