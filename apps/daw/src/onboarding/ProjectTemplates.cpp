#include "onboarding/ProjectTemplates.hpp"

#include <array>
#include <stdexcept>

namespace lamusica::daw::onboarding {
namespace {

using session::ApplicationSession;
using session::AutomationCurve;
using session::AutomationLane;
using session::AutomationMode;
using session::AutomationRegion;
using session::AutomationTargetKind;
using session::Clip;
using session::ClipType;
using session::Marker;
using session::MidiClipReference;
using session::PluginReference;
using session::RoutingConnection;
using session::Track;
using session::TrackType;

const std::array<ProjectTemplate, 4> templates{{
    {.id = "empty",
     .nameKey = "onboarding.template.empty.name",
     .descriptionKey = "onboarding.template.empty.description",
     .iconName = "file",
     .order = 0},
    {.id = "basic-multitrack",
     .nameKey = "onboarding.template.basicMultitrack.name",
     .descriptionKey = "onboarding.template.basicMultitrack.description",
     .iconName = "tracks",
     .order = 10},
    {.id = "drum-synth",
     .nameKey = "onboarding.template.drumSynth.name",
     .descriptionKey = "onboarding.template.drumSynth.description",
     .iconName = "drum",
     .order = 20},
    {.id = "podcast-voice",
     .nameKey = "onboarding.template.podcastVoice.name",
     .descriptionKey = "onboarding.template.podcastVoice.description",
     .iconName = "mic",
     .order = 30},
}};

void addMaster(ApplicationSession& session) {
    session.addTrack({.id = "master", .name = "Master", .type = TrackType::Master});
}

void createEmpty(ApplicationSession& session, std::filesystem::path path, std::string name) {
    session.createProject(std::move(path), std::move(name));
    addMaster(session);
    session.saveProject();
}

void createBasicMultitrack(ApplicationSession& session, std::filesystem::path path,
                           std::string name) {
    session.createProject(std::move(path), std::move(name));
    session.setTempo(120.0);
    session.setTimeSignature(4, 4);
    session.addTrack({.id = "audio-1", .name = "Audio 1", .type = TrackType::Audio});
    session.addTrack({.id = "audio-2", .name = "Audio 2", .type = TrackType::Audio});
    session.addTrack({.id = "audio-3", .name = "Audio 3", .type = TrackType::Audio});
    addMaster(session);
    session.createAudioClip({.id = "audio-1-clip",
                             .trackId = "audio-1",
                             .type = ClipType::Audio,
                             .startSample = 0,
                             .lengthSamples = 96000});
    session.createAudioClip({.id = "audio-2-clip",
                             .trackId = "audio-2",
                             .type = ClipType::Audio,
                             .startSample = 48000,
                             .lengthSamples = 96000});
    for (const auto* trackId : {"audio-1", "audio-2", "audio-3"}) {
        session.addRoutingConnection(
            {.sourceTrackId = trackId, .destinationTrackId = "master"});
    }
    session.saveProject();
}

void createDrumSynth(ApplicationSession& session, std::filesystem::path path, std::string name) {
    session.createProject(std::move(path), std::move(name));
    session.setTempo(128.0);
    session.addTrack({.id = "drums", .name = "Drums", .type = TrackType::Instrument});
    session.addTrack({.id = "synth", .name = "Synth", .type = TrackType::Instrument});
    addMaster(session);
    session.createMidiClip({.id = "drums-midi",
                            .trackId = "drums",
                            .type = ClipType::Midi,
                            .startSample = 0,
                            .lengthSamples = 96000},
                           {.clipId = "drums-midi", .dataId = "template-drums-midi"});
    session.createMidiClip({.id = "synth-midi",
                            .trackId = "synth",
                            .type = ClipType::Midi,
                            .startSample = 0,
                            .lengthSamples = 96000},
                           {.clipId = "synth-midi", .dataId = "template-synth-midi"});
    session.addPlugin({.id = "synth-instrument",
                       .trackId = "synth",
                       .format = "builtin",
                       .identifier = "lamusica.builtin.synth"});
    session.addAutomationLane({.id = "synth-filter",
                               .targetKind = AutomationTargetKind::Plugin,
                               .targetId = "synth-instrument",
                               .parameterId = "filterCutoff",
                               .mode = AutomationMode::Read,
                               .defaultValue = 0.5F,
                               .regions = {{.startSample = 0,
                                            .endSample = 96000,
                                            .points = {{.samplePosition = 0,
                                                        .value = 0.35F,
                                                        .curveToNext = AutomationCurve::Linear},
                                                       {.samplePosition = 96000,
                                                        .value = 0.8F,
                                                        .curveToNext =
                                                            AutomationCurve::Linear}}}}});
    session.addRoutingConnection({.sourceTrackId = "drums", .destinationTrackId = "master"});
    session.addRoutingConnection({.sourceTrackId = "synth", .destinationTrackId = "master"});
    session.saveProject();
}

void createPodcastVoice(ApplicationSession& session, std::filesystem::path path,
                        std::string name) {
    session.createProject(std::move(path), std::move(name));
    session.addTrack({.id = "host", .name = "Host", .type = TrackType::Audio});
    session.addTrack({.id = "guest", .name = "Guest", .type = TrackType::Audio});
    session.addTrack({.id = "voice-bus", .name = "Voice Bus", .type = TrackType::Group});
    addMaster(session);
    session.createAudioClip({.id = "host-placeholder",
                             .trackId = "host",
                             .type = ClipType::Audio,
                             .startSample = 0,
                             .lengthSamples = 96000});
    session.createAudioClip({.id = "guest-placeholder",
                             .trackId = "guest",
                             .type = ClipType::Audio,
                             .startSample = 96000,
                             .lengthSamples = 96000});
    session.addRoutingConnection({.sourceTrackId = "host", .destinationTrackId = "voice-bus"});
    session.addRoutingConnection({.sourceTrackId = "guest", .destinationTrackId = "voice-bus"});
    session.addRoutingConnection({.sourceTrackId = "voice-bus", .destinationTrackId = "master"});
    session.addMarker({.id = "intro", .name = "Intro", .samplePosition = 0});
    session.addMarker({.id = "segment-1", .name = "Segment 1", .samplePosition = 48000});
    session.addMarker({.id = "outro", .name = "Outro", .samplePosition = 192000});
    session.saveProject();
}

} // namespace

std::span<const ProjectTemplate> projectTemplates() noexcept {
    return templates;
}

const ProjectTemplate* findProjectTemplate(std::string_view id) noexcept {
    for (const auto& projectTemplate : templates) {
        if (projectTemplate.id == id) {
            return &projectTemplate;
        }
    }
    return nullptr;
}

void createProjectFromTemplate(ApplicationSession& session, std::string_view templateId,
                               std::filesystem::path path, std::string name) {
    if (templateId == "empty") {
        createEmpty(session, std::move(path), std::move(name));
        return;
    }
    if (templateId == "basic-multitrack") {
        createBasicMultitrack(session, std::move(path), std::move(name));
        return;
    }
    if (templateId == "drum-synth") {
        createDrumSynth(session, std::move(path), std::move(name));
        return;
    }
    if (templateId == "podcast-voice") {
        createPodcastVoice(session, std::move(path), std::move(name));
        return;
    }
    throw std::runtime_error("Unknown project template: " + std::string{templateId});
}

} // namespace lamusica::daw::onboarding
