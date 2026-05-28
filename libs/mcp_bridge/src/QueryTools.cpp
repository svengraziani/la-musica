#include "lamusica/mcp_bridge/QueryTools.hpp"

#include <algorithm>
#include <filesystem>
#include <sstream>

namespace lamusica::mcp_bridge {
namespace {

std::string escapeJson(std::string_view value) {
    std::string escaped;
    escaped.reserve(value.size());
    for (const char character : value) {
        if (character == '"' || character == '\\') {
            escaped.push_back('\\');
        }
        escaped.push_back(character);
    }
    return escaped;
}

template <typename Values, typename Writer>
void writePagedArray(std::ostringstream& output, const Values& values, QueryPage page,
                     Writer writer) {
    const auto begin = std::min(page.offset, values.size());
    const auto end = std::min(values.size(), begin + page.limit);
    output << "\"offset\":" << begin << ",\"limit\":" << page.limit
           << ",\"total\":" << values.size() << ",\"items\":[";
    for (std::size_t index = begin; index < end; ++index) {
        writer(output, values[index]);
        if (index + 1 < end) {
            output << ',';
        }
    }
    output << "]";
}

void writeHeader(std::ostringstream& output, std::string_view toolName) {
    output << "{\"schemaVersion\":1,\"tool\":\"" << toolName << "\",";
}

std::string pathString(const std::filesystem::path& path) {
    return path.generic_string();
}

std::string_view toString(session::AssetKind kind) noexcept {
    switch (kind) {
    case session::AssetKind::Audio:
        return "audio";
    case session::AssetKind::Midi:
        return "midi";
    case session::AssetKind::Preset:
        return "preset";
    case session::AssetKind::DrumKit:
        return "drum_kit";
    case session::AssetKind::Template:
        return "template";
    case session::AssetKind::Other:
        return "other";
    }

    return "other";
}

std::string_view toString(session::AutomationMode mode) noexcept {
    switch (mode) {
    case session::AutomationMode::Off:
        return "off";
    case session::AutomationMode::Read:
        return "read";
    case session::AutomationMode::Write:
        return "write";
    case session::AutomationMode::Touch:
        return "touch";
    case session::AutomationMode::Latch:
        return "latch";
    case session::AutomationMode::Trim:
        return "trim";
    }
    return "read";
}

std::string_view toString(session::AutomationCurve curve) noexcept {
    switch (curve) {
    case session::AutomationCurve::Step:
        return "step";
    case session::AutomationCurve::Linear:
        return "linear";
    }
    return "linear";
}

std::string_view toString(session::AutomationTargetKind targetKind) noexcept {
    switch (targetKind) {
    case session::AutomationTargetKind::Mixer:
        return "mixer";
    case session::AutomationTargetKind::Plugin:
        return "plugin";
    case session::AutomationTargetKind::Instrument:
        return "instrument";
    case session::AutomationTargetKind::Clip:
        return "clip";
    }
    return "mixer";
}

bool containsAutomationPointInRange(const session::AutomationLane& lane, QuerySampleRange range) {
    for (const auto& region : lane.regions) {
        for (const auto& point : region.points) {
            if (point.samplePosition >= range.startSample &&
                point.samplePosition < range.endSample) {
                return true;
            }
        }
    }
    return false;
}

} // namespace

std::string projectSummaryJson(const session::ProjectManifest& manifest) {
    std::ostringstream output;
    writeHeader(output, "project_summary");
    output << "\"name\":\"" << escapeJson(manifest.name)
           << "\",\"tracks\":" << manifest.tracks.size() << ",\"clips\":" << manifest.clips.size()
           << ",\"markers\":" << manifest.markers.size() << "}";
    return output.str();
}

std::string tracksJson(const session::ProjectManifest& manifest, QueryPage page) {
    std::ostringstream output;
    writeHeader(output, "tracks");
    writePagedArray(output, manifest.tracks, page,
                    [](std::ostringstream& item, const session::Track& track) {
                        item << "{\"id\":\"" << escapeJson(track.id) << "\",\"name\":\""
                             << escapeJson(track.name) << "\",\"type\":\""
                             << session::toString(track.type) << "\"}";
                    });
    output << "}";
    return output.str();
}

std::string clipsJson(const session::ProjectManifest& manifest, QueryPage page) {
    std::ostringstream output;
    writeHeader(output, "clips");
    writePagedArray(
        output, manifest.clips, page, [](std::ostringstream& item, const session::Clip& clip) {
            item << "{\"id\":\"" << escapeJson(clip.id) << "\",\"trackId\":\""
                 << escapeJson(clip.trackId) << "\",\"type\":\"" << session::toString(clip.type)
                 << "\",\"startSample\":" << clip.startSample
                 << ",\"lengthSamples\":" << clip.lengthSamples << "}";
        });
    output << "}";
    return output.str();
}

std::string clipsInRangeJson(const session::ProjectManifest& manifest, QuerySampleRange range,
                             QueryPage page) {
    std::vector<session::Clip> clips;
    std::ranges::copy_if(
        manifest.clips, std::back_inserter(clips), [range](const session::Clip& clip) {
            const auto clipEnd = clip.startSample + clip.lengthSamples;
            return clip.startSample < range.endSample && clipEnd > range.startSample;
        });
    std::ranges::sort(clips, {}, &session::Clip::startSample);

    std::ostringstream output;
    writeHeader(output, "clips_range");
    output << "\"range\":{\"startSample\":" << range.startSample
           << ",\"endSample\":" << range.endSample << "},";
    writePagedArray(output, clips, page, [](std::ostringstream& item, const session::Clip& clip) {
        item << "{\"id\":\"" << escapeJson(clip.id) << "\",\"trackId\":\""
             << escapeJson(clip.trackId) << "\",\"type\":\"" << session::toString(clip.type)
             << "\",\"startSample\":" << clip.startSample
             << ",\"lengthSamples\":" << clip.lengthSamples << "}";
    });
    output << "}";
    return output.str();
}

std::string selectionJson(const session::TimelineSelection& selection) {
    std::ostringstream output;
    writeHeader(output, "selection");
    output << "\"trackIds\":[";
    for (std::size_t index = 0; index < selection.trackIds.size(); ++index) {
        output << "\"" << escapeJson(selection.trackIds[index]) << "\"";
        if (index + 1 < selection.trackIds.size()) {
            output << ',';
        }
    }
    output << "],\"clipIds\":[";
    for (std::size_t index = 0; index < selection.clipIds.size(); ++index) {
        output << "\"" << escapeJson(selection.clipIds[index]) << "\"";
        if (index + 1 < selection.clipIds.size()) {
            output << ',';
        }
    }
    output << "],\"range\":";
    if (selection.range.has_value()) {
        output << "{\"startSample\":" << selection.range->startSample
               << ",\"endSample\":" << selection.range->endSample << "}";
    } else {
        output << "null";
    }
    output << "}";
    return output.str();
}

std::string transportJson(const audio::TransportState& transport) {
    std::ostringstream output;
    writeHeader(output, "transport");
    output << "\"playing\":" << (transport.playing ? "true" : "false")
           << ",\"recording\":" << (transport.recording ? "true" : "false")
           << ",\"loopEnabled\":" << (transport.loopEnabled ? "true" : "false")
           << ",\"samplePosition\":" << transport.samplePosition
           << ",\"loopStartSample\":" << transport.loopStartSample
           << ",\"loopEndSample\":" << transport.loopEndSample
           << ",\"tempoBpm\":" << transport.tempoBpm
           << ",\"timeSignature\":{\"numerator\":" << transport.timeSignature.numerator
           << ",\"denominator\":" << transport.timeSignature.denominator << "}}";
    return output.str();
}

std::string tempoJson(const session::ProjectManifest& manifest) {
    std::ostringstream output;
    writeHeader(output, "tempo");
    output << "\"items\":[";
    for (std::size_t index = 0; index < manifest.tempoMap.size(); ++index) {
        const auto& event = manifest.tempoMap[index];
        output << "{\"samplePosition\":" << event.samplePosition << ",\"bpm\":" << event.bpm << "}";
        if (index + 1 < manifest.tempoMap.size()) {
            output << ',';
        }
    }
    output << "]}";
    return output.str();
}

std::string markersJson(const session::ProjectManifest& manifest, QueryPage page) {
    std::ostringstream output;
    writeHeader(output, "markers");
    writePagedArray(output, manifest.markers, page,
                    [](std::ostringstream& item, const session::Marker& marker) {
                        item << "{\"id\":\"" << escapeJson(marker.id) << "\",\"name\":\""
                             << escapeJson(marker.name)
                             << "\",\"samplePosition\":" << marker.samplePosition << "}";
                    });
    output << "}";
    return output.str();
}

std::string routingJson(const session::ProjectManifest& manifest) {
    std::ostringstream output;
    writeHeader(output, "routing");
    output << "\"routes\":[";
    for (std::size_t index = 0; index < manifest.routing.size(); ++index) {
        const auto& route = manifest.routing[index];
        output << "{\"sourceTrackId\":\"" << escapeJson(route.sourceTrackId)
               << "\",\"destinationTrackId\":\"" << escapeJson(route.destinationTrackId) << "\"}";
        if (index + 1 < manifest.routing.size()) {
            output << ',';
        }
    }
    output << "]}";
    return output.str();
}

std::string routingJson(const session::MixerState& mixer) {
    std::ostringstream output;
    writeHeader(output, "routing");
    output << "\"channels\":" << mixer.channels.size() << ",\"routes\":[";
    for (std::size_t index = 0; index < mixer.routing.size(); ++index) {
        const auto& route = mixer.routing[index];
        output << "{\"source\":\"" << escapeJson(route.sourceChannelId) << "\",\"destination\":\""
               << escapeJson(route.destinationChannelId) << "\"}";
        if (index + 1 < mixer.routing.size()) {
            output << ',';
        }
    }
    output << "],\"sends\":[";
    bool firstSend = true;
    for (const auto& channel : mixer.channels) {
        for (const auto& send : channel.sends) {
            if (!firstSend) {
                output << ',';
            }
            output << "{\"source\":\"" << escapeJson(channel.id) << "\",\"id\":\""
                   << escapeJson(send.id) << "\",\"destination\":\""
                   << escapeJson(send.destinationChannelId) << "\",\"gainDb\":" << send.gainDb
                   << ",\"preFader\":" << (send.preFader ? "true" : "false") << "}";
            firstSend = false;
        }
    }
    output << "],\"sidechains\":[";
    for (std::size_t index = 0; index < mixer.sidechains.size(); ++index) {
        const auto& sidechain = mixer.sidechains[index];
        output << "{\"id\":\"" << escapeJson(sidechain.id) << "\",\"source\":\""
               << escapeJson(sidechain.sourceChannelId) << "\",\"destination\":\""
               << escapeJson(sidechain.destinationChannelId) << "\",\"targetInsertId\":\""
               << escapeJson(sidechain.targetInsertId) << "\"}";
        if (index + 1 < mixer.sidechains.size()) {
            output << ',';
        }
    }
    output << "]}";
    return output.str();
}

std::string pluginsJson(const session::ProjectManifest& manifest, QueryPage page) {
    std::ostringstream output;
    writeHeader(output, "plugins");
    writePagedArray(output, manifest.plugins, page,
                    [](std::ostringstream& item, const session::PluginReference& plugin) {
                        item << "{\"id\":\"" << escapeJson(plugin.id) << "\",\"trackId\":\""
                             << escapeJson(plugin.trackId) << "\",\"format\":\""
                             << escapeJson(plugin.format) << "\",\"identifier\":\""
                             << escapeJson(plugin.identifier) << "\"}";
                    });
    output << "}";
    return output.str();
}

std::string pluginsJson(const session::PluginScanCache& cache) {
    std::ostringstream output;
    writeHeader(output, "plugins");
    output << "\"items\":[";
    for (std::size_t index = 0; index < cache.results.size(); ++index) {
        const auto& result = cache.results[index];
        output << "{\"identifier\":\"" << escapeJson(result.description.identifier)
               << "\",\"name\":\"" << escapeJson(result.description.name) << "\",\"format\":\""
               << session::toString(result.description.format)
               << "\",\"valid\":" << (result.valid ? "true" : "false") << "}";
        if (index + 1 < cache.results.size()) {
            output << ',';
        }
    }
    output << "]}";
    return output.str();
}

std::string automationJson(const session::ProjectManifest& manifest, QueryPage page) {
    std::ostringstream output;
    writeHeader(output, "automation");
    writePagedArray(output, manifest.automation, page,
                    [](std::ostringstream& item, const session::AutomationLane& lane) {
                        item << "{\"id\":\"" << escapeJson(lane.id) << "\",\"targetKind\":\""
                             << toString(lane.targetKind) << "\",\"targetId\":\""
                             << escapeJson(lane.targetId) << "\",\"parameterId\":\""
                             << escapeJson(lane.parameterId) << "\"}";
                    });
    output << "}";
    return output.str();
}

std::string automationInRangeJson(const session::ProjectManifest& manifest, QuerySampleRange range,
                                  QueryPage page) {
    std::vector<session::AutomationLane> lanes;
    std::ranges::copy_if(manifest.automation, std::back_inserter(lanes),
                         [range](const session::AutomationLane& lane) {
                             return containsAutomationPointInRange(lane, range);
                         });

    std::ostringstream output;
    writeHeader(output, "automation_range");
    output << "\"range\":{\"startSample\":" << range.startSample
           << ",\"endSample\":" << range.endSample << "},";
    writePagedArray(output, lanes, page,
                    [range](std::ostringstream& item, const session::AutomationLane& lane) {
                        item << "{\"id\":\"" << escapeJson(lane.id) << "\",\"targetKind\":\""
                             << toString(lane.targetKind) << "\",\"targetId\":\""
                             << escapeJson(lane.targetId) << "\",\"parameterId\":\""
                             << escapeJson(lane.parameterId) << "\",\"mode\":\""
                             << toString(lane.mode) << "\",\"defaultValue\":" << lane.defaultValue
                             << ",\"points\":[";
                        bool wrotePoint = false;
                        for (const auto& region : lane.regions) {
                            for (const auto& point : region.points) {
                                if (point.samplePosition < range.startSample ||
                                    point.samplePosition >= range.endSample) {
                                    continue;
                                }
                                if (wrotePoint) {
                                    item << ',';
                                }
                                item << "{\"samplePosition\":" << point.samplePosition
                                     << ",\"value\":" << point.value << ",\"curveToNext\":\""
                                     << toString(point.curveToNext) << "\"}";
                                wrotePoint = true;
                            }
                        }
                        item << "]}";
                    });
    output << "}";
    return output.str();
}

std::string assetsJson(const session::ProjectManifest& manifest, QueryPage page) {
    std::ostringstream output;
    writeHeader(output, "assets");
    writePagedArray(output, manifest.assets, page,
                    [](std::ostringstream& item, const session::Asset& asset) {
                        item << "{\"id\":\"" << escapeJson(asset.id) << "\",\"relativePath\":\""
                             << escapeJson(pathString(asset.relativePath)) << "\",\"mediaType\":\""
                             << escapeJson(asset.mediaType) << "\"}";
                    });
    output << "}";
    return output.str();
}

std::string assetCatalogJson(const session::AssetCatalog& catalog, QueryPage page) {
    std::ostringstream output;
    writeHeader(output, "assets");
    output << "\"projectRoot\":\"" << escapeJson(pathString(catalog.projectRoot)) << "\",";
    writePagedArray(
        output, catalog.assets, page,
        [&catalog](std::ostringstream& item, const session::AssetRecord& asset) {
            const auto* analysis = session::findAnalysis(catalog, asset.id);
            const auto* waveform = session::findWaveform(catalog, asset.id);
            item << "{\"id\":\"" << escapeJson(asset.id) << "\",\"relativePath\":\""
                 << escapeJson(pathString(asset.relativePath)) << "\",\"kind\":\""
                 << toString(asset.kind)
                 << "\",\"favorite\":" << (asset.favorite ? "true" : "false")
                 << ",\"missing\":" << (asset.missing ? "true" : "false");
            if (analysis != nullptr) {
                item << ",\"analysis\":{\"durationSamples\":" << analysis->durationSamples
                     << ",\"channels\":" << analysis->channels
                     << ",\"sampleRate\":" << analysis->sampleRate
                     << ",\"peakAmplitude\":" << analysis->peakAmplitude
                     << ",\"rmsAmplitude\":" << analysis->rmsAmplitude
                     << ",\"loudnessLufs\":" << analysis->loudnessLufs
                     << ",\"tempoBpm\":" << analysis->tempoBpm << ",\"musicalKey\":\""
                     << escapeJson(analysis->musicalKey) << "\""
                     << ",\"transients\":" << analysis->transientSamples.size() << "}";
            } else {
                item << ",\"analysis\":null";
            }
            if (waveform != nullptr) {
                item << ",\"waveform\":{\"valid\":" << (waveform->valid ? "true" : "false")
                     << ",\"samplesPerBucket\":" << waveform->samplesPerBucket
                     << ",\"buckets\":" << waveform->buckets.size() << "}";
            } else {
                item << ",\"waveform\":null";
            }
            item << ",\"tags\":[";
            for (std::size_t index = 0; index < asset.tags.size(); ++index) {
                item << "\"" << escapeJson(asset.tags[index]) << "\"";
                if (index + 1 < asset.tags.size()) {
                    item << ',';
                }
            }
            item << "]}";
        });
    output << "}";
    return output.str();
}

std::string renderCapabilitiesJson() {
    return "{\"schemaVersion\":1,\"tool\":\"render_capabilities\",\"offlineBounce\":true,"
           "\"testTone\":true,\"wavPcm16\":true}";
}

} // namespace lamusica::mcp_bridge
