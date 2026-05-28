#include "lamusica/mcp_bridge/Orchestration.hpp"

#include "lamusica/commands/Command.hpp"

#include <algorithm>
#include <fstream>
#include <functional>
#include <sstream>
#include <stdexcept>
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

std::string_view toString(WorkflowStepStatus status) noexcept {
    switch (status) {
    case WorkflowStepStatus::Pending:
        return "pending";
    case WorkflowStepStatus::Approved:
        return "approved";
    case WorkflowStepStatus::Rejected:
        return "rejected";
    case WorkflowStepStatus::Applied:
        return "applied";
    }
    return "pending";
}

WorkflowStep* findStep(WorkflowPlan& plan, std::string_view stepId) {
    const auto found = std::ranges::find_if(
        plan.steps, [stepId](const WorkflowStep& step) { return step.id == stepId; });
    return found == plan.steps.end() ? nullptr : &*found;
}

void appendJsonStringArray(std::ostringstream& output, std::string_view key,
                           const std::vector<std::string>& values) {
    output << ",\"" << key << "\":[";
    for (std::size_t index = 0; index < values.size(); ++index) {
        output << '"' << escapeJson(values[index]) << '"';
        if (index + 1 < values.size()) {
            output << ',';
        }
    }
    output << ']';
}

std::string readJsonString(std::string_view json, std::string_view key) {
    const auto pattern = "\"" + std::string{key} + "\":\"";
    const auto start = json.find(pattern);
    if (start == std::string_view::npos) {
        throw std::runtime_error("Workflow template JSON is missing field: " + std::string{key});
    }
    const auto valueStart = start + pattern.size();
    std::string value;
    bool escaped = false;
    for (std::size_t index = valueStart; index < json.size(); ++index) {
        const auto character = json[index];
        if (escaped) {
            value.push_back(character);
            escaped = false;
            continue;
        }
        if (character == '\\') {
            escaped = true;
            continue;
        }
        if (character == '"') {
            return value;
        }
        value.push_back(character);
    }
    throw std::runtime_error("Workflow template JSON string is unterminated: " + std::string{key});
}

std::vector<std::string_view> templateObjects(std::string_view json) {
    constexpr std::string_view pattern{"\"templates\":["};
    const auto start = json.find(pattern);
    if (start == std::string_view::npos) {
        throw std::runtime_error("Workflow template JSON is missing templates array");
    }
    std::vector<std::string_view> objects;
    const auto arrayStart = start + pattern.size();
    std::size_t objectStart = std::string_view::npos;
    std::size_t depth = 0;
    bool inString = false;
    bool escaped = false;
    for (std::size_t index = arrayStart; index < json.size(); ++index) {
        const auto character = json[index];
        if (inString) {
            if (escaped) {
                escaped = false;
            } else if (character == '\\') {
                escaped = true;
            } else if (character == '"') {
                inString = false;
            }
            continue;
        }
        if (character == '"') {
            inString = true;
            continue;
        }
        if (character == '{') {
            if (depth == 0) {
                objectStart = index;
            }
            ++depth;
            continue;
        }
        if (character == '}') {
            if (depth == 0) {
                throw std::runtime_error("Workflow template JSON has unbalanced braces");
            }
            --depth;
            if (depth == 0) {
                objects.push_back(json.substr(objectStart, index - objectStart + 1U));
            }
            continue;
        }
        if (character == ']' && depth == 0) {
            return objects;
        }
    }
    throw std::runtime_error("Workflow template JSON templates array is unterminated");
}

WorkflowStep validatedStep(std::string id, std::string description,
                           const commands::ICommand& command,
                           const session::ProjectManifest& manifest) {
    const auto validation = command.validate(manifest);
    return {.id = std::move(id),
            .description = std::move(description),
            .commandName = command.metadata().name,
            .commandPreview = command.preview(manifest),
            .validationOk = validation.ok,
            .validationMessage = validation.message};
}

WorkflowStep validatedStep(std::string id, std::string description,
                           const commands::SetChannelMixCommand& command,
                           const session::MixerState& mixer) {
    const auto validation = command.validate(mixer);
    return {.id = std::move(id),
            .description = std::move(description),
            .commandName = command.metadata().name,
            .commandPreview = command.preview(mixer),
            .validationOk = validation.ok,
            .validationMessage = validation.message};
}

bool canCreateWorkflowPlan(const DaemonSession& session) noexcept {
    return session.attached() && session.hasCapability(Capability::Orchestration);
}

