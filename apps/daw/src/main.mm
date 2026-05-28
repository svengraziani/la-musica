#import <Cocoa/Cocoa.h>

#include "lamusica/audio/AudioEngine.hpp"
#include "lamusica/audio/WavFile.hpp"
#include "lamusica/session/ApplicationSession.hpp"
#include "lamusica/session/Export.hpp"
#include "lamusica/session/GraphCompiler.hpp"
#include "lamusica/session/Project.hpp"
#include "lamusica/session/ProjectDocument.hpp"
#include "lamusica/session/StarterProject.hpp"

#include <algorithm>
#include <exception>
#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr NSInteger panelLabelTag = 1001;

bool hasArgument(int argc, char** argv, std::string_view expected) {
    for (int index = 1; index < argc; ++index) {
        if (std::string_view{argv[index]} == expected) {
            return true;
        }
    }
    return false;
}

lamusica::session::FirstTrackTrackMixSummary
trackMixFor(const std::vector<lamusica::session::FirstTrackTrackMixSummary>& mixes,
            std::string_view trackId) {
    const auto found = std::ranges::find_if(
        mixes, [trackId](const lamusica::session::FirstTrackTrackMixSummary& mix) {
            return mix.trackId == trackId;
        });
    return found == mixes.end() ? lamusica::session::FirstTrackTrackMixSummary{} : *found;
}

std::optional<lamusica::session::FirstTrackClipSummary>
clipSummaryFor(lamusica::session::ApplicationSession& session, std::string_view clipId) {
    const auto clips = session.firstTrackClips();
    const auto found =
        std::ranges::find_if(clips, [clipId](const lamusica::session::FirstTrackClipSummary& clip) {
            return clip.clipId == clipId;
        });
    if (found == clips.end()) {
        return std::nullopt;
    }
    return *found;
}

bool routeEnabled(lamusica::session::ApplicationSession* session, std::string_view command) {
    return session != nullptr && session->routeMenuCommand(command).enabled;
}

std::filesystem::path uniqueUntitledFirstTrackProjectPath() {
    const auto root = std::filesystem::temp_directory_path();
    const std::string baseName{"LaMusica Untitled"};
    for (int index = 0; index < 1000; ++index) {
        const auto suffix = index == 0 ? std::string{} : " " + std::to_string(index + 1);
        auto candidate = root / (baseName + suffix + ".Project.lamusica");
        if (!std::filesystem::exists(candidate)) {
            return candidate;
        }
    }
    throw std::runtime_error("Unable to find a free untitled first-track project path");
}

int createFirstTrackProject(const char* projectPath, const char* name) {
    const auto document = lamusica::session::ProjectDocument::create(
        projectPath, lamusica::session::makeFirstTrackStarterManifest(name));
    std::cout << "LaMusica app created first-track project: " << document.project().name()
              << " path=" << document.path() << " tracks=" << document.manifest().tracks.size()
              << " clips=" << document.manifest().clips.size() << '\n';
    return 0;
}

int renderProject(const char* projectPath, const char* outputPath) {
    const auto document = lamusica::session::ProjectDocument::open(projectPath);
    const auto frames = lamusica::session::renderableArrangementFrames(document.manifest());
    const auto result = lamusica::session::exportProjectMixToWav(
        document.manifest(), {},
        {.outputPath = outputPath,
         .startSample = 0,
         .frames = frames,
         .sampleRate = 48000.0,
         .channels = 2,
         .projectRoot = document.path(),
         .bitDepth = lamusica::audio::ExportBitDepth::Pcm16,
         .ditherMode = lamusica::audio::DitherMode::Triangular,
         .normalizePeak = true,
         .normalizeTargetPeak = 0.98F});
    std::cout << "LaMusica app rendered project: " << document.project().name()
              << " path=" << result.outputPath << " frames=" << result.frames
              << " peak=" << result.peakAfterNormalization << '\n';
    return 0;
}

int inspectProject(const char* projectPath) {
    const auto document = lamusica::session::ProjectDocument::open(projectPath);
    const auto readiness = lamusica::session::inspectFirstTrackReadiness(document.manifest());
    const auto arrangement = lamusica::session::summarizeFirstTrackArrangement(document.manifest());
    bool mediaReady = true;
    std::string mediaError;
    try {
        (void)lamusica::session::compileProjectAudioGraph(document.manifest(), {},
                                                          {.projectRoot = document.path()});
    } catch (const std::exception& error) {
        mediaReady = false;
        mediaError = error.what();
    }
    const bool firstTrackReady = readiness.starterStructureReady && readiness.renderable &&
                                 readiness.loopReady && mediaReady;
    const bool firstTrackEditable = readiness.firstTrackEditable && mediaReady;
    std::cout << "LaMusica app project readiness: " << document.project().name()
              << " firstTrackReady=" << (firstTrackReady ? "true" : "false")
              << " firstTrackEditable=" << (firstTrackEditable ? "true" : "false")
              << " mediaReady=" << (mediaReady ? "true" : "false")
              << " starterStructure=" << (readiness.starterStructureReady ? "true" : "false")
              << " renderable=" << (readiness.renderable ? "true" : "false")
              << " tempoBpm=" << arrangement.tempoBpm
              << " timeSignature=" << arrangement.timeSignatureNumerator << '/'
              << arrangement.timeSignatureDenominator << " sections=" << arrangement.sectionCount
              << " firstSection=\"" << arrangement.firstSectionName << "\""
              << " firstSectionSample=" << arrangement.firstSectionSample << " finalSection=\""
              << arrangement.finalSectionName << "\""
              << " finalSectionSample=" << arrangement.finalSectionSample
              << " audioTracks=" << arrangement.audioTrackCount
              << " midiTracks=" << arrangement.midiTrackCount
              << " masterTracks=" << arrangement.masterTrackCount
              << " tracks=" << readiness.trackCount << " clips=" << readiness.clipCount
              << " markers=" << readiness.markerCount << " routing=" << readiness.routingCount
              << " midiRefs=" << readiness.midiClipReferenceCount
              << " starterMidiNotes=" << readiness.starterMidiNoteCount
              << " plugins=" << readiness.pluginCount
              << " automation=" << readiness.automationLaneCount
              << " bassTranspose=" << readiness.starterBassTransposeSemitones
              << " loopEnabled=" << (readiness.loopReady ? "true" : "false")
              << " loopStart=" << readiness.loopStartSample
              << " loopEnd=" << readiness.loopEndSample << " frames=" << readiness.renderFrames;
    if (!mediaReady && !mediaError.empty()) {
        std::cout << " media error: " << mediaError;
    }
    std::cout << '\n';
    return 0;
}

int appSessionFirstTrackSmoke() {
    const auto path =
        std::filesystem::temp_directory_path() / "lamusica-app-session-smoke.Project.lamusica";
    std::filesystem::remove_all(path);
    lamusica::session::ApplicationSession session;
    session.createFirstTrackProject(path, "Session Smoke");
    const auto status = session.status();
    std::cout << "LaMusica app session first-track smoke: project=" << status.projectName
              << " firstTrackReady=" << (status.firstTrackReady ? "true" : "false")
              << " firstTrackEditable=" << (status.firstTrackEditable ? "true" : "false")
              << " mediaReady=" << (status.mediaReady ? "true" : "false")
              << " tracks=" << status.trackCount << " clips=" << status.clipCount
              << " plugins=" << status.pluginCount << " automation=" << status.automationLaneCount
              << " starterMidiNotes=" << status.starterMidiNoteCount
              << " bassTranspose=" << status.starterBassTransposeSemitones
              << " drumGainDb=" << status.drumClipGainDb << " bassGainDb=" << status.bassClipGainDb
              << " loopEnabled=" << (status.loopEnabled ? "true" : "false")
              << " loopStart=" << status.loopStartSample << " loopEnd=" << status.loopEndSample
              << " loopFrames=" << status.loopFrames << " sections=" << status.markerCount
              << " tempoBpm=" << status.tempoBpm
              << " timeSignature=" << status.timeSignatureNumerator << '/'
              << status.timeSignatureDenominator << " firstSection=\"" << status.firstSectionName
              << "\"" << " finalSection=\"" << status.finalSectionName << "\""
              << " renderFrames=" << status.renderFrames << '\n';
    std::filesystem::remove_all(path);
    return status.firstTrackReady ? 0 : 3;
}

int appSessionVerifyFirstTrackProjectSmoke() {
    const auto unreadyPath =
        std::filesystem::temp_directory_path() / "lamusica-app-session-unready.Project.lamusica";
    const auto readyPath =
        std::filesystem::temp_directory_path() / "lamusica-app-session-ready.Project.lamusica";
    std::filesystem::remove_all(unreadyPath);
    std::filesystem::remove_all(readyPath);

    lamusica::session::ApplicationSession session;
    session.createProject(unreadyPath, "Session Unready");
    const auto unready = session.verifyFirstTrackProject();
    session.createFirstTrackProject(readyPath, "Session Ready");
    const auto ready = session.verifyFirstTrackProject();

    std::cout << "LaMusica app session verify first-track project: unreadyFirstTrackReady="
              << (unready.firstTrackReady ? "true" : "false")
              << " unreadyFirstTrackEditable=" << (unready.firstTrackEditable ? "true" : "false")
              << " unreadyMediaReady=" << (unready.mediaReady ? "true" : "false")
              << " unreadyStarterStructure=" << (unready.starterStructureReady ? "true" : "false")
              << " unreadyRenderable=" << (unready.renderable ? "true" : "false")
              << " readyFirstTrackReady=" << (ready.firstTrackReady ? "true" : "false")
              << " readyFirstTrackEditable=" << (ready.firstTrackEditable ? "true" : "false")
              << " readyMediaReady=" << (ready.mediaReady ? "true" : "false")
              << " readyStarterStructure=" << (ready.starterStructureReady ? "true" : "false")
              << " readyRenderable=" << (ready.renderable ? "true" : "false")
              << " readyLoopEnabled=" << (ready.loopEnabled ? "true" : "false")
              << " readyLoopFrames=" << ready.loopFrames
              << " readyRenderFrames=" << ready.renderFrames << " readyTracks=" << ready.trackCount
              << " readyClips=" << ready.clipCount << " readyPlugins=" << ready.pluginCount
              << " readyAutomation=" << ready.automationLaneCount
              << " readyStarterMidiNotes=" << ready.starterMidiNoteCount
              << " unreadyMissingRequirements=" << unready.missingRequirements.size()
              << " readyMissingRequirements=" << ready.missingRequirements.size() << '\n';

    std::filesystem::remove_all(unreadyPath);
    std::filesystem::remove_all(readyPath);
    return !unready.firstTrackReady && !unready.firstTrackEditable && unready.mediaReady &&
                   !unready.starterStructureReady && !unready.renderable &&
                   unready.missingRequirements.size() ==
                       lamusica::session::firstTrackStarterRequirementIds().size() &&
                   ready.firstTrackReady && ready.firstTrackEditable && ready.mediaReady &&
                   ready.starterStructureReady && ready.renderable && ready.loopEnabled &&
                   ready.loopFrames == 96000 && ready.renderFrames == 96000 &&
                   ready.trackCount == 3 && ready.clipCount == 2 && ready.pluginCount == 3 &&
                   ready.automationLaneCount == 3 && ready.starterMidiNoteCount == 8 &&
                   ready.missingRequirements.empty()
               ? 0
               : 15;
}

