#include "lamusica/session/Midi.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace lamusica::session {
namespace {

std::uint8_t clampMidi7Bit(int value) {
    return static_cast<std::uint8_t>(std::clamp(value, 0, 127));
}

bool canReceiveMidi(MidiDeviceDirection direction) noexcept {
    return direction == MidiDeviceDirection::Input || direction == MidiDeviceDirection::InputOutput;
}

bool canSendMidi(MidiDeviceDirection direction) noexcept {
    return direction == MidiDeviceDirection::Output ||
           direction == MidiDeviceDirection::InputOutput;
}

const MidiDeviceInfo* findMidiDevice(const MidiDeviceConfiguration& configuration,
                                     std::string_view deviceId) noexcept {
    const auto found =
        std::ranges::find_if(configuration.devices, [deviceId](const MidiDeviceInfo& device) {
            return device.id == deviceId;
        });
    return found == configuration.devices.end() ? nullptr : &*found;
}

} // namespace

void mergeMidiDevice(MidiDeviceConfiguration& configuration, MidiDeviceInfo device) {
    if (device.id.empty() || device.name.empty()) {
        throw std::runtime_error("MIDI device id and name are required");
    }
    const auto found =
        std::ranges::find_if(configuration.devices, [&device](const MidiDeviceInfo& existing) {
            return existing.id == device.id;
        });
    if (found == configuration.devices.end()) {
        configuration.devices.push_back(std::move(device));
    } else {
        *found = std::move(device);
    }
}

void setMidiInputEnabled(MidiDeviceConfiguration& configuration, std::string deviceId,
                         bool enabled) {
    const auto* device = findMidiDevice(configuration, deviceId);
    if (device == nullptr || !device->online || !canReceiveMidi(device->direction)) {
        throw std::runtime_error("MIDI input device is unavailable");
    }
    const auto found =
        std::ranges::find_if(configuration.inputs, [&deviceId](const MidiInputEnablement& input) {
            return input.deviceId == deviceId;
        });
    if (found == configuration.inputs.end()) {
        configuration.inputs.push_back({.deviceId = std::move(deviceId), .enabled = enabled});
    } else {
        found->enabled = enabled;
    }
}

void setMidiOutputRoute(MidiDeviceConfiguration& configuration, MidiOutputRoute route) {
    if (route.trackId.empty()) {
        throw std::runtime_error("MIDI output route track id is required");
    }
    const auto* device = findMidiDevice(configuration, route.deviceId);
    if (device == nullptr || !device->online || !canSendMidi(device->direction)) {
        throw std::runtime_error("MIDI output device is unavailable");
    }
    if (route.channel == 0 || route.channel > 16) {
        throw std::runtime_error("MIDI output route channel must be in the range 1-16");
    }
    const auto found =
        std::ranges::find_if(configuration.outputRoutes, [&route](const MidiOutputRoute& existing) {
            return existing.trackId == route.trackId;
        });
    if (found == configuration.outputRoutes.end()) {
        configuration.outputRoutes.push_back(std::move(route));
    } else {
        *found = std::move(route);
    }
}

void setMidiClockOptions(MidiDeviceConfiguration& configuration, MidiClockOptions options) {
    if (options.mode != MidiClockMode::Internal) {
        const auto* device = findMidiDevice(configuration, options.deviceId);
        if (device == nullptr || !device->online) {
            throw std::runtime_error("MIDI clock device is unavailable");
        }
        if (options.mode == MidiClockMode::Send && !canSendMidi(device->direction)) {
            throw std::runtime_error("MIDI clock send requires an output-capable device");
        }
        if (options.mode == MidiClockMode::Receive && !canReceiveMidi(device->direction)) {
            throw std::runtime_error("MIDI clock receive requires an input-capable device");
        }
    }
    configuration.clock = std::move(options);
}

std::vector<MidiDeviceInfo> enabledMidiInputs(const MidiDeviceConfiguration& configuration) {
    std::vector<MidiDeviceInfo> inputs;
    for (const auto& input : configuration.inputs) {
        if (!input.enabled) {
            continue;
        }
        const auto* device = findMidiDevice(configuration, input.deviceId);
        if (device != nullptr && device->online && canReceiveMidi(device->direction)) {
            inputs.push_back(*device);
        }
    }
    return inputs;
}

