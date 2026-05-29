#pragma once

#include "lamusica/session/Plugin.hpp"

#include <filesystem>

namespace lamusica::plugin_host {

struct PluginHostCapabilityProbe {
    bool vst3SdkPresent{false};
    bool vst3LicenseAccepted{false};
    std::filesystem::path workerPath;
};

[[nodiscard]] session::PluginHostEnvironment
probePluginHostEnvironment(const PluginHostCapabilityProbe& probe = {});

} // namespace lamusica::plugin_host
