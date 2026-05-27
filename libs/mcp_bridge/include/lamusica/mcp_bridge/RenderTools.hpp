#pragma once

#include "lamusica/audio/AudioEngine.hpp"
#include "lamusica/audio/AudioGraph.hpp"
#include "lamusica/audio/Bounce.hpp"
#include "lamusica/mcp_bridge/DaemonSession.hpp"
#include "lamusica/session/Export.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace lamusica::mcp_bridge {

enum class RenderJobStatus {
    Queued,
    Running,
    Completed,
    Cancelled,
    Failed,
};

enum class AudioFileTransform {
    Normalize,
    Reverse,
};

struct RenderJob {
    std::string id;
    RenderJobStatus status{RenderJobStatus::Queued};
    float progress{0.0F};
    std::filesystem::path outputPath;
    std::string message;
    std::string resultManifestJson;
    std::string confirmationToken;
};

struct RenderWriteOptions {
    bool allowOverwrite{false};
    std::string confirmationToken;
};

struct BounceInPlaceOptions {
    std::string assetId;
    std::filesystem::path assetRelativePath;
    std::string clipId;
    std::string trackId;
    std::int64_t clipStartSample{0};
};

struct FreezeTrackOptions {
    std::string trackId;
    std::string assetId;
    std::filesystem::path assetRelativePath;
    std::string clipId;
    std::int64_t clipStartSample{0};
};

class RenderJobQueue {
  public:
    [[nodiscard]] const std::vector<RenderJob>& jobs() const noexcept;
    [[nodiscard]] RenderJob* find(std::string_view jobId) noexcept;

    RenderJob enqueueTestTone(const DaemonSession& session, std::string jobId,
                              std::filesystem::path outputPath);
    RenderJob enqueueAnalyzeWav(const DaemonSession& session, std::string jobId,
                                std::filesystem::path inputPath);
    RenderJob enqueueTransformWav(const DaemonSession& session, std::string jobId,
                                  std::filesystem::path inputPath, std::filesystem::path outputPath,
                                  AudioFileTransform transform,
                                  RenderWriteOptions writeOptions = {});
    RenderJob enqueueGraphBounce(const DaemonSession& session, std::string jobId,
                                 const audio::AudioGraph& graph, audio::BounceOptions options,
                                 RenderWriteOptions writeOptions = {});
    RenderJob enqueueProjectMixExport(const DaemonSession& session, std::string jobId,
                                      const session::ProjectManifest& manifest,
                                      const session::MixerState& mixer,
                                      session::ProjectExportOptions options,
                                      RenderWriteOptions writeOptions = {});
    RenderJob enqueueBatchProjectMixExport(const DaemonSession& session, std::string jobId,
                                           const session::ProjectManifest& manifest,
                                           const session::MixerState& mixer,
                                           std::vector<session::ProjectExportOptions> exports,
                                           RenderWriteOptions writeOptions = {});
    RenderJob enqueueProjectStemExport(const DaemonSession& session, std::string jobId,
                                       const session::ProjectManifest& manifest,
                                       const session::MixerState& mixer,
                                       session::StemExportOptions options,
                                       RenderWriteOptions writeOptions = {});
    RenderJob enqueueBounceInPlace(const DaemonSession& session, std::string jobId,
                                   session::ProjectManifest& manifest,
                                   const audio::AudioGraph& graph, audio::BounceOptions options,
                                   BounceInPlaceOptions placement,
                                   RenderWriteOptions writeOptions = {});
    RenderJob enqueueFreezeTrack(const DaemonSession& session, std::string jobId,
                                 session::ProjectManifest& manifest, const audio::AudioGraph& graph,
                                 audio::BounceOptions options, FreezeTrackOptions freeze,
                                 RenderWriteOptions writeOptions = {});
    RenderJob enqueuePending(std::string jobId, std::filesystem::path outputPath,
                             std::string message = "queued");
    bool cancel(std::string_view jobId);

  private:
    std::vector<RenderJob> jobs_;
};

[[nodiscard]] std::string renderConfirmationToken(const std::filesystem::path& outputPath);
[[nodiscard]] std::string renderJobJson(const RenderJob& job);

} // namespace lamusica::mcp_bridge
