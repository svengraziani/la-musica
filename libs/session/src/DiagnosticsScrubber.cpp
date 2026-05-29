#include "lamusica/session/DiagnosticsScrubber.hpp"

#include "lamusica/session/ApplicationSession.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace lamusica::session {
namespace {

bool isPathChar(char character) noexcept {
    return std::isalnum(static_cast<unsigned char>(character)) || character == '/' ||
           character == '_' || character == '-' || character == '.' || character == ' ' ||
           character == ':' || character == '+' || character == '\\';
}

bool isPathSeparator(char character) noexcept {
    return character == '/' || character == '\\';
}

bool startsWindowsDrivePath(std::string_view text, std::size_t index) noexcept {
    return index + 2U < text.size() &&
           std::isalpha(static_cast<unsigned char>(text[index])) != 0 && text[index + 1U] == ':' &&
           isPathSeparator(text[index + 2U]);
}

bool startsWindowsUserPath(std::string_view token) noexcept {
    return token.starts_with("Users\\") || token.starts_with("Users/");
}

bool containsUserPath(std::string_view token) noexcept {
    return startsWindowsUserPath(token) || token.find(" Users\\") != std::string_view::npos ||
           token.find(" Users/") != std::string_view::npos;
}

bool startsUncPath(std::string_view text, std::size_t index) noexcept {
    return index + 1U < text.size() && text[index] == '\\' && text[index + 1U] == '\\';
}

std::string replaceAll(std::string text, std::string_view needle, std::string_view replacement) {
    if (needle.empty()) {
        return text;
    }
    std::size_t position = 0;
    while ((position = text.find(needle, position)) != std::string::npos) {
        text.replace(position, needle.size(), replacement);
        position += replacement.size();
    }
    return text;
}

std::string jsonEscape(std::string_view value) {
    std::string escaped;
    for (const char rawCharacter : value) {
        const auto character = static_cast<unsigned char>(rawCharacter);
        switch (character) {
        case '"':
            escaped += "\\\"";
            break;
        case '\\':
            escaped += "\\\\";
            break;
        case '\b':
            escaped += "\\b";
            break;
        case '\f':
            escaped += "\\f";
            break;
        case '\n':
            escaped += "\\n";
            break;
        case '\r':
            escaped += "\\r";
            break;
        case '\t':
            escaped += "\\t";
            break;
        default:
            if (character < 0x20) {
                constexpr char hex[] = "0123456789abcdef";
                escaped += "\\u00";
                escaped.push_back(hex[(character >> 4U) & 0x0FU]);
                escaped.push_back(hex[character & 0x0FU]);
            } else {
                escaped.push_back(static_cast<char>(character));
            }
            break;
        }
    }
    return escaped;
}

} // namespace

std::string scrubDiagnosticsText(std::string_view text, std::string_view projectName) {
    std::string scrubbed{text};
    if (!projectName.empty()) {
        scrubbed = replaceAll(std::move(scrubbed), projectName, "<project>");
    }

    std::string output;
    output.reserve(scrubbed.size());
    for (std::size_t index = 0; index < scrubbed.size();) {
        const bool absoluteUnixPath = scrubbed[index] == '/';
        const bool homePath = scrubbed.compare(index, 2, "~/") == 0;
        const bool windowsDrivePath = startsWindowsDrivePath(scrubbed, index);
        const bool uncPath = startsUncPath(scrubbed, index);
        if (absoluteUnixPath || homePath || windowsDrivePath || uncPath) {
            while (index < scrubbed.size() && isPathChar(scrubbed[index])) {
                ++index;
            }
            output += "<path>";
            continue;
        }

        const bool startsPathToken = index == 0 || !isPathChar(scrubbed[index - 1U]);
        if (startsPathToken && isPathChar(scrubbed[index])) {
            std::size_t end = index;
            while (end < scrubbed.size() && isPathChar(scrubbed[end])) {
                ++end;
            }
            const auto token = std::string_view{scrubbed}.substr(index, end - index);
            if (token.find(".Project.lamusica") != std::string_view::npos ||
                containsUserPath(token)) {
                index = end;
                output += "<path>";
                continue;
            }
        }
        output.push_back(scrubbed[index]);
        ++index;
    }
    output = replaceAll(std::move(output), "Users/<path>", "<path>");
    output = replaceAll(std::move(output), "Users\\<path>", "<path>");
    return output;
}

