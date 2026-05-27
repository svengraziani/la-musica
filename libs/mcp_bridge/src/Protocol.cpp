#include "lamusica/mcp_bridge/Protocol.hpp"

#include "lamusica/mcp_bridge/QueryTools.hpp"

#include <exception>
#include <initializer_list>
#include <sstream>
#include <string>
#include <vector>

namespace lamusica::mcp_bridge {
namespace {

std::vector<std::string> splitWords(std::string_view line) {
    std::istringstream input{std::string{line}};
    std::vector<std::string> words;
    std::string word;
    while (input >> word) {
        words.push_back(std::move(word));
    }
    return words;
}

bool equalsAny(std::string_view value, std::initializer_list<std::string_view> candidates) {
    for (const auto candidate : candidates) {
        if (value == candidate) {
            return true;
        }
    }
    return false;
}

bool startsWithAny(std::string_view value, std::initializer_list<std::string_view> prefixes) {
    for (const auto prefix : prefixes) {
        if (value.starts_with(prefix)) {
            return true;
        }
    }
    return false;
}

bool parseCapability(std::string_view value, Capability& capability) noexcept {
    if (value == "read_only") {
        capability = Capability::ReadOnly;
    } else if (value == "edit") {
        capability = Capability::Edit;
    } else if (value == "render") {
        capability = Capability::Render;
    } else if (value == "import_export") {
        capability = Capability::ImportExport;
    } else if (value == "plugin_control") {
        capability = Capability::PluginControl;
    } else if (value == "orchestration") {
        capability = Capability::Orchestration;
    } else {
        return false;
    }
    return true;
}

std::set<Capability> parseCapabilities(const std::vector<std::string>& words,
                                       std::size_t firstCapabilityIndex) {
    std::set<Capability> capabilities;
    for (std::size_t index = firstCapabilityIndex; index < words.size(); ++index) {
        Capability capability{Capability::ReadOnly};
        if (parseCapability(words[index], capability)) {
            capabilities.insert(capability);
        }
    }
    if (capabilities.empty()) {
        capabilities.insert(Capability::ReadOnly);
    }
    return capabilities;
}

std::string healthBody(const DaemonSession& session) {
    const auto health = session.health();
    std::ostringstream output;
    output << "health=" << (health.ok ? "ok" : "error") << " state=" << health.message;
    if (session.attached()) {
        output << " project=" << session.projectPath();
    }
    return output.str();
}

bool parsePage(const std::vector<std::string>& words, QueryPage& page) {
    if (words.size() <= 2) {
        return true;
    }
    try {
        page.offset = static_cast<std::size_t>(std::stoull(words[2]));
        if (words.size() > 3) {
            page.limit = static_cast<std::size_t>(std::stoull(words[3]));
        }
    } catch (const std::exception&) {
        return false;
    }
    return words.size() <= 4;
}

bool parseAutomationRangeQuery(const std::vector<std::string>& words, QuerySampleRange& range,
                               QueryPage& page) {
    if (words.size() < 4) {
        return false;
    }
    try {
        range.startSample = std::stoll(words[2]);
        range.endSample = std::stoll(words[3]);
        if (words.size() > 4) {
            page.offset = static_cast<std::size_t>(std::stoull(words[4]));
        }
        if (words.size() > 5) {
            page.limit = static_cast<std::size_t>(std::stoull(words[5]));
        }
    } catch (const std::exception&) {
        return false;
    }
    return words.size() <= 6 && range.endSample > range.startSample;
}

ProtocolResponse handleQuery(DaemonSession& session, const ProtocolProjectState& state,
                             const std::vector<std::string>& words) {
    if (!session.attached()) {
        return {.ok = false, .body = "query_requires_attached_project"};
    }
    if (!session.hasCapability(Capability::ReadOnly)) {
        return {.ok = false, .body = "query_requires_read_only_capability"};
    }
    if (words.size() < 2) {
        return {.ok = false, .body = "query_requires_tool_name"};
    }

    const auto& toolName = words[1];
    const auto forbidden = classifyForbiddenProtocolRequest(words.front(), toolName);
    if (forbidden != ForbiddenProtocolSurface::None) {
        const auto reason = std::string{toString(forbidden)};
        session.recordDeniedProtocolRequest(words.front() + std::string{" "} + toolName, reason);
        return {.ok = false, .body = "forbidden_" + reason};
    }

    const auto success = [&session, &toolName](std::string body) {
        session.recordRead(toolName);
        return ProtocolResponse{.ok = true, .body = std::move(body)};
    };

    static const session::ProjectManifest emptyManifest;
    static const session::TimelineSelection emptySelection;
    static const audio::TransportState emptyTransport;
    static const session::MixerState emptyMixer;
    static const session::PluginScanCache emptyPluginScanCache;
    static const session::AssetCatalog emptyAssetCatalog;

    const auto& manifest = state.manifest == nullptr ? emptyManifest : *state.manifest;
    QueryPage page;
    if (toolName == "automation_range") {
        QuerySampleRange range;
        if (!parseAutomationRangeQuery(words, range, page)) {
            return {.ok = false,
                    .body = "automation_range_query_must_be_start_end_offset_and_limit"};
        }
        return success(automationInRangeJson(manifest, range, page));
    }
    if (!parsePage(words, page)) {
        return {.ok = false, .body = "query_page_must_be_offset_and_limit"};
    }

    if (toolName == "project_summary") {
        return success(projectSummaryJson(manifest));
    }
    if (toolName == "tracks") {
        return success(tracksJson(manifest, page));
    }
    if (toolName == "clips") {
        return success(clipsJson(manifest, page));
    }
    if (toolName == "selection") {
        const auto& selection = state.selection == nullptr ? emptySelection : *state.selection;
        return success(selectionJson(selection));
    }
    if (toolName == "transport") {
        const auto& transport = state.transport == nullptr ? emptyTransport : *state.transport;
        return success(transportJson(transport));
    }
    if (toolName == "tempo") {
        return success(tempoJson(manifest));
    }
    if (toolName == "markers") {
        return success(markersJson(manifest, page));
    }
    if (toolName == "routing") {
        if (state.mixer == nullptr) {
            return success(routingJson(manifest));
        }
        return success(routingJson(*state.mixer));
    }
    if (toolName == "plugins") {
        if (state.pluginScanCache == nullptr) {
            return success(pluginsJson(manifest, page));
        }
        return success(pluginsJson(*state.pluginScanCache));
    }
    if (toolName == "automation") {
        return success(automationJson(manifest, page));
    }
    if (toolName == "assets") {
        if (state.assetCatalog == nullptr) {
            return success(assetsJson(manifest, page));
        }
        return success(assetCatalogJson(
            state.assetCatalog == nullptr ? emptyAssetCatalog : *state.assetCatalog, page));
    }
    if (toolName == "render_capabilities") {
        return success(renderCapabilitiesJson());
    }

    return {.ok = false, .body = "unknown_query_tool"};
}

} // namespace

std::string_view toString(ForbiddenProtocolSurface surface) noexcept {
    switch (surface) {
    case ForbiddenProtocolSurface::None:
        return "none";
    case ForbiddenProtocolSurface::ShellExecution:
        return "shell_execution";
    case ForbiddenProtocolSurface::ProcessExecution:
        return "process_execution";
    case ForbiddenProtocolSurface::FilesystemBrowsing:
        return "filesystem_browsing";
    }
    return "none";
}

ForbiddenProtocolSurface classifyForbiddenProtocolRequest(std::string_view command,
                                                          std::string_view toolName) noexcept {
    const auto value = toolName.empty() ? command : toolName;
    if (equalsAny(value, {"shell", "sh", "bash", "zsh", "run_shell", "exec_shell"}) ||
        startsWithAny(value, {"shell.", "shell/", "terminal.", "terminal/"})) {
        return ForbiddenProtocolSurface::ShellExecution;
    }
    if (equalsAny(value, {"exec", "spawn", "process", "run_process", "subprocess"}) ||
        startsWithAny(value, {"process.", "process/", "subprocess.", "subprocess/"})) {
        return ForbiddenProtocolSurface::ProcessExecution;
    }
    if (equalsAny(value, {"filesystem", "fs", "file", "files", "browse", "open", "cat", "ls",
                          "read_file", "write_file", "list_dir", "list_directory"}) ||
        startsWithAny(value, {"filesystem.", "filesystem/", "fs.", "fs/", "file.", "file/"})) {
        return ForbiddenProtocolSurface::FilesystemBrowsing;
    }
    return ForbiddenProtocolSurface::None;
}

ProtocolResponse handleProtocolLine(DaemonSession& session, std::string_view line) {
    const auto words = splitWords(line);
    if (words.empty()) {
        return {.ok = false, .body = "empty_request"};
    }

    const auto forbidden = classifyForbiddenProtocolRequest(words.front());
    if (forbidden != ForbiddenProtocolSurface::None) {
        const auto reason = std::string{toString(forbidden)};
        session.recordDeniedProtocolRequest(std::string{line}, reason);
        return {.ok = false, .body = "forbidden_" + reason};
    }

    if (words.front() == "health") {
        return {.ok = true, .body = healthBody(session)};
    }
    if (words.front() == "attach") {
        if (words.size() < 2) {
            return {.ok = false, .body = "attach_requires_project_path"};
        }
        const auto token = session.attachProject(words[1], parseCapabilities(words, 2));
        return {.ok = true, .body = "attached token=" + token};
    }
    if (words.front() == "detach") {
        session.detachProject();
        return {.ok = true, .body = "detached"};
    }
    if (words.front() == "can_mutate") {
        return {.ok = true, .body = session.canMutateProject() ? "true" : "false"};
    }

    return {.ok = false, .body = "unknown_request"};
}

ProtocolResponse handleProtocolLine(DaemonSession& session, const ProtocolProjectState& state,
                                    std::string_view line) {
    const auto words = splitWords(line);
    if (!words.empty() && words.front() == "query") {
        return handleQuery(session, state, words);
    }
    return handleProtocolLine(session, line);
}

std::string serializeProtocolResponse(const ProtocolResponse& response) {
    return std::string{response.ok ? "ok " : "error "} + response.body;
}

} // namespace lamusica::mcp_bridge
