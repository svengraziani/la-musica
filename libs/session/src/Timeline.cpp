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
