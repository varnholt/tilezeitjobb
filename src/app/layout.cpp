#include "app/editor.hpp"
#include <algorithm>
#include <imgui.h>
#include <stdexcept>

namespace tzj {
namespace {
constexpr ImGuiWindowFlags fixed_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                         ImGuiWindowFlags_NoSavedSettings |
                                         ImGuiWindowFlags_NoDocking;
ImVec4 paint_color(Byte index) {
    auto c = nes_colors[index];
    return {c.r / 255.f, c.g / 255.f, c.b / 255.f, 1};
}
} // namespace
void Editor::place_panel(const PanelRect &rect) {
    ImGui::SetNextWindowDockID(0);
    ImGui::SetNextWindowPos({rect.x, rect.y}, ImGuiCond_Always);
    ImGui::SetNextWindowSize({rect.width, rect.height}, ImGuiCond_Always);
}
const char *Editor::tool_label() const {
    switch (tool_) {
    case Tool::Pencil:
        return "Pencil";
    case Tool::Eraser:
        return "Eraser";
    case Tool::Fill:
        return "Fill";
    case Tool::Palette:
        return "Color blocks";
    case Tool::Picker:
        return "Pick color";
    case Tool::Sprite:
        return "Place sprite";
    case Tool::Move:
        return "Move sprite";
    case Tool::Select:
        return "Select";
    case Tool::Stamp:
        return "Captured brush";
    case Tool::TileBrush:
        return "Paint tile";
    case Tool::Pan:
        return "Pan";
    }
    return "Pencil";
}
void Editor::start_workspace(std::string_view name) {
    if (name == "draw")
        choose_workspace(Workspace::Draw);
    else if (name == "tiles")
        choose_workspace(Workspace::Tiles);
    else if (name == "sprites")
        choose_workspace(Workspace::Sprites);
    else if (name == "animate")
        choose_workspace(Workspace::Animate);
    else if (name == "export")
        choose_workspace(Workspace::Export);
    else
        throw std::runtime_error("Unknown workspace; use draw, tiles, sprites, animate or export");
}
void Editor::new_project() {
    project = Project{};
    project_path_.clear();
    frame_ = 0;
    layer_ = -1;
    selection_ = {};
    tile_selected_ = false;
    history_.clear();
    dirty_ = false;
    refresh_ = true;
    choose_workspace(Workspace::Draw);
}
void Editor::selection_inspector() {
    ImGui::SeparatorText("Active tool");
    ImGui::TextUnformatted(tool_label());
    if (guidance_) {
        const char *help =
            tool_ == Tool::Select ? "Drag on the canvas to select pixels. Capture the selection to "
                                    "rotate, flip or paint with it."
            : tool_ == Tool::Stamp
                ? "Paint on the canvas with your captured brush. Its options are below."
            : tool_ == Tool::Fill ? "Click a region to fill it with the selected paint color."
            : tool_ == Tool::Eraser
                ? "Drag to restore the background color. Right-drag with Pencil also erases."
            : tool_ == Tool::Picker ? "Click the picture to choose its color and color set."
            : tool_ == Tool::Pan ? "Drag to move around the canvas. The mouse wheel changes zoom."
                                 : "Choose a paint color on the left, then drag on the canvas. "
                                   "Each 16x16 block shares a color set.";
        ImGui::TextWrapped("%s", help);
    }
    if (selection_.width > 0 || !clipboard_.empty()) {
        ImGui::SeparatorText("Selection / brush");
        brush_controls(true);
    } else {
        ImGui::SeparatorText("Selection");
        ImGui::TextDisabled("Nothing selected");
        if (ImGui::Button("Select an area", {-1, 30}))
            tool_ = Tool::Select;
    }
    ImGui::SeparatorText("Picture");
    ImGui::Text("%d x %d pixels", project.width, project.height);
    ImGui::Text("Frame %zu of %zu", frame_ + 1, project.frames.size());
    if (!project.reference.empty())
        ImGui::Text("Source: %d x %d", project.reference.width, project.reference.height);
    if (ImGui::CollapsingHeader("NES limits")) {
        if (compiled_) {
            ImGui::Text("Background tiles: %d / 256", compiled_->background_tiles);
            ImGui::Text("Sprites: %d / 64", compiled_->sprite_count);
        } else
            ImGui::TextWrapped("%s", compile_error_.c_str());
    }
    if (ImGui::CollapsingHeader("Small preview")) {
        float scale = std::min(160.f / project.width, 140.f / project.height);
        ImGui::Image(static_cast<ImTextureID>(texture_),
                     {project.width * scale, project.height * scale});
    }
}
void Editor::guided_workspace() {
    auto *viewport = ImGui::GetMainViewport();
    auto pos = viewport->WorkPos;
    auto size = viewport->WorkSize;
    constexpr float gap = 8, bar = 58, footer = 28;
    float left = std::clamp(size.x * .20f, 250.f, 300.f),
          right = std::clamp(size.x * .23f, 280.f, 350.f);
    if (size.x < 1000) {
        left = 230;
        right = 250;
    }
    float y = pos.y + bar + gap, height = std::max(160.f, size.y - bar - footer - gap * 2);
    float center = std::max(200.f, size.x - left - right - gap * 4);
    float timeline_height = workspace_ == Workspace::Animate ? std::min(180.f, height * .24f) : 0;
    paint_rect_ = {pos.x + gap, y, left, height};
    canvas_rect_ = {pos.x + left + gap * 2, y, center,
                    height - (timeline_height ? timeline_height + gap : 0)};
    inspector_rect_ = {pos.x + size.x - right - gap, y, right, height};
    timeline_rect_ = {canvas_rect_.x, y + canvas_rect_.height + gap, center, timeline_height};
    place_panel({pos.x + gap, pos.y, size.x - gap * 2, bar});
    ImGui::Begin("Action bar", nullptr, fixed_flags | ImGuiWindowFlags_NoScrollbar);
    if (ImGui::Button("New", {48, 32}))
        guard([this] { new_project(); });
    ImGui::SameLine();
    if (ImGui::Button("Open", {55, 32}))
        guard([this] { file_dialog(FileAction::Open); });
    ImGui::SameLine();
    if (ImGui::Button("Save", {55, 32}))
        save();
    ImGui::SameLine();
    if (ImGui::Button("Import", {65, 32}))
        file_dialog(workspace_ == Workspace::Sprites ? FileAction::Sprites
                                                     : FileAction::Background);
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{.12f, .37f, .34f, 1});
    if (ImGui::Button("Export NES", {105, 32}))
        file_dialog(FileAction::Export);
    ImGui::PopStyleColor();
    ImGui::SameLine(0, 24);
    const char *modes[]{"Draw pixels", "Build tile maps", "Place sprites", "Animate"};
    int mode = workspace_ == Workspace::Tiles     ? 1
               : workspace_ == Workspace::Sprites ? 2
               : workspace_ == Workspace::Animate ? 3
                                                  : 0;
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Editing");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(190);
    if (ImGui::Combo("##editing-mode", &mode, modes, 4)) {
        choose_workspace(mode == 1   ? Workspace::Tiles
                         : mode == 2 ? Workspace::Sprites
                         : mode == 3 ? Workspace::Animate
                                     : Workspace::Draw);
    }
    ImGui::SameLine(0, 20);
    if (ImGui::Button(fullscreen_ ? "Windowed (F11)" : "Fullscreen (F11)", {170, 32}))
        fullscreen_ = !fullscreen_;
    ImGui::End();

