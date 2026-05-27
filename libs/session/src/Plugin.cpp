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

    report.appLaunchSafe = true;
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
        const auto& insert = chain.inserts[index];
        output << "{\"id\":\"" << escapeJson(insert.id) << "\",\"pluginIdentifier\":\""
               << escapeJson(insert.pluginIdentifier)
               << "\",\"bypassed\":" << (insert.bypassed ? "true" : "false") << ",";
        writeParameterValues(output, insert.parameterValues);
        output << "}";
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
        PluginInsert insert{.id = readRequiredString(item, "id"),
                            .pluginIdentifier = readRequiredString(item, "pluginIdentifier"),
                            .bypassed = item.find("\"bypassed\":true") != std::string_view::npos,
                            .parameterValues = parseParameterValues(item)};
        addInsert(chain, std::move(insert));
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

} // namespace lamusica::session
