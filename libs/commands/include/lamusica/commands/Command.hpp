#pragma once

#include "lamusica/session/Automation.hpp"
#include "lamusica/session/Midi.hpp"
#include "lamusica/session/Mixer.hpp"
#include "lamusica/session/Pattern.hpp"
#include "lamusica/session/Plugin.hpp"
#include "lamusica/session/ProjectManifest.hpp"
#include "lamusica/session/Warp.hpp"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace lamusica::commands {

struct CommandResult {
    bool ok{false};
    std::string message;
};

struct CommandMetadata {
    std::string commandId;
    std::string auditId;
    std::string name;
};

class ICommand {
  public:
    virtual ~ICommand() = default;

    [[nodiscard]] virtual const CommandMetadata& metadata() const noexcept = 0;
    [[nodiscard]] virtual CommandResult
    validate(const session::ProjectManifest& manifest) const = 0;
    [[nodiscard]] virtual std::string preview(const session::ProjectManifest& manifest) const = 0;
    virtual CommandResult apply(session::ProjectManifest& manifest) = 0;
    virtual CommandResult undo(session::ProjectManifest& manifest) = 0;
    virtual CommandResult redo(session::ProjectManifest& manifest) {
        return apply(manifest);
    }
    [[nodiscard]] virtual std::string serialize() const = 0;
};

using CommandPtr = std::unique_ptr<ICommand>;

class AddTrackCommand final : public ICommand {
  public:
    AddTrackCommand(std::string commandId, std::string auditId, session::Track track);

    [[nodiscard]] const CommandMetadata& metadata() const noexcept override;
    [[nodiscard]] CommandResult validate(const session::ProjectManifest& manifest) const override;
    [[nodiscard]] std::string preview(const session::ProjectManifest& manifest) const override;
    CommandResult apply(session::ProjectManifest& manifest) override;
    CommandResult undo(session::ProjectManifest& manifest) override;
    [[nodiscard]] std::string serialize() const override;

  private:
    CommandMetadata metadata_;
    session::Track track_;
    bool applied_{false};
};

class AddClipCommand final : public ICommand {
  public:
    AddClipCommand(std::string commandId, std::string auditId, session::Clip clip);

    [[nodiscard]] const CommandMetadata& metadata() const noexcept override;
    [[nodiscard]] CommandResult validate(const session::ProjectManifest& manifest) const override;
    [[nodiscard]] std::string preview(const session::ProjectManifest& manifest) const override;
    CommandResult apply(session::ProjectManifest& manifest) override;
    CommandResult undo(session::ProjectManifest& manifest) override;
    [[nodiscard]] std::string serialize() const override;

  private:
    CommandMetadata metadata_;
    session::Clip clip_;
    bool applied_{false};
};

class RemoveClipCommand final : public ICommand {
  public:
    RemoveClipCommand(std::string commandId, std::string auditId, std::string clipId);

    [[nodiscard]] const CommandMetadata& metadata() const noexcept override;
    [[nodiscard]] CommandResult validate(const session::ProjectManifest& manifest) const override;
    [[nodiscard]] std::string preview(const session::ProjectManifest& manifest) const override;
    CommandResult apply(session::ProjectManifest& manifest) override;
    CommandResult undo(session::ProjectManifest& manifest) override;
    [[nodiscard]] std::string serialize() const override;

  private:
    CommandMetadata metadata_;
    std::string clipId_;
    session::Clip removedClip_;
    std::size_t removedIndex_{0};
    bool applied_{false};
};

class DuplicateClipCommand final : public ICommand {
  public:
    DuplicateClipCommand(std::string commandId, std::string auditId, std::string sourceClipId,
                         std::string duplicateClipId, std::int64_t duplicateStartSample);

    [[nodiscard]] const CommandMetadata& metadata() const noexcept override;
    [[nodiscard]] CommandResult validate(const session::ProjectManifest& manifest) const override;
    [[nodiscard]] std::string preview(const session::ProjectManifest& manifest) const override;
    CommandResult apply(session::ProjectManifest& manifest) override;
    CommandResult undo(session::ProjectManifest& manifest) override;
    [[nodiscard]] std::string serialize() const override;

  private:
    CommandMetadata metadata_;
    std::string sourceClipId_;
    std::string duplicateClipId_;
    std::int64_t duplicateStartSample_{0};
    bool applied_{false};
};

class MoveClipCommand final : public ICommand {
  public:
    MoveClipCommand(std::string commandId, std::string auditId, std::string clipId,
                    std::int64_t newStartSample);

    [[nodiscard]] const CommandMetadata& metadata() const noexcept override;
    [[nodiscard]] CommandResult validate(const session::ProjectManifest& manifest) const override;
    [[nodiscard]] std::string preview(const session::ProjectManifest& manifest) const override;
    CommandResult apply(session::ProjectManifest& manifest) override;
    CommandResult undo(session::ProjectManifest& manifest) override;
    [[nodiscard]] std::string serialize() const override;

  private:
    CommandMetadata metadata_;
    std::string clipId_;
    std::int64_t newStartSample_{0};
    std::int64_t oldStartSample_{0};
    bool applied_{false};
};

class TrimClipCommand final : public ICommand {
  public:
    TrimClipCommand(std::string commandId, std::string auditId, std::string clipId,
                    std::int64_t newStartSample, std::int64_t newLengthSamples,
                    std::int64_t newSourceOffsetSamples);

