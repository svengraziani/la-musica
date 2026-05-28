#pragma once

#include "lamusica/commands/Command.hpp"
#include "lamusica/mcp_bridge/DaemonSession.hpp"
#include "lamusica/session/Midi.hpp"
#include "lamusica/session/Mixer.hpp"
#include "lamusica/session/Pattern.hpp"
#include "lamusica/session/ProjectManifest.hpp"

#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace lamusica::mcp_bridge {

enum class WorkflowStepStatus {
    Pending,
    Approved,
    Rejected,
    Applied,
};

struct WorkflowStep {
    std::string id;
    std::string description;
    std::string commandName;
    std::string commandPreview;
    bool validationOk{true};
    std::string validationMessage;
    WorkflowStepStatus status{WorkflowStepStatus::Pending};
};

struct WorkflowPlan {
    std::string id;
    std::string name;
    std::uint32_t seed{0};
    std::vector<WorkflowStep> steps;
};

struct WorkflowTemplate {
    std::string id;
    std::string name;
    std::string description;
    std::string workflowType;
};

struct WorkflowPlanReview {
    bool approveAllValid{false};
    std::vector<std::string> approvedStepIds;
    std::vector<std::string> rejectedStepIds;
};

struct WorkflowPlanApplicationSummary {
    std::size_t appliedCount{0};
    std::size_t rejectedCount{0};
    std::size_t skippedCount{0};
    std::size_t invalidCount{0};
    std::vector<std::string> appliedStepIds;
    std::vector<std::string> rejectedStepIds;
    std::vector<std::string> skippedStepIds;
    std::vector<std::string> invalidStepIds;
};

struct WorkflowPlanCreationResult {
    bool allowed{false};
    std::string message;
    WorkflowPlan plan;
};

class WorkflowTemplateLibrary {
  public:
    [[nodiscard]] bool addTemplate(WorkflowTemplate workflowTemplate);
    [[nodiscard]] const WorkflowTemplate* find(std::string_view templateId) const noexcept;
    [[nodiscard]] const std::vector<WorkflowTemplate>& templates() const noexcept;

  private:
    std::vector<WorkflowTemplate> templates_;
};

[[nodiscard]] WorkflowPlan createHarmonizeMidiPlan(const session::MidiClipData& clip,
                                                   int intervalSemitones, std::uint32_t seed);
[[nodiscard]] WorkflowPlanCreationResult createHarmonizeMidiPlan(const DaemonSession& session,
                                                                 const session::MidiClipData& clip,
                                                                 int intervalSemitones,
                                                                 std::uint32_t seed);
[[nodiscard]] WorkflowPlan createDrumVariationPlan(const session::PatternClip& pattern,
                                                   std::uint32_t seedOffset);
[[nodiscard]] WorkflowPlanCreationResult
createDrumVariationPlan(const DaemonSession& session, const session::PatternClip& pattern,
                        std::uint32_t seedOffset);
[[nodiscard]] WorkflowPlan
createSongStructureLabelPlan(const session::ProjectManifest& manifest,
                             const std::vector<std::pair<std::string, std::string>>& trackLabels,
                             std::uint32_t seed);
[[nodiscard]] WorkflowPlanCreationResult
createSongStructureLabelPlan(const DaemonSession& session, const session::ProjectManifest& manifest,
                             const std::vector<std::pair<std::string, std::string>>& trackLabels,
                             std::uint32_t seed);
[[nodiscard]] WorkflowPlan createMixPreparationPlan(const session::MixerState& mixer,
                                                    std::uint32_t seed);
[[nodiscard]] WorkflowPlanCreationResult createMixPreparationPlan(const DaemonSession& session,
                                                                  const session::MixerState& mixer,
                                                                  std::uint32_t seed);
void approveStep(WorkflowPlan& plan, std::string_view stepId);
void rejectStep(WorkflowPlan& plan, std::string_view stepId);
void markStepApplied(WorkflowPlan& plan, std::string_view stepId);
void reviewWorkflowPlan(WorkflowPlan& plan, const WorkflowPlanReview& review);
[[nodiscard]] WorkflowPlanApplicationSummary applyApprovedWorkflowPlanSteps(
    WorkflowPlan& plan, const std::function<bool(const WorkflowStep&)>& applyPreviewedCommand);
[[nodiscard]] std::string workflowPlanJson(const WorkflowPlan& plan);
[[nodiscard]] std::string
workflowPlanApplicationSummaryJson(const WorkflowPlanApplicationSummary& summary);
[[nodiscard]] std::string workflowTemplateLibraryJson(const WorkflowTemplateLibrary& library);
[[nodiscard]] WorkflowTemplateLibrary parseWorkflowTemplateLibrary(std::string_view json);
void saveWorkflowTemplateLibrary(const WorkflowTemplateLibrary& library,
                                 const std::filesystem::path& path);
[[nodiscard]] WorkflowTemplateLibrary
loadWorkflowTemplateLibrary(const std::filesystem::path& path);

} // namespace lamusica::mcp_bridge
