#include "lamusica/mcp_bridge/EditTools.hpp"

#include <algorithm>
#include <array>
#include <sstream>
#include <utility>

namespace lamusica::mcp_bridge {
namespace {

std::string escapeJson(std::string_view value) {
    std::string escaped;
    escaped.reserve(value.size());
    for (const char character : value) {
        if (character == '"' || character == '\\') {
            escaped.push_back('\\');
        }
        escaped.push_back(character);
    }
    return escaped;
}

EditToolResult denied(std::string message) {
    return {.validationOk = false,
            .applied = false,
            .undoAvailable = false,
            .redoAvailable = false,
            .message = std::move(message)};
}

template <typename Store, typename CommandVariant>
const commands::CommandMetadata& variantMetadata(const CommandVariant& command) noexcept {
    return std::visit(
        [](const auto& concreteCommand) -> const commands::CommandMetadata& {
            return concreteCommand.metadata();
        },
        command);
}

template <typename Store, typename CommandVariant>
EditToolResult previewStoreCommand(const Store& store, const CommandVariant& command,
                                   bool undoAvailable, bool redoAvailable) {
    const auto validation = std::visit(
        [&store](const auto& concreteCommand) { return concreteCommand.validate(store); }, command);
    const auto preview = std::visit(
        [&store](const auto& concreteCommand) { return concreteCommand.preview(store); }, command);
    const auto& metadata = variantMetadata<Store>(command);
    return {.commandId = metadata.commandId,
            .auditId = metadata.auditId,
            .validationOk = validation.ok,
            .applied = false,
            .undoAvailable = undoAvailable,
            .redoAvailable = redoAvailable,
            .message = validation.message,
            .preview = preview};
}

template <typename Store, typename CommandVariant>
EditToolResult applyStoreCommand(Store& store, CommandVariant& command, bool undoAvailable,
                                 bool redoAvailable) {
    const auto validation = std::visit(
        [&store](const auto& concreteCommand) { return concreteCommand.validate(store); }, command);
    const auto preview = std::visit(
        [&store](const auto& concreteCommand) { return concreteCommand.preview(store); }, command);
    const auto& metadata = variantMetadata<Store>(command);
    if (!validation.ok) {
        return {.commandId = metadata.commandId,
                .auditId = metadata.auditId,
                .validationOk = false,
                .applied = false,
                .undoAvailable = undoAvailable,
                .redoAvailable = redoAvailable,
                .message = validation.message,
                .preview = preview};
    }

    const auto result = std::visit(
        [&store](auto& concreteCommand) { return concreteCommand.apply(store); }, command);
    return {.commandId = metadata.commandId,
            .auditId = metadata.auditId,
            .validationOk = validation.ok,
            .applied = result.ok,
            .undoAvailable = undoAvailable,
            .redoAvailable = redoAvailable,
            .message = result.message,
            .preview = preview};
}

template <typename Store, typename CommandVariant>
EditToolResult undoStoreCommand(Store& store, CommandVariant& command, bool undoAvailable,
                                bool redoAvailable) {
    const auto& metadata = variantMetadata<Store>(command);
    const auto result = std::visit(
        [&store](auto& concreteCommand) { return concreteCommand.undo(store); }, command);
    return {.commandId = metadata.commandId,
            .auditId = metadata.auditId,
            .validationOk = result.ok,
            .applied = result.ok,
            .undoAvailable = undoAvailable,
            .redoAvailable = redoAvailable,
            .message = result.message};
}

} // namespace

EditToolResult MidiEditHistory::execute(commands::MidiClipStore& store, MidiEditCommand command) {
    auto result = applyStoreCommand(store, command, canUndo(), canRedo());
    if (result.applied) {
        undoStack_.push_back(std::move(command));
        redoStack_.clear();
        result.undoAvailable = canUndo();
        result.redoAvailable = canRedo();
    }
    return result;
}

EditToolResult MidiEditHistory::undo(commands::MidiClipStore& store) {
    if (undoStack_.empty()) {
        return denied("No MIDI command to undo");
    }

    auto command = std::move(undoStack_.back());
    undoStack_.pop_back();
    auto result = undoStoreCommand(store, command, canUndo(), canRedo());
    if (result.applied) {
        redoStack_.push_back(std::move(command));
    } else {
        undoStack_.push_back(std::move(command));
    }
    result.undoAvailable = canUndo();
    result.redoAvailable = canRedo();
    return result;
}

EditToolResult MidiEditHistory::redo(commands::MidiClipStore& store) {
    if (redoStack_.empty()) {
        return denied("No MIDI command to redo");
    }

    auto command = std::move(redoStack_.back());
    redoStack_.pop_back();
    auto result = applyStoreCommand(store, command, canUndo(), canRedo());
    if (result.applied) {
        undoStack_.push_back(std::move(command));
    } else {
        redoStack_.push_back(std::move(command));
    }
    result.undoAvailable = canUndo();
    result.redoAvailable = canRedo();
    return result;
}

bool MidiEditHistory::canUndo() const noexcept {
    return !undoStack_.empty();
}

bool MidiEditHistory::canRedo() const noexcept {
    return !redoStack_.empty();
}

EditToolResult AutomationEditHistory::execute(commands::AutomationLaneStore& store,
                                              AutomationEditCommand command) {
    auto result = applyStoreCommand(store, command, canUndo(), canRedo());
    if (result.applied) {
        undoStack_.push_back(std::move(command));
        redoStack_.clear();
        result.undoAvailable = canUndo();
        result.redoAvailable = canRedo();
    }
    return result;
}

EditToolResult AutomationEditHistory::undo(commands::AutomationLaneStore& store) {
    if (undoStack_.empty()) {
        return denied("No automation command to undo");
    }

    auto command = std::move(undoStack_.back());
    undoStack_.pop_back();
    auto result = undoStoreCommand(store, command, canUndo(), canRedo());
    if (result.applied) {
        redoStack_.push_back(std::move(command));
    } else {
        undoStack_.push_back(std::move(command));
    }
    result.undoAvailable = canUndo();
    result.redoAvailable = canRedo();
    return result;
}

EditToolResult AutomationEditHistory::redo(commands::AutomationLaneStore& store) {
    if (redoStack_.empty()) {
        return denied("No automation command to redo");
    }

    auto command = std::move(redoStack_.back());
    redoStack_.pop_back();
    auto result = applyStoreCommand(store, command, canUndo(), canRedo());
    if (result.applied) {
        undoStack_.push_back(std::move(command));
    } else {
        redoStack_.push_back(std::move(command));
    }
    result.undoAvailable = canUndo();
    result.redoAvailable = canRedo();
    return result;
}

bool AutomationEditHistory::canUndo() const noexcept {
    return !undoStack_.empty();
}

bool AutomationEditHistory::canRedo() const noexcept {
    return !redoStack_.empty();
}

EditToolResult MixerEditHistory::execute(session::MixerState& mixer, MixerEditCommand command) {
    auto result = applyStoreCommand(mixer, command, canUndo(), canRedo());
    if (result.applied) {
        undoStack_.push_back(std::move(command));
        redoStack_.clear();
        result.undoAvailable = canUndo();
        result.redoAvailable = canRedo();
    }
    return result;
}

EditToolResult MixerEditHistory::undo(session::MixerState& mixer) {
    if (undoStack_.empty()) {
        return denied("No mixer command to undo");
    }

    auto command = std::move(undoStack_.back());
    undoStack_.pop_back();
    auto result = undoStoreCommand(mixer, command, canUndo(), canRedo());
    if (result.applied) {
        redoStack_.push_back(std::move(command));
    } else {
        undoStack_.push_back(std::move(command));
    }
    result.undoAvailable = canUndo();
    result.redoAvailable = canRedo();
    return result;
}

EditToolResult MixerEditHistory::redo(session::MixerState& mixer) {
    if (redoStack_.empty()) {
        return denied("No mixer command to redo");
    }

    auto command = std::move(redoStack_.back());
    redoStack_.pop_back();
    auto result = applyStoreCommand(mixer, command, canUndo(), canRedo());
    if (result.applied) {
        undoStack_.push_back(std::move(command));
    } else {
        redoStack_.push_back(std::move(command));
    }
    result.undoAvailable = canUndo();
    result.redoAvailable = canRedo();
    return result;
}

bool MixerEditHistory::canUndo() const noexcept {
    return !undoStack_.empty();
}

bool MixerEditHistory::canRedo() const noexcept {
    return !redoStack_.empty();
}

EditToolResult PluginEditHistory::execute(commands::PluginInsertChainStore& store,
                                          PluginEditCommand command) {
    auto result = applyStoreCommand(store, command, canUndo(), canRedo());
    if (result.applied) {
        undoStack_.push_back(std::move(command));
        redoStack_.clear();
        result.undoAvailable = canUndo();
        result.redoAvailable = canRedo();
    }
    return result;
}

EditToolResult PluginEditHistory::undo(commands::PluginInsertChainStore& store) {
    if (undoStack_.empty()) {
        return denied("No plugin command to undo");
    }

    auto command = std::move(undoStack_.back());
    undoStack_.pop_back();
    auto result = undoStoreCommand(store, command, canUndo(), canRedo());
    if (result.applied) {
        redoStack_.push_back(std::move(command));
    } else {
        undoStack_.push_back(std::move(command));
    }
    result.undoAvailable = canUndo();
    result.redoAvailable = canRedo();
    return result;
}

EditToolResult PluginEditHistory::redo(commands::PluginInsertChainStore& store) {
    if (redoStack_.empty()) {
        return denied("No plugin command to redo");
    }

    auto command = std::move(redoStack_.back());
    redoStack_.pop_back();
    auto result = applyStoreCommand(store, command, canUndo(), canRedo());
    if (result.applied) {
        undoStack_.push_back(std::move(command));
    } else {
        redoStack_.push_back(std::move(command));
    }
    result.undoAvailable = canUndo();
    result.redoAvailable = canRedo();
    return result;
}

bool PluginEditHistory::canUndo() const noexcept {
    return !undoStack_.empty();
}

bool PluginEditHistory::canRedo() const noexcept {
    return !redoStack_.empty();
}

bool requiresConfirmation(const commands::ICommand& command) noexcept {
    constexpr std::array destructiveCommands{"remove_clip"};
    const auto commandName = command.metadata().name;
    return std::ranges::find(destructiveCommands, commandName) != destructiveCommands.end();
}

std::string confirmationTokenFor(const commands::ICommand& command) {
    return command.metadata().commandId + ":" + command.metadata().auditId + ":confirm";
}

EditToolResult previewCommand(const DaemonSession& session,
                              const session::ProjectManifest& manifest,
                              const commands::ICommand& command) {
    if (!session.canMutateProject()) {
        return denied("MCP edit capability is required");
    }

    const auto validation = command.validate(manifest);
    return {.commandId = command.metadata().commandId,
            .auditId = command.metadata().auditId,
            .validationOk = validation.ok,
            .applied = false,
            .undoAvailable = false,
            .redoAvailable = false,
            .confirmationRequired = requiresConfirmation(command),
            .confirmationToken = requiresConfirmation(command) ? confirmationTokenFor(command) : "",
            .message = validation.message,
            .preview = command.preview(manifest)};
}

EditToolResult applyCommand(const DaemonSession& session, session::ProjectManifest& manifest,
                            commands::CommandHistory& history, commands::CommandPtr command,
                            EditApplyOptions options) {
    if (!session.canMutateProject()) {
        return denied("MCP edit capability is required");
    }

    const auto commandId = command->metadata().commandId;
    const auto auditId = command->metadata().auditId;
    const auto preview = command->preview(manifest);
    const auto validation = command->validate(manifest);
    if (!validation.ok) {
        return {.commandId = commandId,
                .auditId = auditId,
                .validationOk = false,
                .applied = false,
                .undoAvailable = history.canUndo(),
                .redoAvailable = history.canRedo(),
                .message = validation.message,
                .preview = preview};
    }

    const auto confirmationRequired = requiresConfirmation(*command);
    const auto confirmationToken =
        confirmationRequired ? confirmationTokenFor(*command) : std::string{};
    if (confirmationRequired && options.confirmationToken != confirmationToken) {
        return {.commandId = commandId,
                .auditId = auditId,
                .validationOk = true,
                .applied = false,
                .undoAvailable = history.canUndo(),
                .redoAvailable = history.canRedo(),
                .confirmationRequired = true,
                .confirmationToken = confirmationToken,
                .message = "Confirmation token is required",
                .preview = preview};
    }

    const auto result = history.execute(manifest, std::move(command));
    return {.commandId = commandId,
            .auditId = auditId,
            .validationOk = validation.ok,
            .applied = result.ok,
            .undoAvailable = history.canUndo(),
            .redoAvailable = history.canRedo(),
            .confirmationRequired = confirmationRequired,
            .confirmationToken = confirmationToken,
            .message = result.message,
            .preview = preview};
}

EditToolResult undoLastCommand(const DaemonSession& session, session::ProjectManifest& manifest,
                               commands::CommandHistory& history) {
    if (!session.canMutateProject()) {
        return denied("MCP edit capability is required");
    }

    const auto result = history.undo(manifest);
    return {.commandId = "undo",
            .auditId = "undo",
            .validationOk = result.ok,
            .applied = result.ok,
            .undoAvailable = history.canUndo(),
            .redoAvailable = history.canRedo(),
            .message = result.message};
}

EditToolResult redoLastCommand(const DaemonSession& session, session::ProjectManifest& manifest,
                               commands::CommandHistory& history) {
    if (!session.canMutateProject()) {
        return denied("MCP edit capability is required");
    }

    const auto result = history.redo(manifest);
    return {.commandId = "redo",
            .auditId = "redo",
            .validationOk = result.ok,
            .applied = result.ok,
            .undoAvailable = history.canUndo(),
            .redoAvailable = history.canRedo(),
            .message = result.message};
}

EditToolResult previewMidiCommand(const DaemonSession& session,
                                  const commands::MidiClipStore& store,
                                  const MidiEditCommand& command) {
    if (!session.canMutateProject()) {
        return denied("MCP edit capability is required");
    }

    return previewStoreCommand(store, command, false, false);
}

EditToolResult applyMidiCommand(const DaemonSession& session, commands::MidiClipStore& store,
                                MidiEditHistory& history, MidiEditCommand command) {
    if (!session.canMutateProject()) {
        return denied("MCP edit capability is required");
    }

    return history.execute(store, std::move(command));
}

EditToolResult undoLastMidiCommand(const DaemonSession& session, commands::MidiClipStore& store,
                                   MidiEditHistory& history) {
    if (!session.canMutateProject()) {
        return denied("MCP edit capability is required");
    }

    return history.undo(store);
}

EditToolResult redoLastMidiCommand(const DaemonSession& session, commands::MidiClipStore& store,
                                   MidiEditHistory& history) {
    if (!session.canMutateProject()) {
        return denied("MCP edit capability is required");
    }

    return history.redo(store);
}

EditToolResult previewAutomationCommand(const DaemonSession& session,
                                        const commands::AutomationLaneStore& store,
                                        const AutomationEditCommand& command) {
    if (!session.canMutateProject()) {
        return denied("MCP edit capability is required");
    }

    return previewStoreCommand(store, command, false, false);
}

EditToolResult applyAutomationCommand(const DaemonSession& session,
                                      commands::AutomationLaneStore& store,
                                      AutomationEditHistory& history,
                                      AutomationEditCommand command) {
    if (!session.canMutateProject()) {
        return denied("MCP edit capability is required");
    }

    return history.execute(store, std::move(command));
}

EditToolResult undoLastAutomationCommand(const DaemonSession& session,
                                         commands::AutomationLaneStore& store,
                                         AutomationEditHistory& history) {
    if (!session.canMutateProject()) {
        return denied("MCP edit capability is required");
    }

    return history.undo(store);
}

EditToolResult redoLastAutomationCommand(const DaemonSession& session,
                                         commands::AutomationLaneStore& store,
                                         AutomationEditHistory& history) {
    if (!session.canMutateProject()) {
        return denied("MCP edit capability is required");
    }

    return history.redo(store);
}

EditToolResult previewMixerCommand(const DaemonSession& session, const session::MixerState& mixer,
                                   const MixerEditCommand& command) {
    if (!session.canMutateProject()) {
        return denied("MCP edit capability is required");
    }

    return previewStoreCommand(mixer, command, false, false);
}

EditToolResult applyMixerCommand(const DaemonSession& session, session::MixerState& mixer,
                                 MixerEditHistory& history, MixerEditCommand command) {
    if (!session.canMutateProject()) {
        return denied("MCP edit capability is required");
    }

    return history.execute(mixer, std::move(command));
}

EditToolResult undoLastMixerCommand(const DaemonSession& session, session::MixerState& mixer,
                                    MixerEditHistory& history) {
    if (!session.canMutateProject()) {
        return denied("MCP edit capability is required");
    }

    return history.undo(mixer);
}

EditToolResult redoLastMixerCommand(const DaemonSession& session, session::MixerState& mixer,
                                    MixerEditHistory& history) {
    if (!session.canMutateProject()) {
        return denied("MCP edit capability is required");
    }

    return history.redo(mixer);
}

EditToolResult previewPluginCommand(const DaemonSession& session,
                                    const commands::PluginInsertChainStore& store,
                                    const PluginEditCommand& command) {
    if (!session.canMutateProject()) {
        return denied("MCP edit capability is required");
    }

    return previewStoreCommand(store, command, false, false);
}

EditToolResult applyPluginCommand(const DaemonSession& session,
                                  commands::PluginInsertChainStore& store,
                                  PluginEditHistory& history, PluginEditCommand command) {
    if (!session.canMutateProject()) {
        return denied("MCP edit capability is required");
    }

    return history.execute(store, std::move(command));
}

EditToolResult undoLastPluginCommand(const DaemonSession& session,
                                     commands::PluginInsertChainStore& store,
                                     PluginEditHistory& history) {
    if (!session.canMutateProject()) {
        return denied("MCP edit capability is required");
    }

    return history.undo(store);
}

EditToolResult redoLastPluginCommand(const DaemonSession& session,
                                     commands::PluginInsertChainStore& store,
                                     PluginEditHistory& history) {
    if (!session.canMutateProject()) {
        return denied("MCP edit capability is required");
    }

    return history.redo(store);
}

std::string editToolResultJson(const EditToolResult& result) {
    std::ostringstream output;
    output << "{\"schemaVersion\":1,\"commandId\":\"" << escapeJson(result.commandId)
           << "\",\"auditId\":\"" << escapeJson(result.auditId)
           << "\",\"validationOk\":" << (result.validationOk ? "true" : "false")
           << ",\"applied\":" << (result.applied ? "true" : "false")
           << ",\"undoAvailable\":" << (result.undoAvailable ? "true" : "false")
           << ",\"redoAvailable\":" << (result.redoAvailable ? "true" : "false")
           << ",\"confirmationRequired\":" << (result.confirmationRequired ? "true" : "false")
           << ",\"confirmationToken\":\"" << escapeJson(result.confirmationToken) << "\""
           << ",\"message\":\"" << escapeJson(result.message) << "\",\"preview\":\""
           << escapeJson(result.preview) << "\"}";
    return output.str();
}

} // namespace lamusica::mcp_bridge