int appSessionLoopFirstTrackSmoke() {
    const auto projectPath =
        std::filesystem::temp_directory_path() / "lamusica-app-session-loop.Project.lamusica";
    const auto outputPath =
        std::filesystem::temp_directory_path() / "lamusica-app-session-loop.wav";
    std::filesystem::remove_all(projectPath);
    std::filesystem::remove(outputPath);
    lamusica::session::ApplicationSession session;
    session.createFirstTrackProject(projectPath, "Session Loop");
    session.clearLoopRegion();
    session.setFirstTrackLoopToIntro();
    session.openProject(projectPath);
    const auto result = session.exportCurrentLoop(outputPath);
    const auto status = session.status();
    std::cout << "LaMusica app session loop: project=" << status.projectName
              << " firstTrackReady=" << (status.firstTrackReady ? "true" : "false")
              << " loopEnabled=" << (status.loopEnabled ? "true" : "false")
              << " loopStart=" << status.loopStartSample << " loopEnd=" << status.loopEndSample
              << " loopFrames=" << status.loopFrames << " exportFrames=" << result.frames
              << " peak=" << result.peakAfterNormalization << '\n';
    std::filesystem::remove_all(projectPath);
    std::filesystem::remove(outputPath);
    return status.firstTrackReady && status.loopEnabled && status.loopStartSample == 0 &&
                   status.loopEndSample == 96000 && result.frames == status.loopFrames &&
                   result.peakAfterNormalization > 0.0F
               ? 0
               : 7;
}

int appSessionTransportFirstTrackSmoke() {
    const auto projectPath =
        std::filesystem::temp_directory_path() / "lamusica-app-session-transport.Project.lamusica";
    std::filesystem::remove_all(projectPath);
    lamusica::session::ApplicationSession session;
    session.createFirstTrackProject(projectPath, "Session Transport");
    session.seek(95936);
    session.play();
    const auto rendered = session.auditionCurrentMixBlock(128);
    session.stop();
    const auto status = session.status();
    const auto peak = lamusica::audio::peakAbsoluteSample(rendered);
    std::cout << "LaMusica app session transport: project=" << status.projectName
              << " firstTrackReady=" << (status.firstTrackReady ? "true" : "false")
              << " playing=" << (status.transportPlaying ? "true" : "false")
              << " playhead=" << status.playheadSample
              << " loopEnabled=" << (status.loopEnabled ? "true" : "false")
              << " loopStart=" << status.loopStartSample << " loopEnd=" << status.loopEndSample
              << " auditionFrames=" << rendered.frames << " peak=" << peak << '\n';
    std::filesystem::remove_all(projectPath);
    return status.firstTrackReady && !status.transportPlaying && status.playheadSample == 64 &&
                   rendered.frames == 128 && peak > 0.0F
               ? 0
               : 8;
}

int appSessionArrangeFirstTrackSmoke() {
    const auto projectPath =
        std::filesystem::temp_directory_path() / "lamusica-app-session-arrange.Project.lamusica";
    std::filesystem::remove_all(projectPath);
    lamusica::session::ApplicationSession session;
    session.createFirstTrackProject(projectPath, "Session Arrange");
    session.extendFirstTrackArrangementToVerse();
    session.openProject(projectPath);
    const auto status = session.status();
    const auto graph = lamusica::session::compileProjectAudioGraph(
        session.currentDocument()->manifest(), lamusica::session::MixerState{});
    const auto bassNode =
        std::ranges::find_if(graph.nodes, [](const lamusica::audio::GraphNode& node) {
            return node.id == "clip:bass-pattern";
        });
    const auto bassNotes =
        bassNode == graph.nodes.end() ? 0U : static_cast<unsigned>(bassNode->noteSequence.size());
    std::cout << "LaMusica app session arrange: project=" << status.projectName
              << " firstTrackReady=" << (status.firstTrackReady ? "true" : "false")
              << " renderFrames=" << status.renderFrames << " loopFrames=" << status.loopFrames
              << " bassNotes=" << bassNotes << '\n';
    std::filesystem::remove_all(projectPath);
    return status.firstTrackReady && status.renderFrames == 192000 && status.loopFrames == 96000 &&
                   bassNotes == 16
               ? 0
               : 9;
}

int appSessionStemFirstTrackSmoke() {
    const auto projectPath =
        std::filesystem::temp_directory_path() / "lamusica-app-session-stems.Project.lamusica";
    const auto outputDirectory = std::filesystem::temp_directory_path() / "lamusica-app-stems";
    std::filesystem::remove_all(projectPath);
    std::filesystem::remove_all(outputDirectory);
    lamusica::session::ApplicationSession session;
    session.createFirstTrackProject(projectPath, "Session Stems");
    session.extendFirstTrackArrangementToVerse();
    const auto results = session.exportCurrentStems(outputDirectory);
    const auto status = session.status();
    const bool filesExist =
        std::ranges::all_of(results, [](const lamusica::session::StemExportResult& result) {
            return std::filesystem::exists(result.bounce.outputPath);
        });
    std::cout << "LaMusica app session stems: project=" << status.projectName
              << " firstTrackReady=" << (status.firstTrackReady ? "true" : "false")
              << " stemCount=" << results.size() << " stemFrames=" << status.lastStemExportFrames
              << " filesExist=" << (filesExist ? "true" : "false") << '\n';
    std::filesystem::remove_all(projectPath);
    std::filesystem::remove_all(outputDirectory);
    return status.firstTrackReady && results.size() == 2 && status.lastStemExportFrames == 192000 &&
                   filesExist
               ? 0
               : 10;
}

int appSessionPackageFirstTrackSmoke() {
    const auto projectPath =
        std::filesystem::temp_directory_path() / "lamusica-app-session-package.Project.lamusica";
    const auto outputDirectory = std::filesystem::temp_directory_path() / "lamusica-app-package";
    std::filesystem::remove_all(projectPath);
    std::filesystem::remove_all(outputDirectory);
    lamusica::session::ApplicationSession session;
    session.createFirstTrackProject(projectPath, "Session Package");
    session.extendFirstTrackArrangementToVerse();
    const auto result = session.exportFirstTrackPackage(outputDirectory);
    const auto verification = session.verifyFirstTrackPackage(outputDirectory);
    const auto status = session.status();
    const bool filesExist =
        std::filesystem::exists(result.manifestPath) &&
        std::filesystem::exists(result.mix.outputPath) &&
        std::filesystem::exists(result.loop.outputPath) &&
        std::ranges::all_of(result.stems, [](const lamusica::session::StemExportResult& stem) {
            return std::filesystem::exists(stem.bounce.outputPath);
        });
    std::cout << "LaMusica app session package: project=" << status.projectName
              << " firstTrackReady=" << (status.firstTrackReady ? "true" : "false")
              << " mixFrames=" << result.mix.frames << " loopFrames=" << result.loop.frames
              << " stemCount=" << result.stems.size()
              << " packageMixFrames=" << status.lastPackageMixFrames
              << " packageLoopFrames=" << status.lastPackageLoopFrames
              << " packageStemCount=" << status.lastPackageStemCount << " packageManifest="
              << (std::filesystem::exists(result.manifestPath) ? "true" : "false")
              << " packageVerified=" << (status.lastPackageVerified ? "true" : "false")
              << " projectSnapshotVerified="
              << (verification.projectSnapshotVerified ? "true" : "false")
              << " projectAssets=" << verification.projectAssetCount
              << " filesExist=" << (filesExist ? "true" : "false") << '\n';
    std::filesystem::remove_all(projectPath);
    std::filesystem::remove_all(outputDirectory);
    return status.firstTrackReady && result.mix.frames == 192000 && result.loop.frames == 96000 &&
                   result.stems.size() == 2 && status.lastPackageMixFrames == 192000 &&
                   status.lastPackageLoopFrames == 96000 && status.lastPackageStemCount == 2 &&
                   verification.stemCount == 2 && verification.projectSnapshotVerified &&
                   verification.projectAssetCount == 0 &&
                   status.lastPackageManifestPath == result.manifestPath &&
                   status.lastPackageVerifiedDirectory == outputDirectory &&
                   status.lastPackageVerified && filesExist
               ? 0
               : 11;
}

int appSessionRecordFirstTrackSmoke() {
    const auto projectPath =
        std::filesystem::temp_directory_path() / "lamusica-app-session-record.Project.lamusica";
    std::filesystem::remove_all(projectPath);
    lamusica::session::ApplicationSession session;
    session.createFirstTrackProject(projectPath, "Session Record");
    session.extendFirstTrackArrangementToVerse();
    const auto result = session.recordFirstTrackTake(48000);
    session.openProject(projectPath);
    const auto status = session.status();
    const bool fileExists = std::filesystem::exists(result.committed.path);
    std::cout << "LaMusica app session record: project=" << status.projectName
              << " firstTrackReady=" << (status.firstTrackReady ? "true" : "false")
              << " recordedTakeCount=" << status.recordedTakeCount
              << " tracks=" << status.trackCount << " clips=" << status.clipCount
              << " recordedFrames=" << result.committed.frames
              << " recordedChannels=" << result.committed.channels
              << " reopenedLastRecordingClip=" << status.lastRecordingClipId
              << " fileExists=" << (fileExists ? "true" : "false") << '\n';
    std::filesystem::remove_all(projectPath);
    return status.firstTrackReady && status.recordedTakeCount == 1 && status.trackCount == 4 &&
                   status.clipCount == 3 && result.committed.frames == 48000 &&
                   result.committed.channels == 2 && status.lastRecordingClipId == result.clipId &&
                   status.lastRecordingFrames == result.committed.frames && fileExists
               ? 0
               : 12;
}

int appSessionRecordPackageFirstTrackSmoke() {
    const auto projectPath = std::filesystem::temp_directory_path() /
                             "lamusica-app-session-record-package.Project.lamusica";
    const auto outputDirectory =
        std::filesystem::temp_directory_path() / "lamusica-app-record-package";
    std::filesystem::remove_all(projectPath);
    std::filesystem::remove_all(outputDirectory);
    lamusica::session::ApplicationSession session;
    session.createFirstTrackProject(projectPath, "Session Record Package");
    session.extendFirstTrackArrangementToVerse();
    const auto recording = session.recordFirstTrackTake(48000);
    session.setFirstTrackTrackMix("recorded-takes", -6.0F, 0.5F, false, false);
    session.openProject(projectPath);
    const auto package = session.exportFirstTrackPackage(outputDirectory);
    const auto verification = session.verifyFirstTrackPackage(outputDirectory);
    const auto status = session.status();
    const auto recordedMix = trackMixFor(session.firstTrackTrackMix(), "recorded-takes");
    const bool hasRecordedStem =
        std::ranges::any_of(package.stems, [](const lamusica::session::StemExportResult& stem) {
            return stem.trackId == "recorded-takes" &&
                   std::filesystem::exists(stem.bounce.outputPath) &&
                   stem.bounce.peakAfterNormalization > 0.0F;
        });
    const bool filesExist =
        std::filesystem::exists(package.manifestPath) &&
        std::filesystem::exists(package.mix.outputPath) &&
        std::filesystem::exists(package.loop.outputPath) &&
        std::filesystem::exists(recording.committed.path) &&
        std::ranges::all_of(package.stems, [](const lamusica::session::StemExportResult& stem) {
            return std::filesystem::exists(stem.bounce.outputPath);
        });
    std::cout << "LaMusica app session record package: project=" << status.projectName
              << " firstTrackReady=" << (status.firstTrackReady ? "true" : "false")
              << " recordedTakeCount=" << status.recordedTakeCount
              << " mixFrames=" << package.mix.frames << " loopFrames=" << package.loop.frames
              << " stemCount=" << package.stems.size()
              << " recordedTrackVolumeDb=" << recordedMix.volumeDb
              << " recordedPan=" << recordedMix.pan
              << " packageStemCount=" << status.lastPackageStemCount
              << " hasRecordedStem=" << (hasRecordedStem ? "true" : "false") << " packageManifest="
              << (std::filesystem::exists(package.manifestPath) ? "true" : "false")
              << " packageVerified=" << (status.lastPackageVerified ? "true" : "false")
              << " projectSnapshotVerified="
              << (verification.projectSnapshotVerified ? "true" : "false")
              << " projectAssets=" << verification.projectAssetCount
              << " filesExist=" << (filesExist ? "true" : "false") << '\n';
    std::filesystem::remove_all(projectPath);
    std::filesystem::remove_all(outputDirectory);
    return status.firstTrackReady && status.recordedTakeCount == 1 &&
                   package.mix.frames == 192000 && package.loop.frames == 96000 &&
                   package.stems.size() == 3 && recordedMix.volumeDb == -6.0F &&
                   recordedMix.pan == 0.5F && status.lastPackageStemCount == 3 &&
                   verification.stemCount == 3 && verification.projectSnapshotVerified &&
                   verification.projectAssetCount == 1 && verification.recordedTakeCount == 1 &&
                   status.lastPackageManifestPath == package.manifestPath &&
                   status.lastPackageVerifiedDirectory == outputDirectory &&
                   status.lastPackageVerified && hasRecordedStem && filesExist
               ? 0
               : 13;
}

