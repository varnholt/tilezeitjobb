#include "app/editor.hpp"
#include "app/ui_help.hpp"
#include <imgui.h>
namespace tzj {
void Editor::start_mcp() {
    if (!mcp_server_)
        mcp_server_ = std::make_unique<mcp::Server>();
    mcp_server_->start();
    show_mcp_ = true;
    status_ = "MCP server running; Tools > MCP server shows connection details.";
}
void Editor::poll_mcp() {
    if (!mcp_server_ || !mcp_server_->running())
        return;
    // Let local painting and modal edits finish before applying remote commands.
    if (stroke_ || shape_drag_ || tile_edit_stroke_ || !path_.empty() || pending_ || file_popup_ ||
        ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel) ||
        palette_editor_open_ || ImGui::IsMouseDown(0) || ImGui::IsMouseDown(1) ||
        ImGui::GetIO().WantTextInput)
        return;
    mcp_server_->poll([this](const std::string &name, const mcp::Json &arguments) {
        bool edited{};
        auto result = mcp::execute(project, history_, frame_, edited, name, arguments);
        if (edited) {
            cancel_drawing();
            playing_ = false;
            layer_ = -1;
            palette_group_ = 0;
            welcome_ = false;
            selection_ = {};
            tile_selected_ = false;
            changed();
        }
        status_ = "MCP: " + name + (edited ? " (undo available)" : "");
        return result;
    });
}
void Editor::mcp_window() {
    if (!show_mcp_)
        return;
    ImGui::SetNextWindowSize({620, 360}, ImGuiCond_FirstUseEver);
    if (ImGui::Begin("MCP server", &show_mcp_)) {
        const bool running = mcp_server_ && mcp_server_->running();
        ui::TextUnformatted(running ? "Running locally" : "Stopped");
        ImGui::TextWrapped("Connect an MCP client to inspect, edit and export the open project. "
                           "Edits appear here and use the editor's undo history.");
        if (ui::Button(running ? "Stop server" : "Start server")) {
            try {
                if (running) {
                    mcp_server_->stop();
                    status_ = "MCP server stopped.";
                } else
                    start_mcp();
            } catch (const std::exception &e) {
                error_ = e.what();
            }
        }
        ui::help("MCP start/stop", "The server is off at startup. Stopping disconnects clients; "
                                   "restarting generates a new connection token.");
        if (mcp_server_ && mcp_server_->running()) {
            mcp::Json configuration = {{"mcpServers",
                                        {{"tilezeitjobb",
                                          {{"command", path_to_utf8(mcp_executable_)},
                                           {"args",
                                            {"--mcp-connect", std::to_string(mcp_server_->port()),
                                             mcp_server_->token()}}}}}}};
            const auto text = configuration.dump(2);
            ImGui::Separator();
            if (ui::Button("Copy connection configuration"))
                ImGui::SetClipboardText(text.c_str());
            ui::help("MCP configuration",
                     "Copy the stdio server definition into your MCP client's configuration. It "
                     "connects to this running editor instance only.");
            if (ImGui::BeginChild("Connection configuration", {0, 150}, ImGuiChildFlags_Borders))
                ImGui::TextUnformatted(text.c_str());
            ImGui::EndChild();
            ImGui::TextWrapped("Local connection only. Keep this editor open. "
                               "Copy a fresh configuration after restarting the server.");
        }
    }
    ImGui::End();
}
} // namespace tzj
