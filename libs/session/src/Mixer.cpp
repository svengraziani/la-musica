#include "lamusica/session/Mixer.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <stdexcept>

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

} // namespace

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

    if (hasRoutingCycle(mixer)) {
        if (errorMessage != nullptr) {
            *errorMessage = "Routing graph contains a feedback cycle";
        }
        return false;
    }

    return true;
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

} // namespace lamusica::session