    [[nodiscard]] const CommandMetadata& metadata() const noexcept override;
    [[nodiscard]] CommandResult validate(const session::ProjectManifest& manifest) const override;
    [[nodiscard]] std::string preview(const session::ProjectManifest& manifest) const override;
    CommandResult apply(session::ProjectManifest& manifest) override;
    CommandResult undo(session::ProjectManifest& manifest) override;
    [[nodiscard]] std::string serialize() const override;

  private:
    CommandMetadata metadata_;
    std::string clipId_;
    std::int64_t newStartSample_{0};
    std::int64_t newLengthSamples_{0};
    std::int64_t newSourceOffsetSamples_{0};
    session::Clip previousClip_;
    bool applied_{false};
};

class SlipClipCommand final : public ICommand {
  public:
    SlipClipCommand(std::string commandId, std::string auditId, std::string clipId,
                    std::int64_t newSourceOffsetSamples);

    [[nodiscard]] const CommandMetadata& metadata() const noexcept override;
    [[nodiscard]] CommandResult validate(const session::ProjectManifest& manifest) const override;
    [[nodiscard]] std::string preview(const session::ProjectManifest& manifest) const override;
    CommandResult apply(session::ProjectManifest& manifest) override;
    CommandResult undo(session::ProjectManifest& manifest) override;
    [[nodiscard]] std::string serialize() const override;

  private:
    CommandMetadata metadata_;
    std::string clipId_;
    std::int64_t newSourceOffsetSamples_{0};
    std::int64_t previousSourceOffsetSamples_{0};
    bool applied_{false};
};

class SetClipFadeCommand final : public ICommand {
  public:
    SetClipFadeCommand(std::string commandId, std::string auditId, std::string clipId,
                       std::int64_t fadeInSamples, std::int64_t fadeOutSamples);

    [[nodiscard]] const CommandMetadata& metadata() const noexcept override;
    [[nodiscard]] CommandResult validate(const session::ProjectManifest& manifest) const override;
    [[nodiscard]] std::string preview(const session::ProjectManifest& manifest) const override;
    CommandResult apply(session::ProjectManifest& manifest) override;
    CommandResult undo(session::ProjectManifest& manifest) override;
    [[nodiscard]] std::string serialize() const override;

  private:
    CommandMetadata metadata_;
    std::string clipId_;
    std::int64_t fadeInSamples_{0};
    std::int64_t fadeOutSamples_{0};
    session::Clip previousClip_;
    bool applied_{false};
};

class SetClipRenderPropertiesCommand final : public ICommand {
  public:
    SetClipRenderPropertiesCommand(std::string commandId, std::string auditId, std::string clipId,
                                   float gainDb, bool muted, bool reversed);

    [[nodiscard]] const CommandMetadata& metadata() const noexcept override;
    [[nodiscard]] CommandResult validate(const session::ProjectManifest& manifest) const override;
    [[nodiscard]] std::string preview(const session::ProjectManifest& manifest) const override;
    CommandResult apply(session::ProjectManifest& manifest) override;
    CommandResult undo(session::ProjectManifest& manifest) override;
    [[nodiscard]] std::string serialize() const override;

  private:
    CommandMetadata metadata_;
    std::string clipId_;
    float gainDb_{0.0F};
    bool muted_{false};
    bool reversed_{false};
    session::Clip previousClip_;
    bool applied_{false};
};

class SetTrackNameCommand final : public ICommand {
  public:
    SetTrackNameCommand(std::string commandId, std::string auditId, std::string trackId,
                        std::string name);

    [[nodiscard]] const CommandMetadata& metadata() const noexcept override;
    [[nodiscard]] CommandResult validate(const session::ProjectManifest& manifest) const override;
    [[nodiscard]] std::string preview(const session::ProjectManifest& manifest) const override;
    CommandResult apply(session::ProjectManifest& manifest) override;
    CommandResult undo(session::ProjectManifest& manifest) override;
    [[nodiscard]] std::string serialize() const override;

  private:
    CommandMetadata metadata_;
    std::string trackId_;
    std::string name_;
    std::string previousName_;
    bool applied_{false};
};

class AddRoutingConnectionCommand final : public ICommand {
  public:
    AddRoutingConnectionCommand(std::string commandId, std::string auditId,
                                session::RoutingConnection connection);

    [[nodiscard]] const CommandMetadata& metadata() const noexcept override;
    [[nodiscard]] CommandResult validate(const session::ProjectManifest& manifest) const override;
    [[nodiscard]] std::string preview(const session::ProjectManifest& manifest) const override;
    CommandResult apply(session::ProjectManifest& manifest) override;
    CommandResult undo(session::ProjectManifest& manifest) override;
    [[nodiscard]] std::string serialize() const override;

  private:
    CommandMetadata metadata_;
    session::RoutingConnection connection_;
    bool applied_{false};
};

class RemoveRoutingConnectionCommand final : public ICommand {
  public:
    RemoveRoutingConnectionCommand(std::string commandId, std::string auditId,
                                   session::RoutingConnection connection);

