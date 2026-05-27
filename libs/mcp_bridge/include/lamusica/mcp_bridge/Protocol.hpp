#pragma once

#include "lamusica/audio/AudioEngine.hpp"
#include "lamusica/mcp_bridge/DaemonSession.hpp"
#include "lamusica/session/Assets.hpp"
#include "lamusica/session/Mixer.hpp"
#include "lamusica/session/ProjectManifest.hpp"
#include "lamusica/session/Timeline.hpp"

#include <string>
#include <string_view>

namespace lamusica::mcp_bridge {

enum class ForbiddenProtocolSurface { None, ShellExecution, ProcessExecution, FilesystemBrowsing };

struct ProtocolResponse {
    bool ok{false};
    std::string body;
};

struct ProtocolProjectState {
    const session::ProjectManifest* manifest{nullptr};
    const session::TimelineSelection* selection{nullptr};
    const audio::TransportState* transport{nullptr};
    const session::MixerState* mixer{nullptr};
    const session::PluginScanCache* pluginScanCache{nullptr};
    const session::AssetCatalog* assetCatalog{nullptr};
};

[[nodiscard]] std::string_view toString(ForbiddenProtocolSurface surface) noexcept;
[[nodiscard]] ForbiddenProtocolSurface
classifyForbiddenProtocolRequest(std::string_view command, std::string_view toolName = {}) noexcept;
[[nodiscard]] ProtocolResponse handleProtocolLine(DaemonSession& session, std::string_view line);
[[nodiscard]] ProtocolResponse handleProtocolLine(DaemonSession& session,
                                                  const ProtocolProjectState& state,
                                                  std::string_view line);
[[nodiscard]] std::string serializeProtocolResponse(const ProtocolResponse& response);

} // namespace lamusica::mcp_bridge
