#pragma once

#include <string_view>

namespace lamusica::mcp_bridge {

enum class Capability {
    ReadOnly,
    Edit,
    Render,
    ImportExport,
    PluginControl,
    Orchestration,
};

[[nodiscard]] std::string_view toString(Capability capability) noexcept;

} // namespace lamusica::mcp_bridge