const MidiOutputRoute* findMidiOutputRoute(const MidiDeviceConfiguration& configuration,
                                           std::string_view trackId) {
    const auto found =
        std::ranges::find_if(configuration.outputRoutes, [trackId](const MidiOutputRoute& route) {
            return route.trackId == trackId;
        });
    return found == configuration.outputRoutes.end() ? nullptr : &*found;
}

std::int64_t quantizeSample(std::int64_t sample, QuantizeSettings settings) {
    if (settings.gridSamples <= 0) {
        return sample;
    }

    const auto lower = (sample / settings.gridSamples) * settings.gridSamples;
    const auto upper = lower + settings.gridSamples;
    const auto target = (sample - lower) <= (upper - sample) ? lower : upper;
    const auto delta = target - sample;
    return sample +
           static_cast<std::int64_t>(std::llround(static_cast<float>(delta) * settings.strength));
}

void quantizeNotes(MidiClipData& clip, QuantizeSettings settings) {
    for (auto& note : clip.notes) {
        note.startSample = quantizeSample(note.startSample, settings);
    }
}

void transposeNotes(MidiClipData& clip, int semitones) {
    for (auto& note : clip.notes) {
        note.pitch = clampMidi7Bit(static_cast<int>(note.pitch) + semitones);
    }
}

void transformVelocity(MidiClipData& clip, VelocityTransform transform) {
    for (auto& note : clip.notes) {
        const auto scaled =
            static_cast<int>(std::llround(static_cast<float>(note.velocity) * transform.scale));
        note.velocity = clampMidi7Bit(scaled + transform.add);
    }
}

void humanizeNotes(MidiClipData& clip, HumanizeSettings settings) {
    const auto timingRange = std::max<std::int64_t>(0, settings.maxTimingOffsetSamples);
    const auto velocityRange = std::max(0, settings.maxVelocityOffset);
    if (timingRange == 0 && velocityRange == 0) {
        return;
    }

    auto state = settings.seed == 0U ? 0xA5A5A5A5U : settings.seed;
    const auto nextRandom = [&state]() {
        state = (state * 1664525U) + 1013904223U;
        return state;
    };

    for (auto& note : clip.notes) {
        if (timingRange > 0) {
            const auto span = static_cast<std::uint64_t>((timingRange * 2) + 1);
            const auto offset = static_cast<std::int64_t>(nextRandom() % span) - timingRange;
            note.startSample = std::max<std::int64_t>(0, note.startSample + offset);
        }
        if (velocityRange > 0) {
            const auto span = static_cast<std::uint32_t>((velocityRange * 2) + 1);
            const auto offset = static_cast<int>(nextRandom() % span) - velocityRange;
            note.velocity = clampMidi7Bit(static_cast<int>(note.velocity) + offset);
        }
    }
}

void setNoteLengths(MidiClipData& clip, std::int64_t lengthSamples) {
    const auto safeLength = std::max<std::int64_t>(0, lengthSamples);
    for (auto& note : clip.notes) {
        note.lengthSamples = safeLength;
    }
}

void legato(MidiClipData& clip) {
    std::ranges::sort(clip.notes, {}, &MidiNote::startSample);
    for (std::size_t index = 0; index + 1 < clip.notes.size(); ++index) {
        clip.notes[index].lengthSamples = std::max<std::int64_t>(
            0, clip.notes[index + 1].startSample - clip.notes[index].startSample);
    }
}

