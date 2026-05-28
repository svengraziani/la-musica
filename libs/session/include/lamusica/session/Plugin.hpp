#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace lamusica::session {

enum class PluginFormat {
    AudioUnit,
    Vst3,
    BuiltIn,
};

[[nodiscard]] std::string_view toString(PluginFormat format) noexcept;

struct PluginParameter {
    std::string id;
    std::string name;
    float defaultValue{0.0F};
};

struct PluginDescription {
    std::string identifier;
    std::string name;
    std::string vendor;
    PluginFormat format{PluginFormat::BuiltIn};
    bool instrument{false};
    std::vector<PluginParameter> parameters;
};

struct PluginScanResult {
    PluginDescription description;
    bool valid{true};
    std::string failureReason;
};

enum class PluginScanOutcome {
    Valid,
    Invalid,
    Crashed,
    TimedOut,
    SkippedBlacklisted,
};

struct PluginScanCandidate {
    PluginDescription description;
    PluginScanOutcome outcome{PluginScanOutcome::Valid};
    std::string failureReason;
};

struct PluginScanPolicy {
    std::uint32_t timeoutMilliseconds{5000};
    bool blacklistOnCrash{true};
    bool blacklistOnTimeout{true};
};

struct PluginScanReport {
    std::vector<PluginScanResult> scanned;
    std::vector<std::string> skippedBlacklisted;
    bool appLaunchSafe{true};
};

struct PluginScanCache {
    std::vector<PluginScanResult> results;
    std::vector<std::string> blacklist;
};

struct PluginParameterValue {
    std::string parameterId;
    float value{0.0F};
};

struct PluginInsert {
    std::string id;
    std::string pluginIdentifier;
    bool bypassed{false};
    std::vector<PluginParameterValue> parameterValues;
};

struct PluginInsertChain {
    std::string trackId;
    std::vector<PluginInsert> inserts;
};

struct PluginPreset {
    std::string id;
    std::string name;
    std::string pluginIdentifier;
    std::vector<PluginParameterValue> parameterValues;
};

void mergeScanResult(PluginScanCache& cache, PluginScanResult result);
[[nodiscard]] bool isBlacklisted(const PluginScanCache& cache, std::string_view identifier);
void blacklistPlugin(PluginScanCache& cache, std::string identifier, std::string reason);
void allowPluginRescan(PluginScanCache& cache, std::string_view identifier);
[[nodiscard]] PluginScanReport scanPluginCandidates(PluginScanCache& cache,
                                                    std::span<const PluginScanCandidate> candidates,
                                                    PluginScanPolicy policy = {});
[[nodiscard]] std::optional<PluginDescription> findPlugin(const PluginScanCache& cache,
                                                          std::string_view identifier);
[[nodiscard]] std::string stableParameterAddress(std::string_view pluginIdentifier,
                                                 std::string_view parameterId);
[[nodiscard]] PluginInsert* findInsert(PluginInsertChain& chain,
                                       std::string_view insertId) noexcept;
[[nodiscard]] const PluginInsert* findInsert(const PluginInsertChain& chain,
                                             std::string_view insertId) noexcept;
[[nodiscard]] std::optional<float> findParameterValue(const PluginInsert& insert,
                                                      std::string_view parameterId);
void setParameterValue(PluginInsert& insert, std::string parameterId, float value);
void addInsert(PluginInsertChain& chain, PluginInsert insert);
void removeInsert(PluginInsertChain& chain, std::string_view insertId);
void moveInsert(PluginInsertChain& chain, std::string_view insertId, std::size_t newIndex);
void applyPreset(PluginInsert& insert, const PluginPreset& preset);
[[nodiscard]] std::string serializePluginInsertChain(const PluginInsertChain& chain);
[[nodiscard]] PluginInsertChain parsePluginInsertChain(std::string_view json);
[[nodiscard]] std::string serializePluginPreset(const PluginPreset& preset);
[[nodiscard]] PluginPreset parsePluginPreset(std::string_view json);

} // namespace lamusica::session