WorkflowPlanCreationResult deniedWorkflowPlan(std::string message) {
    return {.allowed = false, .message = std::move(message)};
}

} // namespace

bool WorkflowTemplateLibrary::addTemplate(WorkflowTemplate workflowTemplate) {
    if (workflowTemplate.id.empty() || workflowTemplate.name.empty() ||
        workflowTemplate.workflowType.empty() || find(workflowTemplate.id) != nullptr) {
        return false;
    }

    templates_.push_back(std::move(workflowTemplate));
    return true;
}

const WorkflowTemplate* WorkflowTemplateLibrary::find(std::string_view templateId) const noexcept {
    const auto found = std::ranges::find_if(templates_, [templateId](const auto& workflowTemplate) {
        return workflowTemplate.id == templateId;
    });
    return found == templates_.end() ? nullptr : &*found;
}

const std::vector<WorkflowTemplate>& WorkflowTemplateLibrary::templates() const noexcept {
    return templates_;
}

WorkflowPlan createHarmonizeMidiPlan(const session::MidiClipData& clip, int intervalSemitones,
                                     std::uint32_t seed) {
    WorkflowPlan plan{.id = "harmonize-" + clip.clipId, .name = "Harmonize MIDI", .seed = seed};
    auto notes = session::notesInPlaybackOrder(clip);
    for (std::size_t index = 0; index < notes.size(); ++index) {
        const auto& note = notes[index];
        plan.steps.push_back(
            {.id = "harmonize-note-" + std::to_string(index),
             .description = "Add harmony for note " + note.id,
             .commandName = "add_midi_note",
             .commandPreview = "Add MIDI note pitch " +
                               std::to_string(static_cast<int>(note.pitch) + intervalSemitones),
             .validationOk = static_cast<int>(note.pitch) + intervalSemitones >= 0 &&
                             static_cast<int>(note.pitch) + intervalSemitones <= 127,
             .validationMessage = static_cast<int>(note.pitch) + intervalSemitones >= 0 &&
                                          static_cast<int>(note.pitch) + intervalSemitones <= 127
                                      ? "MIDI note can be added"
                                      : "Harmony pitch would be outside 0-127"});
    }
    return plan;
}

WorkflowPlanCreationResult createHarmonizeMidiPlan(const DaemonSession& session,
                                                   const session::MidiClipData& clip,
                                                   int intervalSemitones, std::uint32_t seed) {
    if (!canCreateWorkflowPlan(session)) {
        return deniedWorkflowPlan("MCP orchestration capability is required");
    }
    return {.allowed = true,
            .message = "workflow plan created",
            .plan = createHarmonizeMidiPlan(clip, intervalSemitones, seed)};
}

WorkflowPlan createDrumVariationPlan(const session::PatternClip& pattern,
                                     std::uint32_t seedOffset) {
    const auto variation = session::duplicatePatternVariation(
        pattern, pattern.id + "-variation", pattern.name + " Variation", seedOffset);
    return {.id = "drum-variation-" + pattern.id,
            .name = "Create Drum Variation",
            .seed = variation.seed,
            .steps = {{.id = "duplicate-pattern",
                       .description = "Duplicate pattern as variation",
                       .commandName = "duplicate_pattern_variation",
                       .commandPreview = "Create pattern " + variation.id},
                      {.id = "set-variation-seed",
                       .description = "Set deterministic variation seed",
                       .commandName = "set_pattern_seed",
                       .commandPreview = "Set seed " + std::to_string(variation.seed)}}};
}

WorkflowPlanCreationResult createDrumVariationPlan(const DaemonSession& session,
                                                   const session::PatternClip& pattern,
                                                   std::uint32_t seedOffset) {
    if (!canCreateWorkflowPlan(session)) {
        return deniedWorkflowPlan("MCP orchestration capability is required");
    }
    return {.allowed = true,
            .message = "workflow plan created",
            .plan = createDrumVariationPlan(pattern, seedOffset)};
}

WorkflowPlan
createSongStructureLabelPlan(const session::ProjectManifest& manifest,
                             const std::vector<std::pair<std::string, std::string>>& trackLabels,
                             std::uint32_t seed) {
    WorkflowPlan plan{.id = "song-structure-labels", .name = "Label Song Structure", .seed = seed};
    for (std::size_t index = 0; index < trackLabels.size(); ++index) {
        const auto& [trackId, label] = trackLabels[index];
        const commands::SetTrackNameCommand command{"workflow-label-" + std::to_string(index),
                                                    "workflow-label-audit-" + std::to_string(index),
                                                    trackId, label};
        plan.steps.push_back(validatedStep("label-track-" + std::to_string(index),
                                           "Label track " + trackId, command, manifest));
    }
    return plan;
}