    [[nodiscard]] const CommandMetadata& metadata() const noexcept override;
    [[nodiscard]] CommandResult validate(const session::ProjectManifest& manifest) const override;
    [[nodiscard]] std::string preview(const session::ProjectManifest& manifest) const override;
    CommandResult apply(session::ProjectManifest& manifest) override;
    CommandResult undo(session::ProjectManifest& manifest) override;
    [[nodiscard]] std::string serialize() const override;

  private:
    CommandMetadata metadata_;
    session::RoutingConnection connection_;
    std::size_t removedIndex_{0};
    bool applied_{false};
};

class SetProjectNameCommand final : public ICommand {
  public:
    SetProjectNameCommand(std::string commandId, std::string auditId, std::string name);

    [[nodiscard]] const CommandMetadata& metadata() const noexcept override;
    [[nodiscard]] CommandResult validate(const session::ProjectManifest& manifest) const override;
    [[nodiscard]] std::string preview(const session::ProjectManifest& manifest) const override;
    CommandResult apply(session::ProjectManifest& manifest) override;
    CommandResult undo(session::ProjectManifest& manifest) override;
    [[nodiscard]] std::string serialize() const override;

  private:
    CommandMetadata metadata_;
    std::string name_;
    std::string previousName_;
    bool applied_{false};
};

class AddMarkerCommand final : public ICommand {
  public:
    AddMarkerCommand(std::string commandId, std::string auditId, session::Marker marker);

    [[nodiscard]] const CommandMetadata& metadata() const noexcept override;
    [[nodiscard]] CommandResult validate(const session::ProjectManifest& manifest) const override;
    [[nodiscard]] std::string preview(const session::ProjectManifest& manifest) const override;
    CommandResult apply(session::ProjectManifest& manifest) override;
    CommandResult undo(session::ProjectManifest& manifest) override;
    [[nodiscard]] std::string serialize() const override;

  private:
    CommandMetadata metadata_;
    session::Marker marker_;
    bool applied_{false};
};

class RemoveMarkerCommand final : public ICommand {
  public:
    RemoveMarkerCommand(std::string commandId, std::string auditId, std::string markerId);

    [[nodiscard]] const CommandMetadata& metadata() const noexcept override;
    [[nodiscard]] CommandResult validate(const session::ProjectManifest& manifest) const override;
    [[nodiscard]] std::string preview(const session::ProjectManifest& manifest) const override;
    CommandResult apply(session::ProjectManifest& manifest) override;
    CommandResult undo(session::ProjectManifest& manifest) override;
    [[nodiscard]] std::string serialize() const override;

  private:
    CommandMetadata metadata_;
    std::string markerId_;
    session::Marker removedMarker_;
    std::size_t removedIndex_{0};
    bool applied_{false};
};

class AddTempoEventCommand final : public ICommand {
  public:
    AddTempoEventCommand(std::string commandId, std::string auditId, session::TempoEvent event);

    [[nodiscard]] const CommandMetadata& metadata() const noexcept override;
    [[nodiscard]] CommandResult validate(const session::ProjectManifest& manifest) const override;
    [[nodiscard]] std::string preview(const session::ProjectManifest& manifest) const override;
    CommandResult apply(session::ProjectManifest& manifest) override;
    CommandResult undo(session::ProjectManifest& manifest) override;
    [[nodiscard]] std::string serialize() const override;

  private:
    CommandMetadata metadata_;
    session::TempoEvent event_;
    std::size_t insertedIndex_{0};
    bool applied_{false};
};

class MidiClipStore {
  public:
    [[nodiscard]] session::MidiClipData* find(std::string_view clipId) noexcept;
    [[nodiscard]] const session::MidiClipData* find(std::string_view clipId) const noexcept;
    session::MidiClipData& getOrCreate(std::string clipId);

  private:
    std::vector<session::MidiClipData> clips_;
};

class AddMidiNoteCommand final {
  public:
    AddMidiNoteCommand(std::string commandId, std::string auditId, std::string clipId,
                       session::MidiNote note);

    [[nodiscard]] const CommandMetadata& metadata() const noexcept;
    [[nodiscard]] CommandResult validate(const MidiClipStore& store) const;
    [[nodiscard]] std::string preview(const MidiClipStore& store) const;
    CommandResult apply(MidiClipStore& store);
    CommandResult undo(MidiClipStore& store);
    [[nodiscard]] std::string serialize() const;

  private:
    CommandMetadata metadata_;
    std::string clipId_;
    session::MidiNote note_;
    bool applied_{false};
};

class QuantizeMidiClipCommand final {
  public:
    QuantizeMidiClipCommand(std::string commandId, std::string auditId, std::string clipId,
                            session::QuantizeSettings settings);

    [[nodiscard]] const CommandMetadata& metadata() const noexcept;
    [[nodiscard]] CommandResult validate(const MidiClipStore& store) const;
    [[nodiscard]] std::string preview(const MidiClipStore& store) const;
    CommandResult apply(MidiClipStore& store);
    CommandResult undo(MidiClipStore& store);
    [[nodiscard]] std::string serialize() const;

  private:
    CommandMetadata metadata_;
    std::string clipId_;
    session::QuantizeSettings settings_;
    session::MidiClipData previousClip_;
    bool applied_{false};
};

class TransposeMidiClipCommand final {
  public:
    TransposeMidiClipCommand(std::string commandId, std::string auditId, std::string clipId,
                             int semitones);

