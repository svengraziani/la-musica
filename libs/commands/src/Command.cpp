#include "lamusica/commands/Command.hpp"

#include <algorithm>
#include <charconv>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace lamusica::commands {
namespace {

CommandResult ok(std::string message) {
    return {.ok = true, .message = std::move(message)};
}

CommandResult fail(std::string message) {
    return {.ok = false, .message = std::move(message)};
}

bool containsTrackId(const session::ProjectManifest& manifest, std::string_view trackId) {
    return std::ranges::any_of(
        manifest.tracks, [trackId](const session::Track& track) { return track.id == trackId; });
}

bool containsClipId(const session::ProjectManifest& manifest, std::string_view clipId) {
    return std::ranges::any_of(manifest.clips,
                               [clipId](const session::Clip& clip) { return clip.id == clipId; });
}

session::Clip* findClip(session::ProjectManifest& manifest, std::string_view clipId) {
    const auto found = std::ranges::find_if(
        manifest.clips, [clipId](const session::Clip& clip) { return clip.id == clipId; });
    return found == manifest.clips.end() ? nullptr : &*found;
}

const session::Clip* findClip(const session::ProjectManifest& manifest, std::string_view clipId) {
    const auto found = std::ranges::find_if(
        manifest.clips, [clipId](const session::Clip& clip) { return clip.id == clipId; });
    return found == manifest.clips.end() ? nullptr : &*found;
}

session::Track* findTrack(session::ProjectManifest& manifest, std::string_view trackId) {
    const auto found = std::ranges::find_if(
        manifest.tracks, [trackId](const session::Track& track) { return track.id == trackId; });
    return found == manifest.tracks.end() ? nullptr : &*found;
}

bool containsRoutingConnection(const session::ProjectManifest& manifest,
                               const session::RoutingConnection& connection) {
    return std::ranges::any_of(manifest.routing, [&connection](const auto& existing) {
        return existing.sourceTrackId == connection.sourceTrackId &&
               existing.destinationTrackId == connection.destinationTrackId;
    });
}

std::string readSerializedString(std::string_view json, std::string_view key) {
    const auto pattern = "\"" + std::string{key} + "\":\"";
    const auto start = json.find(pattern);
    if (start == std::string_view::npos) {
        throw std::runtime_error("Serialized command is missing string field: " + std::string{key});
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
    throw std::runtime_error("Serialized command string field is unterminated: " +
                             std::string{key});
}

std::int64_t readSerializedInt64(std::string_view json, std::string_view key) {
    const auto pattern = "\"" + std::string{key} + "\":";
    const auto start = json.find(pattern);
    if (start == std::string_view::npos) {
        throw std::runtime_error("Serialized command is missing integer field: " +
                                 std::string{key});
    }
    const auto valueStart = start + pattern.size();
    auto valueEnd = valueStart;
    while (valueEnd < json.size() &&
           (json[valueEnd] == '-' || (json[valueEnd] >= '0' && json[valueEnd] <= '9'))) {
        ++valueEnd;
    }
    std::int64_t value{0};
    const auto result = std::from_chars(json.data() + valueStart, json.data() + valueEnd, value);
    if (result.ec != std::errc{}) {
        throw std::runtime_error("Serialized command integer field is invalid: " +
                                 std::string{key});
    }
    return value;
}

} // namespace

AddTrackCommand::AddTrackCommand(std::string commandId, std::string auditId, session::Track track)
    : metadata_{.commandId = std::move(commandId),
                .auditId = std::move(auditId),
                .name = "add_track"},
      track_(std::move(track)) {}

const CommandMetadata& AddTrackCommand::metadata() const noexcept {
    return metadata_;
}

CommandResult AddTrackCommand::validate(const session::ProjectManifest& manifest) const {
    if (track_.id.empty()) {
        return fail("Track id must not be empty");
    }
    if (track_.name.empty()) {
        return fail("Track name must not be empty");
    }
    if (containsTrackId(manifest, track_.id)) {
        return fail("Track id already exists: " + track_.id);
    }
    return ok("Track can be added");
}

std::string AddTrackCommand::preview(const session::ProjectManifest&) const {
    return "Add " + std::string{session::toString(track_.type)} + " track \"" + track_.name + "\"";
}

CommandResult AddTrackCommand::apply(session::ProjectManifest& manifest) {
    const auto validation = validate(manifest);
    if (!validation.ok) {
        return validation;
    }

    manifest.tracks.push_back(track_);
    applied_ = true;
    return ok("Track added");
}

CommandResult AddTrackCommand::undo(session::ProjectManifest& manifest) {
    if (!applied_) {
        return fail("Cannot undo command that has not been applied");
    }

    const auto oldSize = manifest.tracks.size();
    std::erase_if(manifest.tracks,
                  [this](const session::Track& track) { return track.id == track_.id; });
    if (manifest.tracks.size() == oldSize) {
        return fail("Track to undo was not found: " + track_.id);
    }

    applied_ = false;
    return ok("Track removed");
}

std::string AddTrackCommand::serialize() const {
    std::ostringstream output;
    output << "{\"name\":\"" << metadata_.name << "\",\"commandId\":\"" << metadata_.commandId
           << "\",\"auditId\":\"" << metadata_.auditId << "\",\"trackId\":\"" << track_.id
           << "\",\"trackName\":\"" << track_.name << "\",\"trackType\":\""
           << session::toString(track_.type) << "\"}";
    return output.str();
}

AddClipCommand::AddClipCommand(std::string commandId, std::string auditId, session::Clip clip)
    : metadata_{.commandId = std::move(commandId),
                .auditId = std::move(auditId),
                .name = "add_clip"},
      clip_(std::move(clip)) {}

const CommandMetadata& AddClipCommand::metadata() const noexcept {
    return metadata_;
}

CommandResult AddClipCommand::validate(const session::ProjectManifest& manifest) const {
    if (clip_.id.empty()) {
        return fail("Clip id must not be empty");
    }
    if (containsClipId(manifest, clip_.id)) {
        return fail("Clip id already exists: " + clip_.id);
    }
    if (!containsTrackId(manifest, clip_.trackId)) {
        return fail("Clip track does not exist: " + clip_.trackId);
    }
    if (clip_.lengthSamples < 0) {
        return fail("Clip length must not be negative");
    }
    if (clip_.startSample < 0) {
        return fail("Clip start must not be negative");
    }
    return ok("Clip can be added");
}

std::string AddClipCommand::preview(const session::ProjectManifest&) const {
    return "Add " + std::string{session::toString(clip_.type)} + " clip \"" + clip_.id + "\"";
}

CommandResult AddClipCommand::apply(session::ProjectManifest& manifest) {
    const auto validation = validate(manifest);
    if (!validation.ok) {
        return validation;
    }

    manifest.clips.push_back(clip_);
    applied_ = true;
    return ok("Clip added");
}

CommandResult AddClipCommand::undo(session::ProjectManifest& manifest) {
    if (!applied_) {
        return fail("Cannot undo command that has not been applied");
    }

    const auto oldSize = manifest.clips.size();
    std::erase_if(manifest.clips,
                  [this](const session::Clip& clip) { return clip.id == clip_.id; });
    if (manifest.clips.size() == oldSize) {
        return fail("Clip to undo was not found: " + clip_.id);
    }

    applied_ = false;
    return ok("Clip removed");
}

std::string AddClipCommand::serialize() const {
    std::ostringstream output;
    output << "{\"name\":\"" << metadata_.name << "\",\"commandId\":\"" << metadata_.commandId
           << "\",\"auditId\":\"" << metadata_.auditId << "\",\"clipId\":\"" << clip_.id
           << "\",\"trackId\":\"" << clip_.trackId << "\",\"startSample\":" << clip_.startSample
           << ",\"lengthSamples\":" << clip_.lengthSamples << "}";
    return output.str();
}

RemoveClipCommand::RemoveClipCommand(std::string commandId, std::string auditId, std::string clipId)
    : metadata_{.commandId = std::move(commandId),
                .auditId = std::move(auditId),
                .name = "remove_clip"},
      clipId_(std::move(clipId)) {}

const CommandMetadata& RemoveClipCommand::metadata() const noexcept {
    return metadata_;
}

CommandResult RemoveClipCommand::validate(const session::ProjectManifest& manifest) const {
    if (clipId_.empty()) {
        return fail("Clip id must not be empty");
    }
    if (!containsClipId(manifest, clipId_)) {
        return fail("Clip does not exist: " + clipId_);
    }
    return ok("Clip can be removed");
}

std::string RemoveClipCommand::preview(const session::ProjectManifest&) const {
    return "Remove clip \"" + clipId_ + "\"";
}

CommandResult RemoveClipCommand::apply(session::ProjectManifest& manifest) {
    const auto validation = validate(manifest);
    if (!validation.ok) {
        return validation;
    }

    const auto found = std::ranges::find_if(
        manifest.clips, [this](const session::Clip& clip) { return clip.id == clipId_; });
    removedIndex_ = static_cast<std::size_t>(std::distance(manifest.clips.begin(), found));
    removedClip_ = *found;
    manifest.clips.erase(found);
    applied_ = true;
    return ok("Clip removed");
}

CommandResult RemoveClipCommand::undo(session::ProjectManifest& manifest) {
    if (!applied_) {
        return fail("Cannot undo command that has not been applied");
    }
    if (containsClipId(manifest, removedClip_.id)) {
        return fail("Clip id already exists: " + removedClip_.id);
    }

    const auto insertionIndex = std::min(removedIndex_, manifest.clips.size());
    manifest.clips.insert(manifest.clips.begin() + static_cast<std::ptrdiff_t>(insertionIndex),
                          removedClip_);
    applied_ = false;
    return ok("Clip removal undone");
}

std::string RemoveClipCommand::serialize() const {
    std::ostringstream output;
    output << "{\"name\":\"" << metadata_.name << "\",\"commandId\":\"" << metadata_.commandId
           << "\",\"auditId\":\"" << metadata_.auditId << "\",\"clipId\":\"" << clipId_ << "\"}";
    return output.str();
}

DuplicateClipCommand::DuplicateClipCommand(std::string commandId, std::string auditId,
                                           std::string sourceClipId, std::string duplicateClipId,
                                           std::int64_t duplicateStartSample)
    : metadata_{.commandId = std::move(commandId),
                .auditId = std::move(auditId),
                .name = "duplicate_clip"},
      sourceClipId_(std::move(sourceClipId)), duplicateClipId_(std::move(duplicateClipId)),
      duplicateStartSample_(duplicateStartSample) {}

const CommandMetadata& DuplicateClipCommand::metadata() const noexcept {
    return metadata_;
}

CommandResult DuplicateClipCommand::validate(const session::ProjectManifest& manifest) const {
    if (!containsClipId(manifest, sourceClipId_)) {
        return fail("Source clip does not exist: " + sourceClipId_);
    }
    if (duplicateClipId_.empty()) {
        return fail("Duplicate clip id must not be empty");
    }
    if (containsClipId(manifest, duplicateClipId_)) {
        return fail("Duplicate clip id already exists: " + duplicateClipId_);
    }
    if (duplicateStartSample_ < 0) {
        return fail("Duplicate clip start must not be negative");
    }
    return ok("Clip can be duplicated");
}

std::string DuplicateClipCommand::preview(const session::ProjectManifest&) const {
    return "Duplicate clip \"" + sourceClipId_ + "\" as \"" + duplicateClipId_ + "\"";
}

CommandResult DuplicateClipCommand::apply(session::ProjectManifest& manifest) {
    const auto validation = validate(manifest);
    if (!validation.ok) {
        return validation;
    }

    auto duplicate = *findClip(manifest, sourceClipId_);
    duplicate.id = duplicateClipId_;
    duplicate.startSample = duplicateStartSample_;
    manifest.clips.push_back(std::move(duplicate));
    applied_ = true;
    return ok("Clip duplicated");
}

CommandResult DuplicateClipCommand::undo(session::ProjectManifest& manifest) {
    if (!applied_) {
        return fail("Cannot undo command that has not been applied");
    }

    const auto oldSize = manifest.clips.size();
    std::erase_if(manifest.clips,
                  [this](const session::Clip& clip) { return clip.id == duplicateClipId_; });
    if (manifest.clips.size() == oldSize) {
        return fail("Duplicate clip to undo was not found: " + duplicateClipId_);
    }

    applied_ = false;
    return ok("Clip duplicate removed");
}

std::string DuplicateClipCommand::serialize() const {
    std::ostringstream output;
    output << "{\"name\":\"" << metadata_.name << "\",\"commandId\":\"" << metadata_.commandId
           << "\",\"auditId\":\"" << metadata_.auditId << "\",\"sourceClipId\":\"" << sourceClipId_
           << "\",\"duplicateClipId\":\"" << duplicateClipId_
           << "\",\"duplicateStartSample\":" << duplicateStartSample_ << "}";
    return output.str();
}

MoveClipCommand::MoveClipCommand(std::string commandId, std::string auditId, std::string clipId,
                                 std::int64_t newStartSample)
    : metadata_{.commandId = std::move(commandId),
                .auditId = std::move(auditId),
                .name = "move_clip"},
      clipId_(std::move(clipId)), newStartSample_(newStartSample) {}

const CommandMetadata& MoveClipCommand::metadata() const noexcept {
    return metadata_;
}

CommandResult MoveClipCommand::validate(const session::ProjectManifest& manifest) const {
    if (clipId_.empty()) {
        return fail("Clip id must not be empty");
    }
    if (newStartSample_ < 0) {
        return fail("Clip start must not be negative");
    }
    if (!containsClipId(manifest, clipId_)) {
        return fail("Clip does not exist: " + clipId_);
    }
    return ok("Clip can be moved");
}

std::string MoveClipCommand::preview(const session::ProjectManifest&) const {
    return "Move clip \"" + clipId_ + "\" to sample " + std::to_string(newStartSample_);
}

CommandResult MoveClipCommand::apply(session::ProjectManifest& manifest) {
    const auto validation = validate(manifest);
    if (!validation.ok) {
        return validation;
    }

    auto* clip = findClip(manifest, clipId_);
    oldStartSample_ = clip->startSample;
    clip->startSample = newStartSample_;
    applied_ = true;
    return ok("Clip moved");
}

CommandResult MoveClipCommand::undo(session::ProjectManifest& manifest) {
    if (!applied_) {
        return fail("Cannot undo command that has not been applied");
    }

    auto* clip = findClip(manifest, clipId_);
    if (clip == nullptr) {
        return fail("Clip to undo was not found: " + clipId_);
    }

    clip->startSample = oldStartSample_;
    applied_ = false;
    return ok("Clip move undone");
}

std::string MoveClipCommand::serialize() const {
    std::ostringstream output;
    output << "{\"name\":\"" << metadata_.name << "\",\"commandId\":\"" << metadata_.commandId
           << "\",\"auditId\":\"" << metadata_.auditId << "\",\"clipId\":\"" << clipId_
           << "\",\"newStartSample\":" << newStartSample_ << "}";
    return output.str();
}

TrimClipCommand::TrimClipCommand(std::string commandId, std::string auditId, std::string clipId,
                                 std::int64_t newStartSample, std::int64_t newLengthSamples,
                                 std::int64_t newSourceOffsetSamples)
    : metadata_{.commandId = std::move(commandId),
                .auditId = std::move(auditId),
                .name = "trim_clip"},
      clipId_(std::move(clipId)), newStartSample_(newStartSample),
      newLengthSamples_(newLengthSamples), newSourceOffsetSamples_(newSourceOffsetSamples) {}

const CommandMetadata& TrimClipCommand::metadata() const noexcept {
    return metadata_;
}

CommandResult TrimClipCommand::validate(const session::ProjectManifest& manifest) const {
    if (!containsClipId(manifest, clipId_)) {
        return fail("Clip does not exist: " + clipId_);
    }
    if (newStartSample_ < 0) {
        return fail("Clip start must not be negative");
    }
    if (newLengthSamples_ < 0) {
        return fail("Clip length must not be negative");
    }
    if (newSourceOffsetSamples_ < 0) {
        return fail("Clip source offset must not be negative");
    }
    return ok("Clip can be trimmed");
}

std::string TrimClipCommand::preview(const session::ProjectManifest&) const {
    return "Trim clip \"" + clipId_ + "\"";
}

CommandResult TrimClipCommand::apply(session::ProjectManifest& manifest) {
    const auto validation = validate(manifest);
    if (!validation.ok) {
        return validation;
    }

    auto* clip = findClip(manifest, clipId_);
    previousClip_ = *clip;
    clip->startSample = newStartSample_;
    clip->lengthSamples = newLengthSamples_;
    clip->sourceOffsetSamples = newSourceOffsetSamples_;
    applied_ = true;
    return ok("Clip trimmed");
}

CommandResult TrimClipCommand::undo(session::ProjectManifest& manifest) {
    if (!applied_) {
        return fail("Cannot undo command that has not been applied");
    }

    auto* clip = findClip(manifest, clipId_);
    if (clip == nullptr) {
        return fail("Clip to undo was not found: " + clipId_);
    }

    *clip = previousClip_;
    applied_ = false;
    return ok("Clip trim undone");
}

std::string TrimClipCommand::serialize() const {
    std::ostringstream output;
    output << "{\"name\":\"" << metadata_.name << "\",\"commandId\":\"" << metadata_.commandId
           << "\",\"auditId\":\"" << metadata_.auditId << "\",\"clipId\":\"" << clipId_
           << "\",\"newStartSample\":" << newStartSample_
           << ",\"newLengthSamples\":" << newLengthSamples_
           << ",\"newSourceOffsetSamples\":" << newSourceOffsetSamples_ << "}";
    return output.str();
}

SetClipFadeCommand::SetClipFadeCommand(std::string commandId, std::string auditId,
                                       std::string clipId, std::int64_t fadeInSamples,
                                       std::int64_t fadeOutSamples)
    : metadata_{.commandId = std::move(commandId),
                .auditId = std::move(auditId),
                .name = "set_clip_fade"},
      clipId_(std::move(clipId)), fadeInSamples_(fadeInSamples), fadeOutSamples_(fadeOutSamples) {}

const CommandMetadata& SetClipFadeCommand::metadata() const noexcept {
    return metadata_;
}

CommandResult SetClipFadeCommand::validate(const session::ProjectManifest& manifest) const {
    if (fadeInSamples_ < 0 || fadeOutSamples_ < 0) {
        return fail("Clip fades must not be negative");
    }

    const auto found = std::ranges::find_if(
        manifest.clips, [this](const session::Clip& clip) { return clip.id == clipId_; });
    if (found == manifest.clips.end()) {
        return fail("Clip does not exist: " + clipId_);
    }

    if (fadeInSamples_ + fadeOutSamples_ > found->lengthSamples) {
        return fail("Clip fades must fit inside clip length");
    }

    return ok("Clip fades can be set");
}

std::string SetClipFadeCommand::preview(const session::ProjectManifest&) const {
    return "Set fades on clip \"" + clipId_ + "\"";
}

CommandResult SetClipFadeCommand::apply(session::ProjectManifest& manifest) {
    const auto validation = validate(manifest);
    if (!validation.ok) {
        return validation;
    }

    auto* clip = findClip(manifest, clipId_);
    previousClip_ = *clip;
    clip->fadeInSamples = fadeInSamples_;
    clip->fadeOutSamples = fadeOutSamples_;
    applied_ = true;
    return ok("Clip fades set");
}

CommandResult SetClipFadeCommand::undo(session::ProjectManifest& manifest) {
    if (!applied_) {
        return fail("Cannot undo command that has not been applied");
    }

    auto* clip = findClip(manifest, clipId_);
    if (clip == nullptr) {
        return fail("Clip to undo was not found: " + clipId_);
    }

    *clip = previousClip_;
    applied_ = false;
    return ok("Clip fade edit undone");
}

std::string SetClipFadeCommand::serialize() const {
    std::ostringstream output;
    output << "{\"name\":\"" << metadata_.name << "\",\"commandId\":\"" << metadata_.commandId
           << "\",\"auditId\":\"" << metadata_.auditId << "\",\"clipId\":\"" << clipId_
           << "\",\"fadeInSamples\":" << fadeInSamples_ << ",\"fadeOutSamples\":" << fadeOutSamples_
           << "}";
    return output.str();
}

SetTrackNameCommand::SetTrackNameCommand(std::string commandId, std::string auditId,
                                         std::string trackId, std::string name)
    : metadata_{.commandId = std::move(commandId),
                .auditId = std::move(auditId),
                .name = "set_track_name"},
      trackId_(std::move(trackId)), name_(std::move(name)) {}

const CommandMetadata& SetTrackNameCommand::metadata() const noexcept {
    return metadata_;
}

CommandResult SetTrackNameCommand::validate(const session::ProjectManifest& manifest) const {
    if (!containsTrackId(manifest, trackId_)) {
        return fail("Track does not exist: " + trackId_);
    }
    if (name_.empty()) {
        return fail("Track name must not be empty");
    }
    return ok("Track name can be set");
}

std::string SetTrackNameCommand::preview(const session::ProjectManifest&) const {
    return "Rename track \"" + trackId_ + "\" to \"" + name_ + "\"";
}

CommandResult SetTrackNameCommand::apply(session::ProjectManifest& manifest) {
    const auto validation = validate(manifest);
    if (!validation.ok) {
        return validation;
    }

    auto* track = findTrack(manifest, trackId_);
    previousName_ = track->name;
    track->name = name_;
    applied_ = true;
    return ok("Track renamed");
}

CommandResult SetTrackNameCommand::undo(session::ProjectManifest& manifest) {
    if (!applied_) {
        return fail("Cannot undo command that has not been applied");
    }

    auto* track = findTrack(manifest, trackId_);
    if (track == nullptr) {
        return fail("Track to undo was not found: " + trackId_);
    }

    track->name = previousName_;
    applied_ = false;
    return ok("Track rename undone");
}

std::string SetTrackNameCommand::serialize() const {
    std::ostringstream output;
    output << "{\"name\":\"" << metadata_.name << "\",\"commandId\":\"" << metadata_.commandId
           << "\",\"auditId\":\"" << metadata_.auditId << "\",\"trackId\":\"" << trackId_
           << "\",\"trackName\":\"" << name_ << "\"}";
    return output.str();
}

AddRoutingConnectionCommand::AddRoutingConnectionCommand(std::string commandId, std::string auditId,
                                                         session::RoutingConnection connection)
    : metadata_{.commandId = std::move(commandId),
                .auditId = std::move(auditId),
                .name = "add_routing_connection"},
      connection_(std::move(connection)) {}

const CommandMetadata& AddRoutingConnectionCommand::metadata() const noexcept {
    return metadata_;
}

CommandResult
AddRoutingConnectionCommand::validate(const session::ProjectManifest& manifest) const {
    if (!containsTrackId(manifest, connection_.sourceTrackId)) {
        return fail("Routing source track does not exist: " + connection_.sourceTrackId);
    }
    if (!containsTrackId(manifest, connection_.destinationTrackId)) {
        return fail("Routing destination track does not exist: " + connection_.destinationTrackId);
    }
    if (connection_.sourceTrackId == connection_.destinationTrackId) {
        return fail("Routing connection cannot target the same track");
    }
    if (containsRoutingConnection(manifest, connection_)) {
        return fail("Routing connection already exists");
    }
    return ok("Routing connection can be added");
}

std::string AddRoutingConnectionCommand::preview(const session::ProjectManifest&) const {
    return "Route track \"" + connection_.sourceTrackId + "\" to \"" +
           connection_.destinationTrackId + "\"";
}

CommandResult AddRoutingConnectionCommand::apply(session::ProjectManifest& manifest) {
    const auto validation = validate(manifest);
    if (!validation.ok) {
        return validation;
    }

    manifest.routing.push_back(connection_);
    applied_ = true;
    return ok("Routing connection added");
}

CommandResult AddRoutingConnectionCommand::undo(session::ProjectManifest& manifest) {
    if (!applied_) {
        return fail("Cannot undo command that has not been applied");
    }

    const auto oldSize = manifest.routing.size();
    std::erase_if(manifest.routing, [this](const session::RoutingConnection& connection) {
        return connection.sourceTrackId == connection_.sourceTrackId &&
               connection.destinationTrackId == connection_.destinationTrackId;
    });
    if (manifest.routing.size() == oldSize) {
        return fail("Routing connection to undo was not found");
    }

    applied_ = false;
    return ok("Routing connection removed");
}

std::string AddRoutingConnectionCommand::serialize() const {
    std::ostringstream output;
    output << "{\"name\":\"" << metadata_.name << "\",\"commandId\":\"" << metadata_.commandId
           << "\",\"auditId\":\"" << metadata_.auditId << "\",\"sourceTrackId\":\""
           << connection_.sourceTrackId << "\",\"destinationTrackId\":\""
           << connection_.destinationTrackId << "\"}";
    return output.str();
}

RemoveRoutingConnectionCommand::RemoveRoutingConnectionCommand(
    std::string commandId, std::string auditId, session::RoutingConnection connection)
    : metadata_{.commandId = std::move(commandId),
                .auditId = std::move(auditId),
                .name = "remove_routing_connection"},
      connection_(std::move(connection)) {}

const CommandMetadata& RemoveRoutingConnectionCommand::metadata() const noexcept {
    return metadata_;
}

CommandResult
RemoveRoutingConnectionCommand::validate(const session::ProjectManifest& manifest) const {
    if (!containsTrackId(manifest, connection_.sourceTrackId)) {
        return fail("Routing source track does not exist: " + connection_.sourceTrackId);
    }
    if (!containsTrackId(manifest, connection_.destinationTrackId)) {
        return fail("Routing destination track does not exist: " + connection_.destinationTrackId);
    }
    if (!containsRoutingConnection(manifest, connection_)) {
        return fail("Routing connection does not exist");
    }
    return ok("Routing connection can be removed");
}

std::string RemoveRoutingConnectionCommand::preview(const session::ProjectManifest&) const {
    return "Remove route from track \"" + connection_.sourceTrackId + "\" to \"" +
           connection_.destinationTrackId + "\"";
}

CommandResult RemoveRoutingConnectionCommand::apply(session::ProjectManifest& manifest) {
    const auto validation = validate(manifest);
    if (!validation.ok) {
        return validation;
    }

    const auto found = std::ranges::find_if(manifest.routing, [this](const auto& connection) {
        return connection.sourceTrackId == connection_.sourceTrackId &&
               connection.destinationTrackId == connection_.destinationTrackId;
    });
    if (found == manifest.routing.end()) {
        return fail("Routing connection to remove was not found");
    }

    removedIndex_ = static_cast<std::size_t>(std::distance(manifest.routing.begin(), found));
    manifest.routing.erase(found);
    applied_ = true;
    return ok("Routing connection removed");
}

CommandResult RemoveRoutingConnectionCommand::undo(session::ProjectManifest& manifest) {
    if (!applied_) {
        return fail("Cannot undo command that has not been applied");
    }
    if (containsRoutingConnection(manifest, connection_)) {
        return fail("Routing connection to restore already exists");
    }

    const auto insertIndex = std::min(removedIndex_, manifest.routing.size());
    manifest.routing.insert(manifest.routing.begin() + static_cast<std::ptrdiff_t>(insertIndex),
                            connection_);
    applied_ = false;
    return ok("Routing connection restored");
}

std::string RemoveRoutingConnectionCommand::serialize() const {
    std::ostringstream output;
    output << "{\"name\":\"" << metadata_.name << "\",\"commandId\":\"" << metadata_.commandId
           << "\",\"auditId\":\"" << metadata_.auditId << "\",\"sourceTrackId\":\""
           << connection_.sourceTrackId << "\",\"destinationTrackId\":\""
           << connection_.destinationTrackId << "\"}";
    return output.str();
}

session::MidiClipData* MidiClipStore::find(std::string_view clipId) noexcept {
    const auto found = std::ranges::find_if(
        clips_, [clipId](const session::MidiClipData& clip) { return clip.clipId == clipId; });
    return found == clips_.end() ? nullptr : &*found;
}

const session::MidiClipData* MidiClipStore::find(std::string_view clipId) const noexcept {
    const auto found = std::ranges::find_if(
        clips_, [clipId](const session::MidiClipData& clip) { return clip.clipId == clipId; });
    return found == clips_.end() ? nullptr : &*found;
}

session::MidiClipData& MidiClipStore::getOrCreate(std::string clipId) {
    if (auto* clip = find(clipId); clip != nullptr) {
        return *clip;
    }

    clips_.push_back({.clipId = std::move(clipId)});
    return clips_.back();
}

AddMidiNoteCommand::AddMidiNoteCommand(std::string commandId, std::string auditId,
                                       std::string clipId, session::MidiNote note)
    : metadata_{.commandId = std::move(commandId),
                .auditId = std::move(auditId),
                .name = "add_midi_note"},
      clipId_(std::move(clipId)), note_(std::move(note)) {}

const CommandMetadata& AddMidiNoteCommand::metadata() const noexcept {
    return metadata_;
}

CommandResult AddMidiNoteCommand::validate(const MidiClipStore&) const {
    if (clipId_.empty()) {
        return fail("MIDI clip id must not be empty");
    }
    if (note_.id.empty()) {
        return fail("MIDI note id must not be empty");
    }
    if (note_.lengthSamples < 0 || note_.startSample < 0) {
        return fail("MIDI note timing must not be negative");
    }
    if (note_.channel == 0 || note_.channel > 16) {
        return fail("MIDI channel must be in the range 1-16");
    }
    return ok("MIDI note can be added");
}

std::string AddMidiNoteCommand::preview(const MidiClipStore&) const {
    return "Add MIDI note " + std::to_string(note_.pitch) + " to clip \"" + clipId_ + "\"";
}

CommandResult AddMidiNoteCommand::apply(MidiClipStore& store) {
    const auto validation = validate(store);
    if (!validation.ok) {
        return validation;
    }

    auto& clip = store.getOrCreate(clipId_);
    if (std::ranges::any_of(
            clip.notes, [this](const session::MidiNote& note) { return note.id == note_.id; })) {
        return fail("MIDI note id already exists: " + note_.id);
    }

    clip.notes.push_back(note_);
    applied_ = true;
    return ok("MIDI note added");
}

CommandResult AddMidiNoteCommand::undo(MidiClipStore& store) {
    if (!applied_) {
        return fail("Cannot undo command that has not been applied");
    }

    auto* clip = store.find(clipId_);
    if (clip == nullptr) {
        return fail("MIDI clip to undo was not found: " + clipId_);
    }

    const auto oldSize = clip->notes.size();
    std::erase_if(clip->notes,
                  [this](const session::MidiNote& note) { return note.id == note_.id; });
    if (clip->notes.size() == oldSize) {
        return fail("MIDI note to undo was not found: " + note_.id);
    }

    applied_ = false;
    return ok("MIDI note removed");
}

std::string AddMidiNoteCommand::serialize() const {
    std::ostringstream output;
    output << "{\"name\":\"" << metadata_.name << "\",\"commandId\":\"" << metadata_.commandId
           << "\",\"auditId\":\"" << metadata_.auditId << "\",\"clipId\":\"" << clipId_
           << "\",\"noteId\":\"" << note_.id << "\",\"pitch\":" << static_cast<int>(note_.pitch)
           << ",\"velocity\":" << static_cast<int>(note_.velocity)
           << ",\"startSample\":" << note_.startSample
           << ",\"lengthSamples\":" << note_.lengthSamples << "}";
    return output.str();
}

QuantizeMidiClipCommand::QuantizeMidiClipCommand(std::string commandId, std::string auditId,
                                                 std::string clipId,
                                                 session::QuantizeSettings settings)
    : metadata_{.commandId = std::move(commandId),
                .auditId = std::move(auditId),
                .name = "quantize_midi_clip"},
      clipId_(std::move(clipId)), settings_(settings) {}

const CommandMetadata& QuantizeMidiClipCommand::metadata() const noexcept {
    return metadata_;
}

CommandResult QuantizeMidiClipCommand::validate(const MidiClipStore& store) const {
    if (store.find(clipId_) == nullptr) {
        return fail("MIDI clip does not exist: " + clipId_);
    }
    if (settings_.gridSamples <= 0) {
        return fail("Quantize grid must be positive");
    }
    return ok("MIDI clip can be quantized");
}

std::string QuantizeMidiClipCommand::preview(const MidiClipStore&) const {
    return "Quantize MIDI clip \"" + clipId_ + "\"";
}

CommandResult QuantizeMidiClipCommand::apply(MidiClipStore& store) {
    const auto validation = validate(store);
    if (!validation.ok) {
        return validation;
    }

    auto* clip = store.find(clipId_);
    previousClip_ = *clip;
    session::quantizeNotes(*clip, settings_);
    applied_ = true;
    return ok("MIDI clip quantized");
}

CommandResult QuantizeMidiClipCommand::undo(MidiClipStore& store) {
    if (!applied_) {
        return fail("Cannot undo command that has not been applied");
    }

    auto* clip = store.find(clipId_);
    if (clip == nullptr) {
        return fail("MIDI clip to undo was not found: " + clipId_);
    }

    *clip = previousClip_;
    applied_ = false;
    return ok("MIDI quantize undone");
}

std::string QuantizeMidiClipCommand::serialize() const {
    std::ostringstream output;
    output << "{\"name\":\"" << metadata_.name << "\",\"commandId\":\"" << metadata_.commandId
           << "\",\"auditId\":\"" << metadata_.auditId << "\",\"clipId\":\"" << clipId_
           << "\",\"gridSamples\":" << settings_.gridSamples
           << ",\"strength\":" << settings_.strength << "}";
    return output.str();
}

TransposeMidiClipCommand::TransposeMidiClipCommand(std::string commandId, std::string auditId,
                                                   std::string clipId, int semitones)
    : metadata_{.commandId = std::move(commandId),
                .auditId = std::move(auditId),
                .name = "transpose_midi_clip"},
      clipId_(std::move(clipId)), semitones_(semitones) {}

const CommandMetadata& TransposeMidiClipCommand::metadata() const noexcept {
    return metadata_;
}

CommandResult TransposeMidiClipCommand::validate(const MidiClipStore& store) const {
    const auto* clip = store.find(clipId_);
    if (clip == nullptr) {
        return fail("MIDI clip does not exist: " + clipId_);
    }
    for (const auto& note : clip->notes) {
        const auto transposedPitch = static_cast<int>(note.pitch) + semitones_;
        if (transposedPitch < 0 || transposedPitch > 127) {
            return fail("MIDI transpose would move a note outside 0-127");
        }
    }
    return ok("MIDI clip can be transposed");
}

std::string TransposeMidiClipCommand::preview(const MidiClipStore&) const {
    return "Transpose MIDI clip \"" + clipId_ + "\" by " + std::to_string(semitones_) +
           " semitones";
}

CommandResult TransposeMidiClipCommand::apply(MidiClipStore& store) {
    const auto validation = validate(store);
    if (!validation.ok) {
        return validation;
    }

    auto* clip = store.find(clipId_);
    previousClip_ = *clip;
    session::transposeNotes(*clip, semitones_);
    applied_ = true;
    return ok("MIDI clip transposed");
}

CommandResult TransposeMidiClipCommand::undo(MidiClipStore& store) {
    if (!applied_) {
        return fail("Cannot undo command that has not been applied");
    }

    auto* clip = store.find(clipId_);
    if (clip == nullptr) {
        return fail("MIDI clip to undo was not found: " + clipId_);
    }

    *clip = previousClip_;
    applied_ = false;
    return ok("MIDI transpose undone");
}

std::string TransposeMidiClipCommand::serialize() const {
    std::ostringstream output;
    output << "{\"name\":\"" << metadata_.name << "\",\"commandId\":\"" << metadata_.commandId
           << "\",\"auditId\":\"" << metadata_.auditId << "\",\"clipId\":\"" << clipId_
           << "\",\"semitones\":" << semitones_ << "}";
    return output.str();
}

EditMidiNoteCommand::EditMidiNoteCommand(std::string commandId, std::string auditId,
                                         std::string clipId, std::string noteId,
                                         session::MidiNote replacement)
    : metadata_{.commandId = std::move(commandId),
                .auditId = std::move(auditId),
                .name = "edit_midi_note"},
      clipId_(std::move(clipId)), noteId_(std::move(noteId)), replacement_(std::move(replacement)) {
}

const CommandMetadata& EditMidiNoteCommand::metadata() const noexcept {
    return metadata_;
}

CommandResult EditMidiNoteCommand::validate(const MidiClipStore& store) const {
    const auto* clip = store.find(clipId_);
    if (clip == nullptr) {
        return fail("MIDI clip does not exist: " + clipId_);
    }
    if (noteId_.empty()) {
        return fail("MIDI note id must not be empty");
    }
    if (replacement_.id != noteId_) {
        return fail("Replacement note id must match edited note id");
    }
    if (replacement_.startSample < 0 || replacement_.lengthSamples < 0) {
        return fail("MIDI note timing must not be negative");
    }
    if (replacement_.channel == 0 || replacement_.channel > 16) {
        return fail("MIDI channel must be in the range 1-16");
    }
    if (!std::ranges::any_of(
            clip->notes, [this](const session::MidiNote& note) { return note.id == noteId_; })) {
        return fail("MIDI note does not exist: " + noteId_);
    }
    return ok("MIDI note can be edited");
}

std::string EditMidiNoteCommand::preview(const MidiClipStore&) const {
    return "Edit MIDI note \"" + noteId_ + "\"";
}

CommandResult EditMidiNoteCommand::apply(MidiClipStore& store) {
    const auto validation = validate(store);
    if (!validation.ok) {
        return validation;
    }

    auto* clip = store.find(clipId_);
    const auto found = std::ranges::find_if(
        clip->notes, [this](const session::MidiNote& note) { return note.id == noteId_; });
    previous_ = *found;
    *found = replacement_;
    applied_ = true;
    return ok("MIDI note edited");
}

CommandResult EditMidiNoteCommand::undo(MidiClipStore& store) {
    if (!applied_) {
        return fail("Cannot undo command that has not been applied");
    }

    auto* clip = store.find(clipId_);
    if (clip == nullptr) {
        return fail("MIDI clip to undo was not found: " + clipId_);
    }

    const auto found = std::ranges::find_if(
        clip->notes, [this](const session::MidiNote& note) { return note.id == noteId_; });
    if (found == clip->notes.end()) {
        return fail("MIDI note to undo was not found: " + noteId_);
    }

    *found = previous_;
    applied_ = false;
    return ok("MIDI note edit undone");
}

std::string EditMidiNoteCommand::serialize() const {
    std::ostringstream output;
    output << "{\"name\":\"" << metadata_.name << "\",\"commandId\":\"" << metadata_.commandId
           << "\",\"auditId\":\"" << metadata_.auditId << "\",\"clipId\":\"" << clipId_
           << "\",\"noteId\":\"" << noteId_
           << "\",\"pitch\":" << static_cast<int>(replacement_.pitch)
           << ",\"velocity\":" << static_cast<int>(replacement_.velocity)
           << ",\"startSample\":" << replacement_.startSample
           << ",\"lengthSamples\":" << replacement_.lengthSamples
           << ",\"muted\":" << (replacement_.muted ? "true" : "false") << "}";
    return output.str();
}

session::PatternClip* PatternClipStore::find(std::string_view patternId) noexcept {
    const auto found = std::ranges::find_if(
        patterns_, [patternId](const session::PatternClip& item) { return item.id == patternId; });
    return found == patterns_.end() ? nullptr : &*found;
}

const session::PatternClip* PatternClipStore::find(std::string_view patternId) const noexcept {
    const auto found = std::ranges::find_if(
        patterns_, [patternId](const session::PatternClip& item) { return item.id == patternId; });
    return found == patterns_.end() ? nullptr : &*found;
}

void PatternClipStore::add(session::PatternClip pattern) {
    if (pattern.id.empty()) {
        throw std::runtime_error("Pattern id must not be empty");
    }
    if (find(pattern.id) != nullptr) {
        throw std::runtime_error("Duplicate pattern id: " + pattern.id);
    }
    patterns_.push_back(std::move(pattern));
}

void PatternClipStore::remove(std::string_view patternId) {
    std::erase_if(patterns_,
                  [patternId](const session::PatternClip& item) { return item.id == patternId; });
}

namespace {

CommandResult validatePatternClip(const session::PatternClip& pattern) {
    if (pattern.id.empty()) {
        return fail("Pattern id must not be empty");
    }
    if (pattern.name.empty()) {
        return fail("Pattern name must not be empty");
    }
    if (pattern.lengthSteps == 0U) {
        return fail("Pattern length must be positive");
    }
    if (pattern.stepLengthSamples <= 0) {
        return fail("Pattern step length must be positive");
    }
    for (const auto& lane : pattern.lanes) {
        if (lane.id.empty()) {
            return fail("Pattern lane id must not be empty");
        }
        if (lane.lengthSteps == 0U) {
            return fail("Pattern lane length must be positive");
        }
        for (const auto& step : lane.steps) {
            if (step.probability < 0.0F || step.probability > 1.0F) {
                return fail("Pattern step probability must be in the range 0..1");
            }
            if (step.ratchets == 0U) {
                return fail("Pattern step ratchets must be positive");
            }
        }
    }
    return ok("Pattern clip is valid");
}

} // namespace

AddPatternClipCommand::AddPatternClipCommand(std::string commandId, std::string auditId,
                                             session::PatternClip pattern)
    : metadata_{.commandId = std::move(commandId),
                .auditId = std::move(auditId),
                .name = "add_pattern_clip"},
      pattern_(std::move(pattern)) {}

const CommandMetadata& AddPatternClipCommand::metadata() const noexcept {
    return metadata_;
}

CommandResult AddPatternClipCommand::validate(const PatternClipStore& store) const {
    const auto validPattern = validatePatternClip(pattern_);
    if (!validPattern.ok) {
        return validPattern;
    }
    if (store.find(pattern_.id) != nullptr) {
        return fail("Pattern id already exists: " + pattern_.id);
    }
    return ok("Pattern clip can be added");
}

std::string AddPatternClipCommand::preview(const PatternClipStore&) const {
    return "Add pattern clip \"" + pattern_.id + "\"";
}

CommandResult AddPatternClipCommand::apply(PatternClipStore& store) {
    const auto validation = validate(store);
    if (!validation.ok) {
        return validation;
    }
    store.add(pattern_);
    applied_ = true;
    return ok("Pattern clip added");
}

CommandResult AddPatternClipCommand::undo(PatternClipStore& store) {
    if (!applied_) {
        return fail("Cannot undo command that has not been applied");
    }
    if (store.find(pattern_.id) == nullptr) {
        return fail("Pattern clip to undo was not found: " + pattern_.id);
    }
    store.remove(pattern_.id);
    applied_ = false;
    return ok("Pattern clip removed");
}

std::string AddPatternClipCommand::serialize() const {
    std::ostringstream output;
    output << "{\"name\":\"" << metadata_.name << "\",\"commandId\":\"" << metadata_.commandId
           << "\",\"auditId\":\"" << metadata_.auditId << "\",\"patternId\":\"" << pattern_.id
           << "\",\"patternName\":\"" << pattern_.name
           << "\",\"lengthSteps\":" << pattern_.lengthSteps
           << ",\"stepLengthSamples\":" << pattern_.stepLengthSamples
           << ",\"seed\":" << pattern_.seed << "}";
    return output.str();
}

DuplicatePatternVariationCommand::DuplicatePatternVariationCommand(
    std::string commandId, std::string auditId, std::string sourcePatternId,
    std::string newPatternId, std::string newName, std::uint32_t seedOffset)
    : metadata_{.commandId = std::move(commandId),
                .auditId = std::move(auditId),
                .name = "duplicate_pattern_variation"},
      sourcePatternId_(std::move(sourcePatternId)), newPatternId_(std::move(newPatternId)),
      newName_(std::move(newName)), seedOffset_(seedOffset) {}

const CommandMetadata& DuplicatePatternVariationCommand::metadata() const noexcept {
    return metadata_;
}

CommandResult DuplicatePatternVariationCommand::validate(const PatternClipStore& store) const {
    if (sourcePatternId_.empty() || newPatternId_.empty()) {
        return fail("Pattern ids must not be empty");
    }
    if (newName_.empty()) {
        return fail("Pattern variation name must not be empty");
    }
    if (store.find(sourcePatternId_) == nullptr) {
        return fail("Source pattern does not exist: " + sourcePatternId_);
    }
    if (store.find(newPatternId_) != nullptr) {
        return fail("Pattern id already exists: " + newPatternId_);
    }
    return ok("Pattern variation can be duplicated");
}

std::string DuplicatePatternVariationCommand::preview(const PatternClipStore&) const {
    return "Duplicate pattern \"" + sourcePatternId_ + "\" as \"" + newPatternId_ + "\"";
}

CommandResult DuplicatePatternVariationCommand::apply(PatternClipStore& store) {
    const auto validation = validate(store);
    if (!validation.ok) {
        return validation;
    }
    const auto* source = store.find(sourcePatternId_);
    store.add(session::duplicatePatternVariation(*source, newPatternId_, newName_, seedOffset_));
    applied_ = true;
    return ok("Pattern variation duplicated");
}

CommandResult DuplicatePatternVariationCommand::undo(PatternClipStore& store) {
    if (!applied_) {
        return fail("Cannot undo command that has not been applied");
    }
    if (store.find(newPatternId_) == nullptr) {
        return fail("Pattern variation to undo was not found: " + newPatternId_);
    }
    store.remove(newPatternId_);
    applied_ = false;
    return ok("Pattern variation removed");
}

std::string DuplicatePatternVariationCommand::serialize() const {
    std::ostringstream output;
    output << "{\"name\":\"" << metadata_.name << "\",\"commandId\":\"" << metadata_.commandId
           << "\",\"auditId\":\"" << metadata_.auditId << "\",\"sourcePatternId\":\""
           << sourcePatternId_ << "\",\"newPatternId\":\"" << newPatternId_ << "\",\"newName\":\""
           << newName_ << "\",\"seedOffset\":" << seedOffset_ << "}";
    return output.str();
}

session::AutomationLaneData* AutomationLaneStore::find(std::string_view laneId) noexcept {
    const auto found = std::ranges::find_if(
        lanes_, [laneId](const session::AutomationLaneData& lane) { return lane.id == laneId; });
    return found == lanes_.end() ? nullptr : &*found;
}

const session::AutomationLaneData*
AutomationLaneStore::find(std::string_view laneId) const noexcept {
    const auto found = std::ranges::find_if(
        lanes_, [laneId](const session::AutomationLaneData& lane) { return lane.id == laneId; });
    return found == lanes_.end() ? nullptr : &*found;
}

session::AutomationLaneData& AutomationLaneStore::getOrCreate(session::AutomationLaneData lane) {
    if (auto* existing = find(lane.id); existing != nullptr) {
        return *existing;
    }

    lanes_.push_back(std::move(lane));
    return lanes_.back();
}

AddAutomationPointCommand::AddAutomationPointCommand(std::string commandId, std::string auditId,
                                                     session::AutomationLaneData lane,
                                                     std::int64_t samplePosition, float value,
                                                     session::AutomationCurve curveToNext)
    : metadata_{.commandId = std::move(commandId),
                .auditId = std::move(auditId),
                .name = "add_automation_point"},
      lane_(std::move(lane)), samplePosition_(samplePosition), value_(value),
      curveToNext_(curveToNext) {}

const CommandMetadata& AddAutomationPointCommand::metadata() const noexcept {
    return metadata_;
}

CommandResult AddAutomationPointCommand::validate(const AutomationLaneStore&) const {
    if (lane_.id.empty()) {
        return fail("Automation lane id must not be empty");
    }
    if (lane_.targetId.empty()) {
        return fail("Automation target id must not be empty");
    }
    if (lane_.parameterId.empty()) {
        return fail("Automation parameter id must not be empty");
    }
    if (samplePosition_ < 0) {
        return fail("Automation point sample must not be negative");
    }
    return ok("Automation point can be added");
}

std::string AddAutomationPointCommand::preview(const AutomationLaneStore&) const {
    return "Add automation point to lane \"" + lane_.id + "\"";
}

CommandResult AddAutomationPointCommand::apply(AutomationLaneStore& store) {
    const auto validation = validate(store);
    if (!validation.ok) {
        return validation;
    }

    auto* previous = store.find(lane_.id);
    hadPreviousLane_ = previous != nullptr;
    if (previous != nullptr) {
        previousLane_ = *previous;
    }

    auto& lane = store.getOrCreate(lane_);
    session::addAutomationPoint(lane, samplePosition_, value_, curveToNext_);
    applied_ = true;
    return ok("Automation point added");
}

CommandResult AddAutomationPointCommand::undo(AutomationLaneStore& store) {
    if (!applied_) {
        return fail("Cannot undo command that has not been applied");
    }

    auto* lane = store.find(lane_.id);
    if (lane == nullptr) {
        return fail("Automation lane to undo was not found: " + lane_.id);
    }

    if (hadPreviousLane_) {
        *lane = previousLane_;
    } else {
        lane->regions.clear();
    }
    applied_ = false;
    return ok("Automation point undone");
}

std::string AddAutomationPointCommand::serialize() const {
    std::ostringstream output;
    output << "{\"name\":\"" << metadata_.name << "\",\"commandId\":\"" << metadata_.commandId
           << "\",\"auditId\":\"" << metadata_.auditId << "\",\"laneId\":\"" << lane_.id
           << "\",\"samplePosition\":" << samplePosition_ << ",\"value\":" << value_ << "}";
    return output.str();
}

CaptureAutomationWriteCommand::CaptureAutomationWriteCommand(
    std::string commandId, std::string auditId, session::AutomationLaneData lane,
    std::vector<session::AutomationWriteSample> samples, float thinningTolerance)
    : metadata_{.commandId = std::move(commandId),
                .auditId = std::move(auditId),
                .name = "capture_automation_write"},
      lane_(std::move(lane)), samples_(std::move(samples)), thinningTolerance_(thinningTolerance) {}

const CommandMetadata& CaptureAutomationWriteCommand::metadata() const noexcept {
    return metadata_;
}

CommandResult CaptureAutomationWriteCommand::validate(const AutomationLaneStore&) const {
    if (lane_.id.empty()) {
        return fail("Automation lane id must not be empty");
    }
    if (lane_.targetId.empty()) {
        return fail("Automation target id must not be empty");
    }
    if (lane_.parameterId.empty()) {
        return fail("Automation parameter id must not be empty");
    }
    if (samples_.empty()) {
        return fail("Automation write requires at least one sample");
    }
    if (thinningTolerance_ < 0.0F) {
        return fail("Automation thinning tolerance must not be negative");
    }
    if (std::ranges::any_of(samples_, [](const session::AutomationWriteSample& sample) {
            return sample.samplePosition < 0;
        })) {
        return fail("Automation write sample positions must not be negative");
    }
    return ok("Automation write can be captured");
}

std::string CaptureAutomationWriteCommand::preview(const AutomationLaneStore&) const {
    return "Capture automation write for lane \"" + lane_.id + "\"";
}

CommandResult CaptureAutomationWriteCommand::apply(AutomationLaneStore& store) {
    const auto validation = validate(store);
    if (!validation.ok) {
        return validation;
    }

    auto* previous = store.find(lane_.id);
    hadPreviousLane_ = previous != nullptr;
    if (previous != nullptr) {
        previousLane_ = *previous;
    }

    auto& lane = store.getOrCreate(lane_);
    capturedBatch_ = session::captureAutomationWrite(lane, samples_, thinningTolerance_);
    applied_ = true;
    return ok("Automation write captured");
}

CommandResult CaptureAutomationWriteCommand::undo(AutomationLaneStore& store) {
    if (!applied_) {
        return fail("Cannot undo command that has not been applied");
    }

    auto* lane = store.find(lane_.id);
    if (lane == nullptr) {
        return fail("Automation lane to undo was not found: " + lane_.id);
    }

    if (hadPreviousLane_) {
        *lane = previousLane_;
    } else {
        lane->regions.clear();
    }
    applied_ = false;
    return ok("Automation write undone");
}

std::string CaptureAutomationWriteCommand::serialize() const {
    std::ostringstream output;
    output << "{\"name\":\"" << metadata_.name << "\",\"commandId\":\"" << metadata_.commandId
           << "\",\"auditId\":\"" << metadata_.auditId << "\",\"laneId\":\"" << lane_.id
           << "\",\"samples\":" << samples_.size() << "}";
    return output.str();
}

SetChannelMixCommand::SetChannelMixCommand(std::string commandId, std::string auditId,
                                           std::string channelId, ChannelMixSettings settings)
    : metadata_{.commandId = std::move(commandId),
                .auditId = std::move(auditId),
                .name = "set_channel_mix"},
      channelId_(std::move(channelId)), settings_(settings) {}

const CommandMetadata& SetChannelMixCommand::metadata() const noexcept {
    return metadata_;
}

CommandResult SetChannelMixCommand::validate(const session::MixerState& mixer) const {
    if (session::findChannel(mixer, channelId_) == nullptr) {
        return fail("Mixer channel does not exist: " + channelId_);
    }
    if (settings_.pan.has_value() && (*settings_.pan < -1.0F || *settings_.pan > 1.0F)) {
        return fail("Mixer pan must be in the range -1..1");
    }
    if (!settings_.volumeDb.has_value() && !settings_.pan.has_value() &&
        !settings_.muted.has_value() && !settings_.solo.has_value()) {
        return fail("Mixer edit must change at least one field");
    }
    return ok("Mixer channel can be updated");
}

std::string SetChannelMixCommand::preview(const session::MixerState&) const {
    return "Update mixer channel \"" + channelId_ + "\"";
}

CommandResult SetChannelMixCommand::apply(session::MixerState& mixer) {
    const auto validation = validate(mixer);
    if (!validation.ok) {
        return validation;
    }

    auto* channel = session::findChannel(mixer, channelId_);
    previousChannel_ = *channel;
    if (settings_.volumeDb.has_value()) {
        channel->volumeDb = *settings_.volumeDb;
    }
    if (settings_.pan.has_value()) {
        channel->pan = *settings_.pan;
    }
    if (settings_.muted.has_value()) {
        channel->muted = *settings_.muted;
    }
    if (settings_.solo.has_value()) {
        channel->solo = *settings_.solo;
    }
    applied_ = true;
    return ok("Mixer channel updated");
}

CommandResult SetChannelMixCommand::undo(session::MixerState& mixer) {
    if (!applied_) {
        return fail("Cannot undo command that has not been applied");
    }

    auto* channel = session::findChannel(mixer, channelId_);
    if (channel == nullptr) {
        return fail("Mixer channel to undo was not found: " + channelId_);
    }

    *channel = previousChannel_;
    applied_ = false;
    return ok("Mixer channel update undone");
}

std::string SetChannelMixCommand::serialize() const {
    std::ostringstream output;
    output << "{\"name\":\"" << metadata_.name << "\",\"commandId\":\"" << metadata_.commandId
           << "\",\"auditId\":\"" << metadata_.auditId << "\",\"channelId\":\"" << channelId_
           << "\"";
    if (settings_.volumeDb.has_value()) {
        output << ",\"volumeDb\":" << *settings_.volumeDb;
    }
    if (settings_.pan.has_value()) {
        output << ",\"pan\":" << *settings_.pan;
    }
    if (settings_.muted.has_value()) {
        output << ",\"muted\":" << (*settings_.muted ? "true" : "false");
    }
    if (settings_.solo.has_value()) {
        output << ",\"solo\":" << (*settings_.solo ? "true" : "false");
    }
    output << "}";
    return output.str();
}

session::PluginInsertChain* PluginInsertChainStore::find(std::string_view trackId) noexcept {
    const auto found = std::ranges::find_if(
        chains_, [trackId](const auto& chain) { return chain.trackId == trackId; });
    return found == chains_.end() ? nullptr : &*found;
}

const session::PluginInsertChain*
PluginInsertChainStore::find(std::string_view trackId) const noexcept {
    const auto found = std::ranges::find_if(
        chains_, [trackId](const auto& chain) { return chain.trackId == trackId; });
    return found == chains_.end() ? nullptr : &*found;
}

session::PluginInsertChain& PluginInsertChainStore::getOrCreate(std::string trackId) {
    if (auto* chain = find(trackId); chain != nullptr) {
        return *chain;
    }

    chains_.push_back({.trackId = std::move(trackId)});
    return chains_.back();
}

AddPluginInsertCommand::AddPluginInsertCommand(std::string commandId, std::string auditId,
                                               std::string trackId, session::PluginInsert insert)
    : metadata_{.commandId = std::move(commandId),
                .auditId = std::move(auditId),
                .name = "add_plugin_insert"},
      trackId_(std::move(trackId)), insert_(std::move(insert)) {}

const CommandMetadata& AddPluginInsertCommand::metadata() const noexcept {
    return metadata_;
}

CommandResult AddPluginInsertCommand::validate(const PluginInsertChainStore& store) const {
    if (trackId_.empty()) {
        return fail("Plugin insert track id must not be empty");
    }
    if (insert_.id.empty()) {
        return fail("Plugin insert id must not be empty");
    }
    if (insert_.pluginIdentifier.empty()) {
        return fail("Plugin identifier must not be empty");
    }

    if (const auto* chain = store.find(trackId_); chain != nullptr) {
        if (session::findInsert(*chain, insert_.id) != nullptr) {
            return fail("Plugin insert id already exists: " + insert_.id);
        }
    }
    return ok("Plugin insert can be added");
}

std::string AddPluginInsertCommand::preview(const PluginInsertChainStore&) const {
    return "Add plugin insert \"" + insert_.id + "\" to track \"" + trackId_ + "\"";
}

CommandResult AddPluginInsertCommand::apply(PluginInsertChainStore& store) {
    const auto validation = validate(store);
    if (!validation.ok) {
        return validation;
    }

    session::addInsert(store.getOrCreate(trackId_), insert_);
    applied_ = true;
    return ok("Plugin insert added");
}

CommandResult AddPluginInsertCommand::undo(PluginInsertChainStore& store) {
    if (!applied_) {
        return fail("Cannot undo command that has not been applied");
    }

    auto* chain = store.find(trackId_);
    if (chain == nullptr) {
        return fail("Plugin insert chain to undo was not found: " + trackId_);
    }
    if (session::findInsert(*chain, insert_.id) == nullptr) {
        return fail("Plugin insert to undo was not found: " + insert_.id);
    }

    session::removeInsert(*chain, insert_.id);
    applied_ = false;
    return ok("Plugin insert removed");
}

std::string AddPluginInsertCommand::serialize() const {
    std::ostringstream output;
    output << "{\"name\":\"" << metadata_.name << "\",\"commandId\":\"" << metadata_.commandId
           << "\",\"auditId\":\"" << metadata_.auditId << "\",\"trackId\":\"" << trackId_
           << "\",\"insertId\":\"" << insert_.id << "\",\"pluginIdentifier\":\""
           << insert_.pluginIdentifier << "\"}";
    return output.str();
}

RemovePluginInsertCommand::RemovePluginInsertCommand(std::string commandId, std::string auditId,
                                                     std::string trackId, std::string insertId)
    : metadata_{.commandId = std::move(commandId),
                .auditId = std::move(auditId),
                .name = "remove_plugin_insert"},
      trackId_(std::move(trackId)), insertId_(std::move(insertId)) {}

const CommandMetadata& RemovePluginInsertCommand::metadata() const noexcept {
    return metadata_;
}

CommandResult RemovePluginInsertCommand::validate(const PluginInsertChainStore& store) const {
    const auto* chain = store.find(trackId_);
    if (chain == nullptr) {
        return fail("Plugin insert chain does not exist: " + trackId_);
    }
    if (insertId_.empty()) {
        return fail("Plugin insert id must not be empty");
    }
    if (session::findInsert(*chain, insertId_) == nullptr) {
        return fail("Plugin insert does not exist: " + insertId_);
    }
    return ok("Plugin insert can be removed");
}

std::string RemovePluginInsertCommand::preview(const PluginInsertChainStore&) const {
    return "Remove plugin insert \"" + insertId_ + "\" from track \"" + trackId_ + "\"";
}

CommandResult RemovePluginInsertCommand::apply(PluginInsertChainStore& store) {
    const auto validation = validate(store);
    if (!validation.ok) {
        return validation;
    }

    auto* chain = store.find(trackId_);
    const auto found = std::ranges::find_if(
        chain->inserts, [this](const auto& insert) { return insert.id == insertId_; });
    removedIndex_ = static_cast<std::size_t>(std::distance(chain->inserts.begin(), found));
    removedInsert_ = *found;
    chain->inserts.erase(found);
    applied_ = true;
    return ok("Plugin insert removed");
}

CommandResult RemovePluginInsertCommand::undo(PluginInsertChainStore& store) {
    if (!applied_) {
        return fail("Cannot undo command that has not been applied");
    }

    auto* chain = store.find(trackId_);
    if (chain == nullptr) {
        return fail("Plugin insert chain to undo was not found: " + trackId_);
    }
    if (session::findInsert(*chain, removedInsert_.id) != nullptr) {
        return fail("Plugin insert id already exists: " + removedInsert_.id);
    }

    const auto insertionIndex = std::min(removedIndex_, chain->inserts.size());
    chain->inserts.insert(chain->inserts.begin() + static_cast<std::ptrdiff_t>(insertionIndex),
                          removedInsert_);
    applied_ = false;
    return ok("Plugin insert removal undone");
}

std::string RemovePluginInsertCommand::serialize() const {
    std::ostringstream output;
    output << "{\"name\":\"" << metadata_.name << "\",\"commandId\":\"" << metadata_.commandId
           << "\",\"auditId\":\"" << metadata_.auditId << "\",\"trackId\":\"" << trackId_
           << "\",\"insertId\":\"" << insertId_ << "\"}";
    return output.str();
}

MovePluginInsertCommand::MovePluginInsertCommand(std::string commandId, std::string auditId,
                                                 std::string trackId, std::string insertId,
                                                 std::size_t newIndex)
    : metadata_{.commandId = std::move(commandId),
                .auditId = std::move(auditId),
                .name = "move_plugin_insert"},
      trackId_(std::move(trackId)), insertId_(std::move(insertId)), newIndex_(newIndex) {}

const CommandMetadata& MovePluginInsertCommand::metadata() const noexcept {
    return metadata_;
}

CommandResult MovePluginInsertCommand::validate(const PluginInsertChainStore& store) const {
    const auto* chain = store.find(trackId_);
    if (chain == nullptr) {
        return fail("Plugin insert chain does not exist: " + trackId_);
    }
    if (insertId_.empty()) {
        return fail("Plugin insert id must not be empty");
    }
    if (session::findInsert(*chain, insertId_) == nullptr) {
        return fail("Plugin insert does not exist: " + insertId_);
    }
    return ok("Plugin insert can be moved");
}

std::string MovePluginInsertCommand::preview(const PluginInsertChainStore&) const {
    return "Move plugin insert \"" + insertId_ + "\" to index " + std::to_string(newIndex_);
}

CommandResult MovePluginInsertCommand::apply(PluginInsertChainStore& store) {
    const auto validation = validate(store);
    if (!validation.ok) {
        return validation;
    }

    auto* chain = store.find(trackId_);
    const auto found = std::ranges::find_if(
        chain->inserts, [this](const auto& insert) { return insert.id == insertId_; });
    oldIndex_ = static_cast<std::size_t>(std::distance(chain->inserts.begin(), found));
    session::moveInsert(*chain, insertId_, newIndex_);
    applied_ = true;
    return ok("Plugin insert moved");
}

CommandResult MovePluginInsertCommand::undo(PluginInsertChainStore& store) {
    if (!applied_) {
        return fail("Cannot undo command that has not been applied");
    }

    auto* chain = store.find(trackId_);
    if (chain == nullptr) {
        return fail("Plugin insert chain to undo was not found: " + trackId_);
    }
    if (session::findInsert(*chain, insertId_) == nullptr) {
        return fail("Plugin insert to undo was not found: " + insertId_);
    }

    session::moveInsert(*chain, insertId_, oldIndex_);
    applied_ = false;
    return ok("Plugin insert move undone");
}

std::string MovePluginInsertCommand::serialize() const {
    std::ostringstream output;
    output << "{\"name\":\"" << metadata_.name << "\",\"commandId\":\"" << metadata_.commandId
           << "\",\"auditId\":\"" << metadata_.auditId << "\",\"trackId\":\"" << trackId_
           << "\",\"insertId\":\"" << insertId_ << "\",\"newIndex\":" << newIndex_ << "}";
    return output.str();
}

ApplyPluginPresetCommand::ApplyPluginPresetCommand(std::string commandId, std::string auditId,
                                                   std::string trackId, std::string insertId,
                                                   session::PluginPreset preset)
    : metadata_{.commandId = std::move(commandId),
                .auditId = std::move(auditId),
                .name = "apply_plugin_preset"},
      trackId_(std::move(trackId)), insertId_(std::move(insertId)), preset_(std::move(preset)) {}

const CommandMetadata& ApplyPluginPresetCommand::metadata() const noexcept {
    return metadata_;
}

CommandResult ApplyPluginPresetCommand::validate(const PluginInsertChainStore& store) const {
    const auto* chain = store.find(trackId_);
    if (chain == nullptr) {
        return fail("Plugin insert chain does not exist: " + trackId_);
    }
    const auto* insert = session::findInsert(*chain, insertId_);
    if (insert == nullptr) {
        return fail("Plugin insert does not exist: " + insertId_);
    }
    if (preset_.id.empty() || preset_.name.empty()) {
        return fail("Plugin preset id and name must not be empty");
    }
    if (preset_.pluginIdentifier != insert->pluginIdentifier) {
        return fail("Plugin preset does not match insert plugin identifier");
    }
    return ok("Plugin preset can be applied");
}

std::string ApplyPluginPresetCommand::preview(const PluginInsertChainStore&) const {
    return "Apply plugin preset \"" + preset_.name + "\" to insert \"" + insertId_ + "\"";
}

CommandResult ApplyPluginPresetCommand::apply(PluginInsertChainStore& store) {
    const auto validation = validate(store);
    if (!validation.ok) {
        return validation;
    }

    auto* insert = session::findInsert(*store.find(trackId_), insertId_);
    previousInsert_ = *insert;
    session::applyPreset(*insert, preset_);
    applied_ = true;
    return ok("Plugin preset applied");
}

CommandResult ApplyPluginPresetCommand::undo(PluginInsertChainStore& store) {
    if (!applied_) {
        return fail("Cannot undo command that has not been applied");
    }

    auto* chain = store.find(trackId_);
    if (chain == nullptr) {
        return fail("Plugin insert chain to undo was not found: " + trackId_);
    }
    auto* insert = session::findInsert(*chain, insertId_);
    if (insert == nullptr) {
        return fail("Plugin insert to undo was not found: " + insertId_);
    }

    *insert = previousInsert_;
    applied_ = false;
    return ok("Plugin preset application undone");
}

std::string ApplyPluginPresetCommand::serialize() const {
    std::ostringstream output;
    output << "{\"name\":\"" << metadata_.name << "\",\"commandId\":\"" << metadata_.commandId
           << "\",\"auditId\":\"" << metadata_.auditId << "\",\"trackId\":\"" << trackId_
           << "\",\"insertId\":\"" << insertId_ << "\",\"presetId\":\"" << preset_.id << "\"}";
    return output.str();
}

session::WarpState* WarpStateStore::find(std::string_view clipId) noexcept {
    const auto found = std::ranges::find_if(
        warps_, [clipId](const session::WarpState& warp) { return warp.clipId == clipId; });
    return found == warps_.end() ? nullptr : &*found;
}

const session::WarpState* WarpStateStore::find(std::string_view clipId) const noexcept {
    const auto found = std::ranges::find_if(
        warps_, [clipId](const session::WarpState& warp) { return warp.clipId == clipId; });
    return found == warps_.end() ? nullptr : &*found;
}

session::WarpState& WarpStateStore::getOrCreate(session::WarpState warp) {
    if (auto* existing = find(warp.clipId); existing != nullptr) {
        return *existing;
    }

    warps_.push_back(std::move(warp));
    return warps_.back();
}

AddWarpMarkerCommand::AddWarpMarkerCommand(std::string commandId, std::string auditId,
                                           session::WarpState warp, session::WarpMarker marker)
    : metadata_{.commandId = std::move(commandId),
                .auditId = std::move(auditId),
                .name = "add_warp_marker"},
      warp_(std::move(warp)), marker_(std::move(marker)) {}

const CommandMetadata& AddWarpMarkerCommand::metadata() const noexcept {
    return metadata_;
}

CommandResult AddWarpMarkerCommand::validate(const WarpStateStore& store) const {
    if (warp_.clipId.empty()) {
        return fail("Warp clip id must not be empty");
    }
    if (marker_.id.empty()) {
        return fail("Warp marker id must not be empty");
    }
    if (marker_.sourceSample < 0 || marker_.timelineSample < 0) {
        return fail("Warp marker samples must not be negative");
    }
    if (const auto* warp = store.find(warp_.clipId); warp != nullptr) {
        if (std::ranges::any_of(warp->markers, [this](const session::WarpMarker& marker) {
                return marker.id == marker_.id;
            })) {
            return fail("Warp marker id already exists: " + marker_.id);
        }
    }
    return ok("Warp marker can be added");
}

std::string AddWarpMarkerCommand::preview(const WarpStateStore&) const {
    return "Add warp marker \"" + marker_.id + "\" to clip \"" + warp_.clipId + "\"";
}

CommandResult AddWarpMarkerCommand::apply(WarpStateStore& store) {
    const auto validation = validate(store);
    if (!validation.ok) {
        return validation;
    }

    auto& warp = store.getOrCreate(warp_);
    warp.markers.push_back(marker_);
    applied_ = true;
    return ok("Warp marker added");
}

CommandResult AddWarpMarkerCommand::undo(WarpStateStore& store) {
    if (!applied_) {
        return fail("Cannot undo command that has not been applied");
    }

    auto* warp = store.find(warp_.clipId);
    if (warp == nullptr) {
        return fail("Warp state to undo was not found: " + warp_.clipId);
    }

    const auto oldSize = warp->markers.size();
    std::erase_if(warp->markers,
                  [this](const session::WarpMarker& marker) { return marker.id == marker_.id; });
    if (warp->markers.size() == oldSize) {
        return fail("Warp marker to undo was not found: " + marker_.id);
    }

    applied_ = false;
    return ok("Warp marker removed");
}

std::string AddWarpMarkerCommand::serialize() const {
    std::ostringstream output;
    output << "{\"name\":\"" << metadata_.name << "\",\"commandId\":\"" << metadata_.commandId
           << "\",\"auditId\":\"" << metadata_.auditId << "\",\"clipId\":\"" << warp_.clipId
           << "\",\"markerId\":\"" << marker_.id << "\",\"sourceSample\":" << marker_.sourceSample
           << ",\"timelineSample\":" << marker_.timelineSample << "}";
    return output.str();
}

MoveWarpMarkerCommand::MoveWarpMarkerCommand(std::string commandId, std::string auditId,
                                             std::string clipId, std::string markerId,
                                             std::int64_t newSourceSample,
                                             std::int64_t newTimelineSample)
    : metadata_{.commandId = std::move(commandId),
                .auditId = std::move(auditId),
                .name = "move_warp_marker"},
      clipId_(std::move(clipId)), markerId_(std::move(markerId)), newSourceSample_(newSourceSample),
      newTimelineSample_(newTimelineSample) {}

const CommandMetadata& MoveWarpMarkerCommand::metadata() const noexcept {
    return metadata_;
}

CommandResult MoveWarpMarkerCommand::validate(const WarpStateStore& store) const {
    const auto* warp = store.find(clipId_);
    if (warp == nullptr) {
        return fail("Warp state does not exist: " + clipId_);
    }
    if (markerId_.empty()) {
        return fail("Warp marker id must not be empty");
    }
    if (newSourceSample_ < 0 || newTimelineSample_ < 0) {
        return fail("Warp marker samples must not be negative");
    }
    if (!std::ranges::any_of(warp->markers, [this](const session::WarpMarker& marker) {
            return marker.id == markerId_;
        })) {
        return fail("Warp marker does not exist: " + markerId_);
    }
    return ok("Warp marker can be moved");
}

std::string MoveWarpMarkerCommand::preview(const WarpStateStore&) const {
    return "Move warp marker \"" + markerId_ + "\"";
}

CommandResult MoveWarpMarkerCommand::apply(WarpStateStore& store) {
    const auto validation = validate(store);
    if (!validation.ok) {
        return validation;
    }

    auto* warp = store.find(clipId_);
    auto found = std::ranges::find_if(warp->markers, [this](const session::WarpMarker& marker) {
        return marker.id == markerId_;
    });
    previousMarker_ = *found;
    found->sourceSample = newSourceSample_;
    found->timelineSample = newTimelineSample_;
    applied_ = true;
    return ok("Warp marker moved");
}

CommandResult MoveWarpMarkerCommand::undo(WarpStateStore& store) {
    if (!applied_) {
        return fail("Cannot undo command that has not been applied");
    }

    auto* warp = store.find(clipId_);
    if (warp == nullptr) {
        return fail("Warp state to undo was not found: " + clipId_);
    }
    auto found = std::ranges::find_if(warp->markers, [this](const session::WarpMarker& marker) {
        return marker.id == markerId_;
    });
    if (found == warp->markers.end()) {
        return fail("Warp marker to undo was not found: " + markerId_);
    }

    *found = previousMarker_;
    applied_ = false;
    return ok("Warp marker move undone");
}

std::string MoveWarpMarkerCommand::serialize() const {
    std::ostringstream output;
    output << "{\"name\":\"" << metadata_.name << "\",\"commandId\":\"" << metadata_.commandId
           << "\",\"auditId\":\"" << metadata_.auditId << "\",\"clipId\":\"" << clipId_
           << "\",\"markerId\":\"" << markerId_ << "\",\"sourceSample\":" << newSourceSample_
           << ",\"timelineSample\":" << newTimelineSample_ << "}";
    return output.str();
}

RemoveWarpMarkerCommand::RemoveWarpMarkerCommand(std::string commandId, std::string auditId,
                                                 std::string clipId, std::string markerId)
    : metadata_{.commandId = std::move(commandId),
                .auditId = std::move(auditId),
                .name = "remove_warp_marker"},
      clipId_(std::move(clipId)), markerId_(std::move(markerId)) {}

const CommandMetadata& RemoveWarpMarkerCommand::metadata() const noexcept {
    return metadata_;
}

CommandResult RemoveWarpMarkerCommand::validate(const WarpStateStore& store) const {
    const auto* warp = store.find(clipId_);
    if (warp == nullptr) {
        return fail("Warp state does not exist: " + clipId_);
    }
    if (markerId_.empty()) {
        return fail("Warp marker id must not be empty");
    }
    if (!std::ranges::any_of(warp->markers, [this](const session::WarpMarker& marker) {
            return marker.id == markerId_;
        })) {
        return fail("Warp marker does not exist: " + markerId_);
    }
    return ok("Warp marker can be removed");
}

std::string RemoveWarpMarkerCommand::preview(const WarpStateStore&) const {
    return "Remove warp marker \"" + markerId_ + "\"";
}

CommandResult RemoveWarpMarkerCommand::apply(WarpStateStore& store) {
    const auto validation = validate(store);
    if (!validation.ok) {
        return validation;
    }

    auto* warp = store.find(clipId_);
    const auto found =
        std::ranges::find_if(warp->markers, [this](const session::WarpMarker& marker) {
            return marker.id == markerId_;
        });
    removedIndex_ = static_cast<std::size_t>(std::distance(warp->markers.begin(), found));
    removedMarker_ = *found;
    warp->markers.erase(found);
    applied_ = true;
    return ok("Warp marker removed");
}

CommandResult RemoveWarpMarkerCommand::undo(WarpStateStore& store) {
    if (!applied_) {
        return fail("Cannot undo command that has not been applied");
    }

    auto* warp = store.find(clipId_);
    if (warp == nullptr) {
        return fail("Warp state to undo was not found: " + clipId_);
    }
    if (std::ranges::any_of(warp->markers, [this](const session::WarpMarker& marker) {
            return marker.id == removedMarker_.id;
        })) {
        return fail("Warp marker id already exists: " + removedMarker_.id);
    }

    const auto insertionIndex = std::min(removedIndex_, warp->markers.size());
    warp->markers.insert(warp->markers.begin() + static_cast<std::ptrdiff_t>(insertionIndex),
                         removedMarker_);
    applied_ = false;
    return ok("Warp marker removal undone");
}

std::string RemoveWarpMarkerCommand::serialize() const {
    std::ostringstream output;
    output << "{\"name\":\"" << metadata_.name << "\",\"commandId\":\"" << metadata_.commandId
           << "\",\"auditId\":\"" << metadata_.auditId << "\",\"clipId\":\"" << clipId_
           << "\",\"markerId\":\"" << markerId_ << "\"}";
    return output.str();
}

QuantizeWarpMarkersCommand::QuantizeWarpMarkersCommand(std::string commandId, std::string auditId,
                                                       std::string clipId, std::int64_t gridSamples,
                                                       float strength)
    : metadata_{.commandId = std::move(commandId),
                .auditId = std::move(auditId),
                .name = "quantize_warp_markers"},
      clipId_(std::move(clipId)), gridSamples_(gridSamples), strength_(strength) {}

const CommandMetadata& QuantizeWarpMarkersCommand::metadata() const noexcept {
    return metadata_;
}

CommandResult QuantizeWarpMarkersCommand::validate(const WarpStateStore& store) const {
    if (store.find(clipId_) == nullptr) {
        return fail("Warp state does not exist: " + clipId_);
    }
    if (gridSamples_ <= 0) {
        return fail("Warp quantize grid must be positive");
    }
    if (strength_ < 0.0F || strength_ > 1.0F) {
        return fail("Warp quantize strength must be in the range 0..1");
    }
    return ok("Warp markers can be quantized");
}

std::string QuantizeWarpMarkersCommand::preview(const WarpStateStore&) const {
    return "Quantize warp markers on clip \"" + clipId_ + "\"";
}

CommandResult QuantizeWarpMarkersCommand::apply(WarpStateStore& store) {
    const auto validation = validate(store);
    if (!validation.ok) {
        return validation;
    }

    auto* warp = store.find(clipId_);
    previousWarp_ = *warp;
    session::quantizeWarpMarkers(*warp, gridSamples_, strength_);
    applied_ = true;
    return ok("Warp markers quantized");
}

CommandResult QuantizeWarpMarkersCommand::undo(WarpStateStore& store) {
    if (!applied_) {
        return fail("Cannot undo command that has not been applied");
    }

    auto* warp = store.find(clipId_);
    if (warp == nullptr) {
        return fail("Warp state to undo was not found: " + clipId_);
    }

    *warp = previousWarp_;
    applied_ = false;
    return ok("Warp marker quantize undone");
}

std::string QuantizeWarpMarkersCommand::serialize() const {
    std::ostringstream output;
    output << "{\"name\":\"" << metadata_.name << "\",\"commandId\":\"" << metadata_.commandId
           << "\",\"auditId\":\"" << metadata_.auditId << "\",\"clipId\":\"" << clipId_
           << "\",\"gridSamples\":" << gridSamples_ << ",\"strength\":" << strength_ << "}";
    return output.str();
}

TransactionCommand::TransactionCommand(std::string commandId, std::string auditId, std::string name,
                                       std::vector<CommandPtr> commands)
    : metadata_{.commandId = std::move(commandId),
                .auditId = std::move(auditId),
                .name = std::move(name)},
      commands_(std::move(commands)) {}

const CommandMetadata& TransactionCommand::metadata() const noexcept {
    return metadata_;
}

CommandResult TransactionCommand::validate(const session::ProjectManifest& manifest) const {
    auto snapshot = manifest;
    for (const auto& command : commands_) {
        const auto validation = command->validate(snapshot);
        if (!validation.ok) {
            return validation;
        }
        auto mutableCommandPreview = command->preview(snapshot);
        static_cast<void>(mutableCommandPreview);
    }
    return ok("Transaction can be applied");
}

std::string TransactionCommand::preview(const session::ProjectManifest& manifest) const {
    std::ostringstream output;
    output << metadata_.name << ":";
    for (const auto& command : commands_) {
        output << "\n- " << command->preview(manifest);
    }
    return output.str();
}

CommandResult TransactionCommand::apply(session::ProjectManifest& manifest) {
    appliedCount_ = 0;
    for (auto& command : commands_) {
        const auto result = command->apply(manifest);
        if (!result.ok) {
            while (appliedCount_ > 0) {
                --appliedCount_;
                static_cast<void>(commands_[appliedCount_]->undo(manifest));
            }
            return result;
        }
        ++appliedCount_;
    }
    return ok("Transaction applied");
}

CommandResult TransactionCommand::undo(session::ProjectManifest& manifest) {
    while (appliedCount_ > 0) {
        --appliedCount_;
        const auto result = commands_[appliedCount_]->undo(manifest);
        if (!result.ok) {
            return result;
        }
    }
    return ok("Transaction undone");
}

std::string TransactionCommand::serialize() const {
    std::ostringstream output;
    output << "{\"name\":\"" << metadata_.name << "\",\"commandId\":\"" << metadata_.commandId
           << "\",\"auditId\":\"" << metadata_.auditId << "\",\"commands\":[";
    for (std::size_t index = 0; index < commands_.size(); ++index) {
        output << commands_[index]->serialize();
        if (index + 1 < commands_.size()) {
            output << ',';
        }
    }
    output << "]}";
    return output.str();
}

CommandResult CommandHistory::execute(session::ProjectManifest& manifest, CommandPtr command) {
    const auto result = command->apply(manifest);
    if (!result.ok) {
        return result;
    }

    undoStack_.push_back(std::move(command));
    redoStack_.clear();
    return result;
}

CommandResult CommandHistory::undo(session::ProjectManifest& manifest) {
    if (undoStack_.empty()) {
        return fail("No command to undo");
    }

    auto command = std::move(undoStack_.back());
    undoStack_.pop_back();

    const auto result = command->undo(manifest);
    if (!result.ok) {
        undoStack_.push_back(std::move(command));
        return result;
    }

    redoStack_.push_back(std::move(command));
    return result;
}

CommandResult CommandHistory::redo(session::ProjectManifest& manifest) {
    if (redoStack_.empty()) {
        return fail("No command to redo");
    }

    auto command = std::move(redoStack_.back());
    redoStack_.pop_back();

    const auto result = command->apply(manifest);
    if (!result.ok) {
        redoStack_.push_back(std::move(command));
        return result;
    }

    undoStack_.push_back(std::move(command));
    return result;
}

bool CommandHistory::canUndo() const noexcept {
    return !undoStack_.empty();
}

bool CommandHistory::canRedo() const noexcept {
    return !redoStack_.empty();
}

std::size_t CommandHistory::undoDepth() const noexcept {
    return undoStack_.size();
}

std::size_t CommandHistory::redoDepth() const noexcept {
    return redoStack_.size();
}

std::vector<std::string> registeredProjectCommandNames() {
    return {"add_track",
            "add_clip",
            "remove_clip",
            "duplicate_clip",
            "move_clip",
            "trim_clip",
            "set_clip_fade",
            "set_track_name",
            "add_routing_connection",
            "remove_routing_connection",
            "split_clip",
            "transaction"};
}

CommandPtr commandFromSerialized(std::string_view serializedCommand) {
    const auto name = readSerializedString(serializedCommand, "name");
    const auto commandId = readSerializedString(serializedCommand, "commandId");
    const auto auditId = readSerializedString(serializedCommand, "auditId");

    if (name == "add_track") {
        return makeAddTrackCommand(commandId, auditId,
                                   {.id = readSerializedString(serializedCommand, "trackId"),
                                    .name = readSerializedString(serializedCommand, "trackName"),
                                    .type = session::trackTypeFromString(
                                        readSerializedString(serializedCommand, "trackType"))});
    }
    if (name == "add_clip") {
        return makeAddClipCommand(
            commandId, auditId,
            {.id = readSerializedString(serializedCommand, "clipId"),
             .trackId = readSerializedString(serializedCommand, "trackId"),
             .type = session::ClipType::Audio,
             .startSample = readSerializedInt64(serializedCommand, "startSample"),
             .lengthSamples = readSerializedInt64(serializedCommand, "lengthSamples")});
    }
    if (name == "remove_clip") {
        return makeRemoveClipCommand(commandId, auditId,
                                     readSerializedString(serializedCommand, "clipId"));
    }
    if (name == "duplicate_clip") {
        return makeDuplicateClipCommand(
            commandId, auditId, readSerializedString(serializedCommand, "sourceClipId"),
            readSerializedString(serializedCommand, "duplicateClipId"),
            readSerializedInt64(serializedCommand, "duplicateStartSample"));
    }
    if (name == "move_clip") {
        return makeMoveClipCommand(commandId, auditId,
                                   readSerializedString(serializedCommand, "clipId"),
                                   readSerializedInt64(serializedCommand, "newStartSample"));
    }
    if (name == "trim_clip") {
        return makeTrimClipCommand(
            commandId, auditId, readSerializedString(serializedCommand, "clipId"),
            readSerializedInt64(serializedCommand, "newStartSample"),
            readSerializedInt64(serializedCommand, "newLengthSamples"),
            readSerializedInt64(serializedCommand, "newSourceOffsetSamples"));
    }
    if (name == "set_clip_fade") {
        return makeSetClipFadeCommand(commandId, auditId,
                                      readSerializedString(serializedCommand, "clipId"),
                                      readSerializedInt64(serializedCommand, "fadeInSamples"),
                                      readSerializedInt64(serializedCommand, "fadeOutSamples"));
    }
    if (name == "set_track_name") {
        return makeSetTrackNameCommand(commandId, auditId,
                                       readSerializedString(serializedCommand, "trackId"),
                                       readSerializedString(serializedCommand, "trackName"));
    }
    if (name == "add_routing_connection") {
        return makeAddRoutingConnectionCommand(
            commandId, auditId,
            {.sourceTrackId = readSerializedString(serializedCommand, "sourceTrackId"),
             .destinationTrackId = readSerializedString(serializedCommand, "destinationTrackId")});
    }
    if (name == "remove_routing_connection") {
        return makeRemoveRoutingConnectionCommand(
            commandId, auditId,
            {.sourceTrackId = readSerializedString(serializedCommand, "sourceTrackId"),
             .destinationTrackId = readSerializedString(serializedCommand, "destinationTrackId")});
    }

    throw std::runtime_error("Serialized project command is not registered: " + name);
}

CommandReplayReport replaySerializedCommands(session::ProjectManifest& manifest,
                                             const std::vector<std::string>& serializedCommands) {
    CommandReplayReport report;
    CommandHistory history;
    for (const auto& serializedCommand : serializedCommands) {
        auto command = commandFromSerialized(serializedCommand);
        auto result = history.execute(manifest, std::move(command));
        report.results.push_back(result);
        if (!result.ok) {
            return report;
        }
        ++report.appliedCount;
    }
    return report;
}

CommandPtr makeAddTrackCommand(std::string commandId, std::string auditId, session::Track track) {
    return std::make_unique<AddTrackCommand>(std::move(commandId), std::move(auditId),
                                             std::move(track));
}

CommandPtr makeAddClipCommand(std::string commandId, std::string auditId, session::Clip clip) {
    return std::make_unique<AddClipCommand>(std::move(commandId), std::move(auditId),
                                            std::move(clip));
}

CommandPtr makeRemoveClipCommand(std::string commandId, std::string auditId, std::string clipId) {
    return std::make_unique<RemoveClipCommand>(std::move(commandId), std::move(auditId),
                                               std::move(clipId));
}

CommandPtr makeDuplicateClipCommand(std::string commandId, std::string auditId,
                                    std::string sourceClipId, std::string duplicateClipId,
                                    std::int64_t duplicateStartSample) {
    return std::make_unique<DuplicateClipCommand>(std::move(commandId), std::move(auditId),
                                                  std::move(sourceClipId),
                                                  std::move(duplicateClipId), duplicateStartSample);
}

CommandPtr makeMoveClipCommand(std::string commandId, std::string auditId, std::string clipId,
                               std::int64_t newStartSample) {
    return std::make_unique<MoveClipCommand>(std::move(commandId), std::move(auditId),
                                             std::move(clipId), newStartSample);
}

CommandPtr makeTrimClipCommand(std::string commandId, std::string auditId, std::string clipId,
                               std::int64_t newStartSample, std::int64_t newLengthSamples,
                               std::int64_t newSourceOffsetSamples) {
    return std::make_unique<TrimClipCommand>(std::move(commandId), std::move(auditId),
                                             std::move(clipId), newStartSample, newLengthSamples,
                                             newSourceOffsetSamples);
}

CommandPtr makeSetClipFadeCommand(std::string commandId, std::string auditId, std::string clipId,
                                  std::int64_t fadeInSamples, std::int64_t fadeOutSamples) {
    return std::make_unique<SetClipFadeCommand>(std::move(commandId), std::move(auditId),
                                                std::move(clipId), fadeInSamples, fadeOutSamples);
}

CommandPtr makeSplitClipCommand(const session::ProjectManifest& manifest, std::string commandId,
                                std::string auditId, std::string leftClipId,
                                std::string rightClipId, std::int64_t splitSample) {
    const auto* clip = findClip(manifest, leftClipId);
    if (clip == nullptr) {
        return std::make_unique<TransactionCommand>(std::move(commandId), std::move(auditId),
                                                    "split_clip", std::vector<CommandPtr>{});
    }

    const auto clipEnd = clip->startSample + clip->lengthSamples;
    const auto leftLength = splitSample - clip->startSample;
    const auto rightLength = clipEnd - splitSample;

    auto rightClip = *clip;
    rightClip.id = std::move(rightClipId);
    rightClip.startSample = splitSample;
    rightClip.lengthSamples = rightLength;
    rightClip.sourceOffsetSamples = clip->sourceOffsetSamples + leftLength;
    rightClip.fadeInSamples = 0;

    std::vector<CommandPtr> commands;
    commands.push_back(makeTrimClipCommand(commandId + "-trim", auditId + "-trim",
                                           std::move(leftClipId), clip->startSample, leftLength,
                                           clip->sourceOffsetSamples));
    commands.push_back(
        makeAddClipCommand(commandId + "-add-right", auditId + "-add-right", std::move(rightClip)));

    return std::make_unique<TransactionCommand>(std::move(commandId), std::move(auditId),
                                                "split_clip", std::move(commands));
}

CommandPtr makeSetTrackNameCommand(std::string commandId, std::string auditId, std::string trackId,
                                   std::string name) {
    return std::make_unique<SetTrackNameCommand>(std::move(commandId), std::move(auditId),
                                                 std::move(trackId), std::move(name));
}

CommandPtr makeAddRoutingConnectionCommand(std::string commandId, std::string auditId,
                                           session::RoutingConnection connection) {
    return std::make_unique<AddRoutingConnectionCommand>(std::move(commandId), std::move(auditId),
                                                         std::move(connection));
}

CommandPtr makeRemoveRoutingConnectionCommand(std::string commandId, std::string auditId,
                                              session::RoutingConnection connection) {
    return std::make_unique<RemoveRoutingConnectionCommand>(
        std::move(commandId), std::move(auditId), std::move(connection));
}

} // namespace lamusica::commands
