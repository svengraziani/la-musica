#include "lamusica/mcp_bridge/Capability.hpp"
#include "lamusica/mcp_bridge/DaemonSession.hpp"
#include "lamusica/mcp_bridge/Protocol.hpp"
#include "lamusica/session/ProjectDocument.hpp"

#include <exception>
#include <iostream>
#include <optional>
#include <string>

int main(int argc, char** argv) {
    lamusica::mcp_bridge::DaemonSession session;
    if (argc > 1 && std::string{argv[1]} == "--stdio") {
        std::optional<lamusica::session::ProjectManifest> manifest;
        std::string line;
        while (std::getline(std::cin, line)) {
            const lamusica::mcp_bridge::ProtocolProjectState state{
                .manifest = manifest.has_value() ? &*manifest : nullptr};
            auto response = lamusica::mcp_bridge::handleProtocolLine(session, state, line);
            if (response.ok && session.attached() && line.starts_with("attach ")) {
                try {
                    manifest =
                        lamusica::session::ProjectDocument::open(std::string{session.projectPath()})
                            .manifest();
                } catch (const std::exception& exception) {
                    session.detachProject();
                    manifest.reset();
                    response = {.ok = false, .body = exception.what()};
                }
            } else if (response.ok && !session.attached()) {
                manifest.reset();
            }
            std::cout << lamusica::mcp_bridge::serializeProtocolResponse(response) << '\n';
        }
        return 0;
    }

    const auto health = session.health();
    std::cout << "lamusica-mcpd health=" << (health.ok ? "ok" : "error")
              << " state=" << health.message << " capability="
              << lamusica::mcp_bridge::toString(lamusica::mcp_bridge::Capability::ReadOnly) << '\n';
    return 0;
}