int appSessionImportPackageFirstTrackSmoke() {
    const auto projectPath = std::filesystem::temp_directory_path() /
                             "lamusica-app-session-import-package.Project.lamusica";
    const auto outputDirectory =
        std::filesystem::temp_directory_path() / "lamusica-app-import-package";
    const auto importSource =
        std::filesystem::temp_directory_path() / "lamusica-app-import-source.wav";
    std::filesystem::remove_all(projectPath);
    std::filesystem::remove_all(outputDirectory);
    std::filesystem::remove(importSource);
    lamusica::audio::AudioEngine importEngine{{}};
    lamusica::audio::writePcm16Wav(importSource,
                                   importEngine.renderSineOffline(24000, 523.25, 0.35F), 48000.0);
    lamusica::session::ApplicationSession session;
    session.createFirstTrackProject(projectPath, "Session Import Package");
    session.extendFirstTrackArrangementToVerse();
    const auto imported = session.importAudioFileToFirstTrack(importSource, 24000);
    session.setFirstTrackTrackMix("imported-audio", -9.0F, -0.5F, false, false);
    session.openProject(projectPath);
    const auto package = session.exportFirstTrackPackage(outputDirectory);
    const auto verification = session.verifyFirstTrackPackage(outputDirectory);
    const auto status = session.status();
    const auto importedMix = trackMixFor(session.firstTrackTrackMix(), "imported-audio");
    const bool hasImportedStem =
        std::ranges::any_of(package.stems, [](const lamusica::session::StemExportResult& stem) {
            return stem.trackId == "imported-audio" &&
                   std::filesystem::exists(stem.bounce.outputPath) &&
                   stem.bounce.peakAfterNormalization > 0.0F;
        });
    const bool filesExist =
        std::filesystem::exists(package.manifestPath) &&
        std::filesystem::exists(imported.copiedPath) &&
        std::filesystem::exists(package.mix.outputPath) &&
        std::filesystem::exists(package.loop.outputPath) &&
        std::ranges::all_of(package.stems, [](const lamusica::session::StemExportResult& stem) {
            return std::filesystem::exists(stem.bounce.outputPath);
        });
    std::cout << "LaMusica app session import package: project=" << status.projectName
              << " firstTrackReady=" << (status.firstTrackReady ? "true" : "false")
              << " importedAudioClipCount=" << status.importedAudioClipCount
              << " importFrames=" << imported.frames << " mixFrames=" << package.mix.frames
              << " loopFrames=" << package.loop.frames << " stemCount=" << package.stems.size()
              << " importedTrackVolumeDb=" << importedMix.volumeDb
              << " importedPan=" << importedMix.pan
              << " reopenedLastImportClip=" << status.lastImportClipId
              << " packageStemCount=" << status.lastPackageStemCount
              << " hasImportedStem=" << (hasImportedStem ? "true" : "false") << " packageManifest="
              << (std::filesystem::exists(package.manifestPath) ? "true" : "false")
              << " packageVerified=" << (status.lastPackageVerified ? "true" : "false")
              << " projectSnapshotVerified="
              << (verification.projectSnapshotVerified ? "true" : "false")
              << " projectAssets=" << verification.projectAssetCount
              << " filesExist=" << (filesExist ? "true" : "false") << '\n';
    std::filesystem::remove_all(projectPath);
    std::filesystem::remove_all(outputDirectory);
    std::filesystem::remove(importSource);
    return status.firstTrackReady && status.importedAudioClipCount == 1 &&
                   imported.frames == 24000 && package.mix.frames == 192000 &&
                   package.loop.frames == 96000 && package.stems.size() == 3 &&
                   importedMix.volumeDb == -9.0F && importedMix.pan == -0.5F &&
                   status.lastImportClipId == imported.clipId &&
                   status.lastImportFrames == imported.frames && status.lastPackageStemCount == 3 &&
                   verification.stemCount == 3 && verification.projectSnapshotVerified &&
                   verification.projectAssetCount == 1 &&
                   verification.importedAudioClipCount == 1 &&
                   status.lastPackageManifestPath == package.manifestPath && hasImportedStem &&
                   status.lastPackageVerifiedDirectory == outputDirectory &&
                   status.lastPackageVerified && filesExist
               ? 0
               : 14;
}

int appSessionImportEditFirstTrackSmoke() {
    const auto projectPath = std::filesystem::temp_directory_path() /
                             "lamusica-app-session-import-edit.Project.lamusica";
    const auto importSource =
        std::filesystem::temp_directory_path() / "lamusica-app-import-edit-source.wav";
    std::filesystem::remove_all(projectPath);
    std::filesystem::remove(importSource);
    lamusica::audio::AudioEngine importEngine{{}};
    lamusica::audio::writePcm16Wav(importSource,
                                   importEngine.renderSineOffline(48000, 659.25, 0.35F), 48000.0);

    lamusica::session::ApplicationSession session;
    session.createFirstTrackProject(projectPath, "Session Import Edit");
    session.extendFirstTrackArrangementToVerse();
    const auto imported = session.importAudioFileToFirstTrack(importSource, 24000);
    session.setClipFades(imported.clipId, 256, 512);
    session.setClipMuted(imported.clipId, true);
    session.setClipMuted(imported.clipId, false);
    session.setClipReversed(imported.clipId, true);
    session.setClipReversed(imported.clipId, false);
    session.setClipReversed(imported.clipId, true);
    session.setClipTiming(imported.clipId, 48000, 24000, 12000);
    const auto duplicate =
        session.duplicateClip(imported.clipId, "imported-audio-edit-copy", 96000);
    const auto removed = session.removeClip(imported.clipId);
    session.openProject(projectPath);

    const auto status = session.status();
    const auto verification = session.verifyFirstTrackProject();
    const auto clips = session.firstTrackClips();
    const auto copied =
        std::ranges::find_if(clips, [](const lamusica::session::FirstTrackClipSummary& clip) {
            return clip.clipId == "imported-audio-edit-copy";
        });
    const bool copiedReady = copied != clips.end() && copied->trackId == "imported-audio" &&
                             copied->startSample == 96000 && copied->lengthSamples == 24000 &&
                             copied->sourceOffsetSamples == 12000 && copied->fadeInSamples == 256 &&
                             copied->fadeOutSamples == 512 && copied->reversed && !copied->muted &&
                             copied->mediaAvailable;
    std::cout << "LaMusica app session import edit: project=" << status.projectName
              << " firstTrackReady=" << (status.firstTrackReady ? "true" : "false")
              << " verified=" << (verification.firstTrackReady ? "true" : "false")
              << " importedAudioClipCount=" << status.importedAudioClipCount
              << " importedFrames=" << imported.frames << " duplicateClip=" << duplicate.clipId
              << " removedClip=" << removed.clipId
              << " reopenedLastImportClip=" << status.lastImportClipId
              << " copiedReady=" << (copiedReady ? "true" : "false")
              << " clips=" << status.clipCount << " tracks=" << status.trackCount << '\n';
    std::filesystem::remove_all(projectPath);
    std::filesystem::remove(importSource);
    return status.firstTrackReady && verification.firstTrackReady &&
                   status.importedAudioClipCount == 1 && status.clipCount == 3 &&
                   status.trackCount == 4 && imported.frames == 48000 &&
                   status.lastImportClipId == duplicate.clipId && copiedReady
               ? 0
               : 16;
}

int appSessionMixFirstTrackSmoke() {
    const auto projectPath =
        std::filesystem::temp_directory_path() / "lamusica-app-session-mix.Project.lamusica";
    const auto outputPath = std::filesystem::temp_directory_path() / "lamusica-app-session-mix.wav";
    std::filesystem::remove_all(projectPath);
    std::filesystem::remove(outputPath);
    lamusica::session::ApplicationSession session;
    session.createFirstTrackProject(projectPath, "Session Mix");
    session.setClipGain("drum-loop", -18.0F);
    session.setClipGain("bass-pattern", -3.0F);
    session.setFirstTrackTrackMix("drums", -12.0F, 0.25F, false, false);
    session.setFirstTrackTrackMix("bass", 0.0F, -0.25F, true, false);
    session.openProject(projectPath);
    const auto result = session.exportCurrentMix(outputPath);
    const auto status = session.status();
    const auto mixes = session.firstTrackTrackMix();
    const auto drumMix = trackMixFor(mixes, "drums");
    const auto bassMix = trackMixFor(mixes, "bass");
    std::cout << "LaMusica app session mix: project=" << status.projectName
              << " firstTrackReady=" << (status.firstTrackReady ? "true" : "false")
              << " drumGainDb=" << status.drumClipGainDb << " bassGainDb=" << status.bassClipGainDb
              << " drumTrackVolumeDb=" << drumMix.volumeDb << " drumPan=" << drumMix.pan
              << " bassPan=" << bassMix.pan << " bassMuted=" << (bassMix.muted ? "true" : "false")
              << " frames=" << result.frames << " peak=" << result.peakAfterNormalization << '\n';
    std::filesystem::remove_all(projectPath);
    std::filesystem::remove(outputPath);
    return status.firstTrackReady && status.drumClipGainDb == -18.0F &&
                   status.bassClipGainDb == -3.0F && drumMix.volumeDb == -12.0F &&
                   drumMix.pan == 0.25F && bassMix.pan == -0.25F && bassMix.muted &&
                   result.frames == status.renderFrames && result.peakAfterNormalization > 0.0F
               ? 0
               : 6;
}

int appSessionRenderFirstTrackSmoke() {
    const auto projectPath =
        std::filesystem::temp_directory_path() / "lamusica-app-session-render.Project.lamusica";
    const auto outputPath =
        std::filesystem::temp_directory_path() / "lamusica-app-session-render.wav";
    std::filesystem::remove_all(projectPath);
    std::filesystem::remove(outputPath);
    lamusica::session::ApplicationSession session;
    session.createFirstTrackProject(projectPath, "Session Render");
    const auto result = session.exportCurrentMix(outputPath);
    const auto status = session.status();
    std::cout << "LaMusica app session render: project=" << status.projectName
              << " firstTrackReady=" << (status.firstTrackReady ? "true" : "false")
              << " path=" << result.outputPath << " frames=" << result.frames
              << " peak=" << result.peakAfterNormalization
              << " statusFrames=" << status.lastMixExportFrames
              << " starterMidiNotes=" << status.starterMidiNoteCount << '\n';
    std::filesystem::remove_all(projectPath);
    std::filesystem::remove(outputPath);
    return status.firstTrackReady && result.frames == status.renderFrames &&
                   status.lastMixExportFrames == result.frames &&
                   result.peakAfterNormalization > 0.0F
               ? 0
               : 4;
}

