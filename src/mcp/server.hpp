#pragma once
#include "core/project.hpp"
#include <functional>
#include <memory>
#include <nlohmann/json.hpp>

namespace tzj::mcp {
using Json = nlohmann::json;
using Handler = std::function<Json(const std::string &, const Json &)>;
Json tools();
// Runs on the editor thread. Successful edits create exactly one history entry.
Json execute(Project &, History &, std::size_t &frame, bool &edited, const std::string &name,
             const Json &arguments);
class Session {
    bool initialized_{}, ready_{};

  public:
    // Null means a notification: no response should be sent.
    Json receive(const Json &, const Handler &);
};
class Server {
    struct Impl;
    std::unique_ptr<Impl> impl_;

  public:
    Server();
    ~Server();
    Server(const Server &) = delete;
    Server &operator=(const Server &) = delete;
    void start();
    void stop();
    bool running() const;
    unsigned short port() const;
    std::string token() const;
    // Dispatch pending requests only here, on the editor thread.
    void poll(const Handler &);
};
// Standard MCP stdio transport; relays to the editor's private loopback connection.
int connect_stdio(unsigned short port, const std::string &token);
} // namespace tzj::mcp