    [[nodiscard]] const CommandMetadata& metadata() const noexcept;
    [[nodiscard]] CommandResult validate(const MidiClipStore& store) const;
    [[nodiscard]] std::string preview(const MidiClipStore& store) const;
    CommandResult apply(MidiClipStore& store);
    CommandResult undo(MidiClipStore& store);
    [[nodiscard]] std::string serialize() const;

  private:
    CommandMetadata metadata_;
    std::string clipId_;
    int semitones_{0};
    session::MidiClipData previousClip_;
    bool applied_{false};
};

class TransformMidiVelocityCommand final {
  public:
    TransformMidiVelocityCommand(std::string commandId, std::string auditId, std::string clipId,
                                 session::VelocityTransform transform);

    [[nodiscard]] const CommandMetadata& metadata() const noexcept;
    [[nodiscard]] CommandResult validate(const MidiClipStore& store) const;
    [[nodiscard]] std::string preview(const MidiClipStore& store) const;
    CommandResult apply(MidiClipStore& store);
    CommandResult undo(MidiClipStore& store);
    [[nodiscard]] std::string serialize() const;

  private:
    CommandMetadata metadata_;
    std::string clipId_;
    session::VelocityTransform transform_;
    session::MidiClipData previousClip_;
    bool applied_{false};
};

class HumanizeMidiClipCommand final {
  public:
    HumanizeMidiClipCommand(std::string commandId, std::string auditId, std::string clipId,
                            session::HumanizeSettings settings);

    [[nodiscard]] const CommandMetadata& metadata() const noexcept;
    [[nodiscard]] CommandResult validate(const MidiClipStore& store) const;
    [[nodiscard]] std::string preview(const MidiClipStore& store) const;
    CommandResult apply(MidiClipStore& store);
    CommandResult undo(MidiClipStore& store);
    [[nodiscard]] std::string serialize() const;

  private:
    CommandMetadata metadata_;
    std::string clipId_;
    session::HumanizeSettings settings_;
    session::MidiClipData previousClip_;
    bool applied_{false};
};

class SetMidiNoteLengthsCommand final {
  public:
    SetMidiNoteLengthsCommand(std::string commandId, std::string auditId, std::string clipId,
                              std::int64_t lengthSamples);

    [[nodiscard]] const CommandMetadata& metadata() const noexcept;
    [[nodiscard]] CommandResult validate(const MidiClipStore& store) const;
    [[nodiscard]] std::string preview(const MidiClipStore& store) const;
    CommandResult apply(MidiClipStore& store);
    CommandResult undo(MidiClipStore& store);
    [[nodiscard]] std::string serialize() const;

  private:
    CommandMetadata metadata_;
    std::string clipId_;
    std::int64_t lengthSamples_{0};
    session::MidiClipData previousClip_;
    bool applied_{false};
};

class LegatoMidiClipCommand final {
  public:
    LegatoMidiClipCommand(std::string commandId, std::string auditId, std::string clipId);

    [[nodiscard]] const CommandMetadata& metadata() const noexcept;
    [[nodiscard]] CommandResult validate(const MidiClipStore& store) const;
    [[nodiscard]] std::string preview(const MidiClipStore& store) const;
    CommandResult apply(MidiClipStore& store);
    CommandResult undo(MidiClipStore& store);
    [[nodiscard]] std::string serialize() const;

  private:
    CommandMetadata metadata_;
    std::string clipId_;
    session::MidiClipData previousClip_;
    bool applied_{false};
};

class EditMidiNoteCommand final {
  public:
    EditMidiNoteCommand(std::string commandId, std::string auditId, std::string clipId,
                        std::string noteId, session::MidiNote replacement);

    [[nodiscard]] const CommandMetadata& metadata() const noexcept;
    [[nodiscard]] CommandResult validate(const MidiClipStore& store) const;
    [[nodiscard]] std::string preview(const MidiClipStore& store) const;
    CommandResult apply(MidiClipStore& store);
    CommandResult undo(MidiClipStore& store);
    [[nodiscard]] std::string serialize() const;

  private:
    CommandMetadata metadata_;
    std::string clipId_;
    std::string noteId_;
    session::MidiNote replacement_;
    session::MidiNote previous_;
    bool applied_{false};
};

class SplitMidiNoteCommand final {
  public:
    SplitMidiNoteCommand(std::string commandId, std::string auditId, std::string clipId,
                         std::string noteId, std::string rightNoteId, std::int64_t splitSample);

    [[nodiscard]] const CommandMetadata& metadata() const noexcept;
    [[nodiscard]] CommandResult validate(const MidiClipStore& store) const;
    [[nodiscard]] std::string preview(const MidiClipStore& store) const;
    CommandResult apply(MidiClipStore& store);
    CommandResult undo(MidiClipStore& store);
    [[nodiscard]] std::string serialize() const;

  private:
    CommandMetadata metadata_;
    std::string clipId_;
    std::string noteId_;
    std::string rightNoteId_;
    std::int64_t splitSample_{0};
    session::MidiNote previous_;
    bool applied_{false};
};

class AddMidiControlChangeCommand final {
  public:
    AddMidiControlChangeCommand(std::string commandId, std::string auditId, std::string clipId,
                                session::MidiControlChange change);

