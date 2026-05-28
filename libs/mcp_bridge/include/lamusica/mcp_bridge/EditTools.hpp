#pragma once

#include "lamusica/commands/Command.hpp"
#include "lamusica/mcp_bridge/DaemonSession.hpp"

#include <string>
#include <variant>
#include <vector>

namespace lamusica::mcp_bridge {

struct EditToolResult {
    std::string commandId;
    std::string auditId;
    bool validationOk{false};
    bool applied{false};
    bool undoAvailable{false};
    bool redoAvailable{false};
    bool confirmationRequired{false};
    std::string confirmationToken;
    std::string message;
    std::string preview;
};

struct EditApplyOptions {
    std::string confirmationToken;
};

using MidiEditCommand =
    std::variant<commands::AddMidiNoteCommand, commands::QuantizeMidiClipCommand,
                 commands::TransposeMidiClipCommand, commands::TransformMidiVelocityCommand,
                 commands::HumanizeMidiClipCommand, commands::SetMidiNoteLengthsCommand,
                 commands::LegatoMidiClipCommand, commands::EditMidiNoteCommand,
                 commands::SplitMidiNoteCommand, commands::AddMidiControlChangeCommand,
                 commands::AddMidiPitchBendCommand, commands::AddMidiAftertouchCommand,
                 commands::AddMidiProgramChangeCommand>;
using AutomationEditCommand =
    std::variant<commands::AddAutomationPointCommand, commands::CaptureAutomationWriteCommand>;
using MixerEditCommand = std::variant<commands::SetChannelMixCommand>;
using PluginEditCommand =
    std::variant<commands::AddPluginInsertCommand, commands::RemovePluginInsertCommand,
                 commands::MovePluginInsertCommand, commands::ApplyPluginPresetCommand>;

class MidiEditHistory {
  public:
    [[nodiscard]] EditToolResult execute(commands::MidiClipStore& store, MidiEditCommand command);
    [[nodiscard]] EditToolResult undo(commands::MidiClipStore& store);
    [[nodiscard]] EditToolResult redo(commands::MidiClipStore& store);

    [[nodiscard]] bool canUndo() const noexcept;
    [[nodiscard]] bool canRedo() const noexcept;

  private:
    std::vector<MidiEditCommand> undoStack_;
    std::vector<MidiEditCommand> redoStack_;
};

class AutomationEditHistory {
  public:
    [[nodiscard]] EditToolResult execute(commands::AutomationLaneStore& store,
                                         AutomationEditCommand command);
    [[nodiscard]] EditToolResult undo(commands::AutomationLaneStore& store);
    [[nodiscard]] EditToolResult redo(commands::AutomationLaneStore& store);

    [[nodiscard]] bool canUndo() const noexcept;
    [[nodiscard]] bool canRedo() const noexcept;

  private:
    std::vector<AutomationEditCommand> undoStack_;
    std::vector<AutomationEditCommand> redoStack_;
};

class MixerEditHistory {
  public:
    [[nodiscard]] EditToolResult execute(session::MixerState& mixer, MixerEditCommand command);
    [[nodiscard]] EditToolResult undo(session::MixerState& mixer);
    [[nodiscard]] EditToolResult redo(session::MixerState& mixer);

    [[nodiscard]] bool canUndo() const noexcept;
    [[nodiscard]] bool canRedo() const noexcept;

  private:
    std::vector<MixerEditCommand> undoStack_;
    std::vector<MixerEditCommand> redoStack_;
};

class PluginEditHistory {
  public:
    [[nodiscard]] EditToolResult execute(commands::PluginInsertChainStore& store,
                                         PluginEditCommand command);
    [[nodiscard]] EditToolResult undo(commands::PluginInsertChainStore& store);
    [[nodiscard]] EditToolResult redo(commands::PluginInsertChainStore& store);

    [[nodiscard]] bool canUndo() const noexcept;
    [[nodiscard]] bool canRedo() const noexcept;

