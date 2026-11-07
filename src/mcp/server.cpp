#include "mcp/server.hpp"
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#endif
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <deque>
#include <future>
#include <iostream>
#include <mutex>
#include <random>
#include <stdexcept>
#include <thread>

namespace tzj::mcp {
namespace {
#ifdef _WIN32
using Socket = SOCKET;
constexpr Socket invalid_socket = INVALID_SOCKET;
void close_socket(Socket s) {
    closesocket(s);
}
struct Network {
    Network() {
        WSADATA data;
        if (WSAStartup(MAKEWORD(2, 2), &data))
            throw std::runtime_error("Cannot initialize local sockets");
    }
    ~Network() { WSACleanup(); }
};
#else
using Socket = int;
constexpr Socket invalid_socket = -1;
void close_socket(Socket s) {
    close(s);
}
struct Network {};
#endif
struct Connection {
    Socket socket{invalid_socket};
    ~Connection() {
        if (socket != invalid_socket)
            close_socket(socket);
    }
};
bool wait_readable(Socket socket, long milliseconds) {
    fd_set read;
    FD_ZERO(&read);
    FD_SET(socket, &read);
    timeval timeout{milliseconds / 1000, (milliseconds % 1000) * 1000};
    return select(int(socket) + 1, &read, nullptr, nullptr, &timeout) > 0;
}
void socket_timeouts(Socket socket) {
#ifdef _WIN32
    DWORD timeout = 2000;
#else
    timeval timeout{2, 0};
#endif
    setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char *>(&timeout),
               sizeof(timeout));
}
bool send_line(Socket socket, const std::string &message) {
    const auto line = message + '\n';
    std::size_t sent = 0;
    while (sent < line.size()) {
#ifdef MSG_NOSIGNAL
        constexpr int flags = MSG_NOSIGNAL;
#else
        constexpr int flags = 0;
#endif
        auto count = send(socket, line.data() + sent,
                          int(std::min<std::size_t>(line.size() - sent, 65536)), flags);
        if (count <= 0)
            return false;
        sent += std::size_t(count);
    }
    return true;
}
struct Reader {
    std::string buffer;
    bool line(Socket socket, std::string &out, const std::atomic_bool *running = nullptr,
              int timeout_seconds = 0) {
        const auto deadline =
            std::chrono::steady_clock::now() + std::chrono::seconds(timeout_seconds);
        while (!running || running->load()) {
            auto end = buffer.find('\n');
            if (end != std::string::npos) {
                out = buffer.substr(0, end);
                buffer.erase(0, end + 1);
                return true;
            }
            if (buffer.size() > 4 * 1024 * 1024)
                return false;
            if (timeout_seconds && std::chrono::steady_clock::now() >= deadline)
                return false;
            if (!wait_readable(socket, 100))
                continue;
            char bytes[8192];
            auto count = recv(socket, bytes, sizeof(bytes), 0);
            if (count <= 0)
                return false;
            buffer.append(bytes, std::size_t(count));
        }
        return false;
    }
};
} // namespace
struct Server::Impl {
    Network network;
    std::atomic_bool running{};
    Socket listener{invalid_socket};
    unsigned short port{};
    std::string token;
    std::thread thread;
    struct Request {
        std::string name;
        Json arguments;
        std::promise<Json> response;
    };
    std::mutex mutex;
    std::deque<std::shared_ptr<Request>> requests;
    void serve() {
        while (running) {
            if (!wait_readable(listener, 100))
                continue;
            Connection client{accept(listener, nullptr, nullptr)};
            if (client.socket == invalid_socket)
                continue;
            socket_timeouts(client.socket);
            Reader reader;
            std::string line;
            if (!reader.line(client.socket, line, &running, 3) || line != token)
                continue;
            if (!send_line(client.socket, "ok"))
                continue;
            Session session;
            while (running && reader.line(client.socket, line, &running)) {
                Json response;
                try {
                    response = session.receive(
                        Json::parse(line), [&](const std::string &name, const Json &args) {
                            auto request = std::make_shared<Request>();
                            request->name = name;
                            request->arguments = args;
                            auto future = request->response.get_future();
                            {
                                std::lock_guard lock(mutex);
                                requests.push_back(request);
                            }
                            while (running) {
                                if (future.wait_for(std::chrono::milliseconds(100)) ==
                                    std::future_status::ready)
                                    return future.get();
                            }
                            throw std::runtime_error("MCP server stopped");
                        });
                } catch (const std::exception &) {
                    response = {{"jsonrpc", "2.0"},
                                {"id", nullptr},
                                {"error", {{"code", -32700}, {"message", "Invalid JSON"}}}};
                }
                if (!response.is_null() && !send_line(client.socket, response.dump()))
                    break;
            }
        }
    }
};
Server::Server() : impl_(std::make_unique<Impl>()) {}
Server::~Server() {
    stop();
}
void Server::start() {
    if (running())
        return;
    Connection listener{socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)};
    if (listener.socket == invalid_socket)
        throw std::runtime_error("Cannot create MCP connection");
