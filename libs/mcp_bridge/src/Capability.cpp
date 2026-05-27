#include "lamusica/mcp_bridge/Capability.hpp"

namespace lamusica::mcp_bridge {

std::string_view toString(Capability capability) noexcept {
    switch (capability) {
    case Capability::ReadOnly:
        return "read_only";
    case Capability::Edit:
        return "edit";
    case Capability::Render:
        return "render";
    case Capability::ImportExport:
        return "import_export";
    case Capability::PluginControl:
        return "plugin_control";
    case Capability::Orchestration:
        return "orchestration";
    }

    return "unknown";
}

} // namespace lamusica::mcp_bridge