    place_panel(paint_rect_);
    ImGui::Begin("Paint", nullptr, fixed_flags);
    tools(true);
    ImGui::Spacing();
    palettes(true);
    ImGui::End();
    canvas();
    place_panel(inspector_rect_);
    ImGui::Begin("Inspector", nullptr, fixed_flags);
    if (workspace_ == Workspace::Tiles) {
        ImGui::SeparatorText("Tile map");
        tiles(true);
    } else if (workspace_ == Workspace::Sprites) {
        ImGui::SeparatorText("Sprite objects");
        float object_height = std::max(220.f, ImGui::GetContentRegionAvail().y * .6f);
        ImGui::BeginChild("sprite-properties", {0, object_height});
        layers(true);
        ImGui::EndChild();
        ImGui::SeparatorText("Sprite sheet");
        sprite_sheet(true);
    } else if (workspace_ == Workspace::Export)
        hardware(true);
    else
        selection_inspector();
    ImGui::End();
    if (workspace_ == Workspace::Animate)
        timeline();

    place_panel({pos.x + gap, pos.y + size.y - footer, size.x - gap * 2, footer});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{8, 4});
    ImGui::Begin("Status", nullptr, fixed_flags | ImGuiWindowFlags_NoScrollbar);
    ImGui::TextUnformatted(status_.c_str());
    ImGui::SameLine(0, 20);
    ImGui::TextDisabled("%d x %d | Frame %zu | %s%s", project.width, project.height, frame_ + 1,
                        project_path_.empty() ? "Untitled"
                                              : path_to_utf8(project_path_.filename()).c_str(),
                        dirty_ ? " *" : "");
    ImGui::End();
    ImGui::PopStyleVar();
}
} // namespace tzj