int appSessionPreferencesFirstTrackSmoke() {
    lamusica::session::ApplicationSession session;
    lamusica::session::ApplicationPreferences preferences{
        .audioDeviceId = "first-track-output",
        .enabledMidiInputIds = {"first-track-keyboard"},
        .pluginSearchPaths = {"/Library/Audio/Plug-Ins/Components"},
        .mcpEnabled = true,
        .allowMcpProjectMutation = true,
        .keyboardShortcuts = {{.command = "transport.play", .keyEquivalent = "space"}},
        .allowUserFolderScanning = true,
        .shareDiagnostics = false};
    session.setPreferences(preferences);
    session.setKeyboardShortcut("transport.play", "p");
    bool rejectedUnsafeMutation = false;
    try {
        lamusica::session::ApplicationPreferences unsafePreferences;
        unsafePreferences.mcpEnabled = false;
        unsafePreferences.allowMcpProjectMutation = true;
        session.setPreferences(unsafePreferences);
    } catch (const std::exception&) {
        rejectedUnsafeMutation = true;
    }

    const auto& stored = session.preferences();
    const bool ready =
        stored.audioDeviceId == "first-track-output" &&
        stored.enabledMidiInputIds == std::vector<std::string>{"first-track-keyboard"} &&
        stored.pluginSearchPaths.size() == 1 && stored.mcpEnabled &&
        stored.allowMcpProjectMutation && stored.keyboardShortcuts.size() == 1 &&
        stored.keyboardShortcuts.front().command == "transport.play" &&
        stored.keyboardShortcuts.front().keyEquivalent == "p" && stored.allowUserFolderScanning &&
        !stored.shareDiagnostics && rejectedUnsafeMutation;
    std::cout << "LaMusica app session preferences: audioDevice=" << stored.audioDeviceId
              << " midiInputs=" << stored.enabledMidiInputIds.size()
              << " pluginPaths=" << stored.pluginSearchPaths.size()
              << " mcpEnabled=" << (stored.mcpEnabled ? "true" : "false")
              << " mutationAllowed=" << (stored.allowMcpProjectMutation ? "true" : "false")
              << " shortcuts=" << stored.keyboardShortcuts.size()
              << " playShortcut=" << stored.keyboardShortcuts.front().keyEquivalent
              << " privacyScan=" << (stored.allowUserFolderScanning ? "true" : "false")
              << " rejectedUnsafeMutation=" << (rejectedUnsafeMutation ? "true" : "false") << '\n';
    return ready ? 0 : 17;
}

int appSessionTransposeFirstTrackSmoke() {
    const auto path =
        std::filesystem::temp_directory_path() / "lamusica-app-session-transpose.Project.lamusica";
    std::filesystem::remove_all(path);
    lamusica::session::ApplicationSession session;
    session.createFirstTrackProject(path, "Session Transpose");
    session.transposeFirstTrackBass(12);
    session.openProject(path);
    const auto status = session.status();
    std::cout << "LaMusica app session transpose: project=" << status.projectName
              << " firstTrackReady=" << (status.firstTrackReady ? "true" : "false")
              << " starterMidiNotes=" << status.starterMidiNoteCount
              << " bassTranspose=" << status.starterBassTransposeSemitones
              << " renderFrames=" << status.renderFrames << '\n';
    std::filesystem::remove_all(path);
    return status.firstTrackReady && status.starterBassTransposeSemitones == 12 ? 0 : 5;
}

NSMenuItem* addMenuItem(NSMenu* menu, NSString* title, SEL action, NSString* keyEquivalent,
                        id target = nil) {
    auto* item = [[NSMenuItem alloc] initWithTitle:title action:action keyEquivalent:keyEquivalent];
    item.target = target;
    [menu addItem:item];
    return item;
}

NSView* makePanel(NSString* label, NSColor* color) {
    auto* view = [[NSView alloc] initWithFrame:NSZeroRect];
    view.wantsLayer = YES;
    view.layer.backgroundColor = color.CGColor;

    auto* text = [NSTextField labelWithString:label];
    text.tag = panelLabelTag;
    text.translatesAutoresizingMaskIntoConstraints = NO;
    text.textColor = NSColor.labelColor;
    text.font = [NSFont systemFontOfSize:13.0 weight:NSFontWeightSemibold];
    text.lineBreakMode = NSLineBreakByWordWrapping;
    text.usesSingleLineMode = NO;
    text.maximumNumberOfLines = 0;
    [view addSubview:text];

    [NSLayoutConstraint activateConstraints:@[
        [text.leadingAnchor constraintEqualToAnchor:view.leadingAnchor constant:12.0],
        [text.trailingAnchor constraintLessThanOrEqualToAnchor:view.trailingAnchor constant:-12.0],
        [text.topAnchor constraintEqualToAnchor:view.topAnchor constant:10.0],
    ]];

    return view;
}

std::string joinStrings(const std::vector<std::string>& values) {
    if (values.empty()) {
        return "none";
    }
    std::string joined;
    for (const auto& value : values) {
        if (!joined.empty()) {
            joined += ", ";
        }
        joined += value;
    }
    return joined;
}

std::string
shortcutSummary(const std::vector<lamusica::session::KeyboardShortcutPreference>& shortcuts) {
    if (shortcuts.empty()) {
        return "defaults";
    }
    std::string joined;
    for (const auto& shortcut : shortcuts) {
        if (!joined.empty()) {
            joined += ", ";
        }
        joined += shortcut.command + "=" + shortcut.keyEquivalent;
    }
    return joined;
}

NSString* yesNo(bool value) {
    return value ? @"yes" : @"no";
}

NSView* makePreferencesPanel(NSString* title, NSString* details) {
    return makePanel([NSString stringWithFormat:@"%@\n%@", title, details],
                     NSColor.controlBackgroundColor);
}

void addPreferencesTab(NSTabView* tabs, NSString* title, NSString* details) {
    auto* item = [[NSTabViewItem alloc] initWithIdentifier:title];
    item.label = title;
    item.view = makePreferencesPanel(title, details);
    [tabs addTabViewItem:item];
}

NSWindow* buildPreferencesWindow(const lamusica::session::ApplicationPreferences& preferences) {
    const NSRect frame = NSMakeRect(180.0, 180.0, 680.0, 460.0);
    auto* window =
        [[NSWindow alloc] initWithContentRect:frame
                                    styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable
                                      backing:NSBackingStoreBuffered
                                        defer:NO];
    window.title = @"LaMusica Preferences";

    auto* tabs = [[NSTabView alloc] initWithFrame:NSZeroRect];
    tabs.translatesAutoresizingMaskIntoConstraints = NO;
    const auto midiInputs = joinStrings(preferences.enabledMidiInputIds);
    const auto pluginPaths = joinStrings(preferences.pluginSearchPaths);
    const auto shortcuts = shortcutSummary(preferences.keyboardShortcuts);
    addPreferencesTab(
        tabs, @"Audio",
        [NSString stringWithFormat:@"Device: %s", preferences.audioDeviceId.empty()
                                                      ? "System Default"
                                                      : preferences.audioDeviceId.c_str()]);
    addPreferencesTab(tabs, @"MIDI",
                      [NSString stringWithFormat:@"Enabled inputs: %s", midiInputs.c_str()]);
    addPreferencesTab(tabs, @"Plugins",
                      [NSString stringWithFormat:@"Search paths: %s", pluginPaths.c_str()]);
    addPreferencesTab(tabs, @"MCP",
                      [NSString stringWithFormat:@"Enabled: %@\nProject mutation: %@",
                                                 yesNo(preferences.mcpEnabled),
                                                 yesNo(preferences.allowMcpProjectMutation)]);
    addPreferencesTab(tabs, @"Shortcuts",
                      [NSString stringWithFormat:@"Overrides: %s", shortcuts.c_str()]);
    addPreferencesTab(tabs, @"Privacy",
                      [NSString stringWithFormat:@"User folder scanning: %@\nDiagnostics: %@",
                                                 yesNo(preferences.allowUserFolderScanning),
                                                 yesNo(preferences.shareDiagnostics)]);

    auto* contentView = [[NSView alloc] initWithFrame:NSZeroRect];
    window.contentView = contentView;
    [contentView addSubview:tabs];
    [NSLayoutConstraint activateConstraints:@[
        [tabs.leadingAnchor constraintEqualToAnchor:contentView.leadingAnchor constant:12.0],
        [tabs.trailingAnchor constraintEqualToAnchor:contentView.trailingAnchor constant:-12.0],
        [tabs.topAnchor constraintEqualToAnchor:contentView.topAnchor constant:12.0],
        [tabs.bottomAnchor constraintEqualToAnchor:contentView.bottomAnchor constant:-12.0],
    ]];

    return window;
}

} // namespace

@interface LaMusicaAppDelegate : NSObject <NSApplicationDelegate, NSMenuItemValidation> {
    std::unique_ptr<lamusica::session::ApplicationSession> _session;
}
@property(strong) NSWindow* mainWindow;
@property(strong) NSWindow* preferencesWindow;
@property(strong) NSView* transportPanel;
@property(strong) NSView* browserPanel;
@property(strong) NSView* timelinePanel;
@property(strong) NSView* inspectorPanel;
@property(strong) NSView* mixerPanel;
- (void)refreshMainWindowPanels;
- (void)setPanel:(NSView*)panel text:(NSString*)text;
@end

@implementation LaMusicaAppDelegate

- (instancetype)init {
    self = [super init];
    if (self != nil) {
        _session = std::make_unique<lamusica::session::ApplicationSession>();
    }
    return self;
}

- (void)newProject:(id)sender {
    (void)sender;
    try {
        const auto path = uniqueUntitledFirstTrackProjectPath();
        _session->createFirstTrackProject(path, "Untitled");
        [self refreshMainWindowPanels];
    } catch (const std::exception& error) {
        [self presentSessionError:error];
    }
}

- (void)openDocument:(id)sender {
    (void)sender;
    auto* panel = [NSOpenPanel openPanel];
    panel.canChooseFiles = NO;
    panel.canChooseDirectories = YES;
    panel.allowsMultipleSelection = NO;
    if ([panel runModal] == NSModalResponseOK) {
        try {
            _session->openProject(std::filesystem::path{panel.URL.path.UTF8String});
            [self refreshMainWindowPanels];
        } catch (const std::exception& error) {
            [self presentSessionError:error];
        }
    }
}

- (void)saveDocument:(id)sender {
    (void)sender;
    if (!routeEnabled(_session.get(), "project.save")) {
        return;
    }
    try {
        _session->saveProject();
        [self refreshMainWindowPanels];
    } catch (const std::exception& error) {
        [self presentSessionError:error];
    }
}

- (void)undo:(id)sender {
    (void)sender;
    if (routeEnabled(_session.get(), "edit.undo") && _session->undoLastEdit()) {
        [self refreshMainWindowPanels];
    }
}

- (void)redo:(id)sender {
    (void)sender;
    if (routeEnabled(_session.get(), "edit.redo") && _session->redoLastEdit()) {
        [self refreshMainWindowPanels];
    }
}

- (void)exportMix:(id)sender {
    (void)sender;
    if (!routeEnabled(_session.get(), "project.exportMix")) {
        return;
    }

    auto* panel = [NSSavePanel savePanel];
    panel.allowedFileTypes = @[ @"wav" ];
    panel.nameFieldStringValue = @"first-track.wav";
    if ([panel runModal] == NSModalResponseOK) {
        try {
            _session->exportCurrentMix(std::filesystem::path{panel.URL.path.UTF8String});
            [self refreshMainWindowPanels];
        } catch (const std::exception& error) {
            [self presentSessionError:error];
        }
    }
}

