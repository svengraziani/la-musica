#include "lamusica/plugin_host/PluginHostCapabilities.hpp"

namespace lamusica::plugin_host {

session::PluginHostEnvironment probePluginHostEnvironment(const PluginHostCapabilityProbe& probe) {
    return {.macOS =
#if defined(__APPLE__)
                true,
#else
                false,
#endif
            .audioUnitRuntimeAvailable =
#if defined(__APPLE__)
                true,
#else
                false,
#endif
            .vst3SdkAvailable = probe.vst3SdkPresent,
            .vst3LicenseAccepted = probe.vst3LicenseAccepted,
            .outOfProcessHostingAvailable =
                !probe.workerPath.empty() && std::filesystem::exists(probe.workerPath)};
}

} // namespace lamusica::plugin_host
