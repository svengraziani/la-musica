#include "lamusica/session/DrumMachine.hpp"

#include "lamusica/session/GraphCompiler.hpp"
#include "lamusica/session/Warp.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <map>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string_view>

namespace lamusica::session {
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

bool anySolo(const DrumMachinePreset& preset) {
    return std::ranges::any_of(preset.pads, [](const DrumPad& pad) { return pad.solo; });
}

const audio::RenderedAudio* findSampleAsset(const std::vector<DrumSampleAsset>& samples,
                                            std::string_view assetId) noexcept {
    const auto found = std::ranges::find_if(
        samples, [assetId](const DrumSampleAsset& sample) { return sample.assetId == assetId; });
    return found == samples.end() ? nullptr : &found->audio;
}

std::size_t findValueStart(std::string_view json, std::string_view key) {
    const auto keyToken = "\"" + std::string{key} + "\"";
    const auto keyPosition = json.find(keyToken);
    if (keyPosition == std::string_view::npos) {
        throw std::runtime_error("Missing drum preset key: " + std::string{key});
    }
    const auto colonPosition = json.find(':', keyPosition + keyToken.size());
    if (colonPosition == std::string_view::npos) {
        throw std::runtime_error("Missing drum preset value separator: " + std::string{key});
    }
    return json.find_first_not_of(" \n\r\t", colonPosition + 1);
}

std::optional<std::size_t> findOptionalValueStart(std::string_view json, std::string_view key) {
    const auto keyToken = "\"" + std::string{key} + "\"";
    const auto keyPosition = json.find(keyToken);
    if (keyPosition == std::string_view::npos) {
        return std::nullopt;
    }
    const auto colonPosition = json.find(':', keyPosition + keyToken.size());
    if (colonPosition == std::string_view::npos) {
        throw std::runtime_error("Missing drum preset value separator: " + std::string{key});
    }
    return json.find_first_not_of(" \n\r\t", colonPosition + 1);
}

std::string readJsonString(std::string_view json, std::size_t quotePosition) {
    if (quotePosition >= json.size() || json[quotePosition] != '"') {
        throw std::runtime_error("Expected drum preset string");
    }

    std::string value;
    bool escaped = false;
    for (std::size_t index = quotePosition + 1; index < json.size(); ++index) {
        const char character = json[index];
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
    throw std::runtime_error("Unterminated drum preset string");
}

std::string readRequiredString(std::string_view json, std::string_view key) {
    return readJsonString(json, findValueStart(json, key));
}

std::string readOptionalString(std::string_view json, std::string_view key,
                               std::string fallback = {}) {
    const auto start = findOptionalValueStart(json, key);
    return start.has_value() ? readJsonString(json, *start) : std::move(fallback);
}

template <typename Value> Value readNumberToken(std::string_view json, std::string_view key) {
    const auto start = findValueStart(json, key);
    const auto end = json.find_first_not_of("-+0123456789.eE", start);
    const auto token = json.substr(start, end - start);
    Value value{};
    const auto result = std::from_chars(token.data(), token.data() + token.size(), value);
    if (result.ec != std::errc{}) {
        throw std::runtime_error("Expected drum preset number: " + std::string{key});
    }
    return value;
}

template <typename Value>
Value readOptionalNumberToken(std::string_view json, std::string_view key, Value fallback) {
    const auto start = findOptionalValueStart(json, key);
    if (!start.has_value()) {
        return fallback;
    }
    const auto end = json.find_first_not_of("-+0123456789.eE", *start);
    const auto token = json.substr(*start, end - *start);
    Value value{};
    const auto result = std::from_chars(token.data(), token.data() + token.size(), value);
    if (result.ec != std::errc{}) {
        throw std::runtime_error("Expected drum preset number: " + std::string{key});
    }
    return value;
}

bool readOptionalBool(std::string_view json, std::string_view key, bool fallback) {
    const auto start = findOptionalValueStart(json, key);
    if (!start.has_value()) {
        return fallback;
    }
    if (json.substr(*start, 4) == "true") {
        return true;
    }
    if (json.substr(*start, 5) == "false") {
        return false;
    }
    throw std::runtime_error("Expected drum preset boolean: " + std::string{key});
}

std::optional<std::uint8_t> readOptionalUint8OrNull(std::string_view json, std::string_view key) {
    const auto start = findOptionalValueStart(json, key);
    if (!start.has_value() || json.substr(*start, 4) == "null") {
        return std::nullopt;
    }
    const auto value = readOptionalNumberToken<int>(json, key, 0);
    if (value < 0 || value > 255) {
        throw std::runtime_error("Drum preset uint8 value out of range: " + std::string{key});
    }
    return static_cast<std::uint8_t>(value);
}

std::vector<std::string_view> objectArrayItems(std::string_view json, std::string_view key) {
    const auto start = findValueStart(json, key);
    if (start >= json.size() || json[start] != '[') {
        throw std::runtime_error("Expected drum preset array: " + std::string{key});
    }

    std::vector<std::string_view> items;
    int depth = 0;
    std::size_t objectStart = std::string_view::npos;
    bool inString = false;
    bool escaped = false;
    for (std::size_t index = start + 1; index < json.size(); ++index) {
        const char character = json[index];
        if (inString) {
            if (escaped) {
                escaped = false;
            } else if (character == '\\') {
                escaped = true;
            } else if (character == '"') {
                inString = false;
            }
            continue;
        }
        if (character == '"') {
            inString = true;
            continue;
        }
        if (character == '{') {
            if (depth == 0) {
                objectStart = index;
            }
            ++depth;
        } else if (character == '}') {
            --depth;
            if (depth == 0 && objectStart != std::string_view::npos) {
                items.push_back(json.substr(objectStart, index - objectStart + 1));
                objectStart = std::string_view::npos;
            }
        } else if (character == ']' && depth == 0) {
            return items;
        }
    }

    throw std::runtime_error("Unterminated drum preset array: " + std::string{key});
}

void validatePad(const DrumPad& pad) {
    if (pad.id.empty() || pad.name.empty()) {
        throw std::runtime_error("Drum pad id and name are required");
    }
    if (pad.midiNote > 127) {
        throw std::runtime_error("Drum pad MIDI note must be in the range 0-127");
    }
    if (pad.sampleStart < 0 || pad.sampleEnd < 0 || pad.attackSamples < 0 ||
        pad.releaseSamples < 0) {
        throw std::runtime_error("Drum pad sample and envelope metadata must not be negative");
    }
    if (pad.sampleEnd != 0 && pad.sampleEnd <= pad.sampleStart) {
        throw std::runtime_error("Drum pad sample end must follow sample start");
    }
    if (pad.lowPassCoefficient < 0.0F || pad.lowPassCoefficient > 1.0F) {
        throw std::runtime_error("Drum pad low-pass coefficient must be in [0, 1]");
    }
    for (const auto& layer : pad.velocityLayers) {
        if (layer.minVelocity < 1 || layer.maxVelocity > 127 ||
            layer.maxVelocity < layer.minVelocity || layer.assetId.empty()) {
            throw std::runtime_error("Drum velocity layer is invalid");
        }
    }
}

float envelopeGain(const DrumPad& pad, std::int64_t frame, std::int64_t frames) noexcept {
    float gain = 1.0F;
    if (pad.attackSamples > 0 && frame < pad.attackSamples) {
        gain *= static_cast<float>(frame) / static_cast<float>(pad.attackSamples);
    }
    if (pad.releaseSamples > 0) {
        const auto releaseStart = frames - pad.releaseSamples;
        if (frame >= releaseStart) {
            const auto remaining = frames - frame - 1;
            gain *= std::max(0.0F, static_cast<float>(remaining) /
                                       static_cast<float>(pad.releaseSamples));
        }
    }
    return gain;
}

} // namespace

const DrumPad* findPadForMidiNote(const DrumMachinePreset& preset, std::uint8_t midiNote) {
    const auto found = std::ranges::find_if(
        preset.pads, [midiNote](const DrumPad& pad) { return pad.midiNote == midiNote; });
    return found == preset.pads.end() ? nullptr : &*found;
}

DrumPad* findPadById(DrumMachinePreset& preset, std::string_view padId) noexcept {
    const auto found =
        std::ranges::find_if(preset.pads, [padId](const DrumPad& pad) { return pad.id == padId; });
    return found == preset.pads.end() ? nullptr : &*found;
}

const DrumPad* findPadById(const DrumMachinePreset& preset, std::string_view padId) noexcept {
    const auto found =
        std::ranges::find_if(preset.pads, [padId](const DrumPad& pad) { return pad.id == padId; });
    return found == preset.pads.end() ? nullptr : &*found;
}

std::string selectLayerAsset(const DrumPad& pad, std::uint8_t velocity) {
    const auto found =
        std::ranges::find_if(pad.velocityLayers, [velocity](const VelocityLayer& layer) {
            return velocity >= layer.minVelocity && velocity <= layer.maxVelocity;
        });
    return found == pad.velocityLayers.end() ? std::string{} : found->assetId;
}

void assignAssetToPad(DrumMachinePreset& preset, std::string_view padId, VelocityLayer layer) {
    if (layer.minVelocity < 1 || layer.maxVelocity > 127 || layer.maxVelocity < layer.minVelocity ||
        layer.assetId.empty()) {
        throw std::runtime_error("Drum pad asset assignment requires a valid velocity layer");
    }

    auto* pad = findPadById(preset, padId);
    if (pad == nullptr) {
        throw std::runtime_error("Drum pad asset assignment target was not found");
    }

    const auto found =
        std::ranges::find_if(pad->velocityLayers, [&layer](const VelocityLayer& existing) {
            return existing.minVelocity == layer.minVelocity &&
                   existing.maxVelocity == layer.maxVelocity;
        });
    if (found == pad->velocityLayers.end()) {
        pad->velocityLayers.push_back(std::move(layer));
    } else {
        *found = std::move(layer);
    }
    validatePad(*pad);
}

std::vector<DrumTrigger>
renderDrumTriggers(const DrumMachinePreset& preset,
                   const std::vector<std::pair<std::int64_t, std::uint8_t>>& events) {
    std::vector<DrumPadEvent> padEvents;
    padEvents.reserve(events.size());
    for (const auto& [samplePosition, midiNote] : events) {
        padEvents.push_back(
            {.samplePosition = samplePosition, .midiNote = midiNote, .velocity = 100});
    }
    return renderDrumTriggers(preset, padEvents);
}

std::vector<DrumTrigger> renderDrumTriggers(const DrumMachinePreset& preset,
                                            const std::vector<DrumPadEvent>& events) {
    std::vector<DrumTrigger> triggers;
    std::map<std::uint8_t, std::size_t> latestByChokeGroup;
    const bool soloMode = anySolo(preset);

    for (const auto& event : events) {
        const auto* pad = findPadForMidiNote(preset, event.midiNote);
        if (pad == nullptr || pad->muted || (soloMode && !pad->solo)) {
            continue;
        }

        const auto velocity = std::clamp<std::uint8_t>(event.velocity, 1, 127);
        const auto assetId = selectLayerAsset(*pad, velocity);
        if (assetId.empty()) {
            continue;
        }

        bool chokedPrevious = false;
        if (pad->chokeGroup.has_value()) {
            const auto latest = latestByChokeGroup.find(*pad->chokeGroup);
            if (latest != latestByChokeGroup.end()) {
                triggers[latest->second].chokedPrevious = true;
                chokedPrevious = true;
            }
        }

        triggers.push_back({.padId = pad->id,
                            .assetId = assetId,
                            .outputRoute = pad->outputRoute,
                            .samplePosition = event.samplePosition,
                            .velocity = velocity,
                            .chokedPrevious = chokedPrevious});

        if (pad->chokeGroup.has_value()) {
            latestByChokeGroup[*pad->chokeGroup] = triggers.size() - 1;
        }
    }

    return triggers;
}

std::string serializeDrumMachinePreset(const DrumMachinePreset& preset) {
    if (preset.id.empty() || preset.name.empty() || preset.license.empty()) {
        throw std::runtime_error("Drum machine preset id and name are required");
    }
    for (const auto& pad : preset.pads) {
        validatePad(pad);
    }

    std::ostringstream output;
    output << "{\n";
    output << "  \"id\": \"" << escapeJson(preset.id) << "\",\n";
    output << "  \"name\": \"" << escapeJson(preset.name) << "\",\n";
    output << "  \"license\": \"" << escapeJson(preset.license) << "\",\n";
    output << "  \"licenseUrl\": \"" << escapeJson(preset.licenseUrl) << "\",\n";
    output << "  \"bundledAssetsIncluded\": " << (preset.bundledAssetsIncluded ? "true" : "false")
           << ",\n";
    output << "  \"pads\": [";
    if (!preset.pads.empty()) {
        output << '\n';
        for (std::size_t index = 0; index < preset.pads.size(); ++index) {
            const auto& pad = preset.pads[index];
            output << "    {\"id\": \"" << escapeJson(pad.id) << "\", \"name\": \""
                   << escapeJson(pad.name) << "\", \"midiNote\": " << static_cast<int>(pad.midiNote)
                   << ", \"color\": \"" << escapeJson(pad.color) << "\", \"chokeGroup\": ";
            if (pad.chokeGroup.has_value()) {
                output << static_cast<int>(*pad.chokeGroup);
            } else {
                output << "null";
            }
            output << ", \"muted\": " << (pad.muted ? "true" : "false")
                   << ", \"solo\": " << (pad.solo ? "true" : "false") << ", \"outputRoute\": \""
                   << escapeJson(pad.outputRoute) << "\", \"gainDb\": " << pad.gainDb
                   << ", \"pitchSemitones\": " << pad.pitchSemitones
                   << ", \"reversed\": " << (pad.reversed ? "true" : "false")
                   << ", \"sampleStart\": " << pad.sampleStart
                   << ", \"sampleEnd\": " << pad.sampleEnd
                   << ", \"attackSamples\": " << pad.attackSamples
                   << ", \"releaseSamples\": " << pad.releaseSamples
                   << ", \"lowPassCoefficient\": " << pad.lowPassCoefficient
                   << ", \"velocityLayers\": [";
            for (std::size_t layerIndex = 0; layerIndex < pad.velocityLayers.size(); ++layerIndex) {
                const auto& layer = pad.velocityLayers[layerIndex];
                output << "{\"minVelocity\": " << static_cast<int>(layer.minVelocity)
                       << ", \"maxVelocity\": " << static_cast<int>(layer.maxVelocity)
                       << ", \"assetId\": \"" << escapeJson(layer.assetId) << "\"}";
                if (layerIndex + 1 < pad.velocityLayers.size()) {
                    output << ", ";
                }
            }
            output << "]}";
            output << (index + 1 == preset.pads.size() ? "\n" : ",\n");
        }
        output << "  ";
    }
    output << "]\n}\n";
    return output.str();
}

DrumMachinePreset parseDrumMachinePreset(std::string_view json) {
    DrumMachinePreset preset{.id = readRequiredString(json, "id"),
                             .name = readRequiredString(json, "name"),
                             .license = readOptionalString(json, "license", "unknown"),
                             .licenseUrl = readOptionalString(json, "licenseUrl"),
                             .bundledAssetsIncluded =
                                 readOptionalBool(json, "bundledAssetsIncluded", false)};

    for (const auto padJson : objectArrayItems(json, "pads")) {
        DrumPad pad{
            .id = readRequiredString(padJson, "id"),
            .name = readRequiredString(padJson, "name"),
            .midiNote = static_cast<std::uint8_t>(readNumberToken<int>(padJson, "midiNote")),
            .color = readOptionalString(padJson, "color", "#808080"),
            .chokeGroup = readOptionalUint8OrNull(padJson, "chokeGroup"),
            .muted = readOptionalBool(padJson, "muted", false),
            .solo = readOptionalBool(padJson, "solo", false),
            .outputRoute = readOptionalString(padJson, "outputRoute", "master"),
            .gainDb = readOptionalNumberToken<float>(padJson, "gainDb", 0.0F),
            .pitchSemitones = readOptionalNumberToken<float>(padJson, "pitchSemitones", 0.0F),
            .reversed = readOptionalBool(padJson, "reversed", false),
            .sampleStart = readOptionalNumberToken<std::int64_t>(padJson, "sampleStart", 0),
            .sampleEnd = readOptionalNumberToken<std::int64_t>(padJson, "sampleEnd", 0),
            .attackSamples = readOptionalNumberToken<std::int64_t>(padJson, "attackSamples", 0),
            .releaseSamples = readOptionalNumberToken<std::int64_t>(padJson, "releaseSamples", 0),
            .lowPassCoefficient =
                readOptionalNumberToken<float>(padJson, "lowPassCoefficient", 1.0F)};
        for (const auto layerJson : objectArrayItems(padJson, "velocityLayers")) {
            pad.velocityLayers.push_back({.minVelocity = static_cast<std::uint8_t>(
                                              readNumberToken<int>(layerJson, "minVelocity")),
                                          .maxVelocity = static_cast<std::uint8_t>(
                                              readNumberToken<int>(layerJson, "maxVelocity")),
                                          .assetId = readRequiredString(layerJson, "assetId")});
        }
        validatePad(pad);
        preset.pads.push_back(std::move(pad));
    }

    if (preset.id.empty() || preset.name.empty() || preset.license.empty()) {
        throw std::runtime_error("Drum machine preset id, name, and license are required");
    }
    return preset;
}

bool hasClearDrumPresetRedistributionRights(const DrumMachinePreset& preset) {
    if (preset.license.empty() || preset.license == "unknown") {
        return false;
    }
    if (preset.bundledAssetsIncluded && preset.license == "placeholder-assets-not-included") {
        return false;
    }
    return true;
}

audio::RenderedAudio renderDrumPadSample(const DrumPad& pad, const audio::RenderedAudio& source,
                                         std::uint8_t velocity) {
    if (source.channels == 0U) {
        throw std::runtime_error("Drum pad source must have at least one channel");
    }
    if (pad.sampleStart < 0 || pad.sampleEnd < 0 || pad.attackSamples < 0 ||
        pad.releaseSamples < 0) {
        throw std::runtime_error("Drum pad sample and envelope ranges must not be negative");
    }
    const auto sourceEnd =
        pad.sampleEnd == 0 ? static_cast<std::int64_t>(source.frames) : pad.sampleEnd;
    if (sourceEnd <= pad.sampleStart || sourceEnd > source.frames) {
        throw std::runtime_error("Drum pad sample range must fit inside source");
    }
    if (pad.lowPassCoefficient < 0.0F || pad.lowPassCoefficient > 1.0F) {
        throw std::runtime_error("Drum pad low-pass coefficient must be in [0, 1]");
    }

    const auto sourceFrames = sourceEnd - pad.sampleStart;
    const auto pitchRatio = pitchShiftRatio(pad.pitchSemitones);
    const auto outputFrames =
        std::max<std::int64_t>(1, static_cast<std::int64_t>(std::ceil(sourceFrames / pitchRatio)));
    audio::RenderedAudio rendered{.channels = source.channels,
                                  .frames = static_cast<std::uint32_t>(outputFrames),
                                  .interleavedSamples = std::vector<float>(
                                      static_cast<std::size_t>(outputFrames) * source.channels)};

    const auto velocityGain = static_cast<float>(velocity) / 127.0F;
    const auto linearGain = dbToLinearGain(pad.gainDb) * velocityGain;
    std::vector<float> filterState(source.channels, 0.0F);

    for (std::int64_t frame = 0; frame < outputFrames; ++frame) {
        const auto pitchedOffset = std::min<std::int64_t>(
            sourceFrames - 1, static_cast<std::int64_t>(std::floor(frame * pitchRatio)));
        const auto sourceFrame =
            pad.reversed ? sourceEnd - pitchedOffset - 1 : pad.sampleStart + pitchedOffset;
        const auto gain = linearGain * envelopeGain(pad, frame, outputFrames);
        for (std::uint32_t channel = 0; channel < source.channels; ++channel) {
            const auto input =
                source.interleavedSamples[static_cast<std::size_t>(sourceFrame) * source.channels +
                                          channel] *
                gain;
            filterState[channel] =
                filterState[channel] + pad.lowPassCoefficient * (input - filterState[channel]);
            rendered
                .interleavedSamples[static_cast<std::size_t>(frame) * source.channels + channel] =
                filterState[channel];
        }
    }

    return rendered;
}

std::vector<DrumRouteRender> renderDrumMachineRoutes(const DrumMachinePreset& preset,
                                                     const std::vector<DrumPadEvent>& events,
                                                     const std::vector<DrumSampleAsset>& samples,
                                                     std::uint32_t frames, std::uint32_t channels) {
    if (frames == 0U || channels == 0U) {
        throw std::runtime_error("Drum route render requires positive frame and channel counts");
    }

    struct Voice {
        const DrumPad* pad{nullptr};
        std::string assetId;
        std::int64_t startSample{0};
        std::int64_t endSample{0};
        std::uint8_t velocity{100};
    };

    std::vector<Voice> voices;
    std::map<std::uint8_t, std::size_t> latestByChokeGroup;
    const bool soloMode = anySolo(preset);

    for (const auto& event : events) {
        if (event.samplePosition < 0 || event.samplePosition >= frames) {
            continue;
        }

        const auto* pad = findPadForMidiNote(preset, event.midiNote);
        if (pad == nullptr || pad->muted || (soloMode && !pad->solo)) {
            continue;
        }

        const auto velocity = std::clamp<std::uint8_t>(event.velocity, 1, 127);
        const auto assetId = selectLayerAsset(*pad, velocity);
        if (assetId.empty()) {
            continue;
        }

        if (pad->chokeGroup.has_value()) {
            const auto latest = latestByChokeGroup.find(*pad->chokeGroup);
            if (latest != latestByChokeGroup.end()) {
                voices[latest->second].endSample =
                    std::min(voices[latest->second].endSample, event.samplePosition);
            }
        }

        voices.push_back({.pad = pad,
                          .assetId = assetId,
                          .startSample = event.samplePosition,
                          .endSample = static_cast<std::int64_t>(frames),
                          .velocity = velocity});

        if (pad->chokeGroup.has_value()) {
            latestByChokeGroup[*pad->chokeGroup] = voices.size() - 1;
        }
    }

    std::vector<DrumRouteRender> routes;
    const auto routeFor = [&routes, frames, channels](std::string routeName) -> DrumRouteRender& {
        if (routeName.empty()) {
            routeName = "master";
        }
        const auto found = std::ranges::find_if(routes, [&routeName](const DrumRouteRender& route) {
            return route.outputRoute == routeName;
        });
        if (found != routes.end()) {
            return *found;
        }
        routes.push_back({.outputRoute = std::move(routeName),
                          .audio = {.channels = channels,
                                    .frames = frames,
                                    .interleavedSamples = std::vector<float>(
                                        static_cast<std::size_t>(frames) * channels)}});
        return routes.back();
    };

    for (const auto& voice : voices) {
        const auto* source = findSampleAsset(samples, voice.assetId);
        if (source == nullptr) {
            throw std::runtime_error("Drum route render missing sample asset: " + voice.assetId);
        }

        const auto rendered = renderDrumPadSample(*voice.pad, *source, voice.velocity);
        auto& route = routeFor(voice.pad->outputRoute);
        const auto renderableFrames =
            std::min<std::int64_t>(rendered.frames, voice.endSample - voice.startSample);
        for (std::int64_t frame = 0; frame < renderableFrames; ++frame) {
            const auto outputFrame = voice.startSample + frame;
            if (outputFrame < 0 || outputFrame >= route.audio.frames) {
                continue;
            }
            for (std::uint32_t channel = 0; channel < channels; ++channel) {
                const auto sourceChannel = std::min(channel, rendered.channels - 1U);
                route.audio.interleavedSamples[static_cast<std::size_t>(outputFrame) * channels +
                                               channel] +=
                    rendered
                        .interleavedSamples[static_cast<std::size_t>(frame) * rendered.channels +
                                            sourceChannel];
            }
        }
    }

    return routes;
}

std::vector<DrumPresetAssetReference>
collectDrumPresetAssetReferences(const DrumMachinePreset& preset) {
    std::vector<DrumPresetAssetReference> references;
    for (const auto& pad : preset.pads) {
        validatePad(pad);
        for (const auto& layer : pad.velocityLayers) {
            auto found = std::ranges::find_if(references,
                                              [&layer](const DrumPresetAssetReference& reference) {
                                                  return reference.assetId == layer.assetId;
                                              });
            if (found == references.end()) {
                references.push_back({.assetId = layer.assetId, .padIds = {pad.id}});
            } else if (!std::ranges::contains(found->padIds, pad.id)) {
                found->padIds.push_back(pad.id);
            }
        }
    }
    std::ranges::sort(references, {}, &DrumPresetAssetReference::assetId);
    for (auto& reference : references) {
        std::ranges::sort(reference.padIds);
    }
    return references;
}

DrumMachinePreset makePortableDrumMachinePreset(const DrumMachinePreset& preset,
                                                std::span<const DrumPresetAssetMapping> mappings) {
    auto portable = preset;
    for (auto& pad : portable.pads) {
        validatePad(pad);
        for (auto& layer : pad.velocityLayers) {
            const auto found =
                std::ranges::find_if(mappings, [&layer](const DrumPresetAssetMapping& mapping) {
                    return mapping.sourceAssetId == layer.assetId;
                });
            if (found == mappings.end() || found->collectedAssetId.empty()) {
                throw std::runtime_error("Drum preset collection is missing asset mapping: " +
                                         layer.assetId);
            }
            layer.assetId = found->collectedAssetId;
        }
        validatePad(pad);
    }
    return portable;
}

} // namespace lamusica::session