- (void)exportLoop:(id)sender {
    (void)sender;
    if (!routeEnabled(_session.get(), "project.exportLoop")) {
        return;
    }

    auto* panel = [NSSavePanel savePanel];
    panel.allowedFileTypes = @[ @"wav" ];
    panel.nameFieldStringValue = @"first-track-loop.wav";
    if ([panel runModal] == NSModalResponseOK) {
        try {
            _session->exportCurrentLoop(std::filesystem::path{panel.URL.path.UTF8String});
            [self refreshMainWindowPanels];
        } catch (const std::exception& error) {
            [self presentSessionError:error];
        }
    }
}

- (void)exportStems:(id)sender {
    (void)sender;
    if (!routeEnabled(_session.get(), "project.exportStems")) {
        return;
    }

    auto* panel = [NSOpenPanel openPanel];
    panel.canChooseFiles = NO;
    panel.canChooseDirectories = YES;
    panel.canCreateDirectories = YES;
    panel.allowsMultipleSelection = NO;
    if ([panel runModal] == NSModalResponseOK) {
        try {
            _session->exportCurrentStems(std::filesystem::path{panel.URL.path.UTF8String});
            [self refreshMainWindowPanels];
        } catch (const std::exception& error) {
            [self presentSessionError:error];
        }
    }
}

- (void)exportFirstTrackPackage:(id)sender {
    (void)sender;
    if (!routeEnabled(_session.get(), "project.exportFirstTrackPackage")) {
        return;
    }

    auto* panel = [NSOpenPanel openPanel];
    panel.canChooseFiles = NO;
    panel.canChooseDirectories = YES;
    panel.canCreateDirectories = YES;
    panel.allowsMultipleSelection = NO;
    if ([panel runModal] == NSModalResponseOK) {
        try {
            _session->exportFirstTrackPackage(std::filesystem::path{panel.URL.path.UTF8String});
            [self refreshMainWindowPanels];
        } catch (const std::exception& error) {
            [self presentSessionError:error];
        }
    }
}

- (void)verifyFirstTrackPackage:(id)sender {
    (void)sender;
    if (!routeEnabled(_session.get(), "project.verifyFirstTrackPackage")) {
        return;
    }

    auto* panel = [NSOpenPanel openPanel];
    panel.canChooseFiles = NO;
    panel.canChooseDirectories = YES;
    panel.allowsMultipleSelection = NO;
    if ([panel runModal] == NSModalResponseOK) {
        try {
            _session->verifyFirstTrackPackage(std::filesystem::path{panel.URL.path.UTF8String});
            [self refreshMainWindowPanels];
        } catch (const std::exception& error) {
            [self presentSessionError:error];
        }
    }
}

- (void)verifyFirstTrackProject:(id)sender {
    (void)sender;
    if (!routeEnabled(_session.get(), "project.verifyFirstTrackProject")) {
        return;
    }

    [self refreshMainWindowPanels];
    const auto verification = _session->verifyFirstTrackProject();
    if (verification.firstTrackReady) {
        self.mainWindow.title = [NSString
            stringWithFormat:@"LaMusica - First Track Ready: %s", verification.projectName.c_str()];
    } else if (!verification.mediaReady) {
        self.mainWindow.title =
            [NSString stringWithFormat:@"LaMusica - First Track Media Missing: %s",
                                       verification.mediaError.c_str()];
    } else {
        const auto firstMissing = verification.missingRequirements.empty()
                                      ? std::string{"unknown"}
                                      : verification.missingRequirements.front();
        self.mainWindow.title = [NSString
            stringWithFormat:@"LaMusica - First Track Not Ready: %zu missing, first %s",
                             verification.missingRequirements.size(), firstMissing.c_str()];
    }
}

- (void)importAudioFile:(id)sender {
    (void)sender;
    if (!routeEnabled(_session.get(), "project.importAudio")) {
        return;
    }

    auto* panel = [NSOpenPanel openPanel];
    panel.canChooseFiles = YES;
    panel.canChooseDirectories = NO;
    panel.allowsMultipleSelection = NO;
    panel.allowedFileTypes = @[ @"wav", @"wave" ];
    if ([panel runModal] == NSModalResponseOK) {
        try {
            _session->importAudioFileToFirstTrack(std::filesystem::path{panel.URL.path.UTF8String},
                                                  _session->status().playheadSample);
            [self refreshMainWindowPanels];
        } catch (const std::exception& error) {
            [self presentSessionError:error];
        }
    }
}

- (void)relinkMissingMedia:(id)sender {
    (void)sender;
    if (!routeEnabled(_session.get(), "project.relinkFirstTrackMedia")) {
        return;
    }

    auto* panel = [NSOpenPanel openPanel];
    panel.canChooseFiles = YES;
    panel.canChooseDirectories = NO;
    panel.allowsMultipleSelection = NO;
    panel.allowedFileTypes = @[ @"wav", @"wave" ];
    if ([panel runModal] == NSModalResponseOK) {
        try {
            _session->relinkFirstTrackAudioAsset(_session->status().missingMediaAssetId,
                                                 std::filesystem::path{panel.URL.path.UTF8String});
            [self refreshMainWindowPanels];
        } catch (const std::exception& error) {
            [self presentSessionError:error];
        }
    }
}

- (void)recordFirstTrackTake:(id)sender {
    (void)sender;
    if (!routeEnabled(_session.get(), "project.recordFirstTrackTake")) {
        return;
    }

    const auto frames = _session->status().loopFrames == 0 ? 48000U : _session->status().loopFrames;
    try {
        const auto& status = _session->status();
        const auto startSample =
            status.loopEnabled ? status.loopStartSample : status.playheadSample;
        lamusica::session::FirstTrackRecordingOptions options;
        options.frames = frames;
        options.startSample = std::max<std::int64_t>(0, startSample);
        _session->recordFirstTrackTake(options);
        [self refreshMainWindowPanels];
    } catch (const std::exception& error) {
        [self presentSessionError:error];
    }
}

- (void)setIntroLoop:(id)sender {
    (void)sender;
    if (!routeEnabled(_session.get(), "project.loopIntro")) {
        return;
    }
    try {
        _session->setFirstTrackLoopToIntro();
        [self refreshMainWindowPanels];
    } catch (const std::exception& error) {
        [self presentSessionError:error];
    }
}

- (void)extendArrangement:(id)sender {
    (void)sender;
    if (!routeEnabled(_session.get(), "project.extendIntroToVerse")) {
        return;
    }
    try {
        _session->extendFirstTrackArrangementToVerse();
        [self refreshMainWindowPanels];
    } catch (const std::exception& error) {
        [self presentSessionError:error];
    }
}

- (void)transposeBass:(id)sender {
    auto* item = (NSMenuItem*)sender;
    const bool down = [item.title containsString:@"Down"];
    if (!routeEnabled(_session.get(),
                      down ? "project.transposeBassDownOctave" : "project.transposeBassUpOctave")) {
        return;
    }
    const auto current = _session == nullptr ? 0 : _session->status().starterBassTransposeSemitones;
    const auto delta = down ? -12 : 12;
    try {
        _session->transposeFirstTrackBass(std::clamp(current + delta, -24, 24));
        [self refreshMainWindowPanels];
    } catch (const std::exception& error) {
        [self presentSessionError:error];
    }
}

- (void)adjustStarterMix:(id)sender {
    auto* item = (NSMenuItem*)sender;
    const bool drums = [item.title containsString:@"Drums"];
    const bool down = [item.title containsString:@"Down"];
    if (!routeEnabled(_session.get(),
                      drums ? (down ? "project.drumsGainDown" : "project.drumsGainUp")
                            : (down ? "project.bassGainDown" : "project.bassGainUp"))) {
        return;
    }
    try {
        if (drums) {
            const auto current = _session->status().drumClipGainDb;
            const auto delta = down ? -3.0F : 3.0F;
            _session->setClipGain("drum-loop", std::clamp(current + delta, -60.0F, 24.0F));
        } else {
            const auto current = _session->status().bassClipGainDb;
            const auto delta = down ? -3.0F : 3.0F;
            _session->setClipGain("bass-pattern", std::clamp(current + delta, -60.0F, 24.0F));
        }
        [self refreshMainWindowPanels];
    } catch (const std::exception& error) {
        [self presentSessionError:error];
    }
}

- (void)adjustFirstTrackTrackMix:(id)sender {
    auto* item = (NSMenuItem*)sender;
    try {
        const char* trackId = "bass";
        std::string routePrefix{"bass"};
        if ([item.title containsString:@"Drums"]) {
            trackId = "drums";
            routePrefix = "drums";
        } else if ([item.title containsString:@"Recorded"]) {
            trackId = "recorded-takes";
            routePrefix = "recorded";
        } else if ([item.title containsString:@"Imported"]) {
            trackId = "imported-audio";
            routePrefix = "imported";
        }

        std::string command;
        if ([item.title containsString:@"Volume Up"]) {
            command = "project." + routePrefix + "TrackVolumeUp";
        } else if ([item.title containsString:@"Volume Down"]) {
            command = "project." + routePrefix + "TrackVolumeDown";
        } else if ([item.title containsString:@"Pan Left"]) {
            command = "project." + routePrefix + "PanLeft";
        } else if ([item.title containsString:@"Pan Right"]) {
            command = "project." + routePrefix + "PanRight";
        } else if ([item.title containsString:@"Mute"]) {
            command = routePrefix == "recorded"   ? "project.toggleRecordedMute"
                      : routePrefix == "imported" ? "project.toggleImportedMute"
                      : routePrefix == "drums"    ? "project.toggleDrumsMute"
                                                  : "project.toggleBassMute";
        } else if ([item.title containsString:@"Solo"]) {
            command = routePrefix == "recorded"   ? "project.toggleRecordedSolo"
                      : routePrefix == "imported" ? "project.toggleImportedSolo"
                      : routePrefix == "drums"    ? "project.toggleDrumsSolo"
                                                  : "project.toggleBassSolo";
        }
        if (command.empty() || !routeEnabled(_session.get(), command)) {
            return;
        }

        const auto current = trackMixFor(_session->firstTrackTrackMix(), trackId);
        auto volumeDb = current.volumeDb;
        auto pan = current.pan;
        auto muted = current.muted;
        auto solo = current.solo;

        if ([item.title containsString:@"Volume Up"]) {
            volumeDb = std::clamp(volumeDb + 3.0F, -120.0F, 24.0F);
        } else if ([item.title containsString:@"Volume Down"]) {
            volumeDb = std::clamp(volumeDb - 3.0F, -120.0F, 24.0F);
        } else if ([item.title containsString:@"Pan Left"]) {
            pan = std::clamp(pan - 0.25F, -1.0F, 1.0F);
        } else if ([item.title containsString:@"Pan Right"]) {
            pan = std::clamp(pan + 0.25F, -1.0F, 1.0F);
        } else if ([item.title containsString:@"Mute"]) {
            muted = !muted;
        } else if ([item.title containsString:@"Solo"]) {
            solo = !solo;
        }

        _session->setFirstTrackTrackMix(trackId, volumeDb, pan, muted, solo);
        [self refreshMainWindowPanels];
    } catch (const std::exception& error) {
        [self presentSessionError:error];
    }
}

