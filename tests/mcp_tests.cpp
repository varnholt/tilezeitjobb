#include "core/nes.hpp"
#include "mcp/server.hpp"
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <thread>
using namespace tzj;
using mcp::Json;
namespace {
void require(bool condition, const char *message) {
    if (!condition)
        throw std::runtime_error(message);
}
Json request(int id, const char *method, Json params = Json::object()) {
    return {{"jsonrpc", "2.0"}, {"id", id}, {"method", method}, {"params", params}};
}
} // namespace
int main(int argc, char **argv) {
    try {
        Project project;
        project.ensure_layers(0);
        History history;
        std::size_t frame{};
        bool edited{};
        auto call = [&](const std::string &name, const Json &args) {
            return mcp::execute(project, history, frame, edited, name, args);
        };
        if (argc == 2 && std::string_view(argv[1]) == "--serve") {
            mcp::Server server;
            server.start();
            std::cout << Json{{"port", server.port()}, {"token", server.token()}}.dump()
                      << std::endl;
            const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(45);
            while (std::chrono::steady_clock::now() < end) {
                server.poll(call);
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
            }
            server.stop();
            return 0;
        }
        mcp::Session session;
        require(session.receive(request(1, "tools/list"), call).contains("error"),
                "Tools must require initialization");
        auto init =
            session.receive(request(2, "initialize",
                                    {{"protocolVersion", "2025-11-25"},
                                     {"capabilities", Json::object()},
                                     {"clientInfo", {{"name", "tests"}, {"version", "1"}}}}),
                            call);
        require(init["result"]["capabilities"].contains("tools"), "Missing tool capability");
        require(session.receive({{"jsonrpc", "2.0"}, {"method", "notifications/initialized"}}, call)
                    .is_null(),
                "Notification must not produce a response");
        auto listed = session.receive(request(3, "tools/list"), call);
        require(listed["result"]["tools"].size() >= 10, "Tool catalog incomplete");
        require(session.receive(request(4, "bogus"), call)["error"]["code"] == -32601,
                "Unknown methods need a protocol error");
        require(session.receive(Json::array(), call)["error"]["code"] == -32600,
                "Invalid RPC must be rejected");
        call("create_layer", {{"name", "MCP paint"}});
        require(edited && project.frames[0].layers.size() == 2, "Layer creation failed");
        const auto before = project;
        auto failure = session.receive(
            request(5, "tools/call",
                    {{"name", "draw_pixels"},
                     {"arguments", {{"layer", 1}, {"pixels", {{1, 1, 2}, {9999, 1, 3}}}}}}),
            call);
        require(failure["result"]["isError"] == true && project == before,
                "Rejected edit must be atomic");
        call("draw_pixels", {{"layer", 1}, {"pixels", {{1, 1, 2}, {2, 1, 3}}}});
        require(project.frames[0].layers[1].pixels[257] == 2 &&
                    project.frames[0].layers[1].pixels[258] == 3,
                "Painting failed");
        call("undo", Json::object());
        require(project == before, "One undo must remove the entire pixel batch");
        call("redo", Json::object());
        require(project.frames[0].layers[1].pixels[257] == 2, "Redo failed");
        call("set_layer", {{"layer", 1}, {"locked", true}});
        auto locked = project;
        bool refused = false;
        try {
            call("draw_pixels", {{"layer", 1}, {"pixels", {{1, 1, 1}}}});
        } catch (const std::runtime_error &) {
            refused = true;
            require(project == locked, "Locked painting changed state");
        }
        require(refused, "Locked painting succeeded");
        // Failure must not introduce an extra history entry.
        call("undo", Json::object());
        require(!project.frames[0].layers[1].locked, "Rejected operation polluted undo history");
        Image image{16, 16, std::vector<Byte>(16 * 16 * 4, 255)};
        auto image_index = project.add_image_layer(0, image, "Original");
        auto original = project.frames[0].layers[image_index];
        call("set_modifiers", {{"layer", image_index}, {"dither", 5}, {"strength", 37}});
        require(project.frames[0].layers[image_index].source == original.source,
                "Modifiers changed original pixels");
        call("duplicate_layer", {{"layer", image_index}, {"paintable", true}});
        require(project.frames[0].layers.back().kind == LayerKind::Paint,
                "Paintable duplicate must bake to paint");
        auto png = call("render_preview", Json::object());
        require(png["content"][0]["mimeType"] == "image/png" &&
                    png["content"][0]["data"].get<std::string>().starts_with("iVBORw0KGgo"),
                "Preview must contain actual PNG bytes");
        auto invalid =
            session.receive(request(6, "tools/call",
                                    {{"name", "set_modifiers"},
                                     {"arguments", {{"layer", image_index}, {"dither", 99}}}}),
                            call);
        require(invalid["result"]["isError"] == true, "Invalid enum accepted");
        auto wrongtype = session.receive(
            request(7, "tools/call",
                    {{"name", "set_layer"}, {"arguments", {{"layer", 1}, {"locked", "yes"}}}}),
            call);
        require(wrongtype["result"]["isError"] == true, "Wrong argument type accepted");
        auto project_info = call("get_project", Json::object());
        require(!edited && project_info["structuredContent"]["layers"].size() == 4,
                "Inspection must not edit");
        call("add_sprite",
             {{"name", "Highlight"}, {"x", 8}, {"y", 16}, {"pixels", std::vector<int>(64, 1)}});
        require(edited && project.frames[0].sprites.front().pixels[0] == 1,
                "Sprite creation failed");
        require(call("validate_nes", Json::object())["structuredContent"]["valid"] == true,
                "Valid artwork rejected");
        mcp::Server server;
        require(!server.running(), "Server must start disabled");
        server.start();
        auto token = server.token();
        require(server.running() && server.port() != 0 && token.size() == 32,
                "Server startup failed");
        server.stop();
        require(!server.running() && server.port() == 0 && server.token().empty(),
                "Stop must clear connection");
        server.start();
        require(server.token() != token, "Restart must rotate token");
        server.stop();
        std::cout
            << "MCP protocol, transactions, undo, locks, modifiers, preview and lifecycle passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
