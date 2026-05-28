#pragma once

#include "lamusica/session/Automation.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace lamusica::session {

inline constexpr std::uint32_t currentProjectSchemaVersion{1};

struct TempoEvent {
    std::int64_t samplePosition{0};
    double bpm{120.0};
};

struct TimeSignatureEvent {
    std::int64_t samplePosition{0};
    std::uint32_t numerator{4};
    std::uint32_t denominator{4};
};

struct Marker {
    std::string id;
    std::string name;
    std::int64_t samplePosition{0};
};

struct Asset {
    std::string id;
    std::filesystem::path relativePath;
    std::string mediaType;
};

enum class TrackType {
    Audio,
    Midi,
    Instrument,
    Group,
    Return,
    Master,
};

[[nodiscard]] std::string_view toString(TrackType type) noexcept;
[[nodiscard]] TrackType trackTypeFromString(std::string_view value);

struct Track {
    std::string id;
    std::string name;
    TrackType type{TrackType::Audio};
};

enum class ClipType {
    Audio,
    Midi,
    Pattern,
};

[[nodiscard]] std::string_view toString(ClipType type) noexcept;
[[nodiscard]] ClipType clipTypeFromString(std::string_view value);

struct Clip {
    std::string id;
    std::string trackId;
    ClipType type{ClipType::Audio};
    std::int64_t startSample{0};
    std::int64_t lengthSamples{0};
    std::int64_t sourceOffsetSamples{0};
    std::int64_t fadeInSamples{0};
    std::int64_t fadeOutSamples{0};
    float gainDb{0.0F};
    bool muted{false};
    bool reversed{false};
    std::string assetId;
};

struct MidiClipReference {
    std::string clipId;
    std::string dataId;
};

struct RoutingConnection {
    std::string sourceTrackId;
    std::string destinationTrackId;
};

struct PluginReference {
    std::string id;
    std::string trackId;
    std::string format;
    std::string identifier;
};

struct AutomationLane {
    std::string id;
    AutomationTargetKind targetKind{AutomationTargetKind::Mixer};
    std::string targetId;
    std::string parameterId;
    AutomationMode mode{AutomationMode::Read};
    float defaultValue{0.0F};
    std::vector<AutomationRegion> regions;
};

struct McpAuditEntry {
    std::string id;
    std::string toolName;
    std::string capability;
};

struct ProjectManifest {
    std::uint32_t schemaVersion{1};
    std::string name{"Untitled"};
    std::vector<TempoEvent> tempoMap{{}};
    std::vector<TimeSignatureEvent> timeSignatures{{}};
    std::vector<Marker> markers;
    std::vector<Asset> assets;
    std::vector<Track> tracks;
    std::vector<Clip> clips;
    std::vector<MidiClipReference> midiClips;
    std::vector<RoutingConnection> routing;
    std::vector<PluginReference> plugins;
    std::vector<AutomationLane> automation;
    std::vector<McpAuditEntry> mcpAuditLog;
};

[[nodiscard]] std::string serializeProjectManifest(const ProjectManifest& manifest);
[[nodiscard]] ProjectManifest parseProjectManifest(std::string_view json);
[[nodiscard]] ProjectManifest migrateProjectManifest(ProjectManifest manifest);
void validateProjectManifest(const ProjectManifest& manifest);

} // namespace lamusica::session