- (void)softenLastTakeFades:(id)sender {
    (void)sender;
    if (!routeEnabled(_session.get(), "project.softenLastTakeFades")) {
        return;
    }
    try {
        _session->setClipFades(_session->status().lastRecordingClipId, 512, 512);
        [self refreshMainWindowPanels];
    } catch (const std::exception& error) {
        [self presentSessionError:error];
    }
}

- (void)reverseLastTake:(id)sender {
    (void)sender;
    if (!routeEnabled(_session.get(), "project.toggleLastTakeReverse")) {
        return;
    }
    try {
        const auto clip = clipSummaryFor(*_session, _session->status().lastRecordingClipId);
        _session->setClipReversed(_session->status().lastRecordingClipId,
                                  clip.has_value() ? !clip->reversed : true);
        [self refreshMainWindowPanels];
    } catch (const std::exception& error) {
        [self presentSessionError:error];
    }
}

- (void)muteLastTake:(id)sender {
    (void)sender;
    if (!routeEnabled(_session.get(), "project.toggleLastTakeMute")) {
        return;
    }
    try {
        const auto clip = clipSummaryFor(*_session, _session->status().lastRecordingClipId);
        _session->setClipMuted(_session->status().lastRecordingClipId,
                               clip.has_value() ? !clip->muted : true);
        [self refreshMainWindowPanels];
    } catch (const std::exception& error) {
        [self presentSessionError:error];
    }
}

- (void)trimLastTakeToLoop:(id)sender {
    (void)sender;
    if (!routeEnabled(_session.get(), "project.trimLastTakeToLoop")) {
        return;
    }
    try {
        const auto& status = _session->status();
        _session->setClipTiming(status.lastRecordingClipId, status.loopStartSample,
                                status.loopFrames, 0);
        [self refreshMainWindowPanels];
    } catch (const std::exception& error) {
        [self presentSessionError:error];
    }
}

- (void)duplicateLastTakeAtPlayhead:(id)sender {
    (void)sender;
    if (!routeEnabled(_session.get(), "project.duplicateLastTakeAtPlayhead")) {
        return;
    }
    try {
        const auto& status = _session->status();
        _session->duplicateClip(status.lastRecordingClipId, {}, status.playheadSample);
        [self refreshMainWindowPanels];
    } catch (const std::exception& error) {
        [self presentSessionError:error];
    }
}

- (void)removeLastTake:(id)sender {
    (void)sender;
    if (!routeEnabled(_session.get(), "project.removeLastTake")) {
        return;
    }
    try {
        _session->removeClip(_session->status().lastRecordingClipId);
        [self refreshMainWindowPanels];
    } catch (const std::exception& error) {
        [self presentSessionError:error];
    }
}

- (void)softenLastImportFades:(id)sender {
    (void)sender;
    if (!routeEnabled(_session.get(), "project.softenLastImportFades")) {
        return;
    }
    try {
        _session->setClipFades(_session->status().lastImportClipId, 512, 512);
        [self refreshMainWindowPanels];
    } catch (const std::exception& error) {
        [self presentSessionError:error];
    }
}

- (void)reverseLastImport:(id)sender {
    (void)sender;
    if (!routeEnabled(_session.get(), "project.toggleLastImportReverse")) {
        return;
    }
    try {
        const auto clip = clipSummaryFor(*_session, _session->status().lastImportClipId);
        _session->setClipReversed(_session->status().lastImportClipId,
                                  clip.has_value() ? !clip->reversed : true);
        [self refreshMainWindowPanels];
    } catch (const std::exception& error) {
        [self presentSessionError:error];
    }
}

- (void)muteLastImport:(id)sender {
    (void)sender;
    if (!routeEnabled(_session.get(), "project.toggleLastImportMute")) {
        return;
    }
    try {
        const auto clip = clipSummaryFor(*_session, _session->status().lastImportClipId);
        _session->setClipMuted(_session->status().lastImportClipId,
                               clip.has_value() ? !clip->muted : true);
        [self refreshMainWindowPanels];
    } catch (const std::exception& error) {
        [self presentSessionError:error];
    }
}

- (void)trimLastImportToLoop:(id)sender {
    (void)sender;
    if (!routeEnabled(_session.get(), "project.trimLastImportToLoop")) {
        return;
    }
    try {
        const auto& status = _session->status();
        _session->setClipTiming(status.lastImportClipId, status.loopStartSample, status.loopFrames,
                                0);
        [self refreshMainWindowPanels];
    } catch (const std::exception& error) {
        [self presentSessionError:error];
    }
}

- (void)duplicateLastImportAtPlayhead:(id)sender {
    (void)sender;
    if (!routeEnabled(_session.get(), "project.duplicateLastImportAtPlayhead")) {
        return;
    }
    try {
        const auto& status = _session->status();
        _session->duplicateClip(status.lastImportClipId, {}, status.playheadSample);
        [self refreshMainWindowPanels];
    } catch (const std::exception& error) {
        [self presentSessionError:error];
    }
}

- (void)removeLastImport:(id)sender {
    (void)sender;
    if (!routeEnabled(_session.get(), "project.removeLastImport")) {
        return;
    }
    try {
        _session->removeClip(_session->status().lastImportClipId);
        [self refreshMainWindowPanels];
    } catch (const std::exception& error) {
        [self presentSessionError:error];
    }
}

- (void)closeDocument:(id)sender {
    (void)sender;
    if (!routeEnabled(_session.get(), "project.close")) {
        return;
    }
    _session->closeProject();
    [self.mainWindow close];
}

- (void)showPreferences:(id)sender {
    (void)sender;
    self.preferencesWindow =
        buildPreferencesWindow(_session == nullptr ? lamusica::session::ApplicationPreferences{}
                                                   : _session->preferences());
    [self.preferencesWindow makeKeyAndOrderFront:nil];
}

- (void)play:(id)sender {
    (void)sender;
    (void)_session->routeMenuCommand("transport.play");
    try {
        _session->play();
        [self refreshMainWindowPanels];
    } catch (const std::exception& error) {
        [self presentSessionError:error];
    }
}

- (void)stop:(id)sender {
    (void)sender;
    (void)_session->routeMenuCommand("transport.stop");
    _session->stop();
    [self refreshMainWindowPanels];
}

- (void)showPrimaryPanel:(id)sender {
    auto* item = (NSMenuItem*)sender;
    if ([item.title isEqualToString:@"Show Browser"]) {
        (void)_session->routeMenuCommand("view.browser");
    } else if ([item.title isEqualToString:@"Show Timeline"]) {
        (void)_session->routeMenuCommand("view.timeline");
    } else if ([item.title isEqualToString:@"Show Mixer"]) {
        (void)_session->routeMenuCommand("view.mixer");
    } else if ([item.title isEqualToString:@"Show Inspector"]) {
        (void)_session->routeMenuCommand("view.inspector");
    }
    [self refreshMainWindowPanels];
}

- (void)rescanPlugins:(id)sender {
    (void)sender;
    self.mainWindow.title = @"LaMusica - Plugin Scan";
}

- (void)showHelp:(id)sender {
    (void)sender;
    self.mainWindow.title = @"LaMusica - Help";
}

- (void)setPanel:(NSView*)panel text:(NSString*)text {
    auto* label = (NSTextField*)[panel viewWithTag:panelLabelTag];
    label.stringValue = text;
}

- (void)presentSessionError:(const std::exception&)error {
    [self refreshMainWindowPanels];
    self.mainWindow.title = [NSString stringWithFormat:@"LaMusica - %s", error.what()];
}

- (void)refreshMainWindowPanels {
    if (self.mainWindow == nil || _session == nullptr) {
        return;
    }

    const auto& status = _session->status();
    const auto trackMixes = _session->firstTrackTrackMix();
    const auto drumMix = trackMixFor(trackMixes, "drums");
    const auto bassMix = trackMixFor(trackMixes, "bass");
    const auto recordedMix = trackMixFor(trackMixes, "recorded-takes");
    const auto importedMix = trackMixFor(trackMixes, "imported-audio");
    if (!status.hasOpenProject) {
        self.mainWindow.title = @"LaMusica";
        [self setPanel:self.transportPanel text:@"Transport\nNo project open"];
        [self setPanel:self.browserPanel text:@"Browser\nNo project open"];
        [self setPanel:self.timelinePanel text:@"Timeline / Piano Roll\nNo project open"];
        [self setPanel:self.inspectorPanel text:@"Inspector\nNo project open"];
        [self setPanel:self.mixerPanel text:@"Mixer\nNo project open"];
        return;
    }

    const auto* state = status.transportPlaying ? "Playing" : "Stopped";
    self.mainWindow.title =
        [NSString stringWithFormat:@"LaMusica - %s", status.projectName.c_str()];
    [self setPanel:self.transportPanel
              text:[NSString stringWithFormat:@"%s\n%s | %.0f BPM | %u/%u | %u frames\nLoop %s "
                                              @"%lld-%lld | Playhead %lld",
                                              status.projectName.c_str(), state, status.tempoBpm,
                                              status.timeSignatureNumerator,
                                              status.timeSignatureDenominator, status.renderFrames,
                                              status.loopEnabled ? "on" : "off",
                                              status.loopStartSample, status.loopEndSample,
                                              status.playheadSample]];
    [self setPanel:self.browserPanel
              text:[NSString stringWithFormat:
                                 @"Browser\nReady: %s | editable: %s | media: %s\nTracks %zu | "
                                 @"Clips %zu",
                                 status.firstTrackReady ? "yes" : "no",
                                 status.firstTrackEditable ? "yes" : "no",
                                 status.mediaReady ? "ok" : "missing", status.trackCount,
                                 status.clipCount]];
    [self setPanel:self.timelinePanel
              text:[NSString stringWithFormat:
                                 @"Timeline / Piano Roll\n%s -> %s\nSections %zu | MIDI notes %zu "
                                 @"| Bass %+d st",
                                 status.firstSectionName.c_str(), status.finalSectionName.c_str(),
                                 status.markerCount, status.starterMidiNoteCount,
                                 status.starterBassTransposeSemitones]];
    [self setPanel:self.inspectorPanel
              text:[NSString
                       stringWithFormat:@"Inspector\nStarter devices %zu | Automation lanes %zu\n"
                                        @"Drums %.1f dB | Bass %.1f dB\nRecorded takes %zu | "
                                        @"Muted %zu | Imports %zu\nLast take %u frames @ %lld | "
                                        @"Count-in %u",
                                        status.pluginCount, status.automationLaneCount,
                                        status.drumClipGainDb, status.bassClipGainDb,
                                        status.recordedTakeCount, status.mutedRecordedTakeCount,
                                        status.importedAudioClipCount, status.lastRecordingFrames,
                                        status.lastRecordingStartSample,
                                        status.lastRecordingCountInSamples]];
    [self setPanel:self.mixerPanel
              text:[NSString
                       stringWithFormat:@"Mixer\nGenerated Drums -> Master | Generated Bass "
                                        @"-> Master\nDrums %.1f dB pan %.2f %s %s | Bass "
                                        @"%.1f dB pan %.2f %s %s\nLast export %u frames | "
                                        @"peak %.2f\nRecorded %.1f dB pan %.2f | Imported %.1f "
                                        @"dB pan %.2f\nPackage mix %u | loop %u | stems %zu | "
                                        @"verified %s",
                                        drumMix.volumeDb, drumMix.pan,
                                        drumMix.muted ? "muted" : "open",
                                        drumMix.solo ? "solo" : "mix", bassMix.volumeDb,
                                        bassMix.pan, bassMix.muted ? "muted" : "open",
                                        bassMix.solo ? "solo" : "mix", status.lastMixExportFrames,
                                        status.lastMixExportPeak, recordedMix.volumeDb,
                                        recordedMix.pan, importedMix.volumeDb, importedMix.pan,
                                        status.lastPackageMixFrames, status.lastPackageLoopFrames,
                                        status.lastPackageStemCount,
                                        status.lastPackageVerified ? "yes" : "no"]];
}

