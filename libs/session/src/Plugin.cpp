#include "lamusica/session/Plugin.hpp"

#include <algorithm>
#include <charconv>
#include <sstream>
#include <stdexcept>
#include <string_view>

namespace lamusica::session {
namespace {

std::string escapeJson(std::string_view value) {
    std::string escaped;
    escaped.reserve(value.size());
    for (const char character : value) {
        if (character == '"' || character == '\\') {
            escaped.push_back('\\');
        }
        escaped.push_back(character);
    }
    return escaped;
}

std::size_t findValueStart(std::string_view json, std::string_view key) {
    const auto keyToken = "\"" + std::string{key} + "\"";
    const auto keyPosition = json.find(keyToken);
    if (keyPosition == std::string_view::npos) {
        throw std::runtime_error("Missing plugin state key: " + std::string{key});
    }
    const auto colonPosition = json.find(':', keyPosition + keyToken.size());
    if (colonPosition == std::string_view::npos) {
        throw std::runtime_error("Missing plugin state value separator: " + std::string{key});
    }
    return json.find_first_not_of(" \n\r\t", colonPosition + 1);
}

std::string readJsonString(std::string_view json, std::size_t quotePosition) {
    if (quotePosition >= json.size() || json[quotePosition] != '"') {
        throw std::runtime_error("Expected plugin state string");
    }

    std::string value;
    bool escaped = false;
    for (std::size_t index = quotePosition + 1; index < json.size(); ++index) {
        const char character = json[index];
        if (escaped) {
            value.push_back(character);
            escaped = false;
            continue;
        }
        if (character == '\\') {
            escaped = true;
            continue;
        }
        if (character == '"') {
            return value;
        }
        value.push_back(character);
    }

    throw std::runtime_error("Unterminated plugin state string");
}

std::string readRequiredString(std::string_view json, std::string_view key) {
    return readJsonString(json, findValueStart(json, key));
}

float readRequiredFloat(std::string_view json, std::string_view key) {
    const auto start = findValueStart(json, key);
    const auto end = json.find_first_not_of("-+0123456789.eE", start);
    const auto token = json.substr(start, end - start);
    float value{};
    const auto result = std::from_chars(token.data(), token.data() + token.size(), value);
    if (result.ec != std::errc{}) {
        throw std::runtime_error("Expected plugin state number: " + std::string{key});
    }
    return value;
}

std::int32_t readRequiredInt32(std::string_view json, std::string_view key) {
    const auto start = findValueStart(json, key);
    const auto end = json.find_first_not_of("-+0123456789", start);
    const auto token = json.substr(start, end - start);
    std::int32_t value{};
    const auto result = std::from_chars(token.data(), token.data() + token.size(), value);
    if (result.ec != std::errc{}) {
        throw std::runtime_error("Expected plugin state integer: " + std::string{key});
    }
    return value;
}

bool readRequiredBool(std::string_view json, std::string_view key) {
    const auto start = findValueStart(json, key);
    if (json.substr(start, 4) == "true") {
        return true;
    }
    if (json.substr(start, 5) == "false") {
        return false;
    }
    throw std::runtime_error("Expected plugin state bool: " + std::string{key});
}

std::vector<std::string_view> objectArrayItems(std::string_view json, std::string_view key) {
    const auto start = findValueStart(json, key);
    if (start >= json.size() || json[start] != '[') {
        throw std::runtime_error("Expected plugin state array: " + std::string{key});
    }

    std::vector<std::string_view> items;
    int depth = 0;
    std::size_t objectStart = std::string_view::npos;
    bool inString = false;
    bool escaped = false;
    for (std::size_t index = start + 1; index < json.size(); ++index) {
        const char character = json[index];
        if (inString) {
            if (escaped) {
                escaped = false;
            } else if (character == '\\') {
                escaped = true;
            } else if (character == '"') {
                inString = false;
            }
            continue;
        }
        if (character == '"') {
            inString = true;
            continue;
        }
        if (character == '{') {
            if (depth == 0) {
                objectStart = index;
            }
            ++depth;
        } else if (character == '}') {
            --depth;
            if (depth == 0 && objectStart != std::string_view::npos) {
                items.push_back(json.substr(objectStart, index - objectStart + 1));
                objectStart = std::string_view::npos;
            }
        } else if (character == ']' && depth == 0) {
            return items;
        }
    }

    throw std::runtime_error("Unterminated plugin state array: " + std::string{key});
}

void writeParameterValues(std::ostringstream& output,
                          const std::vector<PluginParameterValue>& parameterValues) {
    output << "\"parameterValues\":[";
    for (std::size_t index = 0; index < parameterValues.size(); ++index) {
        const auto& parameter = parameterValues[index];
        output << "{\"parameterId\":\"" << escapeJson(parameter.parameterId)
               << "\",\"value\":" << parameter.value << "}";
        if (index + 1 < parameterValues.size()) {
            output << ',';
        }
    }
    output << "]";
}

std::vector<PluginParameterValue> parseParameterValues(std::string_view json) {
    std::vector<PluginParameterValue> values;
    for (const auto item : objectArrayItems(json, "parameterValues")) {
        values.push_back({.parameterId = readRequiredString(item, "parameterId"),
                          .value = readRequiredFloat(item, "value")});
    }
    return values;
}

void writePluginInsert(std::ostringstream& output, const PluginInsert& insert) {
    output << "{\"id\":\"" << escapeJson(insert.id) << "\",\"pluginIdentifier\":\""
           << escapeJson(insert.pluginIdentifier)
           << "\",\"bypassed\":" << (insert.bypassed ? "true" : "false") << ",";
    writeParameterValues(output, insert.parameterValues);
    output << "}";
}

PluginInsert parsePluginInsert(std::string_view json) {
    return {.id = readRequiredString(json, "id"),
            .pluginIdentifier = readRequiredString(json, "pluginIdentifier"),
            .bypassed = json.find("\"bypassed\":true") != std::string_view::npos,
            .parameterValues = parseParameterValues(json)};
}

} // namespace

std::string_view toString(PluginFormat format) noexcept {
    switch (format) {
    case PluginFormat::AudioUnit:
        return "audio_unit";
    case PluginFormat::Vst3:
        return "vst3";
    case PluginFormat::BuiltIn:
        return "built_in";
    }
    return "built_in";
}

bool isBlacklisted(const PluginScanCache& cache, std::string_view identifier) {
    return std::ranges::contains(cache.blacklist, identifier);
}

void mergeScanResult(PluginScanCache& cache, PluginScanResult result) {
    const auto identifier = result.description.identifier;
    const auto found =
        std::ranges::find_if(cache.results, [&identifier](const PluginScanResult& existing) {
            return existing.description.identifier == identifier;
        });

    if (found == cache.results.end()) {
        cache.results.push_back(std::move(result));
    } else {
        *found = std::move(result);
    }
}

void blacklistPlugin(PluginScanCache& cache, std::string identifier, std::string reason) {
    const auto resultIdentifier = identifier;
    if (!isBlacklisted(cache, identifier)) {
        cache.blacklist.push_back(identifier);
    }

    mergeScanResult(cache, {.description = {.identifier = resultIdentifier},
                            .valid = false,
                            .failureReason = std::move(reason)});
}

void allowPluginRescan(PluginScanCache& cache, std::string_view identifier) {
    std::erase_if(cache.blacklist, [identifier](const std::string& blacklisted) {
        return blacklisted == identifier;
    });
}

std::vector<PluginFormatSupport> pluginFormatSupport(const PluginHostEnvironment& environment) {
    std::vector<PluginFormatSupport> support{
        {.format = PluginFormat::BuiltIn, .available = true, .reason = "built_in_available"}};

    support.push_back({.format = PluginFormat::AudioUnit,
                       .available = environment.macOS && environment.audioUnitRuntimeAvailable,
                       .reason = !environment.macOS ? "audio_unit_requires_macos"
                                                    : (environment.audioUnitRuntimeAvailable
                                                           ? "audio_unit_available"
                                                           : "audio_unit_runtime_missing")});

    support.push_back(
        {.format = PluginFormat::Vst3,
         .available = environment.vst3SdkAvailable && environment.vst3LicenseAccepted,
         .reason = !environment.vst3SdkAvailable
                       ? "vst3_sdk_not_available"
                       : (environment.vst3LicenseAccepted ? "vst3_available"
                                                          : "vst3_license_not_accepted")});

    return support;
}

PluginScanReport scanPluginCandidates(PluginScanCache& cache,
                                      std::span<const PluginScanCandidate> candidates,
                                      PluginScanPolicy policy) {
    PluginScanReport report;
    for (const auto& candidate : candidates) {
        const auto& identifier = candidate.description.identifier;
        if (identifier.empty()) {
            report.scanned.push_back({.description = candidate.description,
                                      .valid = false,
                                      .failureReason = "plugin_identifier_empty"});
            report.appLaunchSafe = false;
            continue;
        }
        if (isBlacklisted(cache, identifier)) {
            report.skippedBlacklisted.push_back(identifier);
            continue;
        }

        switch (candidate.outcome) {
        case PluginScanOutcome::Valid:
            mergeScanResult(cache, {.description = candidate.description, .valid = true});
            report.scanned.push_back({.description = candidate.description, .valid = true});
            break;
        case PluginScanOutcome::Invalid: {
            const auto reason = candidate.failureReason.empty() ? "plugin_validation_failed"
                                                                : candidate.failureReason;
            mergeScanResult(
                cache,
                {.description = candidate.description, .valid = false, .failureReason = reason});
            report.scanned.push_back(
                {.description = candidate.description, .valid = false, .failureReason = reason});
            break;
        }
        case PluginScanOutcome::Crashed: {
            const auto reason =
                candidate.failureReason.empty() ? "plugin_scan_crashed" : candidate.failureReason;
            if (policy.blacklistOnCrash) {
                blacklistPlugin(cache, identifier, reason);
            } else {
                mergeScanResult(cache, {.description = candidate.description,
                                        .valid = false,
                                        .failureReason = reason});
            }
            report.scanned.push_back(
                {.description = candidate.description, .valid = false, .failureReason = reason});
            break;
        }
        case PluginScanOutcome::TimedOut: {
            const auto reason = candidate.failureReason.empty()
                                    ? "plugin_scan_timed_out_after_" +
                                          std::to_string(policy.timeoutMilliseconds) + "ms"
                                    : candidate.failureReason;
            if (policy.blacklistOnTimeout) {
                blacklistPlugin(cache, identifier, reason);
            } else {
                mergeScanResult(cache, {.description = candidate.description,
                                        .valid = false,
                                        .failureReason = reason});
            }
            report.scanned.push_back(
                {.description = candidate.description, .valid = false, .failureReason = reason});
            break;
        }
        case PluginScanOutcome::SkippedBlacklisted:
            blacklistPlugin(cache, identifier,
                            candidate.failureReason.empty() ? "pre_blacklisted"
                                                            : candidate.failureReason);
            report.skippedBlacklisted.push_back(identifier);
            break;
        }
    }

    return report;
}

std::optional<PluginDescription> findPlugin(const PluginScanCache& cache,
                                            std::string_view identifier) {
    if (isBlacklisted(cache, identifier)) {
        return std::nullopt;
    }

    const auto found =
        std::ranges::find_if(cache.results, [identifier](const PluginScanResult& result) {
            return result.valid && result.description.identifier == identifier;
        });
    return found == cache.results.end() ? std::nullopt
                                        : std::optional<PluginDescription>{found->description};
}

std::string stableParameterAddress(std::string_view pluginIdentifier,
                                   std::string_view parameterId) {
    std::ostringstream output;
    output << pluginIdentifier << "::" << parameterId;
    return output.str();
}

std::vector<DiscoveredPluginParameter> discoverPluginParameters(const PluginDescription& plugin) {
    if (plugin.identifier.empty()) {
        throw std::runtime_error("Plugin identifier is required for parameter discovery");
    }

    std::vector<DiscoveredPluginParameter> parameters;
    parameters.reserve(plugin.parameters.size());
    for (const auto& parameter : plugin.parameters) {
        if (parameter.id.empty() || parameter.name.empty()) {
            throw std::runtime_error("Plugin parameter id and name are required");
        }
        parameters.push_back(
            {.id = parameter.id,
             .name = parameter.name,
             .automationAddress = stableParameterAddress(plugin.identifier, parameter.id),
             .defaultValue = parameter.defaultValue});
    }
    return parameters;
}

PluginInsert* findInsert(PluginInsertChain& chain, std::string_view insertId) noexcept {
    const auto found = std::ranges::find_if(
        chain.inserts, [insertId](const PluginInsert& insert) { return insert.id == insertId; });
    return found == chain.inserts.end() ? nullptr : &*found;
}

const PluginInsert* findInsert(const PluginInsertChain& chain, std::string_view insertId) noexcept {
    const auto found = std::ranges::find_if(
        chain.inserts, [insertId](const PluginInsert& insert) { return insert.id == insertId; });
    return found == chain.inserts.end() ? nullptr : &*found;
}

std::optional<float> findParameterValue(const PluginInsert& insert, std::string_view parameterId) {
    const auto found = std::ranges::find_if(
        insert.parameterValues, [parameterId](const PluginParameterValue& parameterValue) {
            return parameterValue.parameterId == parameterId;
        });
    return found == insert.parameterValues.end() ? std::nullopt
                                                 : std::optional<float>{found->value};
}

void setParameterValue(PluginInsert& insert, std::string parameterId, float value) {
    if (parameterId.empty()) {
        throw std::runtime_error("Plugin parameter id must not be empty");
    }

    const auto found = std::ranges::find_if(
        insert.parameterValues, [&parameterId](const PluginParameterValue& parameterValue) {
            return parameterValue.parameterId == parameterId;
        });
    if (found == insert.parameterValues.end()) {
        insert.parameterValues.push_back({.parameterId = std::move(parameterId), .value = value});
    } else {
        found->value = value;
    }
}

void addInsert(PluginInsertChain& chain, PluginInsert insert) {
    if (insert.id.empty() || insert.pluginIdentifier.empty()) {
        throw std::runtime_error("Plugin insert id and plugin identifier are required");
    }

    if (std::ranges::any_of(chain.inserts, [&insert](const PluginInsert& existing) {
            return existing.id == insert.id;
        })) {
        throw std::runtime_error("Plugin insert id already exists");
    }

    chain.inserts.push_back(std::move(insert));
}

void removeInsert(PluginInsertChain& chain, std::string_view insertId) {
    std::erase_if(chain.inserts,
                  [insertId](const PluginInsert& insert) { return insert.id == insertId; });
}

void moveInsert(PluginInsertChain& chain, std::string_view insertId, std::size_t newIndex) {
    const auto found = std::ranges::find_if(
        chain.inserts, [insertId](const PluginInsert& insert) { return insert.id == insertId; });
    if (found == chain.inserts.end()) {
        throw std::runtime_error("Plugin insert was not found");
    }

    auto insert = std::move(*found);
    chain.inserts.erase(found);
    const auto boundedIndex = std::min(newIndex, chain.inserts.size());
    chain.inserts.insert(chain.inserts.begin() + static_cast<std::ptrdiff_t>(boundedIndex),
                         std::move(insert));
}

void applyPreset(PluginInsert& insert, const PluginPreset& preset) {
    if (preset.id.empty() || preset.pluginIdentifier.empty()) {
        throw std::runtime_error("Plugin preset id and plugin identifier are required");
    }
    if (insert.pluginIdentifier != preset.pluginIdentifier) {
        throw std::runtime_error("Plugin preset does not match insert plugin identifier");
    }
    insert.parameterValues = preset.parameterValues;
}

std::string serializePluginInsertChain(const PluginInsertChain& chain) {
    if (chain.trackId.empty()) {
        throw std::runtime_error("Plugin insert chain track id must not be empty");
    }

    std::ostringstream output;
    output << "{\"schemaVersion\":1,\"trackId\":\"" << escapeJson(chain.trackId)
           << "\",\"inserts\":[";
    for (std::size_t index = 0; index < chain.inserts.size(); ++index) {
        writePluginInsert(output, chain.inserts[index]);
        if (index + 1 < chain.inserts.size()) {
            output << ',';
        }
    }
    output << "]}";
    return output.str();
}

PluginInsertChain parsePluginInsertChain(std::string_view json) {
    PluginInsertChain chain{.trackId = readRequiredString(json, "trackId")};
    for (const auto item : objectArrayItems(json, "inserts")) {
        addInsert(chain, parsePluginInsert(item));
    }
    return chain;
}

std::string serializePluginPreset(const PluginPreset& preset) {
    if (preset.id.empty() || preset.name.empty() || preset.pluginIdentifier.empty()) {
        throw std::runtime_error("Plugin preset id, name, and plugin identifier are required");
    }

    std::ostringstream output;
    output << "{\"schemaVersion\":1,\"id\":\"" << escapeJson(preset.id) << "\",\"name\":\""
           << escapeJson(preset.name) << "\",\"pluginIdentifier\":\""
           << escapeJson(preset.pluginIdentifier) << "\",";
    writeParameterValues(output, preset.parameterValues);
    output << "}";
    return output.str();
}

PluginPreset parsePluginPreset(std::string_view json) {
    return {.id = readRequiredString(json, "id"),
            .name = readRequiredString(json, "name"),
            .pluginIdentifier = readRequiredString(json, "pluginIdentifier"),
            .parameterValues = parseParameterValues(json)};
}

PluginInstrumentSlot* findInstrumentSlot(PluginInstrumentRack& rack,
                                         std::string_view trackId) noexcept {
    const auto found =
        std::ranges::find_if(rack.slots, [trackId](const PluginInstrumentSlot& slot) {
            return slot.trackId == trackId;
        });
    return found == rack.slots.end() ? nullptr : &*found;
}

const PluginInstrumentSlot* findInstrumentSlot(const PluginInstrumentRack& rack,
                                               std::string_view trackId) noexcept {
    const auto found =
        std::ranges::find_if(rack.slots, [trackId](const PluginInstrumentSlot& slot) {
            return slot.trackId == trackId;
        });
    return found == rack.slots.end() ? nullptr : &*found;
}

void assignInstrumentSlot(PluginInstrumentRack& rack, const PluginScanCache& cache,
                          std::string trackId, PluginInsert instrument) {
    if (trackId.empty()) {
        throw std::runtime_error("Instrument slot track id must not be empty");
    }
    if (instrument.id.empty() || instrument.pluginIdentifier.empty()) {
        throw std::runtime_error("Instrument slot insert id and plugin identifier are required");
    }

    const auto plugin = findPlugin(cache, instrument.pluginIdentifier);
    if (!plugin.has_value()) {
        throw std::runtime_error("Instrument slot plugin is not available");
    }
    if (!plugin->instrument) {
        throw std::runtime_error("Instrument slot plugin must be an instrument");
    }

    auto* existing = findInstrumentSlot(rack, trackId);
    if (existing == nullptr) {
        rack.slots.push_back({.trackId = std::move(trackId), .instrument = std::move(instrument)});
        return;
    }
    existing->instrument = std::move(instrument);
}

void clearInstrumentSlot(PluginInstrumentRack& rack, std::string_view trackId) {
    std::erase_if(rack.slots,
                  [trackId](const PluginInstrumentSlot& slot) { return slot.trackId == trackId; });
}

std::string serializePluginInstrumentRack(const PluginInstrumentRack& rack) {
    std::ostringstream output;
    output << "{\"schemaVersion\":1,\"slots\":[";
    for (std::size_t index = 0; index < rack.slots.size(); ++index) {
        const auto& slot = rack.slots[index];
        if (slot.trackId.empty()) {
            throw std::runtime_error("Instrument slot track id must not be empty");
        }
        output << "{\"trackId\":\"" << escapeJson(slot.trackId) << "\",\"instrument\":";
        writePluginInsert(output, slot.instrument);
        output << "}";
        if (index + 1 < rack.slots.size()) {
            output << ',';
        }
    }
    output << "]}";
    return output.str();
}

PluginInstrumentRack parsePluginInstrumentRack(std::string_view json) {
    PluginInstrumentRack rack;
    for (const auto item : objectArrayItems(json, "slots")) {
        PluginInstrumentSlot slot{.trackId = readRequiredString(item, "trackId")};
        const auto instrumentStart = findValueStart(item, "instrument");
        if (instrumentStart >= item.size() || item[instrumentStart] != '{') {
            throw std::runtime_error("Expected plugin instrument object");
        }
        int depth = 0;
        bool inString = false;
        bool escaped = false;
        for (std::size_t index = instrumentStart; index < item.size(); ++index) {
            const char character = item[index];
            if (inString) {
                if (escaped) {
                    escaped = false;
                } else if (character == '\\') {
                    escaped = true;
                } else if (character == '"') {
                    inString = false;
                }
                continue;
            }
            if (character == '"') {
                inString = true;
            } else if (character == '{') {
                ++depth;
            } else if (character == '}') {
                --depth;
                if (depth == 0) {
                    slot.instrument = parsePluginInsert(
                        item.substr(instrumentStart, index - instrumentStart + 1));
                    break;
                }
            }
        }
        if (slot.instrument.id.empty()) {
            throw std::runtime_error("Instrument slot is missing instrument insert");
        }
        rack.slots.push_back(std::move(slot));
    }
    return rack;
}

PluginEditorWindow* findEditorWindow(PluginEditorState& state, std::string_view trackId,
                                     std::string_view insertId) noexcept {
    const auto found = std::ranges::find_if(state.windows, [trackId, insertId](const auto& window) {
        return window.trackId == trackId && window.insertId == insertId;
    });
    return found == state.windows.end() ? nullptr : &*found;
}

const PluginEditorWindow* findEditorWindow(const PluginEditorState& state, std::string_view trackId,
                                           std::string_view insertId) noexcept {
    const auto found = std::ranges::find_if(state.windows, [trackId, insertId](const auto& window) {
        return window.trackId == trackId && window.insertId == insertId;
    });
    return found == state.windows.end() ? nullptr : &*found;
}

void openPluginEditor(PluginEditorState& state, PluginEditorWindow window) {
    if (window.trackId.empty() || window.insertId.empty()) {
        throw std::runtime_error("Plugin editor track and insert ids are required");
    }
    if (window.width <= 0 || window.height <= 0) {
        throw std::runtime_error("Plugin editor dimensions must be positive");
    }

    window.open = true;
    auto* existing = findEditorWindow(state, window.trackId, window.insertId);
    if (existing == nullptr) {
        state.windows.push_back(std::move(window));
        return;
    }
    *existing = std::move(window);
}

void closePluginEditor(PluginEditorState& state, std::string_view trackId,
                       std::string_view insertId) {
    auto* window = findEditorWindow(state, trackId, insertId);
    if (window == nullptr) {
        return;
    }
    window->open = false;
}

std::string serializePluginEditorState(const PluginEditorState& state) {
    std::ostringstream output;
    output << "{\"schemaVersion\":1,\"windows\":[";
    for (std::size_t index = 0; index < state.windows.size(); ++index) {
        const auto& window = state.windows[index];
        output << "{\"trackId\":\"" << escapeJson(window.trackId) << "\",\"insertId\":\""
               << escapeJson(window.insertId) << "\",\"open\":" << (window.open ? "true" : "false")
               << ",\"pinned\":" << (window.pinned ? "true" : "false") << ",\"x\":" << window.x
               << ",\"y\":" << window.y << ",\"width\":" << window.width
               << ",\"height\":" << window.height << "}";
        if (index + 1 < state.windows.size()) {
            output << ',';
        }
    }
    output << "]}";
    return output.str();
}

PluginEditorState parsePluginEditorState(std::string_view json) {
    PluginEditorState state;
    for (const auto item : objectArrayItems(json, "windows")) {
        PluginEditorWindow window{.trackId = readRequiredString(item, "trackId"),
                                  .insertId = readRequiredString(item, "insertId"),
                                  .open = readRequiredBool(item, "open"),
                                  .pinned = readRequiredBool(item, "pinned"),
                                  .x = readRequiredInt32(item, "x"),
                                  .y = readRequiredInt32(item, "y"),
                                  .width = readRequiredInt32(item, "width"),
                                  .height = readRequiredInt32(item, "height")};
        openPluginEditor(state, window);
        if (!window.open) {
            closePluginEditor(state, window.trackId, window.insertId);
        }
    }
    return state;
}

} // namespace lamusica::session
