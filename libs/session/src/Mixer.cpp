#include "lamusica/session/Mixer.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string_view>

namespace lamusica::session {
namespace {

bool channelExists(const MixerState& mixer, std::string_view channelId) {
    return std::ranges::any_of(mixer.channels, [channelId](const ChannelStrip& channel) {
        return channel.id == channelId;
    });
}

bool faderGroupExists(const MixerState& mixer, std::string_view groupId) {
    return std::ranges::any_of(mixer.faderGroups,
                               [groupId](const FaderGroup& group) { return group.id == groupId; });
}

bool sendExists(const ChannelStrip& channel, std::string_view sendId) {
    return std::ranges::any_of(channel.sends,
                               [sendId](const Send& send) { return send.id == sendId; });
}

bool sidechainExists(const MixerState& mixer, std::string_view routeId) {
    return std::ranges::any_of(
        mixer.sidechains, [routeId](const SidechainRoute& route) { return route.id == routeId; });
}

FaderGroup* findFaderGroup(MixerState& mixer, std::string_view groupId) {
    const auto found = std::ranges::find_if(
        mixer.faderGroups, [groupId](const FaderGroup& group) { return group.id == groupId; });
    return found == mixer.faderGroups.end() ? nullptr : &*found;
}

void addSendEdges(const MixerState& mixer, std::map<std::string, std::vector<std::string>>& graph) {
    for (const auto& channel : mixer.channels) {
        for (const auto& send : channel.sends) {
            graph[channel.id].push_back(send.destinationChannelId);
        }
    }
}

bool visitCycle(std::string_view node, const std::map<std::string, std::vector<std::string>>& graph,
                std::set<std::string>& visiting, std::set<std::string>& visited) {
    const auto nodeString = std::string{node};
    if (visited.contains(nodeString)) {
        return false;
    }
    if (visiting.contains(nodeString)) {
        return true;
    }

    visiting.insert(nodeString);
    if (const auto found = graph.find(nodeString); found != graph.end()) {
        for (const auto& next : found->second) {
            if (visitCycle(next, graph, visiting, visited)) {
                return true;
            }
        }
    }
    visiting.erase(nodeString);
    visited.insert(nodeString);
    return false;
}

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

std::size_t findValueStart(std::string_view json, std::string_view key) {
    const auto keyToken = "\"" + std::string{key} + "\"";
    const auto keyPosition = json.find(keyToken);
    if (keyPosition == std::string_view::npos) {
        throw std::runtime_error("Missing mixer state key: " + std::string{key});
    }
    const auto colonPosition = json.find(':', keyPosition + keyToken.size());
    if (colonPosition == std::string_view::npos) {
        throw std::runtime_error("Missing mixer state value separator: " + std::string{key});
    }
    return json.find_first_not_of(" \n\r\t", colonPosition + 1);
}

std::string readJsonString(std::string_view json, std::size_t quotePosition) {
    if (quotePosition >= json.size() || json[quotePosition] != '"') {
        throw std::runtime_error("Expected mixer state string");
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
    throw std::runtime_error("Unterminated mixer state string");
}

std::string readRequiredString(std::string_view json, std::string_view key) {
    return readJsonString(json, findValueStart(json, key));
}

template <typename Value> Value readNumberToken(std::string_view json, std::string_view key) {
    const auto start = findValueStart(json, key);
    const auto end = json.find_first_not_of("-+0123456789.eE", start);
    const auto token = json.substr(start, end - start);
    Value value{};
    const auto result = std::from_chars(token.data(), token.data() + token.size(), value);
    if (result.ec != std::errc{}) {
        throw std::runtime_error("Expected mixer state number: " + std::string{key});
    }
    return value;
}

bool readBool(std::string_view json, std::string_view key) {
    const auto start = findValueStart(json, key);
    if (json.substr(start, 4) == "true") {
        return true;
    }
    if (json.substr(start, 5) == "false") {
        return false;
    }
    throw std::runtime_error("Expected mixer state boolean: " + std::string{key});
}

std::vector<std::string_view> objectArrayItems(std::string_view json, std::string_view key) {
    const auto start = findValueStart(json, key);
    if (start >= json.size() || json[start] != '[') {
        throw std::runtime_error("Expected mixer state array: " + std::string{key});
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

    throw std::runtime_error("Unterminated mixer state array: " + std::string{key});
}

std::vector<std::string> stringArrayItems(std::string_view json, std::string_view key) {
    const auto start = findValueStart(json, key);
    if (start >= json.size() || json[start] != '[') {
        throw std::runtime_error("Expected mixer string array: " + std::string{key});
    }

    std::vector<std::string> items;
    bool inString = false;
    bool escaped = false;
    std::size_t quoteStart = std::string_view::npos;
    for (std::size_t index = start + 1; index < json.size(); ++index) {
        const char character = json[index];
        if (inString) {
            if (escaped) {
                escaped = false;
            } else if (character == '\\') {
                escaped = true;
            } else if (character == '"') {
                items.push_back(readJsonString(json, quoteStart));
                inString = false;
            }
            continue;
        }
        if (character == '"') {
            quoteStart = index;
            inString = true;
        } else if (character == ']') {
            return items;
        }
    }
    throw std::runtime_error("Unterminated mixer string array: " + std::string{key});
}

ChannelType channelTypeFromString(std::string_view value) {
    if (value == "audio") {
        return ChannelType::Audio;
    }
    if (value == "midi") {
        return ChannelType::Midi;
    }
    if (value == "instrument") {
        return ChannelType::Instrument;
    }
    if (value == "group") {
        return ChannelType::Group;
    }
    if (value == "return") {
        return ChannelType::Return;
    }
    if (value == "master") {
        return ChannelType::Master;
    }
    if (value == "hardware_input") {
        return ChannelType::HardwareInput;
    }
    if (value == "hardware_output") {
        return ChannelType::HardwareOutput;
    }
    throw std::runtime_error("Unknown mixer channel type");
}

} // namespace

std::string_view toString(ChannelType type) noexcept {
    switch (type) {
    case ChannelType::Audio:
        return "audio";
    case ChannelType::Midi:
        return "midi";
    case ChannelType::Instrument:
        return "instrument";
    case ChannelType::Group:
        return "group";
    case ChannelType::Return:
        return "return";
    case ChannelType::Master:
        return "master";
    case ChannelType::HardwareInput:
        return "hardware_input";
    case ChannelType::HardwareOutput:
        return "hardware_output";
    }
    return "audio";
}

ChannelStrip* findChannel(MixerState& mixer, std::string_view channelId) noexcept {
    const auto found =
        std::ranges::find_if(mixer.channels, [channelId](const ChannelStrip& channel) {
            return channel.id == channelId;
        });
    return found == mixer.channels.end() ? nullptr : &*found;
}

const ChannelStrip* findChannel(const MixerState& mixer, std::string_view channelId) {
    const auto found =
        std::ranges::find_if(mixer.channels, [channelId](const ChannelStrip& channel) {
            return channel.id == channelId;
        });
    return found == mixer.channels.end() ? nullptr : &*found;
}

void addChannel(MixerState& mixer, ChannelStrip channel) {
    if (channel.id.empty()) {
        throw std::runtime_error("Channel id must not be empty");
    }
    if (channel.name.empty()) {
        throw std::runtime_error("Channel name must not be empty");
    }
    if (channel.pan < -1.0F || channel.pan > 1.0F) {
        throw std::runtime_error("Channel pan must be between -1 and 1");
    }
    for (const auto& send : channel.sends) {
        if (send.id.empty()) {
            throw std::runtime_error("Send id must not be empty");
        }
        if (send.destinationChannelId.empty()) {
            throw std::runtime_error("Send destination channel must not be empty");
        }
    }
    if (channelExists(mixer, channel.id)) {
        throw std::runtime_error("Channel id already exists");
    }

    channel.inserts.trackId = channel.id;
    mixer.channels.push_back(std::move(channel));
}

void addRoute(MixerState& mixer, RoutingEdge route) {
    if (!channelExists(mixer, route.sourceChannelId) ||
        !channelExists(mixer, route.destinationChannelId)) {
        throw std::runtime_error("Route references unknown channel");
    }
    if (route.sourceChannelId == route.destinationChannelId) {
        throw std::runtime_error("Route cannot target the same channel");
    }
    if (std::ranges::any_of(mixer.routing, [&route](const RoutingEdge& existing) {
            return existing.sourceChannelId == route.sourceChannelId &&
                   existing.destinationChannelId == route.destinationChannelId;
        })) {
        throw std::runtime_error("Route already exists");
    }
    mixer.routing.push_back(std::move(route));
    if (hasRoutingCycle(mixer)) {
        mixer.routing.pop_back();
        throw std::runtime_error("Route would create feedback cycle");
    }
}

void addSidechainRoute(MixerState& mixer, SidechainRoute route) {
    if (route.id.empty()) {
        throw std::runtime_error("Sidechain route id must not be empty");
    }
    if (!channelExists(mixer, route.sourceChannelId) ||
        !channelExists(mixer, route.destinationChannelId)) {
        throw std::runtime_error("Sidechain route references unknown channel");
    }
    if (route.sourceChannelId == route.destinationChannelId) {
        throw std::runtime_error("Sidechain route cannot target the same channel");
    }
    if (route.targetInsertId.empty()) {
        throw std::runtime_error("Sidechain route target insert id must not be empty");
    }
    if (sidechainExists(mixer, route.id)) {
        throw std::runtime_error("Sidechain route id already exists");
    }
    if (std::ranges::any_of(mixer.sidechains, [&route](const SidechainRoute& existing) {
            return existing.sourceChannelId == route.sourceChannelId &&
                   existing.destinationChannelId == route.destinationChannelId &&
                   existing.targetInsertId == route.targetInsertId;
        })) {
        throw std::runtime_error("Sidechain route already exists");
    }

    mixer.sidechains.push_back(std::move(route));
}

void addSend(MixerState& mixer, std::string_view sourceChannelId, Send send) {
    auto* source = findChannel(mixer, sourceChannelId);
    if (source == nullptr) {
        throw std::runtime_error("Send source channel does not exist");
    }
    if (send.id.empty()) {
        throw std::runtime_error("Send id must not be empty");
    }
    if (send.destinationChannelId.empty()) {
        throw std::runtime_error("Send destination channel must not be empty");
    }
    if (!channelExists(mixer, send.destinationChannelId)) {
        throw std::runtime_error("Send destination channel does not exist");
    }
    if (source->id == send.destinationChannelId) {
        throw std::runtime_error("Send cannot target the same channel");
    }
    if (sendExists(*source, send.id)) {
        throw std::runtime_error("Send id already exists on source channel");
    }

    source->sends.push_back(std::move(send));
    if (hasRoutingCycle(mixer)) {
        source->sends.pop_back();
        throw std::runtime_error("Send would create feedback cycle");
    }
}

void addFaderGroup(MixerState& mixer, FaderGroup group) {
    if (group.id.empty()) {
        throw std::runtime_error("Fader group id must not be empty");
    }
    if (group.name.empty()) {
        throw std::runtime_error("Fader group name must not be empty");
    }
    if (group.channelIds.empty()) {
        throw std::runtime_error("Fader group must contain at least one channel");
    }
    if (faderGroupExists(mixer, group.id)) {
        throw std::runtime_error("Fader group id already exists");
    }
    for (const auto& channelId : group.channelIds) {
        if (!channelExists(mixer, channelId)) {
            throw std::runtime_error("Fader group references unknown channel");
        }
    }

    mixer.faderGroups.push_back(std::move(group));
}

void applyFaderGroupVolumeDelta(MixerState& mixer, std::string_view groupId, float deltaDb) {
    auto* group = findFaderGroup(mixer, groupId);
    if (group == nullptr) {
        throw std::runtime_error("Fader group was not found");
    }
    if (!group->linkVolume) {
        return;
    }

    for (const auto& channelId : group->channelIds) {
        if (auto* channel = findChannel(mixer, channelId); channel != nullptr) {
            channel->volumeDb += deltaDb;
        }
    }
}

void applyFaderGroupMute(MixerState& mixer, std::string_view groupId, bool muted) {
    auto* group = findFaderGroup(mixer, groupId);
    if (group == nullptr) {
        throw std::runtime_error("Fader group was not found");
    }
    if (!group->linkMute) {
        return;
    }

    for (const auto& channelId : group->channelIds) {
        if (auto* channel = findChannel(mixer, channelId); channel != nullptr) {
            channel->muted = muted;
        }
    }
}

bool hasRoutingCycle(const MixerState& mixer) {
    std::map<std::string, std::vector<std::string>> graph;
    for (const auto& route : mixer.routing) {
        graph[route.sourceChannelId].push_back(route.destinationChannelId);
    }
    addSendEdges(mixer, graph);

    std::set<std::string> visiting;
    std::set<std::string> visited;
    for (const auto& channel : mixer.channels) {
        if (visitCycle(channel.id, graph, visiting, visited)) {
            return true;
        }
    }
    return false;
}

bool validateRouting(const MixerState& mixer, std::string* errorMessage) {
    for (const auto& route : mixer.routing) {
        if (!channelExists(mixer, route.sourceChannelId)) {
            if (errorMessage != nullptr) {
                *errorMessage = "Route source channel does not exist: " + route.sourceChannelId;
            }
            return false;
        }
        if (!channelExists(mixer, route.destinationChannelId)) {
            if (errorMessage != nullptr) {
                *errorMessage =
                    "Route destination channel does not exist: " + route.destinationChannelId;
            }
            return false;
        }
    }
    for (const auto& channel : mixer.channels) {
        for (const auto& send : channel.sends) {
            if (!channelExists(mixer, send.destinationChannelId)) {
                if (errorMessage != nullptr) {
                    *errorMessage =
                        "Send destination channel does not exist: " + send.destinationChannelId;
                }
                return false;
            }
        }
    }
    std::set<std::string> sidechainIds;
    for (const auto& sidechain : mixer.sidechains) {
        if (sidechain.id.empty()) {
            if (errorMessage != nullptr) {
                *errorMessage = "Sidechain route id must not be empty";
            }
            return false;
        }
        if (sidechainIds.contains(sidechain.id)) {
            if (errorMessage != nullptr) {
                *errorMessage = "Sidechain route id is duplicated: " + sidechain.id;
            }
            return false;
        }
        sidechainIds.insert(sidechain.id);
        if (!channelExists(mixer, sidechain.sourceChannelId)) {
            if (errorMessage != nullptr) {
                *errorMessage =
                    "Sidechain source channel does not exist: " + sidechain.sourceChannelId;
            }
            return false;
        }
        if (!channelExists(mixer, sidechain.destinationChannelId)) {
            if (errorMessage != nullptr) {
                *errorMessage = "Sidechain destination channel does not exist: " +
                                sidechain.destinationChannelId;
            }
            return false;
        }
        if (sidechain.sourceChannelId == sidechain.destinationChannelId) {
            if (errorMessage != nullptr) {
                *errorMessage = "Sidechain route cannot target the same channel";
            }
            return false;
        }
        if (sidechain.targetInsertId.empty()) {
            if (errorMessage != nullptr) {
                *errorMessage = "Sidechain route target insert id must not be empty";
            }
            return false;
        }
    }

    if (hasRoutingCycle(mixer)) {
        if (errorMessage != nullptr) {
            *errorMessage = "Routing graph contains a feedback cycle";
        }
        return false;
    }

    return true;
}

std::vector<RoutingMatrixCell> buildRoutingMatrix(const MixerState& mixer) {
    std::vector<RoutingMatrixCell> cells;
    cells.reserve(mixer.channels.size() * mixer.channels.size());

    for (const auto& source : mixer.channels) {
        for (const auto& destination : mixer.channels) {
            if (source.id == destination.id) {
                continue;
            }

            const auto existingRoute =
                std::ranges::any_of(mixer.routing, [&source, &destination](const auto& route) {
                    return route.sourceChannelId == source.id &&
                           route.destinationChannelId == destination.id;
                });
            const auto existingSend =
                std::ranges::any_of(source.sends, [&destination](const Send& send) {
                    return send.destinationChannelId == destination.id;
                });
            const auto existingSidechain = std::ranges::any_of(
                mixer.sidechains, [&source, &destination](const SidechainRoute& route) {
                    return route.sourceChannelId == source.id &&
                           route.destinationChannelId == destination.id;
                });

            auto routeProbe = mixer;
            routeProbe.routing.push_back(
                {.sourceChannelId = source.id, .destinationChannelId = destination.id});
            const auto routeWouldCreateFeedback = hasRoutingCycle(routeProbe);

            auto sendProbe = mixer;
            if (auto* probeSource = findChannel(sendProbe, source.id); probeSource != nullptr) {
                probeSource->sends.push_back(
                    {.id = "__matrix_probe__", .destinationChannelId = destination.id});
            }
            const auto sendWouldCreateFeedback = hasRoutingCycle(sendProbe);

            cells.push_back(
                {.sourceChannelId = source.id,
                 .destinationChannelId = destination.id,
                 .existingRoute = existingRoute,
                 .routeAllowed = !existingRoute && !routeWouldCreateFeedback,
                 .sendAllowed = !existingSend && !sendWouldCreateFeedback,
                 .sidechainAllowed = !existingSidechain,
                 .wouldCreateFeedback = routeWouldCreateFeedback || sendWouldCreateFeedback});
        }
    }

    return cells;
}

MeterReading measureInterleaved(std::span<const float> samples, std::uint32_t channels) {
    if (channels == 0 || samples.empty()) {
        return {};
    }

    float peak = 0.0F;
    double squareSum = 0.0;
    for (const auto sample : samples) {
        const auto absolute = std::abs(sample);
        peak = std::max(peak, absolute);
        squareSum += static_cast<double>(sample) * static_cast<double>(sample);
    }

    const auto rms = static_cast<float>(std::sqrt(squareSum / static_cast<double>(samples.size())));
    const auto lufs = rms > 0.0F ? 20.0F * std::log10(rms) - 0.691F : -100.0F;
    return {.peak = peak, .rms = rms, .lufs = lufs, .heldPeak = peak, .clipped = peak >= 1.0F};
}

void updateMeter(MeterState& state, std::span<const float> samples, std::uint32_t channels) {
    auto reading = measureInterleaved(samples, channels);
    reading.heldPeak = std::max(state.reading.heldPeak, reading.peak);
    reading.clipped = state.reading.clipped || reading.clipped;
    state.reading = reading;
}

void resetMeter(MeterState& state) noexcept {
    state.reading = {};
}

std::string serializeMixerState(const MixerState& mixer) {
    std::ostringstream output;
    output << "{\"schemaVersion\":1,\"channels\":[";
    for (std::size_t channelIndex = 0; channelIndex < mixer.channels.size(); ++channelIndex) {
        const auto& channel = mixer.channels[channelIndex];
        output << "{\"id\":\"" << escapeJson(channel.id) << "\",\"name\":\""
               << escapeJson(channel.name) << "\",\"type\":\"" << toString(channel.type)
               << "\",\"volumeDb\":" << channel.volumeDb << ",\"pan\":" << channel.pan
               << ",\"muted\":" << (channel.muted ? "true" : "false")
               << ",\"solo\":" << (channel.solo ? "true" : "false")
               << ",\"recordArmed\":" << (channel.recordArmed ? "true" : "false")
               << ",\"inputMonitoring\":" << (channel.inputMonitoring ? "true" : "false")
               << ",\"phaseInverted\":" << (channel.phaseInverted ? "true" : "false")
               << ",\"sends\":[";
        for (std::size_t sendIndex = 0; sendIndex < channel.sends.size(); ++sendIndex) {
            const auto& send = channel.sends[sendIndex];
            output << "{\"id\":\"" << escapeJson(send.id) << "\",\"destinationChannelId\":\""
                   << escapeJson(send.destinationChannelId) << "\",\"gainDb\":" << send.gainDb
                   << ",\"preFader\":" << (send.preFader ? "true" : "false") << "}";
            if (sendIndex + 1 < channel.sends.size()) {
                output << ',';
            }
        }
        output << "]}";
        if (channelIndex + 1 < mixer.channels.size()) {
            output << ',';
        }
    }
    output << "],\"routing\":[";
    for (std::size_t routeIndex = 0; routeIndex < mixer.routing.size(); ++routeIndex) {
        const auto& route = mixer.routing[routeIndex];
        output << "{\"sourceChannelId\":\"" << escapeJson(route.sourceChannelId)
               << "\",\"destinationChannelId\":\"" << escapeJson(route.destinationChannelId)
               << "\"}";
        if (routeIndex + 1 < mixer.routing.size()) {
            output << ',';
        }
    }
    output << "],\"sidechains\":[";
    for (std::size_t sidechainIndex = 0; sidechainIndex < mixer.sidechains.size();
         ++sidechainIndex) {
        const auto& sidechain = mixer.sidechains[sidechainIndex];
        output << "{\"id\":\"" << escapeJson(sidechain.id) << "\",\"sourceChannelId\":\""
               << escapeJson(sidechain.sourceChannelId) << "\",\"destinationChannelId\":\""
               << escapeJson(sidechain.destinationChannelId) << "\",\"targetInsertId\":\""
               << escapeJson(sidechain.targetInsertId) << "\"}";
        if (sidechainIndex + 1 < mixer.sidechains.size()) {
            output << ',';
        }
    }
    output << "],\"faderGroups\":[";
    for (std::size_t groupIndex = 0; groupIndex < mixer.faderGroups.size(); ++groupIndex) {
        const auto& group = mixer.faderGroups[groupIndex];
        output << "{\"id\":\"" << escapeJson(group.id) << "\",\"name\":\"" << escapeJson(group.name)
               << "\",\"channelIds\":[";
        for (std::size_t channelIndex = 0; channelIndex < group.channelIds.size(); ++channelIndex) {
            output << "\"" << escapeJson(group.channelIds[channelIndex]) << "\"";
            if (channelIndex + 1 < group.channelIds.size()) {
                output << ',';
            }
        }
        output << "],\"linkVolume\":" << (group.linkVolume ? "true" : "false")
               << ",\"linkMute\":" << (group.linkMute ? "true" : "false") << "}";
        if (groupIndex + 1 < mixer.faderGroups.size()) {
            output << ',';
        }
    }
    output << "]}";
    return output.str();
}

MixerState parseMixerState(std::string_view json) {
    MixerState mixer;
    struct PendingSend {
        std::string sourceChannelId;
        Send send;
    };
    std::vector<PendingSend> pendingSends;
    for (const auto channelJson : objectArrayItems(json, "channels")) {
        ChannelStrip channel{.id = readRequiredString(channelJson, "id"),
                             .name = readRequiredString(channelJson, "name"),
                             .type = channelTypeFromString(readRequiredString(channelJson, "type")),
                             .volumeDb = readNumberToken<float>(channelJson, "volumeDb"),
                             .pan = readNumberToken<float>(channelJson, "pan"),
                             .muted = readBool(channelJson, "muted"),
                             .solo = readBool(channelJson, "solo"),
                             .recordArmed = readBool(channelJson, "recordArmed"),
                             .inputMonitoring = readBool(channelJson, "inputMonitoring"),
                             .phaseInverted = readBool(channelJson, "phaseInverted")};
        const auto sourceChannelId = channel.id;
        for (const auto sendJson : objectArrayItems(channelJson, "sends")) {
            pendingSends.push_back({.sourceChannelId = sourceChannelId,
                                    .send = {.id = readRequiredString(sendJson, "id"),
                                             .destinationChannelId = readRequiredString(
                                                 sendJson, "destinationChannelId"),
                                             .gainDb = readNumberToken<float>(sendJson, "gainDb"),
                                             .preFader = readBool(sendJson, "preFader")}});
        }
        addChannel(mixer, std::move(channel));
    }
    for (auto& pending : pendingSends) {
        addSend(mixer, pending.sourceChannelId, std::move(pending.send));
    }
    for (const auto routeJson : objectArrayItems(json, "routing")) {
        addRoute(mixer,
                 {.sourceChannelId = readRequiredString(routeJson, "sourceChannelId"),
                  .destinationChannelId = readRequiredString(routeJson, "destinationChannelId")});
    }
    for (const auto sidechainJson : objectArrayItems(json, "sidechains")) {
        addSidechainRoute(
            mixer,
            {.id = readRequiredString(sidechainJson, "id"),
             .sourceChannelId = readRequiredString(sidechainJson, "sourceChannelId"),
             .destinationChannelId = readRequiredString(sidechainJson, "destinationChannelId"),
             .targetInsertId = readRequiredString(sidechainJson, "targetInsertId")});
    }
    for (const auto groupJson : objectArrayItems(json, "faderGroups")) {
        addFaderGroup(mixer, {.id = readRequiredString(groupJson, "id"),
                              .name = readRequiredString(groupJson, "name"),
                              .channelIds = stringArrayItems(groupJson, "channelIds"),
                              .linkVolume = readBool(groupJson, "linkVolume"),
                              .linkMute = readBool(groupJson, "linkMute")});
    }
    return mixer;
}

} // namespace lamusica::session
