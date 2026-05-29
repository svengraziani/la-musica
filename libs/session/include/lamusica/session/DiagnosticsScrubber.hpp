#pragma once

#include <map>
#include <string>
#include <string_view>

namespace lamusica::session {

enum class DiagnosticsConsent;

struct DiagnosticsPayloadInput {
    std::string applicationName;
    std::string version;
    std::string gitCommit;
    std::string osVersion;
    int signalNumber{0};
    std::string rawBacktrace;
    std::string projectName;
};

struct DiagnosticsPayload {
    std::string json;
    std::map<std::string, std::string> fields;
};

struct DiagnosticsUploadRequest {
    bool permitted{false};
    std::string endpoint;
    std::string reason;
    DiagnosticsPayload payload;
};

[[nodiscard]] std::string scrubDiagnosticsText(std::string_view text,
                                               std::string_view projectName = {});
[[nodiscard]] DiagnosticsPayload makeDiagnosticsPayload(const DiagnosticsPayloadInput& input);
[[nodiscard]] bool diagnosticsEndpointAllowed(std::string_view endpoint) noexcept;
[[nodiscard]] bool diagnosticsUploadPermitted(DiagnosticsConsent consent,
                                              bool shareDiagnostics) noexcept;
[[nodiscard]] std::string resolveDiagnosticsEndpoint(std::string_view preferenceEndpoint,
                                                     std::string_view environmentEndpoint = {});
[[nodiscard]] DiagnosticsUploadRequest makeDiagnosticsUploadRequest(
    DiagnosticsConsent consent, bool shareDiagnostics, std::string_view preferenceEndpoint,
    std::string_view environmentEndpoint, const DiagnosticsPayloadInput& input);

} // namespace lamusica::session