- (BOOL)validateMenuItem:(NSMenuItem*)menuItem {
    const SEL action = menuItem.action;
    if (action == @selector(saveDocument:) || action == @selector(closeDocument:) ||
        action == @selector(undo:) || action == @selector(redo:) ||
        action == @selector(exportMix:) || action == @selector(exportLoop:) ||
        action == @selector(exportStems:) || action == @selector(exportFirstTrackPackage:) ||
        action == @selector(verifyFirstTrackPackage:) ||
        action == @selector(verifyFirstTrackProject:) || action == @selector(importAudioFile:) ||
        action == @selector(relinkMissingMedia:) || action == @selector(recordFirstTrackTake:) ||
        action == @selector(setIntroLoop:) || action == @selector(extendArrangement:) ||
        action == @selector(play:) || action == @selector(transposeBass:) ||
        action == @selector(adjustStarterMix:) || action == @selector(adjustFirstTrackTrackMix:) ||
        action == @selector(softenLastTakeFades:) || action == @selector(trimLastTakeToLoop:) ||
        action == @selector(duplicateLastTakeAtPlayhead:) || action == @selector(stop:) ||
        action == @selector(removeLastTake:) || action == @selector(reverseLastTake:) ||
        action == @selector(muteLastTake:) || action == @selector(softenLastImportFades:) ||
        action == @selector(trimLastImportToLoop:) ||
        action == @selector(duplicateLastImportAtPlayhead:) ||
        action == @selector(removeLastImport:) || action == @selector(reverseLastImport:) ||
        action == @selector(muteLastImport:) || action == @selector(showPrimaryPanel:) ||
        action == @selector(rescanPlugins:)) {
        if (_session == nullptr) {
            return NO;
        }
        const auto& status = _session->status();
        if (action == @selector(showPrimaryPanel:) || action == @selector(rescanPlugins:) ||
            action == @selector(verifyFirstTrackPackage:)) {
            return self.mainWindow != nil;
        }
        if (action == @selector(undo:)) {
            return status.canUndo;
        }
        if (action == @selector(redo:)) {
            return status.canRedo;
        }
        if (action == @selector(play:) || action == @selector(stop:) ||
            action == @selector(saveDocument:) || action == @selector(closeDocument:) ||
            action == @selector(verifyFirstTrackProject:)) {
            return status.hasOpenProject;
        }
        if (action == @selector(exportMix:) || action == @selector(exportStems:)) {
            return status.hasOpenProject && status.mediaReady && status.renderFrames > 0;
        }
        if (action == @selector(exportLoop:)) {
            return status.hasOpenProject && status.mediaReady && status.loopEnabled &&
                   status.loopFrames > 0;
        }
        if (action == @selector(setIntroLoop:)) {
            return status.firstTrackEditable;
        }
        if (action == @selector(relinkMissingMedia:)) {
            return status.hasOpenProject && !status.mediaReady &&
                   !status.missingMediaAssetId.empty();
        }
        if (action == @selector(exportFirstTrackPackage:) ||
            action == @selector(importAudioFile:) || action == @selector(recordFirstTrackTake:) ||
            action == @selector(extendArrangement:) || action == @selector(transposeBass:) ||
            action == @selector(adjustStarterMix:) ||
            action == @selector(adjustFirstTrackTrackMix:) ||
            action == @selector(softenLastTakeFades:)) {
            if (action == @selector(exportFirstTrackPackage:)) {
                return status.hasOpenProject && status.firstTrackReady;
            }
            if (action == @selector(softenLastTakeFades:)) {
                return status.firstTrackEditable && !status.lastRecordingClipId.empty();
            }
            if (action == @selector(adjustFirstTrackTrackMix:)) {
                if ([menuItem.title containsString:@"Recorded"]) {
                    return status.firstTrackEditable && status.recordedTakeCount > 0;
                }
                if ([menuItem.title containsString:@"Imported"]) {
                    return status.firstTrackEditable && status.importedAudioClipCount > 0;
                }
            }
            return status.firstTrackEditable;
        }
        if (action == @selector(reverseLastTake:)) {
            return status.firstTrackEditable && !status.lastRecordingClipId.empty();
        }
        if (action == @selector(muteLastTake:)) {
            return status.firstTrackEditable && !status.lastRecordingClipId.empty();
        }
        if (action == @selector(trimLastTakeToLoop:)) {
            return status.firstTrackEditable && status.loopEnabled && status.loopFrames > 0 &&
                   !status.lastRecordingClipId.empty();
        }
        if (action == @selector(duplicateLastTakeAtPlayhead:)) {
            return status.firstTrackEditable && !status.lastRecordingClipId.empty();
        }
        if (action == @selector(removeLastTake:)) {
            return status.firstTrackEditable && !status.lastRecordingClipId.empty();
        }
        if (action == @selector(softenLastImportFades:) ||
            action == @selector(reverseLastImport:) || action == @selector(muteLastImport:)) {
            return status.firstTrackEditable && !status.lastImportClipId.empty();
        }
        if (action == @selector(trimLastImportToLoop:)) {
            return status.firstTrackEditable && status.loopEnabled && status.loopFrames > 0 &&
                   !status.lastImportClipId.empty();
        }
        if (action == @selector(duplicateLastImportAtPlayhead:)) {
            return status.firstTrackEditable && !status.lastImportClipId.empty();
        }
        if (action == @selector(removeLastImport:)) {
            return status.firstTrackEditable && !status.lastImportClipId.empty();
        }
        return NO;
    }
    return YES;
}

@end

