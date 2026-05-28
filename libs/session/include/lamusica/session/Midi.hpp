#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace lamusica::session {

struct MidiNote {
    std::string id;
    std::int64_t startSample{0};
    std::int64_t lengthSamples{0};
    std::uint8_t pitch{60};
    std::uint8_t velocity{100};
    std::uint8_t channel{1};
    bool muted{false};
};

struct MidiControlChange {
    std::int64_t samplePosition{0};
    std::uint8_t controller{0};
    std::uint8_t value{0};
    std::uint8_t channel{1};
};

struct MidiPitchBend {
    std::int64_t samplePosition{0};
    std::int16_t value{0};
    std::uint8_t channel{1};
};

struct MidiAftertouch {
    std::int64_t samplePosition{0};
    std::uint8_t pressure{0};
    std::uint8_t channel{1};
    std::uint8_t pitch{0};
    bool polyphonic{false};
};

struct MidiProgramChange {
    std::int64_t samplePosition{0};
    std::uint8_t program{0};
    std::uint8_t channel{1};
};

struct MidiMetadata {
    std::string key;
    std::string value;
};

struct MidiClipData {
    std::string clipId;
    std::vector<MidiNote> notes;
    std::vector<MidiControlChange> controlChanges;
    std::vector<MidiPitchBend> pitchBends;
    std::vector<MidiAftertouch> aftertouch;
    std::vector<MidiProgramChange> programChanges;
    std::vector<MidiMetadata> metadata;
};

struct QuantizeSettings {
    std::int64_t gridSamples{24000};
    float strength{1.0F};
};

struct VelocityTransform {
    int add{0};
    float scale{1.0F};
};

struct HumanizeSettings {
    std::int64_t maxTimingOffsetSamples{0};
    int maxVelocityOffset{0};
    std::uint32_t seed{0};
};

struct MidiTimingContext {
    double sampleRate{48000.0};
    double tempoBpm{120.0};
};

struct MidiMusicalTiming {
    double startPpq{0.0};
    double lengthPpq{0.0};
};

enum class MidiEventType {
    NoteOn,
    NoteOff,
    ControlChange,
    PitchBend,
    Aftertouch,
    ProgramChange,
};

struct MidiPlaybackEvent {
    MidiEventType type{MidiEventType::NoteOn};
    std::int64_t samplePosition{0};
    std::uint8_t channel{1};
    std::uint8_t data1{0};
    std::int16_t data2{0};
    std::string sourceId;
};

enum class MidiRecordingMode {
    Overdub,
    Replace,
};

struct RecordedMidiNote {
    std::string id;
    std::int64_t noteOnSample{0};
    std::int64_t noteOffSample{0};
    std::uint8_t pitch{60};
    std::uint8_t velocity{100};
    std::uint8_t channel{1};
};

struct MidiRecordingCommit {
    MidiRecordingMode mode{MidiRecordingMode::Overdub};
    std::int64_t replaceStartSample{0};
    std::int64_t replaceEndSample{0};
    bool quantizeOnCommit{false};
    QuantizeSettings quantize;
    std::vector<RecordedMidiNote> notes;
};

enum class MidiDeviceDirection {
    Input,
    Output,
    InputOutput,
};

struct MidiDeviceInfo {
    std::string id;
    std::string name;
    MidiDeviceDirection direction{MidiDeviceDirection::Input};
    bool online{true};
};

struct MidiInputEnablement {
    std::string deviceId;
    bool enabled{true};
};

struct MidiOutputRoute {
    std::string trackId;
    std::string deviceId;
    std::uint8_t channel{1};
};

enum class MidiClockMode {
    Internal,
    Send,
    Receive,
};

struct MidiClockOptions {
    MidiClockMode mode{MidiClockMode::Internal};
    std::string deviceId;
    bool sendStartStop{true};
};

struct MidiDeviceConfiguration {
    std::vector<MidiDeviceInfo> devices;
    std::vector<MidiInputEnablement> inputs;
    std::vector<MidiOutputRoute> outputRoutes;
    MidiClockOptions clock;
};

void mergeMidiDevice(MidiDeviceConfiguration& configuration, MidiDeviceInfo device);
void setMidiInputEnabled(MidiDeviceConfiguration& configuration, std::string deviceId,
                         bool enabled);
void setMidiOutputRoute(MidiDeviceConfiguration& configuration, MidiOutputRoute route);
void setMidiClockOptions(MidiDeviceConfiguration& configuration, MidiClockOptions options);
[[nodiscard]] std::vector<MidiDeviceInfo>
enabledMidiInputs(const MidiDeviceConfiguration& configuration);
[[nodiscard]] const MidiOutputRoute*
findMidiOutputRoute(const MidiDeviceConfiguration& configuration, std::string_view trackId);
[[nodiscard]] std::int64_t quantizeSample(std::int64_t sample, QuantizeSettings settings);
[[nodiscard]] double midiSamplesToPpq(std::int64_t samples, MidiTimingContext context);
[[nodiscard]] std::int64_t midiPpqToSamples(double ppq, MidiTimingContext context);
[[nodiscard]] MidiMusicalTiming midiNoteToMusicalTiming(const MidiNote& note,
                                                        MidiTimingContext context);
[[nodiscard]] MidiNote midiNoteFromMusicalTiming(MidiNote note, MidiMusicalTiming timing,
                                                 MidiTimingContext context);
void quantizeNotes(MidiClipData& clip, QuantizeSettings settings);
void transposeNotes(MidiClipData& clip, int semitones);
void transformVelocity(MidiClipData& clip, VelocityTransform transform);
void humanizeNotes(MidiClipData& clip, HumanizeSettings settings);
void setNoteLengths(MidiClipData& clip, std::int64_t lengthSamples);
void legato(MidiClipData& clip);
void commitMidiRecording(MidiClipData& clip, const MidiRecordingCommit& recording);
[[nodiscard]] std::vector<MidiNote> notesInPlaybackOrder(const MidiClipData& clip);
[[nodiscard]] std::vector<MidiPlaybackEvent> midiEventsInPlaybackOrder(const MidiClipData& clip);
[[nodiscard]] std::vector<MidiPlaybackEvent>
midiEventsInSampleRange(const MidiClipData& clip, std::int64_t startSample, std::int64_t endSample);

} // namespace lamusica::session