  private:
    std::vector<PluginEditCommand> undoStack_;
    std::vector<PluginEditCommand> redoStack_;
};

[[nodiscard]] EditToolResult previewCommand(const DaemonSession& session,
                                            const session::ProjectManifest& manifest,
                                            const commands::ICommand& command);
[[nodiscard]] EditToolResult applyCommand(const DaemonSession& session,
                                          session::ProjectManifest& manifest,
                                          commands::CommandHistory& history,
                                          commands::CommandPtr command,
                                          EditApplyOptions options = {});
[[nodiscard]] EditToolResult undoLastCommand(const DaemonSession& session,
                                             session::ProjectManifest& manifest,
                                             commands::CommandHistory& history);
[[nodiscard]] EditToolResult redoLastCommand(const DaemonSession& session,
                                             session::ProjectManifest& manifest,
                                             commands::CommandHistory& history);
[[nodiscard]] EditToolResult previewMidiCommand(const DaemonSession& session,
                                                const commands::MidiClipStore& store,
                                                const MidiEditCommand& command);
[[nodiscard]] EditToolResult applyMidiCommand(const DaemonSession& session,
                                              commands::MidiClipStore& store,
                                              MidiEditHistory& history, MidiEditCommand command);
[[nodiscard]] EditToolResult undoLastMidiCommand(const DaemonSession& session,
                                                 commands::MidiClipStore& store,
                                                 MidiEditHistory& history);
[[nodiscard]] EditToolResult redoLastMidiCommand(const DaemonSession& session,
                                                 commands::MidiClipStore& store,
                                                 MidiEditHistory& history);
[[nodiscard]] EditToolResult previewAutomationCommand(const DaemonSession& session,
                                                      const commands::AutomationLaneStore& store,
                                                      const AutomationEditCommand& command);
[[nodiscard]] EditToolResult applyAutomationCommand(const DaemonSession& session,
                                                    commands::AutomationLaneStore& store,
                                                    AutomationEditHistory& history,
                                                    AutomationEditCommand command);
[[nodiscard]] EditToolResult undoLastAutomationCommand(const DaemonSession& session,
                                                       commands::AutomationLaneStore& store,
                                                       AutomationEditHistory& history);
[[nodiscard]] EditToolResult redoLastAutomationCommand(const DaemonSession& session,
                                                       commands::AutomationLaneStore& store,
                                                       AutomationEditHistory& history);
[[nodiscard]] EditToolResult previewMixerCommand(const DaemonSession& session,
                                                 const session::MixerState& mixer,
                                                 const MixerEditCommand& command);
[[nodiscard]] EditToolResult applyMixerCommand(const DaemonSession& session,
                                               session::MixerState& mixer,
                                               MixerEditHistory& history, MixerEditCommand command);
[[nodiscard]] EditToolResult undoLastMixerCommand(const DaemonSession& session,
                                                  session::MixerState& mixer,
                                                  MixerEditHistory& history);
[[nodiscard]] EditToolResult redoLastMixerCommand(const DaemonSession& session,
                                                  session::MixerState& mixer,
                                                  MixerEditHistory& history);
[[nodiscard]] EditToolResult previewPluginCommand(const DaemonSession& session,
                                                  const commands::PluginInsertChainStore& store,
                                                  const PluginEditCommand& command);
[[nodiscard]] EditToolResult applyPluginCommand(const DaemonSession& session,
                                                commands::PluginInsertChainStore& store,
                                                PluginEditHistory& history,
                                                PluginEditCommand command,
                                                EditApplyOptions options = {});
[[nodiscard]] EditToolResult undoLastPluginCommand(const DaemonSession& session,
                                                   commands::PluginInsertChainStore& store,
                                                   PluginEditHistory& history);
[[nodiscard]] EditToolResult redoLastPluginCommand(const DaemonSession& session,
                                                   commands::PluginInsertChainStore& store,
                                                   PluginEditHistory& history);
[[nodiscard]] bool requiresConfirmation(const commands::ICommand& command) noexcept;
[[nodiscard]] std::string confirmationTokenFor(const commands::ICommand& command);
[[nodiscard]] std::string editToolResultJson(const EditToolResult& result);

} // namespace lamusica::mcp_bridge