namespace {

void buildMenuBar(LaMusicaAppDelegate* delegate) {
    auto* mainMenu = [[NSMenu alloc] initWithTitle:@"MainMenu"];
    NSApp.mainMenu = mainMenu;

    auto* appMenuItem = [[NSMenuItem alloc] initWithTitle:@"" action:nil keyEquivalent:@""];
    [mainMenu addItem:appMenuItem];
    auto* appMenu = [[NSMenu alloc] initWithTitle:@"LaMusica"];
    appMenuItem.submenu = appMenu;
    addMenuItem(appMenu, @"Preferences...", @selector(showPreferences:), @",", delegate);
    [appMenu addItem:[NSMenuItem separatorItem]];
    addMenuItem(appMenu, @"Quit LaMusica", @selector(terminate:), @"q", NSApp);

    auto* projectMenuItem = [[NSMenuItem alloc] initWithTitle:@"Project"
                                                       action:nil
                                                keyEquivalent:@""];
    [mainMenu addItem:projectMenuItem];
    auto* projectMenu = [[NSMenu alloc] initWithTitle:@"Project"];
    projectMenuItem.submenu = projectMenu;
    addMenuItem(projectMenu, @"New Project", @selector(newProject:), @"n", delegate);
    addMenuItem(projectMenu, @"Open Project...", @selector(openDocument:), @"o", delegate);
    addMenuItem(projectMenu, @"Save", @selector(saveDocument:), @"s", delegate);
    addMenuItem(projectMenu, @"Export Mix...", @selector(exportMix:), @"e", delegate);
    addMenuItem(projectMenu, @"Export Loop...", @selector(exportLoop:), @"E", delegate);
    addMenuItem(projectMenu, @"Export Stems...", @selector(exportStems:), @"", delegate);
    addMenuItem(projectMenu, @"Export First Track Package...", @selector(exportFirstTrackPackage:),
                @"", delegate);
    addMenuItem(projectMenu, @"Verify First Track Package...", @selector(verifyFirstTrackPackage:),
                @"", delegate);
    addMenuItem(projectMenu, @"Verify First Track Project", @selector(verifyFirstTrackProject:),
                @"", delegate);
    addMenuItem(projectMenu, @"Import Audio...", @selector(importAudioFile:), @"i", delegate);
    addMenuItem(projectMenu, @"Relink Missing Media...", @selector(relinkMissingMedia:), @"",
                delegate);
    addMenuItem(projectMenu, @"Record First Track Take", @selector(recordFirstTrackTake:), @"r",
                delegate);
    addMenuItem(projectMenu, @"Soften Last Take Fades", @selector(softenLastTakeFades:), @"",
                delegate);
    addMenuItem(projectMenu, @"Toggle Last Take Mute", @selector(muteLastTake:), @"", delegate);
    addMenuItem(projectMenu, @"Toggle Last Take Reverse", @selector(reverseLastTake:), @"",
                delegate);
    addMenuItem(projectMenu, @"Trim Last Take To Loop", @selector(trimLastTakeToLoop:), @"",
                delegate);
    addMenuItem(projectMenu, @"Duplicate Last Take At Playhead",
                @selector(duplicateLastTakeAtPlayhead:), @"", delegate);
    addMenuItem(projectMenu, @"Remove Last Take", @selector(removeLastTake:), @"", delegate);
    addMenuItem(projectMenu, @"Soften Last Import Fades", @selector(softenLastImportFades:), @"",
                delegate);
    addMenuItem(projectMenu, @"Toggle Last Import Mute", @selector(muteLastImport:), @"", delegate);
    addMenuItem(projectMenu, @"Toggle Last Import Reverse", @selector(reverseLastImport:), @"",
                delegate);
    addMenuItem(projectMenu, @"Trim Last Import To Loop", @selector(trimLastImportToLoop:), @"",
                delegate);
    addMenuItem(projectMenu, @"Duplicate Last Import At Playhead",
                @selector(duplicateLastImportAtPlayhead:), @"", delegate);
    addMenuItem(projectMenu, @"Remove Last Import", @selector(removeLastImport:), @"", delegate);
    addMenuItem(projectMenu, @"Loop Intro Section", @selector(setIntroLoop:), @"l", delegate);
    addMenuItem(projectMenu, @"Extend Intro To Verse", @selector(extendArrangement:), @"L",
                delegate);
    addMenuItem(projectMenu, @"Transpose Bass Up Octave", @selector(transposeBass:), @"]",
                delegate);
    addMenuItem(projectMenu, @"Transpose Bass Down Octave", @selector(transposeBass:), @"[",
                delegate);
    addMenuItem(projectMenu, @"Drums Up 3 dB", @selector(adjustStarterMix:), @"", delegate);
    addMenuItem(projectMenu, @"Drums Down 3 dB", @selector(adjustStarterMix:), @"", delegate);
    addMenuItem(projectMenu, @"Bass Up 3 dB", @selector(adjustStarterMix:), @"", delegate);
    addMenuItem(projectMenu, @"Bass Down 3 dB", @selector(adjustStarterMix:), @"", delegate);
    addMenuItem(projectMenu, @"Drums Track Volume Up 3 dB", @selector(adjustFirstTrackTrackMix:),
                @"", delegate);
    addMenuItem(projectMenu, @"Drums Track Volume Down 3 dB", @selector(adjustFirstTrackTrackMix:),
                @"", delegate);
    addMenuItem(projectMenu, @"Drums Pan Left", @selector(adjustFirstTrackTrackMix:), @"",
                delegate);
    addMenuItem(projectMenu, @"Drums Pan Right", @selector(adjustFirstTrackTrackMix:), @"",
                delegate);
    addMenuItem(projectMenu, @"Toggle Drums Mute", @selector(adjustFirstTrackTrackMix:), @"",
                delegate);
    addMenuItem(projectMenu, @"Toggle Drums Solo", @selector(adjustFirstTrackTrackMix:), @"",
                delegate);
    addMenuItem(projectMenu, @"Bass Track Volume Up 3 dB", @selector(adjustFirstTrackTrackMix:),
                @"", delegate);
    addMenuItem(projectMenu, @"Bass Track Volume Down 3 dB", @selector(adjustFirstTrackTrackMix:),
                @"", delegate);
    addMenuItem(projectMenu, @"Bass Pan Left", @selector(adjustFirstTrackTrackMix:), @"", delegate);
    addMenuItem(projectMenu, @"Bass Pan Right", @selector(adjustFirstTrackTrackMix:), @"",
                delegate);
    addMenuItem(projectMenu, @"Toggle Bass Mute", @selector(adjustFirstTrackTrackMix:), @"",
                delegate);
    addMenuItem(projectMenu, @"Toggle Bass Solo", @selector(adjustFirstTrackTrackMix:), @"",
                delegate);
    addMenuItem(projectMenu, @"Recorded Track Volume Up 3 dB", @selector(adjustFirstTrackTrackMix:),
                @"", delegate);
    addMenuItem(projectMenu, @"Recorded Track Volume Down 3 dB",
                @selector(adjustFirstTrackTrackMix:), @"", delegate);
    addMenuItem(projectMenu, @"Recorded Pan Left", @selector(adjustFirstTrackTrackMix:), @"",
                delegate);
    addMenuItem(projectMenu, @"Recorded Pan Right", @selector(adjustFirstTrackTrackMix:), @"",
                delegate);
    addMenuItem(projectMenu, @"Toggle Recorded Mute", @selector(adjustFirstTrackTrackMix:), @"",
                delegate);
    addMenuItem(projectMenu, @"Toggle Recorded Solo", @selector(adjustFirstTrackTrackMix:), @"",
                delegate);
    addMenuItem(projectMenu, @"Imported Track Volume Up 3 dB", @selector(adjustFirstTrackTrackMix:),
                @"", delegate);
    addMenuItem(projectMenu, @"Imported Track Volume Down 3 dB",
                @selector(adjustFirstTrackTrackMix:), @"", delegate);
    addMenuItem(projectMenu, @"Imported Pan Left", @selector(adjustFirstTrackTrackMix:), @"",
                delegate);
    addMenuItem(projectMenu, @"Imported Pan Right", @selector(adjustFirstTrackTrackMix:), @"",
                delegate);
    addMenuItem(projectMenu, @"Toggle Imported Mute", @selector(adjustFirstTrackTrackMix:), @"",
                delegate);
    addMenuItem(projectMenu, @"Toggle Imported Solo", @selector(adjustFirstTrackTrackMix:), @"",
                delegate);
    addMenuItem(projectMenu, @"Close Project", @selector(closeDocument:), @"w", delegate);

    auto* editMenuItem = [[NSMenuItem alloc] initWithTitle:@"Edit" action:nil keyEquivalent:@""];
    [mainMenu addItem:editMenuItem];
    auto* editMenu = [[NSMenu alloc] initWithTitle:@"Edit"];
    editMenuItem.submenu = editMenu;
    addMenuItem(editMenu, @"Undo", @selector(undo:), @"z", delegate);
    addMenuItem(editMenu, @"Redo", @selector(redo:), @"Z", delegate);
    [editMenu addItem:[NSMenuItem separatorItem]];
    addMenuItem(editMenu, @"Cut", @selector(cut:), @"x");
    addMenuItem(editMenu, @"Copy", @selector(copy:), @"c");
    addMenuItem(editMenu, @"Paste", @selector(paste:), @"v");

    auto* transportMenuItem = [[NSMenuItem alloc] initWithTitle:@"Transport"
                                                         action:nil
                                                  keyEquivalent:@""];
    [mainMenu addItem:transportMenuItem];
    auto* transportMenu = [[NSMenu alloc] initWithTitle:@"Transport"];
    transportMenuItem.submenu = transportMenu;
    addMenuItem(transportMenu, @"Play", @selector(play:), @" ", delegate);
    addMenuItem(transportMenu, @"Stop", @selector(stop:), @".", delegate);

    NSArray<NSString*>* panelMenus = @[ @"View", @"Audio", @"MIDI", @"Tools", @"Help" ];
    for (NSString* title in panelMenus) {
        auto* menuItem = [[NSMenuItem alloc] initWithTitle:title action:nil keyEquivalent:@""];
        [mainMenu addItem:menuItem];
        auto* menu = [[NSMenu alloc] initWithTitle:title];
        menuItem.submenu = menu;
        if ([title isEqualToString:@"Tools"]) {
            addMenuItem(menu, @"Rescan Plugins", @selector(rescanPlugins:), @"", delegate);
        } else if ([title isEqualToString:@"Help"]) {
            addMenuItem(menu, @"LaMusica Help", @selector(showHelp:), @"?", delegate);
        } else if ([title isEqualToString:@"Audio"] || [title isEqualToString:@"MIDI"]) {
            addMenuItem(menu, [NSString stringWithFormat:@"%@ Preferences", title],
                        @selector(showPreferences:), @"", delegate);
        } else {
            addMenuItem(menu, @"Show Browser", @selector(showPrimaryPanel:), @"1", delegate);
            addMenuItem(menu, @"Show Timeline", @selector(showPrimaryPanel:), @"2", delegate);
            addMenuItem(menu, @"Show Mixer", @selector(showPrimaryPanel:), @"3", delegate);
            addMenuItem(menu, @"Show Inspector", @selector(showPrimaryPanel:), @"4", delegate);
        }
    }
}

NSWindow* buildMainWindow(LaMusicaAppDelegate* delegate) {
    const NSRect frame = NSMakeRect(100.0, 100.0, 1280.0, 800.0);
    auto* window = [[NSWindow alloc]
        initWithContentRect:frame
                  styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                            NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable
                    backing:NSBackingStoreBuffered
                      defer:NO];
    window.title = @"LaMusica";

    auto* root = [[NSStackView alloc] initWithFrame:NSZeroRect];
    root.orientation = NSUserInterfaceLayoutOrientationVertical;
    root.translatesAutoresizingMaskIntoConstraints = NO;
    root.spacing = 1.0;

    auto* transport = makePanel(@"Transport", NSColor.windowBackgroundColor);
    auto* center = [[NSStackView alloc] initWithFrame:NSZeroRect];
    center.orientation = NSUserInterfaceLayoutOrientationHorizontal;
    center.spacing = 1.0;
    auto* browser = makePanel(@"Browser", NSColor.controlBackgroundColor);
    auto* timeline = makePanel(@"Timeline / Piano Roll", NSColor.textBackgroundColor);
    auto* inspector = makePanel(@"Inspector", NSColor.controlBackgroundColor);
    auto* mixer = makePanel(@"Mixer", NSColor.windowBackgroundColor);
    delegate.transportPanel = transport;
    delegate.browserPanel = browser;
    delegate.timelinePanel = timeline;
    delegate.inspectorPanel = inspector;
    delegate.mixerPanel = mixer;

    [center addArrangedSubview:browser];
    [center addArrangedSubview:timeline];
    [center addArrangedSubview:inspector];
    [root addArrangedSubview:transport];
    [root addArrangedSubview:center];
    [root addArrangedSubview:mixer];

    [transport.heightAnchor constraintEqualToConstant:64.0].active = YES;
    [browser.widthAnchor constraintEqualToConstant:220.0].active = YES;
    [inspector.widthAnchor constraintEqualToConstant:260.0].active = YES;
    [mixer.heightAnchor constraintEqualToConstant:180.0].active = YES;

    auto* contentView = [[NSView alloc] initWithFrame:NSZeroRect];
    window.contentView = contentView;
    [contentView addSubview:root];
    [NSLayoutConstraint activateConstraints:@[
        [root.leadingAnchor constraintEqualToAnchor:contentView.leadingAnchor],
        [root.trailingAnchor constraintEqualToAnchor:contentView.trailingAnchor],
        [root.topAnchor constraintEqualToAnchor:contentView.topAnchor],
        [root.bottomAnchor constraintEqualToAnchor:contentView.bottomAnchor],
    ]];

    return window;
}

} // namespace

int main(int argc, char** argv) {
    const lamusica::session::Project project{"Untitled"};
    const lamusica::audio::AudioEngine engine{{}};

    if (hasArgument(argc, argv, "--smoke")) {
        std::cout << "LaMusica app smoke: " << project.name() << " @ " << engine.config().sampleRate
                  << " Hz\n";
        return 0;
    }
    if (hasArgument(argc, argv, "--app-session-first-track-smoke")) {
        return appSessionFirstTrackSmoke();
    }
    if (hasArgument(argc, argv, "--app-session-verify-first-track-project-smoke")) {
        return appSessionVerifyFirstTrackProjectSmoke();
    }
    if (hasArgument(argc, argv, "--app-session-render-first-track-smoke")) {
        return appSessionRenderFirstTrackSmoke();
    }
    if (hasArgument(argc, argv, "--app-session-preferences-first-track-smoke")) {
        return appSessionPreferencesFirstTrackSmoke();
    }
    if (hasArgument(argc, argv, "--app-session-transpose-first-track-smoke")) {
        return appSessionTransposeFirstTrackSmoke();
    }
    if (hasArgument(argc, argv, "--app-session-mix-first-track-smoke")) {
        return appSessionMixFirstTrackSmoke();
    }
    if (hasArgument(argc, argv, "--app-session-loop-first-track-smoke")) {
        return appSessionLoopFirstTrackSmoke();
    }
    if (hasArgument(argc, argv, "--app-session-transport-first-track-smoke")) {
        return appSessionTransportFirstTrackSmoke();
    }
    if (hasArgument(argc, argv, "--app-session-arrange-first-track-smoke")) {
        return appSessionArrangeFirstTrackSmoke();
    }
    if (hasArgument(argc, argv, "--app-session-stem-first-track-smoke")) {
        return appSessionStemFirstTrackSmoke();
    }
    if (hasArgument(argc, argv, "--app-session-package-first-track-smoke")) {
        return appSessionPackageFirstTrackSmoke();
    }
    if (hasArgument(argc, argv, "--app-session-record-first-track-smoke")) {
        return appSessionRecordFirstTrackSmoke();
    }
    if (hasArgument(argc, argv, "--app-session-record-package-first-track-smoke")) {
        return appSessionRecordPackageFirstTrackSmoke();
    }
    if (hasArgument(argc, argv, "--app-session-import-package-first-track-smoke")) {
        return appSessionImportPackageFirstTrackSmoke();
    }
    if (hasArgument(argc, argv, "--app-session-import-edit-first-track-smoke")) {
        return appSessionImportEditFirstTrackSmoke();
    }

    try {
        if (argc == 4 && std::string_view{argv[1]} == "--create-first-track") {
            return createFirstTrackProject(argv[2], argv[3]);
        }
        if (argc == 4 && std::string_view{argv[1]} == "--render-project") {
            return renderProject(argv[2], argv[3]);
        }
        if (argc == 3 && std::string_view{argv[1]} == "--inspect-project") {
            return inspectProject(argv[2]);
        }
    } catch (const std::exception& error) {
        std::cerr << "LaMusica app command failed: " << error.what() << '\n';
        return 2;
    }

    @
    autoreleasepool {
        [NSApplication sharedApplication];
        NSApp.activationPolicy = NSApplicationActivationPolicyRegular;
        auto* delegate = [[LaMusicaAppDelegate alloc] init];
        NSApp.delegate = delegate;
        buildMenuBar(delegate);
        auto* window = buildMainWindow(delegate);
        delegate.mainWindow = window;
        [delegate refreshMainWindowPanels];
        [window makeKeyAndOrderFront:nil];
        [NSApp activateIgnoringOtherApps:YES];
        [NSApp run];
    }

    return 0;
}