    [[nodiscard]] const CommandMetadata& metadata() const noexcept;
    [[nodiscard]] CommandResult validate(const MidiClipStore& store) const;
    [[nodiscard]] std::string preview(const MidiClipStore& store) const;
    CommandResult apply(MidiClipStore& store);
    CommandResult undo(MidiClipStore& store);
    [[nodiscard]] std::string serialize() const;

  private:
    CommandMetadata metadata_;
    std::string clipId_;
    session::MidiControlChange change_;
    session::MidiClipData previousClip_;
    bool applied_{false};
};

class AddMidiPitchBendCommand final {
  public:
    AddMidiPitchBendCommand(std::string commandId, std::string auditId, std::string clipId,
                            session::MidiPitchBend bend);

    [[nodiscard]] const CommandMetadata& metadata() const noexcept;
    [[nodiscard]] CommandResult validate(const MidiClipStore& store) const;
    [[nodiscard]] std::string preview(const MidiClipStore& store) const;
    CommandResult apply(MidiClipStore& store);
    CommandResult undo(MidiClipStore& store);
    [[nodiscard]] std::string serialize() const;

  private:
    CommandMetadata metadata_;
    std::string clipId_;
    session::MidiPitchBend bend_;
    session::MidiClipData previousClip_;
    bool applied_{false};
};

class AddMidiAftertouchCommand final {
  public:
    AddMidiAftertouchCommand(std::string commandId, std::string auditId, std::string clipId,
                             session::MidiAftertouch pressure);

    [[nodiscard]] const CommandMetadata& metadata() const noexcept;
    [[nodiscard]] CommandResult validate(const MidiClipStore& store) const;
    [[nodiscard]] std::string preview(const MidiClipStore& store) const;
    CommandResult apply(MidiClipStore& store);
    CommandResult undo(MidiClipStore& store);
    [[nodiscard]] std::string serialize() const;

  private:
    CommandMetadata metadata_;
    std::string clipId_;
    session::MidiAftertouch pressure_;
    session::MidiClipData previousClip_;
    bool applied_{false};
};

class AddMidiProgramChangeCommand final {
  public:
    AddMidiProgramChangeCommand(std::string commandId, std::string auditId, std::string clipId,
                                session::MidiProgramChange change);

    [[nodiscard]] const CommandMetadata& metadata() const noexcept;
    [[nodiscard]] CommandResult validate(const MidiClipStore& store) const;
    [[nodiscard]] std::string preview(const MidiClipStore& store) const;
    CommandResult apply(MidiClipStore& store);
    CommandResult undo(MidiClipStore& store);
    [[nodiscard]] std::string serialize() const;

  private:
    CommandMetadata metadata_;
    std::string clipId_;
    session::MidiProgramChange change_;
    session::MidiClipData previousClip_;
    bool applied_{false};
};

class PatternClipStore {
  public:
    [[nodiscard]] session::PatternClip* find(std::string_view patternId) noexcept;
    [[nodiscard]] const session::PatternClip* find(std::string_view patternId) const noexcept;
    void add(session::PatternClip pattern);
    void remove(std::string_view patternId);

  private:
    std::vector<session::PatternClip> patterns_;
};

class AddPatternClipCommand final {
  public:
    AddPatternClipCommand(std::string commandId, std::string auditId, session::PatternClip pattern);

    [[nodiscard]] const CommandMetadata& metadata() const noexcept;
    [[nodiscard]] CommandResult validate(const PatternClipStore& store) const;
    [[nodiscard]] std::string preview(const PatternClipStore& store) const;
    CommandResult apply(PatternClipStore& store);
    CommandResult undo(PatternClipStore& store);
    [[nodiscard]] std::string serialize() const;

  private:
    CommandMetadata metadata_;
    session::PatternClip pattern_;
    bool applied_{false};
};

class DuplicatePatternVariationCommand final {
  public:
    DuplicatePatternVariationCommand(std::string commandId, std::string auditId,
                                     std::string sourcePatternId, std::string newPatternId,
                                     std::string newName, std::uint32_t seedOffset);

    [[nodiscard]] const CommandMetadata& metadata() const noexcept;
    [[nodiscard]] CommandResult validate(const PatternClipStore& store) const;
    [[nodiscard]] std::string preview(const PatternClipStore& store) const;
    CommandResult apply(PatternClipStore& store);
    CommandResult undo(PatternClipStore& store);
    [[nodiscard]] std::string serialize() const;

  private:
    CommandMetadata metadata_;
    std::string sourcePatternId_;
    std::string newPatternId_;
    std::string newName_;
    std::uint32_t seedOffset_{0};
    bool applied_{false};
};

class EditPatternStepCommand final {
  public:
    EditPatternStepCommand(std::string commandId, std::string auditId, std::string patternId,
                           std::string laneId, std::uint32_t stepIndex, session::PatternStep step);

    [[nodiscard]] const CommandMetadata& metadata() const noexcept;
    [[nodiscard]] CommandResult validate(const PatternClipStore& store) const;
    [[nodiscard]] std::string preview(const PatternClipStore& store) const;
    CommandResult apply(PatternClipStore& store);
    CommandResult undo(PatternClipStore& store);
    [[nodiscard]] std::string serialize() const;

  private:
    CommandMetadata metadata_;
    std::string patternId_;
    std::string laneId_;
    std::uint32_t stepIndex_{0};
    session::PatternStep step_;
    session::PatternLane previousLane_;
    bool applied_{false};
};