void commitMidiRecording(MidiClipData& clip, const MidiRecordingCommit& recording) {
    if (recording.mode == MidiRecordingMode::Replace) {
        if (recording.replaceStartSample < 0 ||
            recording.replaceEndSample <= recording.replaceStartSample) {
            throw std::runtime_error(
                "MIDI replace recording range must be non-empty and non-negative");
        }
        std::erase_if(clip.notes, [&recording](const MidiNote& note) {
            const auto noteEnd = note.startSample + note.lengthSamples;
            return note.startSample < recording.replaceEndSample &&
                   noteEnd > recording.replaceStartSample;
        });
    }

    for (const auto& recorded : recording.notes) {
        if (recorded.id.empty()) {
            throw std::runtime_error("Recorded MIDI note id must not be empty");
        }
        if (recorded.noteOnSample < 0 || recorded.noteOffSample <= recorded.noteOnSample) {
            throw std::runtime_error("Recorded MIDI note range must be non-empty and non-negative");
        }
        if (recorded.channel == 0 || recorded.channel > 16) {
            throw std::runtime_error("Recorded MIDI channel must be in the range 1-16");
        }
        if (std::ranges::any_of(
                clip.notes, [&recorded](const MidiNote& note) { return note.id == recorded.id; })) {
            throw std::runtime_error("Recorded MIDI note id already exists");
        }

        const auto startSample = recording.quantizeOnCommit
                                     ? quantizeSample(recorded.noteOnSample, recording.quantize)
                                     : recorded.noteOnSample;
        clip.notes.push_back({.id = recorded.id,
                              .startSample = startSample,
                              .lengthSamples = recorded.noteOffSample - recorded.noteOnSample,
                              .pitch = recorded.pitch,
                              .velocity = recorded.velocity,
                              .channel = recorded.channel});
    }
    std::ranges::sort(clip.notes, {}, &MidiNote::startSample);
}

std::vector<MidiNote> notesInPlaybackOrder(const MidiClipData& clip) {
    auto notes = clip.notes;
    std::ranges::sort(notes, [](const MidiNote& left, const MidiNote& right) {
        if (left.startSample != right.startSample) {
            return left.startSample < right.startSample;
        }
        return left.pitch < right.pitch;
    });
    return notes;
}

std::vector<MidiPlaybackEvent> midiEventsInPlaybackOrder(const MidiClipData& clip) {
    std::vector<MidiPlaybackEvent> events;
    events.reserve((clip.notes.size() * 2U) + clip.controlChanges.size() + clip.pitchBends.size() +
                   clip.aftertouch.size() + clip.programChanges.size());

    for (const auto& note : clip.notes) {
        if (note.muted) {
            continue;
        }
        events.push_back({.type = MidiEventType::NoteOn,
                          .samplePosition = note.startSample,
                          .channel = note.channel,
                          .data1 = note.pitch,
                          .data2 = note.velocity,
                          .sourceId = note.id});
        events.push_back({.type = MidiEventType::NoteOff,
                          .samplePosition = note.startSample + note.lengthSamples,
                          .channel = note.channel,
                          .data1 = note.pitch,
                          .data2 = 0,
                          .sourceId = note.id});
    }

    for (const auto& change : clip.controlChanges) {
        events.push_back({.type = MidiEventType::ControlChange,
                          .samplePosition = change.samplePosition,
                          .channel = change.channel,
                          .data1 = change.controller,
                          .data2 = change.value});
    }
    for (const auto& bend : clip.pitchBends) {
        events.push_back({.type = MidiEventType::PitchBend,
                          .samplePosition = bend.samplePosition,
                          .channel = bend.channel,
                          .data2 = bend.value});
    }
    for (const auto& pressure : clip.aftertouch) {
        events.push_back({.type = MidiEventType::Aftertouch,
                          .samplePosition = pressure.samplePosition,
                          .channel = pressure.channel,
                          .data1 = pressure.polyphonic ? pressure.pitch : std::uint8_t{0},
                          .data2 = pressure.pressure});
    }
    for (const auto& program : clip.programChanges) {
        events.push_back({.type = MidiEventType::ProgramChange,
                          .samplePosition = program.samplePosition,
                          .channel = program.channel,
                          .data1 = program.program});
    }

    std::ranges::sort(events, [](const MidiPlaybackEvent& left, const MidiPlaybackEvent& right) {
        if (left.samplePosition != right.samplePosition) {
            return left.samplePosition < right.samplePosition;
        }
        return static_cast<int>(left.type) < static_cast<int>(right.type);
    });
    return events;
}

std::vector<MidiPlaybackEvent> midiEventsInSampleRange(const MidiClipData& clip,
                                                       std::int64_t startSample,
                                                       std::int64_t endSample) {
    if (startSample < 0 || endSample < startSample) {
        throw std::runtime_error("MIDI playback range must be non-negative and ordered");
    }
    auto events = midiEventsInPlaybackOrder(clip);
    std::erase_if(events, [startSample, endSample](const MidiPlaybackEvent& event) {
        return event.samplePosition < startSample || event.samplePosition >= endSample;
    });
    return events;
}

} // namespace lamusica::session
