#pragma once

#include "lamusica/session/ProjectManifest.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace lamusica::session {

enum class TimelineTool {
    Select,
    Range,
    Cut,
    Draw,
    Trim,
    Split,
    Slip,
    Fade,
    Mute,
    Zoom,
};

struct TimelineRange {
    std::int64_t startSample{0};
    std::int64_t endSample{0};

    [[nodiscard]] std::int64_t length() const noexcept;
    [[nodiscard]] bool contains(std::int64_t sample) const noexcept;
};

struct TimelineSelection {
    std::vector<std::string> trackIds;
    std::vector<std::string> clipIds;
    std::optional<TimelineRange> range;

    [[nodiscard]] bool empty() const noexcept;
};

struct TimelineColorLabel {
    std::string id;
    std::string name;
    std::string color;
};

struct TrackFolder {
    std::string id;
    std::string name;
    std::vector<std::string> trackIds;
    std::string colorLabelId;
    bool collapsed{false};
};

struct ArrangerSection {
    std::string id;
    std::string name;
    TimelineRange range;
    std::string colorLabelId;
};

struct TimelineOrganization {
    std::vector<TimelineColorLabel> colorLabels;
    std::vector<TrackFolder> trackFolders;
    std::vector<ArrangerSection> arrangerSections;
};

struct SnapSettings {
    bool enabled{true};
    std::int64_t sampleGrid{1};
    std::int64_t beatGridSamples{24000};
    bool snapToClips{true};
    bool snapToMarkers{true};
    bool snapToTransients{true};
    std::vector<std::int64_t> transientSamples;
};

struct TimelineViewState {
    TimelineRange visibleRange{0, 48000 * 16};
    std::int64_t playheadSample{0};
    std::optional<TimelineRange> loopRegion;
    double pixelsPerSecond{120.0};
    TimelineTool activeTool{TimelineTool::Select};
    TimelineSelection selection;
    SnapSettings snap;
};

[[nodiscard]] TimelineRange normalizedRange(std::int64_t anchorSample,
                                            std::int64_t focusSample) noexcept;
[[nodiscard]] bool selectionReferencesExistingItems(const TimelineSelection& selection,
                                                    const ProjectManifest& manifest) noexcept;
void setTimelineSelection(TimelineViewState& viewState, TimelineSelection selection,
                          const ProjectManifest& manifest);
void setTimelinePlayhead(TimelineViewState& viewState, std::int64_t sample) noexcept;
void setTimelineLoopRegion(TimelineViewState& viewState, std::optional<TimelineRange> loopRegion);
void zoomTimelineAroundSample(TimelineViewState& viewState, double zoomFactor,
                              std::int64_t anchorSample);
[[nodiscard]] std::int64_t snapSample(std::int64_t sample, const SnapSettings& settings,
                                      const ProjectManifest& manifest);
[[nodiscard]] std::vector<Track> orderedTimelineTracks(const ProjectManifest& manifest);
[[nodiscard]] std::vector<Clip> clipsOnTrack(const ProjectManifest& manifest,
                                             std::string_view trackId);
void addColorLabel(TimelineOrganization& organization, TimelineColorLabel label);
void addTrackFolder(TimelineOrganization& organization, const ProjectManifest& manifest,
                    TrackFolder folder);
void addArrangerSection(TimelineOrganization& organization, ArrangerSection section);
[[nodiscard]] std::vector<Track> tracksInFolder(const ProjectManifest& manifest,
                                                const TimelineOrganization& organization,
                                                std::string_view folderId);
[[nodiscard]] std::vector<ArrangerSection>
sectionsIntersecting(const TimelineOrganization& organization, TimelineRange range);

} // namespace lamusica::session