WorkflowPlanCreationResult
createSongStructureLabelPlan(const DaemonSession& session, const session::ProjectManifest& manifest,
                             const std::vector<std::pair<std::string, std::string>>& trackLabels,
                             std::uint32_t seed) {
    if (!canCreateWorkflowPlan(session)) {
        return deniedWorkflowPlan("MCP orchestration capability is required");
    }
    return {.allowed = true,
            .message = "workflow plan created",
            .plan = createSongStructureLabelPlan(manifest, trackLabels, seed)};
}

WorkflowPlan createMixPreparationPlan(const session::MixerState& mixer, std::uint32_t seed) {
    WorkflowPlan plan{.id = "mix-preparation", .name = "Create Mix Preparation Pass", .seed = seed};
    for (std::size_t index = 0; index < mixer.channels.size(); ++index) {
        const auto& channel = mixer.channels[index];
        commands::ChannelMixSettings settings;
        if (channel.type != session::ChannelType::Master && channel.volumeDb > -6.0F) {
            settings.volumeDb = -6.0F;
        }
        if (channel.pan < -1.0F || channel.pan > 1.0F) {
            settings.pan = std::clamp(channel.pan, -1.0F, 1.0F);
        }
        if (!settings.volumeDb.has_value() && !settings.pan.has_value()) {
            settings.volumeDb = channel.volumeDb;
        }

        const commands::SetChannelMixCommand command{"workflow-mix-" + std::to_string(index),
                                                     "workflow-mix-audit-" + std::to_string(index),
                                                     channel.id, settings};
        plan.steps.push_back(validatedStep("mix-channel-" + std::to_string(index),
                                           "Prepare channel " + channel.id, command, mixer));
    }
    return plan;
}

WorkflowPlanCreationResult createMixPreparationPlan(const DaemonSession& session,
                                                    const session::MixerState& mixer,
                                                    std::uint32_t seed) {
    if (!canCreateWorkflowPlan(session)) {
        return deniedWorkflowPlan("MCP orchestration capability is required");
    }
    return {.allowed = true,
            .message = "workflow plan created",
            .plan = createMixPreparationPlan(mixer, seed)};
}

void approveStep(WorkflowPlan& plan, std::string_view stepId) {
    if (auto* step = findStep(plan, stepId);
        step != nullptr && step->status == WorkflowStepStatus::Pending) {
        step->status = WorkflowStepStatus::Approved;
    }
}

void rejectStep(WorkflowPlan& plan, std::string_view stepId) {
    if (auto* step = findStep(plan, stepId);
        step != nullptr && step->status == WorkflowStepStatus::Pending) {
        step->status = WorkflowStepStatus::Rejected;
    }
}

void markStepApplied(WorkflowPlan& plan, std::string_view stepId) {
    if (auto* step = findStep(plan, stepId);
        step != nullptr && step->status == WorkflowStepStatus::Approved && step->validationOk) {
        step->status = WorkflowStepStatus::Applied;
    }
}

void reviewWorkflowPlan(WorkflowPlan& plan, const WorkflowPlanReview& review) {
    if (review.approveAllValid) {
        for (auto& step : plan.steps) {
            if (step.status == WorkflowStepStatus::Pending && step.validationOk) {
                step.status = WorkflowStepStatus::Approved;
            }
        }
    }
    for (const auto& stepId : review.approvedStepIds) {
        approveStep(plan, stepId);
    }
    for (const auto& stepId : review.rejectedStepIds) {
        rejectStep(plan, stepId);
    }
}

WorkflowPlanApplicationSummary applyApprovedWorkflowPlanSteps(
    WorkflowPlan& plan, const std::function<bool(const WorkflowStep&)>& applyPreviewedCommand) {
    WorkflowPlanApplicationSummary summary;
    for (auto& step : plan.steps) {
        if (step.status == WorkflowStepStatus::Rejected) {
            ++summary.rejectedCount;
            summary.rejectedStepIds.push_back(step.id);
            continue;
        }
        if (step.status != WorkflowStepStatus::Approved) {
            ++summary.skippedCount;
            summary.skippedStepIds.push_back(step.id);
            continue;
        }
        if (!step.validationOk) {
            ++summary.invalidCount;
            summary.invalidStepIds.push_back(step.id);
            continue;
        }
        if (applyPreviewedCommand(step)) {
            step.status = WorkflowStepStatus::Applied;
            ++summary.appliedCount;
            summary.appliedStepIds.push_back(step.id);
        } else {
            ++summary.skippedCount;
            summary.skippedStepIds.push_back(step.id);
        }
    }
    return summary;
}