class AutomationLaneStore {
  public:
    [[nodiscard]] session::AutomationLaneData* find(std::string_view laneId) noexcept;
    [[nodiscard]] const session::AutomationLaneData* find(std::string_view laneId) const noexcept;
    session::AutomationLaneData& getOrCreate(session::AutomationLaneData lane);

  private:
    std::vector<session::AutomationLaneData> lanes_;
};

class AddAutomationPointCommand final {
  public:
    AddAutomationPointCommand(std::string commandId, std::string auditId,
                              session::AutomationLaneData lane, std::int64_t samplePosition,
                              float value, session::AutomationCurve curveToNext);

    [[nodiscard]] const CommandMetadata& metadata() const noexcept;
    [[nodiscard]] CommandResult validate(const AutomationLaneStore& store) const;
    [[nodiscard]] std::string preview(const AutomationLaneStore& store) const;
    CommandResult apply(AutomationLaneStore& store);
    CommandResult undo(AutomationLaneStore& store);
    [[nodiscard]] std::string serialize() const;

  private:
    CommandMetadata metadata_;
    session::AutomationLaneData lane_;
    std::int64_t samplePosition_{0};
    float value_{0.0F};
    session::AutomationCurve curveToNext_{session::AutomationCurve::Linear};
    session::AutomationLaneData previousLane_;
    bool hadPreviousLane_{false};
    bool applied_{false};
};

class CaptureAutomationWriteCommand final {
  public:
    CaptureAutomationWriteCommand(std::string commandId, std::string auditId,
                                  session::AutomationLaneData lane,
                                  std::vector<session::AutomationWriteSample> samples,
                                  float thinningTolerance = 0.0F);

    [[nodiscard]] const CommandMetadata& metadata() const noexcept;
    [[nodiscard]] CommandResult validate(const AutomationLaneStore& store) const;
    [[nodiscard]] std::string preview(const AutomationLaneStore& store) const;
    CommandResult apply(AutomationLaneStore& store);
    CommandResult undo(AutomationLaneStore& store);
    [[nodiscard]] std::string serialize() const;
    [[nodiscard]] const session::AutomationCommandBatch& capturedBatch() const noexcept;

  private:
    CommandMetadata metadata_;
    session::AutomationLaneData lane_;
    std::vector<session::AutomationWriteSample> samples_;
    float thinningTolerance_{0.0F};
    session::AutomationCommandBatch capturedBatch_;
    session::AutomationLaneData previousLane_;
    bool hadPreviousLane_{false};
    bool applied_{false};
};

struct ChannelMixSettings {
    std::optional<float> volumeDb;
    std::optional<float> pan;
    std::optional<bool> muted;
    std::optional<bool> solo;
};

class SetChannelMixCommand final {
  public:
    SetChannelMixCommand(std::string commandId, std::string auditId, std::string channelId,
                         ChannelMixSettings settings);

    [[nodiscard]] const CommandMetadata& metadata() const noexcept;
    [[nodiscard]] CommandResult validate(const session::MixerState& mixer) const;
    [[nodiscard]] std::string preview(const session::MixerState& mixer) const;
    CommandResult apply(session::MixerState& mixer);
    CommandResult undo(session::MixerState& mixer);
    [[nodiscard]] std::string serialize() const;

  private:
    CommandMetadata metadata_;
    std::string channelId_;
    ChannelMixSettings settings_;
    session::ChannelStrip previousChannel_;
    bool applied_{false};
};

class PluginInsertChainStore {
  public:
    [[nodiscard]] session::PluginInsertChain* find(std::string_view trackId) noexcept;
    [[nodiscard]] const session::PluginInsertChain* find(std::string_view trackId) const noexcept;
    session::PluginInsertChain& getOrCreate(std::string trackId);

  private:
    std::vector<session::PluginInsertChain> chains_;
};

class AddPluginInsertCommand final {
  public:
    AddPluginInsertCommand(std::string commandId, std::string auditId, std::string trackId,
                           session::PluginInsert insert);

    [[nodiscard]] const CommandMetadata& metadata() const noexcept;
    [[nodiscard]] CommandResult validate(const PluginInsertChainStore& store) const;
    [[nodiscard]] std::string preview(const PluginInsertChainStore& store) const;
    CommandResult apply(PluginInsertChainStore& store);
    CommandResult undo(PluginInsertChainStore& store);
    [[nodiscard]] std::string serialize() const;

  private:
    CommandMetadata metadata_;
    std::string trackId_;
    session::PluginInsert insert_;
    bool applied_{false};
};

class RemovePluginInsertCommand final {
  public:
    RemovePluginInsertCommand(std::string commandId, std::string auditId, std::string trackId,
                              std::string insertId);

    [[nodiscard]] const CommandMetadata& metadata() const noexcept;
    [[nodiscard]] CommandResult validate(const PluginInsertChainStore& store) const;
    [[nodiscard]] std::string preview(const PluginInsertChainStore& store) const;
    CommandResult apply(PluginInsertChainStore& store);
    CommandResult undo(PluginInsertChainStore& store);
    [[nodiscard]] std::string serialize() const;

  private:
    CommandMetadata metadata_;
    std::string trackId_;
    std::string insertId_;
    session::PluginInsert removedInsert_;
    std::size_t removedIndex_{0};
    bool applied_{false};
};

