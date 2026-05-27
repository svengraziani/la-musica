#include "lamusica/mcp_bridge/Orchestration.hpp"

#include "lamusica/commands/Command.hpp"

#include <algorithm>
#include <functional>
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

} // namespace

bool WorkflowTemplateLibrary::addTemplate(WorkflowTemplate workflowTemplate) {
    if (workflowTemplate.id.empty() || find(workflowTemplate.id) != nullptr) {
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

} // namespace lamusica::mcp_bridge
