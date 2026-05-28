#pragma once

#include "lamusica/session/Plugin.hpp"

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace lamusica::session {

enum class ChannelType {
    Audio,
    Midi,
    Instrument,
    Group,
    Return,
    Master,
    HardwareInput,
    HardwareOutput,
};

[[nodiscard]] std::string_view toString(ChannelType type) noexcept;

struct Send {
    std::string id;
    std::string destinationChannelId;
    float gainDb{0.0F};
    bool preFader{false};
};

struct ChannelStrip {
    std::string id;
    std::string name;
    ChannelType type{ChannelType::Audio};
    float volumeDb{0.0F};
    float pan{-0.0F};
    bool muted{false};
    bool solo{false};
    bool recordArmed{false};
    bool inputMonitoring{false};
    bool phaseInverted{false};
    PluginInsertChain inserts;
    std::vector<Send> sends;
};

struct RoutingEdge {
    std::string sourceChannelId;
    std::string destinationChannelId;
};

struct SidechainRoute {
    std::string id;
    std::string sourceChannelId;
    std::string destinationChannelId;
    std::string targetInsertId;
};

struct FaderGroup {
    std::string id;
    std::string name;
    std::vector<std::string> channelIds;
    bool linkVolume{true};
    bool linkMute{false};
};

struct MixerState {
    std::vector<ChannelStrip> channels;
    std::vector<RoutingEdge> routing;
    std::vector<SidechainRoute> sidechains;
    std::vector<FaderGroup> faderGroups;
};

struct MeterReading {
    float peak{0.0F};
    float rms{0.0F};
    float lufs{0.0F};
    float heldPeak{0.0F};
    bool clipped{false};
};

struct MeterState {
    MeterReading reading;
};

struct RoutingMatrixCell {
    std::string sourceChannelId;
    std::string destinationChannelId;
    bool existingRoute{false};
    bool routeAllowed{false};
    bool sendAllowed{false};
    bool sidechainAllowed{false};
    bool wouldCreateFeedback{false};
};

[[nodiscard]] bool hasRoutingCycle(const MixerState& mixer);
[[nodiscard]] bool validateRouting(const MixerState& mixer, std::string* errorMessage = nullptr);
[[nodiscard]] std::vector<RoutingMatrixCell> buildRoutingMatrix(const MixerState& mixer);
[[nodiscard]] MeterReading measureInterleaved(std::span<const float> samples,
                                              std::uint32_t channels);
void updateMeter(MeterState& state, std::span<const float> samples, std::uint32_t channels);
void resetMeter(MeterState& state) noexcept;
[[nodiscard]] ChannelStrip* findChannel(MixerState& mixer, std::string_view channelId) noexcept;
[[nodiscard]] const ChannelStrip* findChannel(const MixerState& mixer, std::string_view channelId);
void addChannel(MixerState& mixer, ChannelStrip channel);
void addRoute(MixerState& mixer, RoutingEdge route);
void addSidechainRoute(MixerState& mixer, SidechainRoute route);
void addSend(MixerState& mixer, std::string_view sourceChannelId, Send send);
void addFaderGroup(MixerState& mixer, FaderGroup group);
void applyFaderGroupVolumeDelta(MixerState& mixer, std::string_view groupId, float deltaDb);
void applyFaderGroupMute(MixerState& mixer, std::string_view groupId, bool muted);
[[nodiscard]] std::string serializeMixerState(const MixerState& mixer);
[[nodiscard]] MixerState parseMixerState(std::string_view json);

} // namespace lamusica::session
