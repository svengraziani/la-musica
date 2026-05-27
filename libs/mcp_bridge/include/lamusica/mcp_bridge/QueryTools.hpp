#pragma once

#include "lamusica/audio/AudioEngine.hpp"
#include "lamusica/session/Assets.hpp"
#include "lamusica/session/Automation.hpp"
#include "lamusica/session/Mixer.hpp"
#include "lamusica/session/ProjectManifest.hpp"
#include "lamusica/session/Timeline.hpp"

#include <cstddef>
#include <string>

namespace lamusica::mcp_bridge {

struct QueryPage {
    std::size_t offset{0};
    std::size_t limit{100};
};

struct QuerySampleRange {
    std::int64_t startSample{0};
    std::int64_t endSample{0};
};

[[nodiscard]] std::string projectSummaryJson(const session::ProjectManifest& manifest);
[[nodiscard]] std::string tracksJson(const session::ProjectManifest& manifest, QueryPage page = {});
[[nodiscard]] std::string clipsJson(const session::ProjectManifest& manifest, QueryPage page = {});
[[nodiscard]] std::string clipsInRangeJson(const session::ProjectManifest& manifest,
                                           QuerySampleRange range, QueryPage page = {});
[[nodiscard]] std::string selectionJson(const session::TimelineSelection& selection);
[[nodiscard]] std::string transportJson(const audio::TransportState& transport);
[[nodiscard]] std::string tempoJson(const session::ProjectManifest& manifest);
[[nodiscard]] std::string markersJson(const session::ProjectManifest& manifest,
                                      QueryPage page = {});
[[nodiscard]] std::string routingJson(const session::ProjectManifest& manifest);
[[nodiscard]] std::string routingJson(const session::MixerState& mixer);
[[nodiscard]] std::string pluginsJson(const session::ProjectManifest& manifest,
                                      QueryPage page = {});
[[nodiscard]] std::string pluginsJson(const session::PluginScanCache& cache);
[[nodiscard]] std::string automationJson(const session::ProjectManifest& manifest,
                                         QueryPage page = {});
[[nodiscard]] std::string automationInRangeJson(const session::ProjectManifest& manifest,
                                                QuerySampleRange range, QueryPage page = {});
[[nodiscard]] std::string assetsJson(const session::ProjectManifest& manifest, QueryPage page = {});
[[nodiscard]] std::string assetCatalogJson(const session::AssetCatalog& catalog,
                                           QueryPage page = {});
[[nodiscard]] std::string renderCapabilitiesJson();

} // namespace lamusica::mcp_bridge