DiagnosticsPayload makeDiagnosticsPayload(const DiagnosticsPayloadInput& input) {
    DiagnosticsPayload payload;
    payload.fields["applicationName"] =
        scrubDiagnosticsText(input.applicationName, input.projectName);
    payload.fields["version"] = scrubDiagnosticsText(input.version, input.projectName);
    payload.fields["gitCommit"] = scrubDiagnosticsText(input.gitCommit, input.projectName);
    payload.fields["osVersion"] = scrubDiagnosticsText(input.osVersion, input.projectName);
    payload.fields["signal"] = std::to_string(input.signalNumber);
    payload.fields["backtrace"] = scrubDiagnosticsText(input.rawBacktrace, input.projectName);

    std::ostringstream json;
    json << "{";
    bool first = true;
    for (const auto& [key, value] : payload.fields) {
        if (!first) {
            json << ",";
        }
        first = false;
        json << "\"" << jsonEscape(key) << "\":\"" << jsonEscape(value) << "\"";
    }
    json << "}";
    payload.json = json.str();
    return payload;
}

bool diagnosticsEndpointAllowed(std::string_view endpoint) noexcept {
    if (endpoint.empty() || endpoint == "LAMUSICA_DIAGNOSTICS_ENDPOINT") {
        return true;
    }
    constexpr std::string_view scheme{"https://"};
    if (!endpoint.starts_with(scheme) || endpoint.size() == scheme.size()) {
        return false;
    }
    const auto hostAndPath = endpoint.substr(scheme.size());
    const auto hostEnd = hostAndPath.find('/');
    const auto authority = hostAndPath.substr(0, hostEnd);
    if (authority.empty() || authority.find_first_of(" \t\r\n@") != std::string_view::npos) {
        return false;
    }

    std::string_view host = authority;
    std::string_view port;
    const auto colon = authority.find(':');
    if (colon != std::string_view::npos) {
        host = authority.substr(0, colon);
        port = authority.substr(colon + 1U);
        if (port.empty()) {
            return false;
        }
        for (const auto digit : port) {
            if (!std::isdigit(static_cast<unsigned char>(digit))) {
                return false;
            }
        }
    }
    if (host.empty()) {
        return false;
    }
    for (const auto character : host) {
        const auto byte = static_cast<unsigned char>(character);
        if (!std::isalnum(byte) && character != '-' && character != '.') {
            return false;
        }
    }
    return true;
}

bool diagnosticsUploadPermitted(DiagnosticsConsent consent, bool shareDiagnostics) noexcept {
    return shareDiagnostics && consent == DiagnosticsConsent::Granted;
}

std::string resolveDiagnosticsEndpoint(std::string_view preferenceEndpoint,
                                       std::string_view environmentEndpoint) {
    constexpr std::string_view defaultEndpoint{"https://diagnostics.lamusica.dev/v1/crash"};
    if (preferenceEndpoint == "LAMUSICA_DIAGNOSTICS_ENDPOINT") {
        return std::string{environmentEndpoint};
    }
    if (!preferenceEndpoint.empty()) {
        return std::string{preferenceEndpoint};
    }
    if (!environmentEndpoint.empty()) {
        return std::string{environmentEndpoint};
    }
    return std::string{defaultEndpoint};
}

DiagnosticsUploadRequest makeDiagnosticsUploadRequest(
    DiagnosticsConsent consent, bool shareDiagnostics, std::string_view preferenceEndpoint,
    std::string_view environmentEndpoint, const DiagnosticsPayloadInput& input) {
    DiagnosticsUploadRequest request;
    if (!diagnosticsUploadPermitted(consent, shareDiagnostics)) {
        request.reason = "diagnostics consent not granted";
        return request;
    }

    request.endpoint = resolveDiagnosticsEndpoint(preferenceEndpoint, environmentEndpoint);
    request.payload = makeDiagnosticsPayload(input);
    if (request.endpoint.empty() || !diagnosticsEndpointAllowed(request.endpoint)) {
        request.reason = "diagnostics endpoint is not HTTPS";
        return request;
    }
    request.permitted = true;
    request.reason = "ready";
    return request;
}

} // namespace lamusica::session