#ifdef _WIN32
    BOOL exclusive = TRUE;
    setsockopt(listener.socket, SOL_SOCKET, SO_EXCLUSIVEADDRUSE,
               reinterpret_cast<const char *>(&exclusive), sizeof(exclusive));
#endif
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (bind(listener.socket, reinterpret_cast<sockaddr *>(&address), sizeof(address)) != 0 ||
        listen(listener.socket, 4) != 0)
        throw std::runtime_error("Cannot listen for local MCP connections");
#ifdef _WIN32
    int length = sizeof(address);
#else
    socklen_t length = sizeof(address);
#endif
    if (getsockname(listener.socket, reinterpret_cast<sockaddr *>(&address), &length) != 0)
        throw std::runtime_error("Cannot find MCP connection port");
    std::random_device random;
    std::string token;
    for (int i = 0; i < 32; ++i)
        token += "0123456789abcdef"[random() & 15];
    impl_->port = ntohs(address.sin_port);
    impl_->token = std::move(token);
    impl_->listener = listener.socket;
    listener.socket = invalid_socket;
    impl_->running = true;
    try {
        impl_->thread = std::thread([this] { impl_->serve(); });
    } catch (...) {
        impl_->running = false;
        close_socket(impl_->listener);
        impl_->listener = invalid_socket;
        throw;
    }
}
void Server::stop() {
    impl_->running = false;
    if (impl_->thread.joinable())
        impl_->thread.join();
    if (impl_->listener != invalid_socket) {
        close_socket(impl_->listener);
        impl_->listener = invalid_socket;
    }
    std::lock_guard lock(impl_->mutex);
    impl_->requests.clear();
    impl_->token.clear();
    impl_->port = 0;
}
bool Server::running() const {
    return impl_->running;
}
unsigned short Server::port() const {
    return impl_->port;
}
std::string Server::token() const {
    return impl_->token;
}
void Server::poll(const Handler &handler) {
    // Limit work per frame to keep painting responsive.
    for (int i = 0; i < 4; ++i) {
        std::shared_ptr<Impl::Request> request;
        {
            std::lock_guard lock(impl_->mutex);
            if (impl_->requests.empty())
                break;
            request = impl_->requests.front();
            impl_->requests.pop_front();
        }
        try {
            request->response.set_value(handler(request->name, request->arguments));
        } catch (...) {
            request->response.set_exception(std::current_exception());
        }
    }
}
int connect_stdio(unsigned short port, const std::string &token) {
    try {
        Network network;
        Connection client{socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)};
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_port = htons(port);
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        if (client.socket == invalid_socket ||
            connect(client.socket, reinterpret_cast<sockaddr *>(&address), sizeof(address)) != 0)
            throw std::runtime_error("Open tilezeitjobb and enable Tools > MCP server first");
        socket_timeouts(client.socket);
        Reader reader;
        std::string line;
        if (!send_line(client.socket, token) || !reader.line(client.socket, line, nullptr, 3) ||
            line != "ok")
            throw std::runtime_error(
                "MCP connection rejected; copy the current configuration from the editor");
        // A separate output reader supports notifications and exits when the editor stops.
        std::thread output([&] {
            while (reader.line(client.socket, line))
                std::cout << line << '\n' << std::flush;
            // stdin may still be blocked in the host; end the dedicated relay process.
            std::_Exit(0);
        });
        output.detach();
        std::string input;
        while (std::getline(std::cin, input)) {
            if (input.size() > 4 * 1024 * 1024 || !send_line(client.socket, input))
                std::_Exit(1);
        }
        // This entry point runs in its own process, before creating any editor resources.
        std::cout.flush();
        std::_Exit(0);
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
} // namespace tzj::mcp
