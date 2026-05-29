#include "lamusica/mcp_bridge/RenderTools.hpp"

#include "lamusica/audio/Bounce.hpp"
#include "lamusica/audio/WavFile.hpp"
#include "lamusica/session/Assets.hpp"
#include "lamusica/session/Export.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <sstream>

namespace lamusica::mcp_bridge {
namespace {

std::string escapeJson(std::string_view value) {
    std::string escaped;
    escaped.reserve(value.size());
    for (const char rawCharacter : value) {
        const auto character = static_cast<unsigned char>(rawCharacter);
        switch (character) {
        case '"':
        case '\\':
            escaped.push_back('\\');
            escaped.push_back(static_cast<char>(character));
            break;
        case '\b':
            escaped += "\\b";
            break;
        case '\f':
            escaped += "\\f";
            break;
        case '\n':
            escaped += "\\n";
            break;
        case '\r':
            escaped += "\\r";
            break;
        case '\t':
            escaped += "\\t";
            break;
        default:
            if (character < 0x20) {
                constexpr char hex[] = "0123456789abcdef";
                escaped += "\\u00";
                escaped.push_back(hex[(character >> 4U) & 0x0FU]);
                escaped.push_back(hex[character & 0x0FU]);
            } else {
                escaped.push_back(static_cast<char>(character));
            }
            break;
        }
    }
    return escaped;
}

std::string_view toString(RenderJobStatus status) noexcept {
    switch (status) {
    case RenderJobStatus::Queued:
        return "queued";
    case RenderJobStatus::Running:
        return "running";
    case RenderJobStatus::Completed:
        return "completed";
    case RenderJobStatus::Cancelled:
        return "cancelled";
    case RenderJobStatus::Failed:
        return "failed";
    }
    return "failed";
}

float rms(const audio::RenderedAudio& rendered) noexcept {
    if (rendered.interleavedSamples.empty()) {
        return 0.0F;
    }

    double sumSquares = 0.0;
    for (const auto sample : rendered.interleavedSamples) {
        sumSquares += static_cast<double>(sample) * static_cast<double>(sample);
    }
    return static_cast<float>(
        std::sqrt(sumSquares / static_cast<double>(rendered.interleavedSamples.size())));
}

std::string analysisManifestJson(const std::filesystem::path& path, const audio::WavAudioData& wav,
                                 bool explicitExport = false) {
    const auto peak = audio::peakAbsoluteSample(wav.audio);
    const auto rmsValue = rms(wav.audio);
    const auto lufsEstimate = rmsValue <= 0.0F ? -90.0F : (20.0F * std::log10(rmsValue));
    constexpr std::int64_t samplesPerBucket = 1024;
    const auto mediaAnalysis = session::analyzeAudioAsset(path.filename().string(), wav.audio,
                                                          wav.sampleRate, samplesPerBucket);
    std::ostringstream output;
    output << "{\"schemaVersion\":1,\"type\":\"wav_analysis\",\"explicitExport\":"
           << (explicitExport ? "true" : "false") << ",\"path\":\"" << escapeJson(path.string())
           << "\",\"frames\":" << wav.audio.frames << ",\"channels\":" << wav.audio.channels
           << ",\"sampleRate\":" << wav.sampleRate << ",\"bitsPerSample\":" << wav.bitsPerSample
           << ",\"peak\":" << peak << ",\"rms\":" << rmsValue
           << ",\"lufsEstimate\":" << lufsEstimate
           << ",\"tempoBpm\":" << mediaAnalysis.analysis.tempoBpm << ",\"musicalKey\":\""
           << escapeJson(mediaAnalysis.analysis.musicalKey)
           << "\",\"transientCount\":" << mediaAnalysis.analysis.transientSamples.size()
           << ",\"waveform\":{\"valid\":" << (mediaAnalysis.waveform.valid ? "true" : "false")
           << ",\"samplesPerBucket\":" << mediaAnalysis.waveform.samplesPerBucket
           << ",\"bucketCount\":" << mediaAnalysis.waveform.buckets.size() << "}}";
    return output.str();
}

std::string bounceManifestJson(const audio::BounceResult& bounce) {
    std::ostringstream output;
    output << "{\"schemaVersion\":1,\"type\":\"graph_bounce\",\"explicitExport\":true,"
           << "\"outputPath\":\"" << escapeJson(bounce.outputPath.string())
           << "\",\"startSample\":" << bounce.startSample << ",\"frames\":" << bounce.frames
           << ",\"channels\":" << bounce.channels << ",\"sampleRate\":" << bounce.sampleRate
           << ",\"peakBeforeNormalization\":" << bounce.peakBeforeNormalization
           << ",\"peakAfterNormalization\":" << bounce.peakAfterNormalization
           << ",\"peakAfterDither\":" << bounce.peakAfterDither
           << ",\"postDitherPeak\":" << bounce.peakAfterDither << "}";
    return output.str();
}

std::string projectMixManifestJson(const audio::BounceResult& bounce) {
    std::ostringstream output;
    output << "{\"schemaVersion\":1,\"type\":\"project_mix_export\",\"explicitExport\":true,"
           << "\"outputPath\":\"" << escapeJson(bounce.outputPath.string())
           << "\",\"startSample\":" << bounce.startSample << ",\"frames\":" << bounce.frames
           << ",\"channels\":" << bounce.channels << ",\"sampleRate\":" << bounce.sampleRate
           << ",\"peakBeforeNormalization\":" << bounce.peakBeforeNormalization
           << ",\"peakAfterNormalization\":" << bounce.peakAfterNormalization
           << ",\"peakAfterDither\":" << bounce.peakAfterDither
           << ",\"postDitherPeak\":" << bounce.peakAfterDither << "}";
    return output.str();
}

std::string batchProjectMixManifestJson(const std::vector<audio::BounceResult>& bounces) {
    std::ostringstream output;
    output << "{\"schemaVersion\":1,\"type\":\"batch_project_mix_export\","
           << "\"explicitExport\":true,\"exports\":[";
    for (std::size_t index = 0; index < bounces.size(); ++index) {
        const auto& bounce = bounces[index];
        output << "{\"outputPath\":\"" << escapeJson(bounce.outputPath.string())
               << "\",\"startSample\":" << bounce.startSample << ",\"frames\":" << bounce.frames
               << ",\"channels\":" << bounce.channels << ",\"sampleRate\":" << bounce.sampleRate
               << ",\"peakBeforeNormalization\":" << bounce.peakBeforeNormalization
               << ",\"peakAfterNormalization\":" << bounce.peakAfterNormalization
               << ",\"peakAfterDither\":" << bounce.peakAfterDither
               << ",\"postDitherPeak\":" << bounce.peakAfterDither << "}";
        if (index + 1 < bounces.size()) {
            output << ',';
        }
    }
    output << "]}";
    return output.str();
}

std::string stemExportManifestJson(const std::vector<session::StemExportResult>& stems) {
    std::ostringstream output;
    output << "{\"schemaVersion\":1,\"type\":\"stem_export\",\"explicitExport\":true,\"stems\":[";
    for (std::size_t index = 0; index < stems.size(); ++index) {
        const auto& stem = stems[index];
        output << "{\"trackId\":\"" << escapeJson(stem.trackId) << "\",\"outputPath\":\""
               << escapeJson(stem.bounce.outputPath.string())
               << "\",\"frames\":" << stem.bounce.frames << ",\"channels\":" << stem.bounce.channels
               << ",\"sampleRate\":" << stem.bounce.sampleRate
               << ",\"peakBeforeNormalization\":" << stem.bounce.peakBeforeNormalization
               << ",\"peakAfterNormalization\":" << stem.bounce.peakAfterNormalization
               << ",\"peakAfterDither\":" << stem.bounce.peakAfterDither
               << ",\"postDitherPeak\":" << stem.bounce.peakAfterDither << "}";
        if (index + 1 < stems.size()) {
            output << ',';
        }
    }
    output << "]}";
    return output.str();
}

std::string bounceInPlaceManifestJson(const audio::BounceResult& bounce,
                                      const BounceInPlaceOptions& placement) {
    std::ostringstream output;
    output << "{\"schemaVersion\":1,\"type\":\"bounce_in_place\",\"assetRegistered\":true,"
           << "\"assetId\":\"" << escapeJson(placement.assetId) << "\",\"clipId\":\""
           << escapeJson(placement.clipId) << "\",\"trackId\":\"" << escapeJson(placement.trackId)
           << "\",\"assetRelativePath\":\"" << escapeJson(placement.assetRelativePath.string())
           << "\",\"outputPath\":\"" << escapeJson(bounce.outputPath.string())
           << "\",\"clipStartSample\":" << placement.clipStartSample
           << ",\"frames\":" << bounce.frames << ",\"channels\":" << bounce.channels
           << ",\"sampleRate\":" << bounce.sampleRate
           << ",\"peakBeforeNormalization\":" << bounce.peakBeforeNormalization
           << ",\"peakAfterNormalization\":" << bounce.peakAfterNormalization
           << ",\"peakAfterDither\":" << bounce.peakAfterDither
           << ",\"postDitherPeak\":" << bounce.peakAfterDither << "}";
    return output.str();
}

std::string freezeTrackManifestJson(const audio::BounceResult& bounce,
                                    const FreezeTrackOptions& freeze,
                                    const std::vector<std::string>& mutedClipIds) {
    std::ostringstream output;
    output << "{\"schemaVersion\":1,\"type\":\"freeze_track\",\"assetRegistered\":true,"
           << "\"trackId\":\"" << escapeJson(freeze.trackId) << "\",\"assetId\":\""
           << escapeJson(freeze.assetId) << "\",\"clipId\":\"" << escapeJson(freeze.clipId)
           << "\",\"assetRelativePath\":\"" << escapeJson(freeze.assetRelativePath.string())
           << "\",\"outputPath\":\"" << escapeJson(bounce.outputPath.string())
           << "\",\"clipStartSample\":" << freeze.clipStartSample << ",\"frames\":" << bounce.frames
           << ",\"channels\":" << bounce.channels << ",\"sampleRate\":" << bounce.sampleRate
           << ",\"peakBeforeNormalization\":" << bounce.peakBeforeNormalization
           << ",\"peakAfterNormalization\":" << bounce.peakAfterNormalization
           << ",\"peakAfterDither\":" << bounce.peakAfterDither
           << ",\"postDitherPeak\":" << bounce.peakAfterDither
           << ",\"mutedSourceClips\":[";
    for (std::size_t index = 0; index < mutedClipIds.size(); ++index) {
        output << "\"" << escapeJson(mutedClipIds[index]) << "\"";
        if (index + 1 < mutedClipIds.size()) {
            output << ',';
        }
    }
    output << "]}";
    return output.str();
}

std::string_view toString(AudioFileTransform transform) noexcept {
    switch (transform) {
    case AudioFileTransform::Normalize:
        return "normalize";
    case AudioFileTransform::Reverse:
        return "reverse";
    }
    return "unknown";
}

std::string transformManifestJson(const std::filesystem::path& inputPath,
                                  const std::filesystem::path& outputPath,
                                  AudioFileTransform transform, const audio::WavAudioData& output,
                                  float inputPeak, float outputPeak) {
    std::ostringstream manifest;
    manifest << "{\"schemaVersion\":1,\"type\":\"audio_transform\",\"operation\":\""
             << toString(transform) << "\",\"inputPath\":\"" << escapeJson(inputPath.string())
             << "\",\"outputPath\":\"" << escapeJson(outputPath.string())
             << "\",\"explicitExport\":true,\"frames\":" << output.audio.frames
             << ",\"channels\":" << output.audio.channels << ",\"sampleRate\":" << output.sampleRate
             << ",\"inputPeak\":" << inputPeak << ",\"outputPeak\":" << outputPeak << "}";
    return manifest.str();
}

bool hasRenderCapability(const DaemonSession& session) noexcept {
    return session.attached() && session.hasCapability(Capability::Render);
}

bool hasAssetCreationCapability(const DaemonSession& session) noexcept {
    return hasRenderCapability(session) && session.hasCapability(Capability::ImportExport);
}

bool manifestHasTrack(const session::ProjectManifest& manifest, std::string_view trackId) {
    return std::ranges::any_of(
        manifest.tracks, [trackId](const session::Track& track) { return track.id == trackId; });
}

bool manifestHasClip(const session::ProjectManifest& manifest, std::string_view clipId) {
    return std::ranges::any_of(manifest.clips,
                               [clipId](const session::Clip& clip) { return clip.id == clipId; });
}

bool manifestHasAsset(const session::ProjectManifest& manifest, std::string_view assetId) {
    return std::ranges::any_of(
        manifest.assets, [assetId](const session::Asset& asset) { return asset.id == assetId; });
}

bool confirmOverwriteIfNeeded(RenderJob& job, const std::filesystem::path& outputPath,
                              const RenderWriteOptions& writeOptions) {
    if (std::filesystem::exists(outputPath) && !writeOptions.allowOverwrite) {
        job.status = RenderJobStatus::Failed;
        job.message = "Output exists; confirmation token is required";
        job.confirmationToken = renderConfirmationToken(outputPath);
        return false;
    }
    if (writeOptions.allowOverwrite &&
        writeOptions.confirmationToken != renderConfirmationToken(outputPath)) {
        job.status = RenderJobStatus::Failed;
        job.message = "Invalid render overwrite confirmation token";
        job.confirmationToken = renderConfirmationToken(outputPath);
        return false;
    }
    return true;
}

bool confirmStemOverwritesIfNeeded(RenderJob& job, const session::StemExportOptions& options,
                                   const RenderWriteOptions& writeOptions) {
    for (const auto& trackId : options.trackIds) {
        const auto outputPath = options.outputDirectory / (trackId + ".wav");
        if (!confirmOverwriteIfNeeded(job, outputPath, writeOptions)) {
            return false;
        }
    }
    return true;
}

bool confirmBatchOverwritesIfNeeded(RenderJob& job,
                                    const std::vector<session::ProjectExportOptions>& exports,
                                    const RenderWriteOptions& writeOptions) {
    for (const auto& options : exports) {
        if (!confirmOverwriteIfNeeded(job, options.outputPath, writeOptions)) {
            return false;
        }
    }
    return true;
}

bool renderJobIdExists(const std::vector<RenderJob>& jobs, std::string_view jobId) {
    return std::ranges::any_of(jobs, [jobId](const RenderJob& job) { return job.id == jobId; });
}

bool rejectDuplicateJobId(const std::vector<RenderJob>& jobs, RenderJob& job) {
    if (!renderJobIdExists(jobs, job.id)) {
        return false;
    }
    job.status = RenderJobStatus::Failed;
    job.progress = 0.0F;
    job.message = "Render job id already exists";
    return true;
}

} // namespace

const std::vector<RenderJob>& RenderJobQueue::jobs() const noexcept {
    return jobs_;
}

RenderJob* RenderJobQueue::find(std::string_view jobId) noexcept {
    const auto found =
        std::ranges::find_if(jobs_, [jobId](const RenderJob& job) { return job.id == jobId; });
    return found == jobs_.end() ? nullptr : &*found;
}

RenderJob RenderJobQueue::enqueueTestTone(const DaemonSession& session, std::string jobId,
                                          std::filesystem::path outputPath) {
    RenderJob job{.id = std::move(jobId), .outputPath = std::move(outputPath)};
    if (rejectDuplicateJobId(jobs_, job)) {
        return job;
    }
    if (!hasRenderCapability(session)) {
        job.status = RenderJobStatus::Failed;
        job.message = "MCP render capability is required";
        jobs_.push_back(job);
        return job;
    }

    job.status = RenderJobStatus::Running;
    job.progress = 0.25F;
    try {
        audio::AudioEngine engine{{.sampleRate = 48000.0, .maxBlockSize = 512}};
        const auto rendered = engine.renderSineOffline(48000, 440.0, 0.25F);
        audio::writePcm16Wav(job.outputPath, rendered, engine.config().sampleRate);
        const audio::WavAudioData wav{
            .audio = rendered, .sampleRate = engine.config().sampleRate, .bitsPerSample = 16};
        job.resultManifestJson = analysisManifestJson(job.outputPath, wav, true);
        job.status = RenderJobStatus::Completed;
        job.progress = 1.0F;
        job.message = "rendered";
    } catch (const std::exception& error) {
        job.status = RenderJobStatus::Failed;
        job.message = error.what();
    }

    jobs_.push_back(job);
    return job;
}

RenderJob RenderJobQueue::enqueueAnalyzeWav(const DaemonSession& session, std::string jobId,
                                            std::filesystem::path inputPath) {
    RenderJob job{.id = std::move(jobId), .outputPath = std::move(inputPath)};
    if (rejectDuplicateJobId(jobs_, job)) {
        return job;
    }
    if (!hasRenderCapability(session)) {
        job.status = RenderJobStatus::Failed;
        job.message = "MCP render capability is required";
        jobs_.push_back(job);
        return job;
    }

    job.status = RenderJobStatus::Running;
    job.progress = 0.5F;
    try {
        const auto wav = audio::readPcm16Wav(job.outputPath);
        job.resultManifestJson = analysisManifestJson(job.outputPath, wav);
        job.status = RenderJobStatus::Completed;
        job.progress = 1.0F;
        job.message = "analyzed";
    } catch (const std::exception& error) {
        job.status = RenderJobStatus::Failed;
        job.message = error.what();
    }
    jobs_.push_back(job);
    return job;
}

RenderJob RenderJobQueue::enqueueTransformWav(const DaemonSession& session, std::string jobId,
                                              std::filesystem::path inputPath,
                                              std::filesystem::path outputPath,
                                              AudioFileTransform transform,
                                              RenderWriteOptions writeOptions) {
    RenderJob job{.id = std::move(jobId), .outputPath = outputPath};
    if (rejectDuplicateJobId(jobs_, job)) {
        return job;
    }
    if (!hasRenderCapability(session)) {
        job.status = RenderJobStatus::Failed;
        job.message = "MCP render capability is required";
        jobs_.push_back(job);
        return job;
    }
    if (inputPath.empty() || outputPath.empty()) {
        job.status = RenderJobStatus::Failed;
        job.message = "Input and output paths are required";
        jobs_.push_back(job);
        return job;
    }
    if (inputPath.lexically_normal() == outputPath.lexically_normal() &&
        !writeOptions.allowOverwrite) {
        job.status = RenderJobStatus::Failed;
        job.message = "Source overwrite requires confirmation token";
        job.confirmationToken = renderConfirmationToken(outputPath);
        jobs_.push_back(job);
        return job;
    }
    if (!confirmOverwriteIfNeeded(job, outputPath, writeOptions)) {
        jobs_.push_back(job);
        return job;
    }

    job.status = RenderJobStatus::Running;
    job.progress = 0.25F;
    try {
        auto wav = audio::readPcm16Wav(inputPath);
        const auto inputPeak = audio::peakAbsoluteSample(wav.audio);
        switch (transform) {
        case AudioFileTransform::Normalize:
            audio::normalizePeak(wav.audio, 0.98F);
            break;
        case AudioFileTransform::Reverse:
            for (std::uint32_t frame = 0; frame < wav.audio.frames / 2U; ++frame) {
                const auto opposite = wav.audio.frames - 1U - frame;
                for (std::uint32_t channel = 0; channel < wav.audio.channels; ++channel) {
                    std::swap(wav.audio.interleavedSamples[static_cast<std::size_t>(frame) *
                                                               wav.audio.channels +
                                                           channel],
                              wav.audio.interleavedSamples[static_cast<std::size_t>(opposite) *
                                                               wav.audio.channels +
                                                           channel]);
                }
            }
            break;
        }

        audio::writePcm16Wav(outputPath, wav.audio, wav.sampleRate);
        const auto output = audio::readPcm16Wav(outputPath);
        const auto outputPeak = audio::peakAbsoluteSample(output.audio);
        job.resultManifestJson =
            transformManifestJson(inputPath, outputPath, transform, output, inputPeak, outputPeak);
        job.status = RenderJobStatus::Completed;
        job.progress = 1.0F;
        job.message = "transformed";
    } catch (const std::exception& error) {
        job.status = RenderJobStatus::Failed;
        job.message = error.what();
    }
    jobs_.push_back(job);
    return job;
}

RenderJob RenderJobQueue::enqueueGraphBounce(const DaemonSession& session, std::string jobId,
                                             const audio::AudioGraph& graph,
                                             audio::BounceOptions options,
                                             RenderWriteOptions writeOptions) {
    RenderJob job{.id = std::move(jobId), .outputPath = options.outputPath};
    if (rejectDuplicateJobId(jobs_, job)) {
        return job;
    }
    if (!hasRenderCapability(session)) {
        job.status = RenderJobStatus::Failed;
        job.message = "MCP render capability is required";
        jobs_.push_back(job);
        return job;
    }

    if (!confirmOverwriteIfNeeded(job, options.outputPath, writeOptions)) {
        jobs_.push_back(job);
        return job;
    }

    job.status = RenderJobStatus::Running;
    job.progress = 0.25F;
    try {
        const auto bounce = audio::bounceGraphToWav(graph, options);
        job.resultManifestJson = bounceManifestJson(bounce);
        job.status = RenderJobStatus::Completed;
        job.progress = 1.0F;
        job.message = "bounced";
    } catch (const std::exception& error) {
        job.status = RenderJobStatus::Failed;
        job.message = error.what();
    }
    jobs_.push_back(job);
    return job;
}

RenderJob RenderJobQueue::enqueueProjectMixExport(const DaemonSession& session, std::string jobId,
                                                  const session::ProjectManifest& manifest,
                                                  const session::MixerState& mixer,
                                                  session::ProjectExportOptions options,
                                                  RenderWriteOptions writeOptions) {
    RenderJob job{.id = std::move(jobId), .outputPath = options.outputPath};
    if (rejectDuplicateJobId(jobs_, job)) {
        return job;
    }
    if (!hasRenderCapability(session)) {
        job.status = RenderJobStatus::Failed;
        job.message = "MCP render capability is required";
        jobs_.push_back(job);
        return job;
    }
    if (!confirmOverwriteIfNeeded(job, options.outputPath, writeOptions)) {
        jobs_.push_back(job);
        return job;
    }

    job.status = RenderJobStatus::Running;
    job.progress = 0.25F;
    try {
        const auto bounce = session::exportProjectMixToWav(manifest, mixer, options);
        job.resultManifestJson = projectMixManifestJson(bounce);
        job.status = RenderJobStatus::Completed;
        job.progress = 1.0F;
        job.message = "exported_mix";
    } catch (const std::exception& error) {
        job.status = RenderJobStatus::Failed;
        job.message = error.what();
    }
    jobs_.push_back(job);
    return job;
}

RenderJob RenderJobQueue::enqueueBatchProjectMixExport(
    const DaemonSession& session, std::string jobId, const session::ProjectManifest& manifest,
    const session::MixerState& mixer, std::vector<session::ProjectExportOptions> exports,
    RenderWriteOptions writeOptions) {
    RenderJob job{.id = std::move(jobId)};
    if (!exports.empty()) {
        job.outputPath = exports.front().outputPath.parent_path();
    }
    if (rejectDuplicateJobId(jobs_, job)) {
        return job;
    }
    if (!hasRenderCapability(session)) {
        job.status = RenderJobStatus::Failed;
        job.message = "MCP render capability is required";
        jobs_.push_back(job);
        return job;
    }
    if (exports.empty()) {
        job.status = RenderJobStatus::Failed;
        job.message = "Batch export requires at least one export";
        jobs_.push_back(job);
        return job;
    }
    if (!confirmBatchOverwritesIfNeeded(job, exports, writeOptions)) {
        jobs_.push_back(job);
        return job;
    }

    job.status = RenderJobStatus::Running;
    job.progress = 0.1F;
    try {
        std::vector<audio::BounceResult> bounces;
        bounces.reserve(exports.size());
        for (std::size_t index = 0; index < exports.size(); ++index) {
            bounces.push_back(session::exportProjectMixToWav(manifest, mixer, exports[index]));
            job.progress = static_cast<float>(index + 1U) / static_cast<float>(exports.size());
        }
        job.resultManifestJson = batchProjectMixManifestJson(bounces);
        job.status = RenderJobStatus::Completed;
        job.progress = 1.0F;
        job.message = "exported_batch";
    } catch (const std::exception& error) {
        job.status = RenderJobStatus::Failed;
        job.message = error.what();
    }
    jobs_.push_back(job);
    return job;
}

RenderJob RenderJobQueue::enqueueProjectStemExport(const DaemonSession& session, std::string jobId,
                                                   const session::ProjectManifest& manifest,
                                                   const session::MixerState& mixer,
                                                   session::StemExportOptions options,
                                                   RenderWriteOptions writeOptions) {
    RenderJob job{.id = std::move(jobId), .outputPath = options.outputDirectory};
    if (rejectDuplicateJobId(jobs_, job)) {
        return job;
    }
    if (!hasRenderCapability(session)) {
        job.status = RenderJobStatus::Failed;
        job.message = "MCP render capability is required";
        jobs_.push_back(job);
        return job;
    }
    if (!confirmStemOverwritesIfNeeded(job, options, writeOptions)) {
        jobs_.push_back(job);
        return job;
    }

    job.status = RenderJobStatus::Running;
    job.progress = 0.25F;
    try {
        const auto stems = session::exportProjectStemsToWav(manifest, mixer, options);
        job.resultManifestJson = stemExportManifestJson(stems);
        job.status = RenderJobStatus::Completed;
        job.progress = 1.0F;
        job.message = "exported_stems";
    } catch (const std::exception& error) {
        job.status = RenderJobStatus::Failed;
        job.message = error.what();
    }
    jobs_.push_back(job);
    return job;
}

RenderJob RenderJobQueue::enqueueBounceInPlace(const DaemonSession& session, std::string jobId,
                                               session::ProjectManifest& manifest,
                                               const audio::AudioGraph& graph,
                                               audio::BounceOptions options,
                                               BounceInPlaceOptions placement,
                                               RenderWriteOptions writeOptions) {
    RenderJob job{.id = std::move(jobId), .outputPath = options.outputPath};
    if (rejectDuplicateJobId(jobs_, job)) {
        return job;
    }
    if (!hasAssetCreationCapability(session)) {
        job.status = RenderJobStatus::Failed;
        job.message = "MCP render and import_export capabilities are required";
        jobs_.push_back(job);
        return job;
    }
    if (placement.assetId.empty() || placement.clipId.empty() || placement.trackId.empty() ||
        placement.assetRelativePath.empty()) {
        job.status = RenderJobStatus::Failed;
        job.message = "Bounce-in-place requires asset, clip, track, and relative path";
        jobs_.push_back(job);
        return job;
    }
    if (!manifestHasTrack(manifest, placement.trackId)) {
        job.status = RenderJobStatus::Failed;
        job.message = "Bounce-in-place track does not exist";
        jobs_.push_back(job);
        return job;
    }
    if (manifestHasAsset(manifest, placement.assetId) ||
        manifestHasClip(manifest, placement.clipId)) {
        job.status = RenderJobStatus::Failed;
        job.message = "Bounce-in-place asset or clip id already exists";
        jobs_.push_back(job);
        return job;
    }
    if (!confirmOverwriteIfNeeded(job, options.outputPath, writeOptions)) {
        jobs_.push_back(job);
        return job;
    }

    job.status = RenderJobStatus::Running;
    job.progress = 0.25F;
    try {
        const auto bounce = audio::bounceGraphToWav(graph, options);
        manifest.assets.push_back({.id = placement.assetId,
                                   .relativePath = placement.assetRelativePath,
                                   .mediaType = "audio/wav"});
        manifest.clips.push_back({.id = placement.clipId,
                                  .trackId = placement.trackId,
                                  .type = session::ClipType::Audio,
                                  .startSample = placement.clipStartSample,
                                  .lengthSamples = static_cast<std::int64_t>(bounce.frames),
                                  .assetId = placement.assetId});
        job.resultManifestJson = bounceInPlaceManifestJson(bounce, placement);
        job.status = RenderJobStatus::Completed;
        job.progress = 1.0F;
        job.message = "bounced_in_place";
    } catch (const std::exception& error) {
        job.status = RenderJobStatus::Failed;
        job.message = error.what();
    }
    jobs_.push_back(job);
    return job;
}

RenderJob RenderJobQueue::enqueueFreezeTrack(const DaemonSession& session, std::string jobId,
                                             session::ProjectManifest& manifest,
                                             const audio::AudioGraph& graph,
                                             audio::BounceOptions options,
                                             FreezeTrackOptions freeze,
                                             RenderWriteOptions writeOptions) {
    RenderJob job{.id = std::move(jobId), .outputPath = options.outputPath};
    if (rejectDuplicateJobId(jobs_, job)) {
        return job;
    }
    if (!hasAssetCreationCapability(session)) {
        job.status = RenderJobStatus::Failed;
        job.message = "MCP render and import_export capabilities are required";
        jobs_.push_back(job);
        return job;
    }
    if (freeze.trackId.empty() || freeze.assetId.empty() || freeze.clipId.empty() ||
        freeze.assetRelativePath.empty()) {
        job.status = RenderJobStatus::Failed;
        job.message = "Freeze requires track, asset, clip, and relative path";
        jobs_.push_back(job);
        return job;
    }
    if (!manifestHasTrack(manifest, freeze.trackId)) {
        job.status = RenderJobStatus::Failed;
        job.message = "Freeze track does not exist";
        jobs_.push_back(job);
        return job;
    }
    if (manifestHasAsset(manifest, freeze.assetId) || manifestHasClip(manifest, freeze.clipId)) {
        job.status = RenderJobStatus::Failed;
        job.message = "Freeze asset or clip id already exists";
        jobs_.push_back(job);
        return job;
    }
    if (!confirmOverwriteIfNeeded(job, options.outputPath, writeOptions)) {
        jobs_.push_back(job);
        return job;
    }

    job.status = RenderJobStatus::Running;
    job.progress = 0.25F;
    try {
        const auto bounce = audio::bounceGraphToWav(graph, options);
        std::vector<std::string> mutedClipIds;
        for (auto& clip : manifest.clips) {
            if (clip.trackId == freeze.trackId && !clip.muted) {
                clip.muted = true;
                mutedClipIds.push_back(clip.id);
            }
        }
        manifest.assets.push_back({.id = freeze.assetId,
                                   .relativePath = freeze.assetRelativePath,
                                   .mediaType = "audio/wav"});
        manifest.clips.push_back({.id = freeze.clipId,
                                  .trackId = freeze.trackId,
                                  .type = session::ClipType::Audio,
                                  .startSample = freeze.clipStartSample,
                                  .lengthSamples = static_cast<std::int64_t>(bounce.frames),
                                  .assetId = freeze.assetId});
        job.resultManifestJson = freezeTrackManifestJson(bounce, freeze, mutedClipIds);
        job.status = RenderJobStatus::Completed;
        job.progress = 1.0F;
        job.message = "frozen_track";
    } catch (const std::exception& error) {
        job.status = RenderJobStatus::Failed;
        job.message = error.what();
    }
    jobs_.push_back(job);
    return job;
}

RenderJob RenderJobQueue::enqueueLongRender(const DaemonSession& session, std::string jobId,
                                            std::filesystem::path outputPath, float progress,
                                            std::string message) {
    RenderJob job{.id = std::move(jobId),
                  .status = RenderJobStatus::Running,
                  .progress = std::clamp(progress, 0.0F, 0.99F),
                  .outputPath = std::move(outputPath),
                  .message = std::move(message)};
    if (rejectDuplicateJobId(jobs_, job)) {
        return job;
    }
    if (!hasRenderCapability(session)) {
        job.status = RenderJobStatus::Failed;
        job.progress = 0.0F;
        job.message = "MCP render capability is required";
        jobs_.push_back(job);
        return job;
    }

    jobs_.push_back(job);
    return job;
}

RenderJob RenderJobQueue::enqueuePending(std::string jobId, std::filesystem::path outputPath,
                                         std::string message) {
    RenderJob job{.id = std::move(jobId),
                  .status = RenderJobStatus::Queued,
                  .progress = 0.0F,
                  .outputPath = std::move(outputPath),
                  .message = std::move(message)};
    if (rejectDuplicateJobId(jobs_, job)) {
        return job;
    }
    jobs_.push_back(job);
    return job;
}

bool RenderJobQueue::cancel(std::string_view jobId) {
    auto* job = find(jobId);
    if (job == nullptr ||
        (job->status != RenderJobStatus::Queued && job->status != RenderJobStatus::Running)) {
        return false;
    }

    job->status = RenderJobStatus::Cancelled;
    job->message = "cancelled";
    return true;
}

std::string renderConfirmationToken(const std::filesystem::path& outputPath) {
    return "render:" + outputPath.lexically_normal().string() + ":confirm";
}

std::string renderJobJson(const RenderJob& job) {
    std::ostringstream output;
    output << "{\"schemaVersion\":1,\"jobId\":\"" << escapeJson(job.id) << "\",\"status\":\""
           << toString(job.status) << "\",\"progress\":" << job.progress << ",\"outputPath\":\""
           << escapeJson(job.outputPath.string()) << "\",\"message\":\"" << escapeJson(job.message)
           << "\",\"confirmationToken\":\"" << escapeJson(job.confirmationToken)
           << "\",\"resultManifest\":";
    if (job.resultManifestJson.empty()) {
        output << "null";
    } else {
        output << job.resultManifestJson;
    }
    output << "}";
    return output.str();
}

} // namespace lamusica::mcp_bridge
