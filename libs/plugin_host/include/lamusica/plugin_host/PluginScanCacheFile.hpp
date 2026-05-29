#pragma once

#include "lamusica/session/Plugin.hpp"

#include <filesystem>
#include <string>
#include <string_view>

namespace lamusica::plugin_host {

[[nodiscard]] session::PluginScanSourceKey scanSourceKeyForFile(std::string identifier,
                                                                const std::filesystem::path& path);
[[nodiscard]] bool cacheKeyMatches(const session::PluginScanCache& cache,
                                   const session::PluginScanSourceKey& key);
void upsertScanSourceKey(session::PluginScanCache& cache, session::PluginScanSourceKey key);
[[nodiscard]] std::string serializeScanCache(const session::PluginScanCache& cache);
[[nodiscard]] session::PluginScanCache parseScanCache(std::string_view text);
void writeScanCacheFile(const std::filesystem::path& path, const session::PluginScanCache& cache);
[[nodiscard]] session::PluginScanCache readScanCacheFile(const std::filesystem::path& path);

} // namespace lamusica::plugin_host
