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
    if (argc > 1 && std::string{argv[1]} == "--install") {
        const auto label = argc > 2 ? std::string{argv[2]} : std::string{"com.lamusica.mcpd"};
        session.install(label);
        std::cout << "lamusica-mcpd installed label=" << session.launchLabel() << '\n';
        return 0;
    }
    if (argc > 1 && std::string{argv[1]} == "--launch") {
        session.launch();
        std::cout << "lamusica-mcpd launched label=" << session.launchLabel() << '\n';
        return 0;
    }
    if (argc > 1 && std::string{argv[1]} == "--stop") {
        session.stop();
        std::cout << "lamusica-mcpd stopped\n";
        return 0;
    }
    if (argc > 1 && std::string{argv[1]} == "--logs") {
        session.install("com.lamusica.mcpd");
        session.launch();
        for (const auto& entry : session.daemonLog()) {
            std::cout << entry.id << ' ' << entry.event << ' ' << entry.detail << '\n';
        }
        return 0;
    }
    if (argc > 1 && std::string{argv[1]} == "--health") {
        const auto health = session.health();
        std::cout << "lamusica-mcpd health=" << (health.ok ? "ok" : "error")
                  << " state=" << health.message
                  << " lifecycle=" << lamusica::mcp_bridge::toString(session.lifecycleStatus())
                  << '\n';
        return health.ok ? 0 : 1;
    }
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
            std::cout.flush();
        }
        return 0;
    }

    const auto health = session.health();
    std::cout << "lamusica-mcpd health=" << (health.ok ? "ok" : "error")
              << " state=" << health.message
              << " lifecycle=" << lamusica::mcp_bridge::toString(session.lifecycleStatus())
              << " capability="
              << lamusica::mcp_bridge::toString(lamusica::mcp_bridge::Capability::ReadOnly) << '\n';
    return 0;
}
