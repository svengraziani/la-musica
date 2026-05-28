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
#include <string>
#include <string_view>
#include <vector>

namespace {

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

int createFirstTrackProject(const char* projectPath, const char* name) {
    const auto document = lamusica::session::ProjectDocument::create(
        projectPath, lamusica::session::makeFirstTrackStarterManifest(name));
    std::cout << "LaMusica DAW created first-track project: " << document.project().name()
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
    std::cout << "LaMusica DAW rendered project: " << document.project().name()
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
    std::cout << "LaMusica DAW project readiness: " << document.project().name()
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
        std::filesystem::temp_directory_path() / "lamusica-daw-session-smoke.Project.lamusica";
    std::filesystem::remove_all(path);
    lamusica::session::ApplicationSession session;
    session.createFirstTrackProject(path, "Session Smoke");
    const auto status = session.status();
    std::cout << "LaMusica DAW app-session first-track smoke: project=" << status.projectName
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
        std::filesystem::temp_directory_path() / "lamusica-daw-session-unready.Project.lamusica";
    const auto readyPath =
        std::filesystem::temp_directory_path() / "lamusica-daw-session-ready.Project.lamusica";
    std::filesystem::remove_all(unreadyPath);
    std::filesystem::remove_all(readyPath);

    lamusica::session::ApplicationSession session;
    session.createProject(unreadyPath, "Session Unready");
    const auto unready = session.verifyFirstTrackProject();
    session.createFirstTrackProject(readyPath, "Session Ready");
    const auto ready = session.verifyFirstTrackProject();

    std::cout << "LaMusica DAW app-session verify first-track project: unreadyFirstTrackReady="
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

int appSessionTransposeFirstTrackSmoke() {
    const auto path =
        std::filesystem::temp_directory_path() / "lamusica-daw-session-transpose.Project.lamusica";
    std::filesystem::remove_all(path);
    lamusica::session::ApplicationSession session;
    session.createFirstTrackProject(path, "Session Transpose");
    session.transposeFirstTrackBass(12);
    session.openProject(path);
    const auto status = session.status();
    std::cout << "LaMusica DAW app-session transpose: project=" << status.projectName
              << " firstTrackReady=" << (status.firstTrackReady ? "true" : "false")
              << " starterMidiNotes=" << status.starterMidiNoteCount
              << " bassTranspose=" << status.starterBassTransposeSemitones
              << " renderFrames=" << status.renderFrames << '\n';
    std::filesystem::remove_all(path);
    return status.firstTrackReady && status.starterBassTransposeSemitones == 12 ? 0 : 5;
}

int appSessionLoopFirstTrackSmoke() {
    const auto projectPath =
        std::filesystem::temp_directory_path() / "lamusica-daw-session-loop.Project.lamusica";
    const auto outputPath =
        std::filesystem::temp_directory_path() / "lamusica-daw-session-loop.wav";
    std::filesystem::remove_all(projectPath);
    std::filesystem::remove(outputPath);
    lamusica::session::ApplicationSession session;
    session.createFirstTrackProject(projectPath, "Session Loop");
    session.clearLoopRegion();
    session.setFirstTrackLoopToIntro();
    session.openProject(projectPath);
    const auto result = session.exportCurrentLoop(outputPath);
    const auto status = session.status();
    std::cout << "LaMusica DAW app-session loop: project=" << status.projectName
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
        std::filesystem::temp_directory_path() / "lamusica-daw-session-transport.Project.lamusica";
    std::filesystem::remove_all(projectPath);
    lamusica::session::ApplicationSession session;
    session.createFirstTrackProject(projectPath, "Session Transport");
    session.seek(95936);
    session.play();
    const auto rendered = session.auditionCurrentMixBlock(128);
    session.stop();
    const auto status = session.status();
    const auto peak = lamusica::audio::peakAbsoluteSample(rendered);
    std::cout << "LaMusica DAW app-session transport: project=" << status.projectName
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
        std::filesystem::temp_directory_path() / "lamusica-daw-session-arrange.Project.lamusica";
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
    std::cout << "LaMusica DAW app-session arrange: project=" << status.projectName
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
        std::filesystem::temp_directory_path() / "lamusica-daw-session-stems.Project.lamusica";
    const auto outputDirectory = std::filesystem::temp_directory_path() / "lamusica-daw-stems";
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
    std::cout << "LaMusica DAW app-session stems: project=" << status.projectName
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
        std::filesystem::temp_directory_path() / "lamusica-daw-session-package.Project.lamusica";
    const auto outputDirectory = std::filesystem::temp_directory_path() / "lamusica-daw-package";
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
    std::cout << "LaMusica DAW app-session package: project=" << status.projectName
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
        std::filesystem::temp_directory_path() / "lamusica-daw-session-record.Project.lamusica";
    std::filesystem::remove_all(projectPath);
    lamusica::session::ApplicationSession session;
    session.createFirstTrackProject(projectPath, "Session Record");
    session.extendFirstTrackArrangementToVerse();
    const auto result = session.recordFirstTrackTake(48000);
    session.openProject(projectPath);
    const auto status = session.status();
    const bool fileExists = std::filesystem::exists(result.committed.path);
    std::cout << "LaMusica DAW app-session record: project=" << status.projectName
              << " firstTrackReady=" << (status.firstTrackReady ? "true" : "false")
              << " recordedTakeCount=" << status.recordedTakeCount
              << " tracks=" << status.trackCount << " clips=" << status.clipCount
              << " recordedFrames=" << result.committed.frames
              << " recordedChannels=" << result.committed.channels
              << " startSample=" << result.timelineStartSample
              << " countInSamples=" << result.countInSamples
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
                             "lamusica-daw-session-record-package.Project.lamusica";
    const auto outputDirectory =
        std::filesystem::temp_directory_path() / "lamusica-daw-record-package";
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
    std::cout << "LaMusica DAW app-session record package: project=" << status.projectName
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
                             "lamusica-daw-session-import-package.Project.lamusica";
    const auto outputDirectory =
        std::filesystem::temp_directory_path() / "lamusica-daw-import-package";
    const auto importSource =
        std::filesystem::temp_directory_path() / "lamusica-daw-import-source.wav";
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
    std::cout << "LaMusica DAW app-session import package: project=" << status.projectName
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
                             "lamusica-daw-session-import-edit.Project.lamusica";
    const auto importSource =
        std::filesystem::temp_directory_path() / "lamusica-daw-import-edit-source.wav";
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
    std::cout << "LaMusica DAW app-session import edit: project=" << status.projectName
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
        std::filesystem::temp_directory_path() / "lamusica-daw-session-mix.Project.lamusica";
    const auto outputPath = std::filesystem::temp_directory_path() / "lamusica-daw-session-mix.wav";
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
    std::cout << "LaMusica DAW app-session mix: project=" << status.projectName
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
        std::filesystem::temp_directory_path() / "lamusica-daw-session-render.Project.lamusica";
    const auto outputPath =
        std::filesystem::temp_directory_path() / "lamusica-daw-session-render.wav";
    std::filesystem::remove_all(projectPath);
    std::filesystem::remove(outputPath);
    lamusica::session::ApplicationSession session;
    session.createFirstTrackProject(projectPath, "Session Render");
    const auto result = session.exportCurrentMix(outputPath);
    const auto status = session.status();
    std::cout << "LaMusica DAW app-session render: project=" << status.projectName
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
    std::cout << "LaMusica DAW app-session preferences: audioDevice=" << stored.audioDeviceId
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

} // namespace

int main(int argc, char** argv) {
    const lamusica::session::Project project{"Untitled"};
    const lamusica::audio::AudioEngine engine{{}};

    if (hasArgument(argc, argv, "--first-track-smoke")) {
        const auto manifest = lamusica::session::makeFirstTrackStarterManifest("First Track");
        std::cout << "LaMusica DAW first-track smoke: tracks=" << manifest.tracks.size()
                  << " clips=" << manifest.clips.size() << " ready="
                  << (lamusica::session::isFirstTrackStarterManifest(manifest) ? "true" : "false")
                  << '\n';
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
        std::cerr << "LaMusica DAW command failed: " << error.what() << '\n';
        return 2;
    }

    std::cout << "LaMusica DAW bootstrap: " << project.name() << " @ " << engine.config().sampleRate
              << " Hz\n";
    return 0;
}
