#include "lamusica/plugin_host/PluginScanCacheFile.hpp"

#include <charconv>
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string_view>

namespace lamusica::plugin_host {
namespace {

constexpr std::string_view cacheHeader{"LAMUSICA_PLUGIN_SCAN_CACHE_V1"};

std::string escapeField(std::string_view value) {
    std::string escaped;
    escaped.reserve(value.size());
    for (const char character : value) {
        switch (character) {
        case '\\':
            escaped += "\\\\";
            break;
        case '\t':
            escaped += "\\t";
            break;
        case '\n':
            escaped += "\\n";
            break;
        case '\r':
            escaped += "\\r";
            break;
        default:
            escaped.push_back(character);
            break;
        }
    }
    return escaped;
}

std::string unescapeField(std::string_view value) {
    std::string unescaped;
    unescaped.reserve(value.size());
    bool escaped = false;
    for (const char character : value) {
        if (!escaped) {
            if (character == '\\') {
                escaped = true;
            } else {
                unescaped.push_back(character);
            }
            continue;
        }
        switch (character) {
        case '\\':
            unescaped.push_back('\\');
            break;
        case 't':
            unescaped.push_back('\t');
            break;
        case 'n':
            unescaped.push_back('\n');
            break;
        case 'r':
            unescaped.push_back('\r');
            break;
        default:
            throw std::runtime_error("Invalid escaped scan-cache field");
        }
        escaped = false;
    }
    if (escaped) {
        throw std::runtime_error("Unterminated escaped scan-cache field");
    }
    return unescaped;
}

std::vector<std::string_view> splitLine(std::string_view line) {
    std::vector<std::string_view> fields;
    std::size_t start = 0;
    while (start <= line.size()) {
        const auto tab = line.find('\t', start);
        if (tab == std::string_view::npos) {
            fields.push_back(line.substr(start));
            break;
        }
        fields.push_back(line.substr(start, tab - start));
        start = tab + 1;
    }
    return fields;
}

float parseFloat(std::string_view value) {
    float parsed = 0.0F;
    const auto result = std::from_chars(value.data(), value.data() + value.size(), parsed);
    if (result.ec != std::errc{} || result.ptr != value.data() + value.size()) {
        throw std::runtime_error("Invalid scan-cache float");
    }
    return parsed;
}

bool parseBool(std::string_view value) {
    if (value == "1") {
        return true;
    }
    if (value == "0") {
        return false;
    }
    throw std::runtime_error("Invalid scan-cache bool");
}

std::uint64_t parseUint64(std::string_view value) {
    std::uint64_t parsed = 0;
    const auto result = std::from_chars(value.data(), value.data() + value.size(), parsed);
    if (result.ec != std::errc{} || result.ptr != value.data() + value.size()) {
        throw std::runtime_error("Invalid scan-cache uint64");
    }
    return parsed;
}

std::int64_t parseInt64(std::string_view value) {
    std::int64_t parsed = 0;
    const auto result = std::from_chars(value.data(), value.data() + value.size(), parsed);
    if (result.ec != std::errc{} || result.ptr != value.data() + value.size()) {
        throw std::runtime_error("Invalid scan-cache int64");
    }
    return parsed;
}

session::PluginFormat parseFormat(std::string_view value) {
    if (value == session::toString(session::PluginFormat::AudioUnit)) {
        return session::PluginFormat::AudioUnit;
    }
    if (value == session::toString(session::PluginFormat::Vst3)) {
        return session::PluginFormat::Vst3;
    }
    return session::PluginFormat::BuiltIn;
}

} // namespace

session::PluginScanSourceKey scanSourceKeyForFile(std::string identifier,
                                                  const std::filesystem::path& path) {
    const auto canonical = std::filesystem::weakly_canonical(path);
    const auto mtime = std::filesystem::last_write_time(canonical).time_since_epoch();
    return {.identifier = std::move(identifier),
            .path = canonical.string(),
            .sizeBytes = std::filesystem::file_size(canonical),
            .mtimeNanoseconds =
                std::chrono::duration_cast<std::chrono::nanoseconds>(mtime).count()};
}

bool cacheKeyMatches(const session::PluginScanCache& cache,
                     const session::PluginScanSourceKey& key) {
    return std::ranges::any_of(cache.sourceKeys, [&key](const auto& cached) {
        return cached.identifier == key.identifier && cached.path == key.path &&
               cached.sizeBytes == key.sizeBytes &&
               cached.mtimeNanoseconds == key.mtimeNanoseconds;
    });
}

void upsertScanSourceKey(session::PluginScanCache& cache, session::PluginScanSourceKey key) {
    const auto found = std::ranges::find_if(cache.sourceKeys, [&key](const auto& cached) {
        return cached.identifier == key.identifier && cached.path == key.path;
    });
    if (found == cache.sourceKeys.end()) {
        cache.sourceKeys.push_back(std::move(key));
    } else {
        *found = std::move(key);
    }
}

std::string serializeScanCache(const session::PluginScanCache& cache) {
    std::ostringstream output;
    output << cacheHeader << '\n';
    for (const auto& key : cache.sourceKeys) {
        output << "source\t" << escapeField(key.identifier) << '\t' << escapeField(key.path) << '\t'
               << key.sizeBytes << '\t' << key.mtimeNanoseconds << '\n';
    }
    for (const auto& blacklisted : cache.blacklist) {
        output << "blacklist\t" << escapeField(blacklisted) << '\n';
    }
    for (const auto& result : cache.results) {
        const auto& plugin = result.description;
        output << "result\t" << (result.valid ? '1' : '0') << '\t'
               << escapeField(plugin.identifier) << '\t' << escapeField(plugin.name) << '\t'
               << escapeField(plugin.vendor) << '\t' << session::toString(plugin.format) << '\t'
               << (plugin.instrument ? '1' : '0') << '\t' << escapeField(result.failureReason)
               << '\n';
        for (const auto& parameter : plugin.parameters) {
            output << "param\t" << escapeField(plugin.identifier) << '\t'
                   << escapeField(parameter.id) << '\t' << escapeField(parameter.name) << '\t'
                   << parameter.defaultValue << '\n';
        }
    }
    return output.str();
}

session::PluginScanCache parseScanCache(std::string_view text) {
    session::PluginScanCache cache;
    std::istringstream input{std::string{text}};
    std::string line;
    if (!std::getline(input, line) || line != cacheHeader) {
        throw std::runtime_error("Invalid plugin scan cache header");
    }

    while (std::getline(input, line)) {
        if (line.empty()) {
            continue;
        }
        const auto fields = splitLine(line);
        if (fields.empty()) {
            continue;
        }
        if (fields[0] == "blacklist") {
            if (fields.size() != 2U) {
                throw std::runtime_error("Invalid scan-cache blacklist row");
            }
            cache.blacklist.push_back(unescapeField(fields[1]));
        } else if (fields[0] == "source") {
            if (fields.size() != 5U) {
                throw std::runtime_error("Invalid scan-cache source row");
            }
            cache.sourceKeys.push_back({.identifier = unescapeField(fields[1]),
                                        .path = unescapeField(fields[2]),
                                        .sizeBytes = parseUint64(fields[3]),
                                        .mtimeNanoseconds = parseInt64(fields[4])});
        } else if (fields[0] == "result") {
            if (fields.size() != 8U) {
                throw std::runtime_error("Invalid scan-cache result row");
            }
            cache.results.push_back(
                {.description = {.identifier = unescapeField(fields[2]),
                                 .name = unescapeField(fields[3]),
                                 .vendor = unescapeField(fields[4]),
                                 .format = parseFormat(fields[5]),
                                 .instrument = parseBool(fields[6])},
                 .valid = parseBool(fields[1]),
                 .failureReason = unescapeField(fields[7])});
        } else if (fields[0] == "param") {
            if (fields.size() != 5U || cache.results.empty()) {
                throw std::runtime_error("Invalid scan-cache parameter row");
            }
            const auto pluginIdentifier = unescapeField(fields[1]);
            if (cache.results.back().description.identifier != pluginIdentifier) {
                throw std::runtime_error("Scan-cache parameter row is not adjacent to plugin");
            }
            cache.results.back().description.parameters.push_back(
                {.id = unescapeField(fields[2]),
                 .name = unescapeField(fields[3]),
                 .defaultValue = parseFloat(fields[4])});
        } else {
            throw std::runtime_error("Unknown scan-cache row: " + std::string{fields[0]});
        }
    }

    return cache;
}

void writeScanCacheFile(const std::filesystem::path& path, const session::PluginScanCache& cache) {
    const auto temporary = path.string() + ".tmp";
    {
        std::ofstream output{temporary, std::ios::binary | std::ios::trunc};
        output << serializeScanCache(cache);
        if (!output.good()) {
            throw std::runtime_error("Failed writing plugin scan cache: " + temporary);
        }
    }
    std::filesystem::rename(temporary, path);
}

session::PluginScanCache readScanCacheFile(const std::filesystem::path& path) {
    std::ifstream input{path, std::ios::binary};
    if (!input.good()) {
        throw std::runtime_error("Failed opening plugin scan cache: " + path.string());
    }
    std::ostringstream text;
    text << input.rdbuf();
    return parseScanCache(text.str());
}

} // namespace lamusica::plugin_host
