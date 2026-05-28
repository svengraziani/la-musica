#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>
#include <sys/wait.h>
#include <unistd.h>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(1);
    }
}

class McpFixtureClient {
  public:
    explicit McpFixtureClient(const char* daemonPath) {
        int childInput[2]{};
        int childOutput[2]{};
        require(pipe(childInput) == 0, "create daemon input pipe");
        require(pipe(childOutput) == 0, "create daemon output pipe");

        pid_ = fork();
        require(pid_ >= 0, "fork daemon process");
        if (pid_ == 0) {
            dup2(childInput[0], STDIN_FILENO);
            dup2(childOutput[1], STDOUT_FILENO);
            close(childInput[0]);
            close(childInput[1]);
            close(childOutput[0]);
            close(childOutput[1]);
            execl(daemonPath, daemonPath, "--stdio", nullptr);
            std::perror("execl");
            _exit(127);
        }

        close(childInput[0]);
        close(childOutput[1]);
        input_ = fdopen(childInput[1], "w");
        output_ = fdopen(childOutput[0], "r");
        require(input_ != nullptr && output_ != nullptr, "open daemon stdio streams");
    }

    McpFixtureClient(const McpFixtureClient&) = delete;
    McpFixtureClient& operator=(const McpFixtureClient&) = delete;

    ~McpFixtureClient() {
        if (input_ != nullptr) {
            fclose(input_);
        }
        if (output_ != nullptr) {
            fclose(output_);
        }
        if (pid_ > 0) {
            int status = 0;
            if (waitpid(pid_, &status, WNOHANG) == 0) {
                kill(pid_, SIGTERM);
                waitpid(pid_, &status, 0);
            }
        }
    }

    [[nodiscard]] std::string request(const std::string& line) {
        require(std::fprintf(input_, "%s\n", line.c_str()) > 0, "write daemon request");
        require(std::fflush(input_) == 0, "flush daemon request");
        char buffer[8192]{};
        require(std::fgets(buffer, sizeof(buffer), output_) != nullptr, "read daemon response");
        std::string response{buffer};
        if (!response.empty() && response.back() == '\n') {
            response.pop_back();
        }
        return response;
    }

  private:
    pid_t pid_{-1};
    FILE* input_{nullptr};
    FILE* output_{nullptr};
};

std::string tokenFromAttachResponse(const std::string& response) {
    const std::string marker = "token=";
    const auto position = response.find(marker);
    require(position != std::string::npos, "attach response includes auth token");
    return response.substr(position + marker.size());
}

} // namespace

int main(int argc, char** argv) {
    require(argc == 3, "usage: mcp_fixture_client <lamusica_mcpd> <fixture_project>");

    McpFixtureClient client{argv[1]};
    const auto health = client.request("health");
    require(health.find("ok health=ok state=idle lifecycle=running") == 0,
            "daemon reports idle health without project");

    const auto attach = client.request(std::string{"attach "} + argv[2] + " read_only");
    require(attach.find("ok attached token=lmcp_") == 0, "daemon returns local auth token");
    const auto token = tokenFromAttachResponse(attach);

    require(client.request("query project_summary") == "error auth_required",
            "query requires local auth token");
    require(client.request("auth wrong query project_summary") == "error auth_invalid",
            "query rejects invalid local auth token");
    const auto summary = client.request("auth " + token + " query project_summary");
    require(summary.find("ok {") == 0 &&
                summary.find("\"name\":\"Empty Fixture\"") != std::string::npos,
            "query accepts valid local auth token");
    require(client.request("auth " + token + " can_mutate") == "ok false",
            "read-only scoped capability cannot mutate");
    require(client.request("shell ls") == "error forbidden_shell_execution",
            "daemon denies shell execution");
    require(client.request("auth " + token + " connection_lost") == "ok interrupted",
            "daemon records interrupted connection");
    require(client.request("auth wrong recover") == "error auth_invalid",
            "daemon rejects invalid recovery token");
    const auto recovered = client.request("auth " + token + " recover");
    require(recovered.find("ok recovered token=lmcp_") == 0, "daemon recovers interrupted session");
    const auto recoveredToken = recovered.substr(std::string{"ok recovered token="}.size());
    require(client.request("auth " + token + " query project_summary") == "error auth_invalid",
            "daemon rotates auth token after recovery");
    require(client.request("auth " + recoveredToken + " query project_summary").find("ok {") == 0,
            "daemon accepts recovered auth token");
    require(client.request("auth " + recoveredToken + " detach") == "ok detached",
            "daemon detaches authenticated session");

    return 0;
}
