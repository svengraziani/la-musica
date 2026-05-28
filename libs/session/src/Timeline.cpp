#include "lamusica/session/Timeline.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iterator>
#include <limits>
#include <stdexcept>

namespace lamusica::session {
namespace {

void considerCandidate(std::int64_t candidate, std::int64_t sample, std::int64_t& bestCandidate,
                       std::int64_t& bestDistance) {
    const auto distance = std::llabs(candidate - sample);
    if (distance < bestDistance) {
        bestCandidate = candidate;
        bestDistance = distance;
    }
}

double sampleToX(std::int64_t sample, TimelineRange visibleRange,
                 const TimelineLayoutOptions& options) {
    const auto visibleLength = std::max<std::int64_t>(1, visibleRange.length());
    const auto timelineWidth = std::max(1.0, options.viewportWidth - options.headerWidth);
    const auto offset = static_cast<double>(sample - visibleRange.startSample);
    return options.headerWidth + (offset / static_cast<double>(visibleLength)) * timelineWidth;
}

bool intersects(TimelineRange left, TimelineRange right) noexcept {
    return left.startSample < right.endSample && left.endSample > right.startSample;
}

} // namespace

std::int64_t TimelineRange::length() const noexcept {
    return endSample - startSample;
}

bool TimelineRange::contains(std::int64_t sample) const noexcept {
    return sample >= startSample && sample < endSample;
}

bool TimelineSelection::empty() const noexcept {
    return trackIds.empty() && clipIds.empty() && !range.has_value();
}

TimelineRange normalizedRange(std::int64_t anchorSample, std::int64_t focusSample) noexcept {
    anchorSample = std::max<std::int64_t>(0, anchorSample);
    focusSample = std::max<std::int64_t>(0, focusSample);
    return {.startSample = std::min(anchorSample, focusSample),
            .endSample = std::max(anchorSample, focusSample)};
}

bool selectionReferencesExistingItems(const TimelineSelection& selection,
                                      const ProjectManifest& manifest) noexcept {
    for (const auto& trackId : selection.trackIds) {
        if (std::ranges::none_of(manifest.tracks,
                                 [&trackId](const Track& track) { return track.id == trackId; })) {
            return false;
        }
    }
    for (const auto& clipId : selection.clipIds) {
        if (std::ranges::none_of(manifest.clips,
                                 [&clipId](const Clip& clip) { return clip.id == clipId; })) {
            return false;
        }
    }
    if (selection.range.has_value() &&
        (selection.range->startSample < 0 ||
         selection.range->endSample < selection.range->startSample)) {
        return false;
    }
    return true;
}

void setTimelineSelection(TimelineViewState& viewState, TimelineSelection selection,
                          const ProjectManifest& manifest) {
    if (!selectionReferencesExistingItems(selection, manifest)) {
        throw std::runtime_error("Timeline selection references missing or invalid items");
    }
    viewState.selection = std::move(selection);
}

void setTimelinePlayhead(TimelineViewState& viewState, std::int64_t sample) noexcept {
    viewState.playheadSample = std::max<std::int64_t>(0, sample);
}

void setTimelineLoopRegion(TimelineViewState& viewState, std::optional<TimelineRange> loopRegion) {
    if (loopRegion.has_value()) {
        if (loopRegion->startSample < 0 || loopRegion->endSample <= loopRegion->startSample) {
            throw std::runtime_error("Timeline loop region must be non-empty and non-negative");
        }
    }
    viewState.loopRegion = loopRegion;
}

void zoomTimelineAroundSample(TimelineViewState& viewState, double zoomFactor,
                              std::int64_t anchorSample) {
    if (!std::isfinite(zoomFactor) || zoomFactor <= 0.0) {
        throw std::runtime_error("Timeline zoom factor must be positive");
    }
    if (viewState.visibleRange.endSample <= viewState.visibleRange.startSample) {
        throw std::runtime_error("Timeline visible range must be non-empty");
    }

    constexpr std::int64_t minimumVisibleSamples = 64;
    anchorSample = std::max<std::int64_t>(0, anchorSample);
    const auto oldLength = viewState.visibleRange.length();
    const auto newLength = std::max<std::int64_t>(
        minimumVisibleSamples,
        static_cast<std::int64_t>(std::llround(static_cast<double>(oldLength) / zoomFactor)));
    const auto anchorOffset =
        std::clamp(anchorSample - viewState.visibleRange.startSample, std::int64_t{0}, oldLength);
    const double anchorRatio = static_cast<double>(anchorOffset) / static_cast<double>(oldLength);
    auto newStart = anchorSample - static_cast<std::int64_t>(std::llround(anchorRatio * newLength));
    newStart = std::max<std::int64_t>(0, newStart);
    viewState.visibleRange = {.startSample = newStart, .endSample = newStart + newLength};
    viewState.pixelsPerSecond = std::max(1.0, viewState.pixelsPerSecond * zoomFactor);
}

TimelineLayout buildTimelineLayout(const ProjectManifest& manifest,
                                   const TimelineViewState& viewState,
                                   TimelineLayoutOptions options) {
    if (options.viewportWidth <= options.headerWidth || options.headerWidth < 0.0 ||
        options.rulerHeight < 0.0 || options.trackHeight <= 0.0) {
        throw std::runtime_error("Timeline layout dimensions are invalid");
    }
    if (viewState.visibleRange.startSample < 0 ||
        viewState.visibleRange.endSample <= viewState.visibleRange.startSample) {
        throw std::runtime_error("Timeline visible range must be non-empty and non-negative");
    }

    TimelineLayout layout;
    const auto timelineWidth = options.viewportWidth - options.headerWidth;
    layout.rulerBounds = {
        .x = options.headerWidth, .y = 0.0, .width = timelineWidth, .height = options.rulerHeight};
    layout.playheadX = sampleToX(viewState.playheadSample, viewState.visibleRange, options);

    double laneY = options.rulerHeight;
    const auto tracks = orderedTimelineTracks(manifest);
    for (const auto& track : tracks) {
        layout.trackHeaders.push_back({.trackId = track.id,
                                       .name = track.name,
                                       .bounds = {.x = 0.0,
                                                  .y = laneY,
                                                  .width = options.headerWidth,
                                                  .height = options.trackHeight}});
        layout.lanes.push_back({.trackId = track.id,
                                .trackType = track.type,
                                .bounds = {.x = options.headerWidth,
                                           .y = laneY,
                                           .width = timelineWidth,
                                           .height = options.trackHeight}});
        laneY += options.trackHeight;
    }
    layout.contentHeight = laneY;

    for (const auto& clip : manifest.clips) {
        const auto lane =
            std::ranges::find_if(layout.lanes, [&clip](const TimelineLaneLayout& candidate) {
                return candidate.trackId == clip.trackId;
            });
        if (lane == layout.lanes.end()) {
            continue;
        }
        const TimelineRange clipRange{clip.startSample, clip.startSample + clip.lengthSamples};
        if (!intersects(clipRange, viewState.visibleRange)) {
            continue;
        }
        const auto visibleStart =
            std::max(clipRange.startSample, viewState.visibleRange.startSample);
        const auto visibleEnd = std::min(clipRange.endSample, viewState.visibleRange.endSample);
        const auto left = sampleToX(visibleStart, viewState.visibleRange, options);
        const auto right = sampleToX(visibleEnd, viewState.visibleRange, options);
        const auto selected = std::ranges::any_of(
            viewState.selection.clipIds, [&clip](const auto& id) { return id == clip.id; });
        layout.clips.push_back({.clipId = clip.id,
                                .trackId = clip.trackId,
                                .clipType = clip.type,
                                .bounds = {.x = left,
                                           .y = lane->bounds.y,
                                           .width = std::max(1.0, right - left),
                                           .height = lane->bounds.height},
                                .selected = selected});
    }

    for (const auto& marker : manifest.markers) {
        if (!viewState.visibleRange.contains(marker.samplePosition)) {
            continue;
        }
        layout.markers.push_back(
            {.markerId = marker.id,
             .name = marker.name,
             .x = sampleToX(marker.samplePosition, viewState.visibleRange, options)});
    }

    const auto tickGrid = viewState.snap.beatGridSamples > 0 ? viewState.snap.beatGridSamples
                                                             : viewState.snap.sampleGrid;
    if (tickGrid > 0) {
        auto tickSample = (viewState.visibleRange.startSample / tickGrid) * tickGrid;
        if (tickSample < viewState.visibleRange.startSample) {
            tickSample += tickGrid;
        }
        for (; tickSample < viewState.visibleRange.endSample; tickSample += tickGrid) {
            const auto tickIndex = tickSample / tickGrid;
            layout.rulerTicks.push_back(
                {.samplePosition = tickSample,
                 .x = sampleToX(tickSample, viewState.visibleRange, options),
                 .major = tickIndex % 4 == 0});
        }
    }

    if (viewState.loopRegion.has_value() &&
        intersects(*viewState.loopRegion, viewState.visibleRange)) {
        const auto visibleStart =
            std::max(viewState.loopRegion->startSample, viewState.visibleRange.startSample);
        const auto visibleEnd =
            std::min(viewState.loopRegion->endSample, viewState.visibleRange.endSample);
        const auto left = sampleToX(visibleStart, viewState.visibleRange, options);
        const auto right = sampleToX(visibleEnd, viewState.visibleRange, options);
        layout.loopRegion =
            TimelineRect{.x = left,
                         .y = options.rulerHeight,
                         .width = std::max(1.0, right - left),
                         .height = std::max(0.0, layout.contentHeight - options.rulerHeight)};
    }

    return layout;
}

std::int64_t snapSample(std::int64_t sample, const SnapSettings& settings,
                        const ProjectManifest& manifest) {
    if (!settings.enabled) {
        return sample;
    }

    std::int64_t bestCandidate = sample;
    std::int64_t bestDistance = std::numeric_limits<std::int64_t>::max();

    if (settings.sampleGrid > 0) {
        const auto lower = (sample / settings.sampleGrid) * settings.sampleGrid;
        considerCandidate(lower, sample, bestCandidate, bestDistance);
        considerCandidate(lower + settings.sampleGrid, sample, bestCandidate, bestDistance);
    }

    if (settings.beatGridSamples > 0) {
        const auto lower = (sample / settings.beatGridSamples) * settings.beatGridSamples;
        considerCandidate(lower, sample, bestCandidate, bestDistance);
        considerCandidate(lower + settings.beatGridSamples, sample, bestCandidate, bestDistance);
    }

    if (settings.snapToMarkers) {
        for (const auto& marker : manifest.markers) {
            considerCandidate(marker.samplePosition, sample, bestCandidate, bestDistance);
        }
    }

    if (settings.snapToTransients) {
        for (const auto transientSample : settings.transientSamples) {
            considerCandidate(transientSample, sample, bestCandidate, bestDistance);
        }
    }

    if (settings.snapToClips) {
        for (const auto& clip : manifest.clips) {
            considerCandidate(clip.startSample, sample, bestCandidate, bestDistance);
            considerCandidate(clip.startSample + clip.lengthSamples, sample, bestCandidate,
                              bestDistance);
        }
    }

    return bestCandidate;
}

std::vector<Track> orderedTimelineTracks(const ProjectManifest& manifest) {
    return manifest.tracks;
}

std::vector<Clip> clipsOnTrack(const ProjectManifest& manifest, std::string_view trackId) {
    std::vector<Clip> clips;
    std::ranges::copy_if(manifest.clips, std::back_inserter(clips),
                         [trackId](const Clip& clip) { return clip.trackId == trackId; });
    std::ranges::sort(clips, {}, &Clip::startSample);
    return clips;
}

void addColorLabel(TimelineOrganization& organization, TimelineColorLabel label) {
    if (label.id.empty() || label.name.empty() || label.color.empty()) {
        throw std::runtime_error("Timeline color label id, name, and color are required");
    }
    if (std::ranges::any_of(organization.colorLabels, [&label](const TimelineColorLabel& existing) {
            return existing.id == label.id;
        })) {
        throw std::runtime_error("Timeline color label id already exists");
    }
    organization.colorLabels.push_back(std::move(label));
}

void addTrackFolder(TimelineOrganization& organization, const ProjectManifest& manifest,
                    TrackFolder folder) {
    if (folder.id.empty() || folder.name.empty()) {
        throw std::runtime_error("Track folder id and name are required");
    }
    if (folder.trackIds.empty()) {
        throw std::runtime_error("Track folder must contain at least one track");
    }
    if (std::ranges::any_of(organization.trackFolders, [&folder](const TrackFolder& existing) {
            return existing.id == folder.id;
        })) {
        throw std::runtime_error("Track folder id already exists");
    }
    for (const auto& trackId : folder.trackIds) {
        if (std::ranges::none_of(manifest.tracks,
                                 [&trackId](const Track& track) { return track.id == trackId; })) {
            throw std::runtime_error("Track folder references missing track: " + trackId);
        }
    }
    if (!folder.colorLabelId.empty() &&
        std::ranges::none_of(organization.colorLabels, [&folder](const TimelineColorLabel& label) {
            return label.id == folder.colorLabelId;
        })) {
        throw std::runtime_error("Track folder references missing color label: " +
                                 folder.colorLabelId);
    }
    organization.trackFolders.push_back(std::move(folder));
}

void addArrangerSection(TimelineOrganization& organization, ArrangerSection section) {
    if (section.id.empty() || section.name.empty()) {
        throw std::runtime_error("Arranger section id and name are required");
    }
    if (section.range.startSample < 0 || section.range.endSample <= section.range.startSample) {
        throw std::runtime_error("Arranger section range must be non-empty and non-negative");
    }
    if (std::ranges::any_of(
            organization.arrangerSections,
            [&section](const ArrangerSection& existing) { return existing.id == section.id; })) {
        throw std::runtime_error("Arranger section id already exists");
    }
    if (!section.colorLabelId.empty() &&
        std::ranges::none_of(organization.colorLabels, [&section](const TimelineColorLabel& label) {
            return label.id == section.colorLabelId;
        })) {
        throw std::runtime_error("Arranger section references missing color label: " +
                                 section.colorLabelId);
    }
    organization.arrangerSections.push_back(std::move(section));
    std::ranges::sort(organization.arrangerSections, {},
                      [](const ArrangerSection& candidate) { return candidate.range.startSample; });
}

std::vector<Track> tracksInFolder(const ProjectManifest& manifest,
                                  const TimelineOrganization& organization,
                                  std::string_view folderId) {
    const auto folder =
        std::ranges::find_if(organization.trackFolders, [folderId](const TrackFolder& candidate) {
            return candidate.id == folderId;
        });
    if (folder == organization.trackFolders.end()) {
        return {};
    }

    std::vector<Track> tracks;
    for (const auto& trackId : folder->trackIds) {
        const auto track =
            std::ranges::find_if(manifest.tracks, [&trackId](const Track& candidate) {
                return candidate.id == trackId;
            });
        if (track != manifest.tracks.end()) {
            tracks.push_back(*track);
        }
    }
    return tracks;
}

std::vector<ArrangerSection> sectionsIntersecting(const TimelineOrganization& organization,
                                                  TimelineRange range) {
    std::vector<ArrangerSection> sections;
    std::ranges::copy_if(organization.arrangerSections, std::back_inserter(sections),
                         [range](const ArrangerSection& section) {
                             return section.range.startSample < range.endSample &&
                                    section.range.endSample > range.startSample;
                         });
    return sections;
}

} // namespace lamusica::session