std::string workflowPlanJson(const WorkflowPlan& plan) {
    std::ostringstream output;
    output << "{\"schemaVersion\":1,\"workflowId\":\"" << escapeJson(plan.id) << "\",\"name\":\""
           << escapeJson(plan.name) << "\",\"seed\":" << plan.seed << ",\"steps\":[";
    for (std::size_t index = 0; index < plan.steps.size(); ++index) {
        const auto& step = plan.steps[index];
        output << "{\"id\":\"" << escapeJson(step.id) << "\",\"status\":\"" << toString(step.status)
               << "\",\"commandName\":\"" << escapeJson(step.commandName) << "\",\"description\":\""
               << escapeJson(step.description) << "\",\"commandPreview\":\""
               << escapeJson(step.commandPreview)
               << "\",\"validationOk\":" << (step.validationOk ? "true" : "false")
               << ",\"validationMessage\":\"" << escapeJson(step.validationMessage) << "\"}";
        if (index + 1 < plan.steps.size()) {
            output << ',';
        }
    }
    output << "]}";
    return output.str();
}

std::string workflowPlanApplicationSummaryJson(const WorkflowPlanApplicationSummary& summary) {
    std::ostringstream output;
    output << "{\"schemaVersion\":1,\"appliedCount\":" << summary.appliedCount
           << ",\"rejectedCount\":" << summary.rejectedCount
           << ",\"skippedCount\":" << summary.skippedCount
           << ",\"invalidCount\":" << summary.invalidCount;
    appendJsonStringArray(output, "appliedStepIds", summary.appliedStepIds);
    appendJsonStringArray(output, "rejectedStepIds", summary.rejectedStepIds);
    appendJsonStringArray(output, "skippedStepIds", summary.skippedStepIds);
    appendJsonStringArray(output, "invalidStepIds", summary.invalidStepIds);
    output << '}';
    return output.str();
}

std::string workflowTemplateLibraryJson(const WorkflowTemplateLibrary& library) {
    std::ostringstream output;
    output << "{\"schemaVersion\":1,\"templates\":[";
    const auto& templates = library.templates();
    for (std::size_t index = 0; index < templates.size(); ++index) {
        const auto& workflowTemplate = templates[index];
        output << "{\"id\":\"" << escapeJson(workflowTemplate.id) << "\",\"name\":\""
               << escapeJson(workflowTemplate.name) << "\",\"description\":\""
               << escapeJson(workflowTemplate.description) << "\",\"workflowType\":\""
               << escapeJson(workflowTemplate.workflowType) << "\"}";
        if (index + 1 < templates.size()) {
            output << ',';
        }
    }
    output << "]}";
    return output.str();
}

WorkflowTemplateLibrary parseWorkflowTemplateLibrary(std::string_view json) {
    if (json.find("\"schemaVersion\":1") == std::string_view::npos) {
        throw std::runtime_error("Workflow template library schemaVersion must be 1");
    }

    WorkflowTemplateLibrary library;
    for (const auto object : templateObjects(json)) {
        if (!library.addTemplate({.id = readJsonString(object, "id"),
                                  .name = readJsonString(object, "name"),
                                  .description = readJsonString(object, "description"),
                                  .workflowType = readJsonString(object, "workflowType")})) {
            throw std::runtime_error("Workflow template library contains an invalid template");
        }
    }
    return library;
}

void saveWorkflowTemplateLibrary(const WorkflowTemplateLibrary& library,
                                 const std::filesystem::path& path) {
    std::ofstream output{path};
    if (!output) {
        throw std::runtime_error("Could not open workflow template library for writing: " +
                                 path.string());
    }
    output << workflowTemplateLibraryJson(library) << '\n';
}

WorkflowTemplateLibrary loadWorkflowTemplateLibrary(const std::filesystem::path& path) {
    std::ifstream input{path};
    if (!input) {
        throw std::runtime_error("Could not open workflow template library for reading: " +
                                 path.string());
    }
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return parseWorkflowTemplateLibrary(buffer.str());
}

} // namespace lamusica::mcp_bridge