class MovePluginInsertCommand final {
  public:
    MovePluginInsertCommand(std::string commandId, std::string auditId, std::string trackId,
                            std::string insertId, std::size_t newIndex);

    [[nodiscard]] const CommandMetadata& metadata() const noexcept;
    [[nodiscard]] CommandResult validate(const PluginInsertChainStore& store) const;
    [[nodiscard]] std::string preview(const PluginInsertChainStore& store) const;
    CommandResult apply(PluginInsertChainStore& store);
    CommandResult undo(PluginInsertChainStore& store);
    [[nodiscard]] std::string serialize() const;

  private:
    CommandMetadata metadata_;
    std::string trackId_;
    std::string insertId_;
    std::size_t newIndex_{0};
    std::size_t oldIndex_{0};
    bool applied_{false};
};

class ApplyPluginPresetCommand final {
  public:
    ApplyPluginPresetCommand(std::string commandId, std::string auditId, std::string trackId,
                             std::string insertId, session::PluginPreset preset);

    [[nodiscard]] const CommandMetadata& metadata() const noexcept;
    [[nodiscard]] CommandResult validate(const PluginInsertChainStore& store) const;
    [[nodiscard]] std::string preview(const PluginInsertChainStore& store) const;
    CommandResult apply(PluginInsertChainStore& store);
    CommandResult undo(PluginInsertChainStore& store);
    [[nodiscard]] std::string serialize() const;

  private:
    CommandMetadata metadata_;
    std::string trackId_;
    std::string insertId_;
    session::PluginPreset preset_;
    session::PluginInsert previousInsert_;
    bool applied_{false};
};

class WarpStateStore {
  public:
    [[nodiscard]] session::WarpState* find(std::string_view clipId) noexcept;
    [[nodiscard]] const session::WarpState* find(std::string_view clipId) const noexcept;
    session::WarpState& getOrCreate(session::WarpState warp);

  private:
    std::vector<session::WarpState> warps_;
};

class AddWarpMarkerCommand final {
  public:
    AddWarpMarkerCommand(std::string commandId, std::string auditId, session::WarpState warp,
                         session::WarpMarker marker);

    [[nodiscard]] const CommandMetadata& metadata() const noexcept;
    [[nodiscard]] CommandResult validate(const WarpStateStore& store) const;
    [[nodiscard]] std::string preview(const WarpStateStore& store) const;
    CommandResult apply(WarpStateStore& store);
    CommandResult undo(WarpStateStore& store);
    [[nodiscard]] std::string serialize() const;

  private:
    CommandMetadata metadata_;
    session::WarpState warp_;
    session::WarpMarker marker_;
    bool applied_{false};
};

class MoveWarpMarkerCommand final {
  public:
    MoveWarpMarkerCommand(std::string commandId, std::string auditId, std::string clipId,
                          std::string markerId, std::int64_t newSourceSample,
                          std::int64_t newTimelineSample);

    [[nodiscard]] const CommandMetadata& metadata() const noexcept;
    [[nodiscard]] CommandResult validate(const WarpStateStore& store) const;
    [[nodiscard]] std::string preview(const WarpStateStore& store) const;
    CommandResult apply(WarpStateStore& store);
    CommandResult undo(WarpStateStore& store);
    [[nodiscard]] std::string serialize() const;

  private:
    CommandMetadata metadata_;
    std::string clipId_;
    std::string markerId_;
    std::int64_t newSourceSample_{0};
    std::int64_t newTimelineSample_{0};
    session::WarpMarker previousMarker_;
    bool applied_{false};
};

class RemoveWarpMarkerCommand final {
  public:
    RemoveWarpMarkerCommand(std::string commandId, std::string auditId, std::string clipId,
                            std::string markerId);

    [[nodiscard]] const CommandMetadata& metadata() const noexcept;
    [[nodiscard]] CommandResult validate(const WarpStateStore& store) const;
    [[nodiscard]] std::string preview(const WarpStateStore& store) const;
    CommandResult apply(WarpStateStore& store);
    CommandResult undo(WarpStateStore& store);
    [[nodiscard]] std::string serialize() const;

  private:
    CommandMetadata metadata_;
    std::string clipId_;
    std::string markerId_;
    session::WarpMarker removedMarker_;
    std::size_t removedIndex_{0};
    bool applied_{false};
};

class QuantizeWarpMarkersCommand final {
  public:
    QuantizeWarpMarkersCommand(std::string commandId, std::string auditId, std::string clipId,
                               std::int64_t gridSamples, float strength);

    [[nodiscard]] const CommandMetadata& metadata() const noexcept;
    [[nodiscard]] CommandResult validate(const WarpStateStore& store) const;
    [[nodiscard]] std::string preview(const WarpStateStore& store) const;
    CommandResult apply(WarpStateStore& store);
    CommandResult undo(WarpStateStore& store);
    [[nodiscard]] std::string serialize() const;

  private:
    CommandMetadata metadata_;
    std::string clipId_;
    std::int64_t gridSamples_{0};
    float strength_{0.0F};
    session::WarpState previousWarp_;
    bool applied_{false};
};

class ApplyWarpGrooveCommand final {
  public:
    ApplyWarpGrooveCommand(std::string commandId, std::string auditId, std::string clipId,
                           session::GrooveTemplate groove, float strength);

    [[nodiscard]] const CommandMetadata& metadata() const noexcept;
    [[nodiscard]] CommandResult validate(const WarpStateStore& store) const;
    [[nodiscard]] std::string preview(const WarpStateStore& store) const;
    CommandResult apply(WarpStateStore& store);
    CommandResult undo(WarpStateStore& store);
    [[nodiscard]] std::string serialize() const;

  private:
    CommandMetadata metadata_;
    std::string clipId_;
    session::GrooveTemplate groove_;
    float strength_{1.0F};
    session::WarpState previousWarp_;
    bool applied_{false};
};

class TransactionCommand final : public ICommand {
  public:
    TransactionCommand(std::string commandId, std::string auditId, std::string name,
                       std::vector<CommandPtr> commands);

    [[nodiscard]] const CommandMetadata& metadata() const noexcept override;
    [[nodiscard]] CommandResult validate(const session::ProjectManifest& manifest) const override;
    [[nodiscard]] std::string preview(const session::ProjectManifest& manifest) const override;
    CommandResult apply(session::ProjectManifest& manifest) override;
    CommandResult undo(session::ProjectManifest& manifest) override;
    [[nodiscard]] std::string serialize() const override;

  private:
    CommandMetadata metadata_;
    std::vector<CommandPtr> commands_;
    std::size_t appliedCount_{0};
};

class CommandHistory {
  public:
    CommandResult execute(session::ProjectManifest& manifest, CommandPtr command);
    CommandResult undo(session::ProjectManifest& manifest);
    CommandResult redo(session::ProjectManifest& manifest);

    [[nodiscard]] bool canUndo() const noexcept;
    [[nodiscard]] bool canRedo() const noexcept;
    [[nodiscard]] std::size_t undoDepth() const noexcept;
    [[nodiscard]] std::size_t redoDepth() const noexcept;

  private:
    std::vector<CommandPtr> undoStack_;
    std::vector<CommandPtr> redoStack_;
};

struct CommandReplayReport {
    std::size_t appliedCount{0};
    std::vector<CommandResult> results;
};

[[nodiscard]] std::vector<std::string> registeredProjectCommandNames();
[[nodiscard]] CommandPtr commandFromSerialized(std::string_view serializedCommand);
[[nodiscard]] CommandReplayReport
replaySerializedCommands(session::ProjectManifest& manifest,
                         const std::vector<std::string>& serializedCommands);
[[nodiscard]] CommandPtr makeAddTrackCommand(std::string commandId, std::string auditId,
                                             session::Track track);
[[nodiscard]] CommandPtr makeAddClipCommand(std::string commandId, std::string auditId,
                                            session::Clip clip);
[[nodiscard]] CommandPtr makeRemoveClipCommand(std::string commandId, std::string auditId,
                                               std::string clipId);
[[nodiscard]] CommandPtr makeDuplicateClipCommand(std::string commandId, std::string auditId,
                                                  std::string sourceClipId,
                                                  std::string duplicateClipId,
                                                  std::int64_t duplicateStartSample);
[[nodiscard]] CommandPtr makeMoveClipCommand(std::string commandId, std::string auditId,
                                             std::string clipId, std::int64_t newStartSample);
[[nodiscard]] CommandPtr makeTrimClipCommand(std::string commandId, std::string auditId,
                                             std::string clipId, std::int64_t newStartSample,
                                             std::int64_t newLengthSamples,
                                             std::int64_t newSourceOffsetSamples);
[[nodiscard]] CommandPtr makeSlipClipCommand(std::string commandId, std::string auditId,
                                             std::string clipId,
                                             std::int64_t newSourceOffsetSamples);
[[nodiscard]] CommandPtr makeSetClipFadeCommand(std::string commandId, std::string auditId,
                                                std::string clipId, std::int64_t fadeInSamples,
                                                std::int64_t fadeOutSamples);
[[nodiscard]] CommandPtr makeSetClipRenderPropertiesCommand(std::string commandId,
                                                            std::string auditId, std::string clipId,
                                                            float gainDb, bool muted,
                                                            bool reversed);
[[nodiscard]] CommandPtr makeSplitClipCommand(const session::ProjectManifest& manifest,
                                              std::string commandId, std::string auditId,
                                              std::string leftClipId, std::string rightClipId,
                                              std::int64_t splitSample);
[[nodiscard]] CommandPtr makeSetTrackNameCommand(std::string commandId, std::string auditId,
                                                 std::string trackId, std::string name);
[[nodiscard]] CommandPtr makeAddRoutingConnectionCommand(std::string commandId, std::string auditId,
                                                         session::RoutingConnection connection);
[[nodiscard]] CommandPtr makeRemoveRoutingConnectionCommand(std::string commandId,
                                                            std::string auditId,
                                                            session::RoutingConnection connection);
[[nodiscard]] CommandPtr makeSetProjectNameCommand(std::string commandId, std::string auditId,
                                                   std::string name);
[[nodiscard]] CommandPtr makeAddMarkerCommand(std::string commandId, std::string auditId,
                                              session::Marker marker);
[[nodiscard]] CommandPtr makeRemoveMarkerCommand(std::string commandId, std::string auditId,
                                                 std::string markerId);
[[nodiscard]] CommandPtr makeAddTempoEventCommand(std::string commandId, std::string auditId,
                                                  session::TempoEvent event);

} // namespace lamusica::commands
