#include "app/editor.hpp"
#include "app/ui_help.hpp"
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
    case Tool::Line:
        return "Line";
    case Tool::Rectangle:
        return "Rectangle";
    case Tool::Ellipse:
        return "Ellipse";
    case Tool::Polyline:
        return "Polyline";
    case Tool::Polygon:
        return "Polygon";
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
void Editor::tool_icon(Tool tool, float x, float y) const {
    auto *draw = ImGui::GetWindowDrawList();
    auto c = ImGui::GetColorU32(ImGuiCol_Text);
    auto p = [&](float a, float b) { return ImVec2{x + a, y + b}; };
    auto line = [&](float a, float b, float d, float e) {
        draw->AddLine(p(a, b), p(d, e), c, 1.5f);
    };
    auto box = [&](float a, float b, float d, float e) {
        draw->AddRect(p(a, b), p(d, e), c, 0, 0, 1.5f);
    };
    switch (tool) {
    case Tool::Pencil:
        line(2, 14, 5, 8);
        line(5, 8, 12, 1);
        line(12, 1, 16, 5);
        line(16, 5, 9, 12);
        line(9, 12, 2, 14);
        line(5, 8, 9, 12);
        break;
    case Tool::Eraser:
        line(2, 10, 10, 2);
        line(10, 2, 16, 8);
        line(16, 8, 9, 15);
        line(9, 15, 6, 15);
        line(6, 15, 2, 10);
        line(6, 6, 12, 12);
        break;
    case Tool::Line:
        line(2, 14, 16, 2);
        break;
    case Tool::Rectangle:
        box(2, 3, 16, 14);
        break;
    case Tool::Ellipse:
        draw->AddEllipse(p(9, 9), {7, 5.5f}, c, 0, 0, 1.5f);
        break;
    case Tool::Polyline:
        line(1, 13, 5, 3);
        line(5, 3, 11, 12);
        line(11, 12, 17, 4);
        break;
    case Tool::Polygon:
        line(2, 13, 5, 2);
        line(5, 2, 16, 6);
        line(16, 6, 12, 15);
        line(12, 15, 2, 13);
        break;
    case Tool::Fill:
        line(2, 8, 8, 2);
        line(8, 2, 14, 8);
        line(14, 8, 8, 14);
        line(8, 14, 2, 8);
        line(3, 8, 13, 8);
        line(5, 0, 10, 5);
        draw->AddCircleFilled(p(16, 14), 2, c);
        break;
    case Tool::Picker:
        line(2, 16, 5, 10);
        line(5, 10, 13, 2);
        line(13, 2, 16, 5);
        line(16, 5, 8, 13);
        line(8, 13, 2, 16);
        line(10, 2, 16, 8);
        break;
    case Tool::Select:
        for (int n = 1; n < 16; n += 5) {
            line(float(n), 2, float(n + 2), 2);
            line(float(n), 15, float(n + 2), 15);
            line(1, float(n), 1, float(n + 2));
            line(16, float(n), 16, float(n + 2));
        }
        break;
    case Tool::Sprite:
        box(3, 2, 15, 15);
        box(5, 5, 6, 6);
        box(12, 5, 13, 6);
        line(6, 11, 12, 11);
        break;
    case Tool::Move:
        line(9, 1, 9, 17);
        line(1, 9, 17, 9);
        line(9, 1, 6, 4);
        line(9, 1, 12, 4);
        line(1, 9, 4, 6);
        line(1, 9, 4, 12);
        line(17, 9, 14, 6);
        line(17, 9, 14, 12);
        line(9, 17, 6, 14);
        line(9, 17, 12, 14);
        break;
    case Tool::Stamp:
        box(6, 1, 12, 10);
        box(2, 11, 16, 15);
        line(1, 17, 17, 17);
        break;
    case Tool::TileBrush:
    case Tool::Palette:
        box(1, 1, 17, 17);
        line(9, 1, 9, 17);
        line(1, 9, 17, 9);
        if (tool == Tool::Palette)
            draw->AddRectFilled(p(10, 2), p(16, 8), c);
        break;
    case Tool::Pan:
        line(5, 9, 5, 3);
        line(5, 3, 7, 3);
        line(7, 3, 7, 1);
        line(7, 1, 9, 1);
        line(9, 1, 9, 3);
        line(9, 3, 12, 3);
        line(12, 3, 12, 5);
        line(12, 5, 15, 5);
        line(15, 5, 15, 12);
        line(15, 12, 12, 16);
        line(12, 16, 6, 16);
        line(6, 16, 1, 10);
        line(1, 10, 3, 8);
        line(3, 8, 5, 9);
        break;
    }
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
    welcome_ = false;
    cancel_drawing();
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
void Editor::add_blank_sprite() {
    cancel_drawing();
    checkpoint();
    Sprite sprite;
    sprite.name = "Sprite " + std::to_string(project.frames[frame_].sprites.size() + 1);
    sprite.x = 120;
    sprite.y = 112;
    sprite.palette = palette_;
    project.frames[frame_].sprites.push_back(sprite);
    choose_workspace(Workspace::Sprites);
    layer_ = int(project.frames[frame_].sprites.size()) - 1;
    palette_group_ = 1;
    tool_ = Tool::Pencil;
    slot_ = 1;
    brush_ = 1;
    zoom_ = 12;
    grid_ = 1;
    fit_countdown_ = -1;
    focus_sprite_ = true;
    changed();
    status_ = "Blank 8x8 sprite ready. Choose a color, then draw in the center or pixel grid.";
}
void Editor::next_steps() {
    if (!ui::CollapsingHeader("What next?", ImGuiTreeNodeFlags_DefaultOpen))
        return;
    ImGui::TextWrapped(
        workspace_ == Workspace::Sprites
            ? "1. Your background stays underneath. Add a sprite below, then choose its paint "
              "colors."
            : "1. Pick a color on the left. Draw on the large canvas in the middle.");
    ImGui::TextWrapped(workspace_ == Workspace::Sprites
                           ? "2. A sprite has 3 paint colors; its first slot is transparent. Start "
                             "with one 8x8 sprite."
                           : "2. Each 16x16 area shares 4 colors. Changing its color set affects "
                             "that whole area.");
    ImGui::TextWrapped(
        "3. Save keeps an editable project. Export NES makes a ROM you can open in an emulator.");
    ui::TextDisabled("Ctrl+Z undoes your last drawing.");
    if (!project.reference.empty() && ui::Button("Fit imported picture's colors", {-1, 28})) {
        checkpoint();
        project.quantize(frame_, true);
        changed();
    }
    if (ui::Button("Save project", {-1, 28}))
        save();
    if (ui::Button("Export for NES", {-1, 28}))
        file_dialog(FileAction::Export);
    ImGui::Spacing();
}
void Editor::start_screen() {
    if (!welcome_)
        return;
    if (!ImGui::IsPopupOpen("Start a picture"))
        ImGui::OpenPopup("Start a picture");
    auto *viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Always, {.5f, .5f});
    ImGui::SetNextWindowSize({620, 0}, ImGuiCond_Always);
    if (ImGui::BeginPopupModal("Start a picture", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ui::TextUnformatted("Start with the background, then add sprites on top.");
        ImGui::TextWrapped(
            "Start small. Choose a color, draw some pixels, then save your project.");
        ImGui::Spacing();
        if (ui::Button("Draw a background", {-1, 38})) {
            new_project();
            status_ = "Choose one of the paint colors on the left, then draw in the center.";
            ImGui::CloseCurrentPopup();
        }
        ImGui::TextWrapped("A blank 256x240 NES screen, with four ready-to-use color sets.");
        if (ui::Button("Create a sprite", {-1, 38})) {
            new_project();
            add_blank_sprite();
            ImGui::CloseCurrentPopup();
        }
        ImGui::TextWrapped("Start a tiny 8x8 character or object. Three colors plus transparency.");
        if (ui::Button("Import a picture", {-1, 38})) {
            new_project();
            ImGui::CloseCurrentPopup();
            file_dialog(FileAction::Background);
        }
        ImGui::TextWrapped("Convert a PNG, BMP or JPEG to NES colors. Larger images can be cropped "
                           "or positioned.");
        ImGui::Separator();
        if (ui::Button("Explore the example", {285, 32})) {
            set_demo();
            choose_workspace(Workspace::Draw);
            status_ = "Example picture: choose a color, paint over it, and try Ctrl+Z.";
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ui::Button("Open a saved project", {285, 32})) {
            welcome_ = false;
            ImGui::CloseCurrentPopup();
            file_dialog(FileAction::Open);
        }
        if (ui::Button("Keep current picture", {-1, 28})) {
            welcome_ = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}
void Editor::selection_inspector() {
    if (guidance_)
        next_steps();
    ui::SeparatorText("Active tool");
    ui::TextUnformatted(tool_label());
    if (guidance_) {
        const char *help =
            path_tool() ? "Click to place points. Enter or right-click finishes; Backspace removes "
                          "the last point; Esc cancels. Click the first point to close a polygon."
            : shape_tool()
                ? "Drag to preview a shape; release to draw. Shift locks lines to 45-degree "
                  "steps or makes squares/circles. Esc cancels. Right-drag erases."
            : tool_ == Tool::Select
                ? "Drag on the canvas to select pixels. Capture the selection to "
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
        ui::SeparatorText("Selection / brush");
        brush_controls(true);
    } else {
        ui::SeparatorText("Selection");
        ui::TextDisabled("Nothing selected");
        if (ui::Button("Select an area", {-1, 30}))
            tool_ = Tool::Select;
    }
    ui::SeparatorText("Picture");
    ui::Text("%d x %d pixels", project.width, project.height);
    ui::Text("Frame %zu of %zu", frame_ + 1, project.frames.size());
    if (!project.reference.empty())
        ui::Text("Source: %d x %d", project.reference.width, project.reference.height);
    if (ui::CollapsingHeader("NES limits")) {
        if (compiled_) {
            ui::Text("Background tiles: %d / 256", compiled_->background_tiles);
            ui::Text("Sprites: %d / 64", compiled_->sprite_count);
        } else
            ImGui::TextWrapped("%s", compile_error_.c_str());
    }
    if (ui::CollapsingHeader("Small preview")) {
        float scale = std::min(160.f / project.width, 140.f / project.height);
        ui::Image(static_cast<ImTextureID>(texture_),
                  {project.width * scale, project.height * scale});
    }
}
void Editor::guided_workspace() {
    auto *viewport = ImGui::GetMainViewport();
    auto pos = viewport->WorkPos;
    auto size = viewport->WorkSize;
    constexpr float gap = 8, bar = 96, footer = 28;
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
    if (ui::Button("New", {48, 32}))
        guard([this] { show_start(); });
    ImGui::SameLine();
    if (ui::Button("Open", {55, 32}))
        guard([this] { file_dialog(FileAction::Open); });
    ImGui::SameLine();
    if (ui::Button("Save", {55, 32}))
        save();
    ImGui::SameLine();
    if (ui::Button(workspace_ == Workspace::Sprites ? "Import sprites" : "Import background",
                   {155, 32}))
        file_dialog(workspace_ == Workspace::Sprites ? FileAction::Sprites
                                                     : FileAction::Background);
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{.12f, .37f, .34f, 1});
    if (ui::Button("Export NES", {105, 32}))
        file_dialog(FileAction::Export);
    ImGui::PopStyleColor();
    ImGui::SameLine(0, 20);
    if (ui::Button(fullscreen_ ? "Windowed (F11)" : "Fullscreen (F11)", {170, 32}))
        fullscreen_ = !fullscreen_;
    ImGui::Spacing();
    auto stage = [&](const char *label, Workspace target, bool active) {
        if (active)
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{.12f, .37f, .34f, 1});
        if (ui::Button(label, {150, 28}))
            choose_workspace(target);
        if (active)
            ImGui::PopStyleColor();
    };
    stage("1. Background", Workspace::Draw,
          workspace_ == Workspace::Draw || workspace_ == Workspace::Tiles);
    ImGui::SameLine();
    ui::TextDisabled(">");
    ImGui::SameLine();
    stage("2. Sprites", Workspace::Sprites, workspace_ == Workspace::Sprites);
    ImGui::SameLine();
    ui::TextDisabled(">");
    ImGui::SameLine();
    stage("3. Animate", Workspace::Animate, workspace_ == Workspace::Animate);
    ImGui::SameLine();
    ui::TextDisabled(">");
    ImGui::SameLine();
    stage("4. Export", Workspace::Export, workspace_ == Workspace::Export);
    if (workspace_ == Workspace::Draw || workspace_ == Workspace::Tiles) {
        ImGui::SameLine(0, 24);
        if (ui::Button(workspace_ == Workspace::Tiles ? "Return to drawing"
                                                      : "Edit background tiles",
                       {200, 28}))
            choose_workspace(workspace_ == Workspace::Tiles ? Workspace::Draw : Workspace::Tiles);
    }
    ImGui::End();

    place_panel(paint_rect_);
    ImGui::Begin("Paint", nullptr, fixed_flags);
    palettes(true);
    ImGui::Spacing();
    tools(true);
    ImGui::End();
    canvas();
    place_panel(inspector_rect_);
    ImGui::Begin("Inspector", nullptr, fixed_flags);
    if (workspace_ == Workspace::Tiles) {
        ui::SeparatorText("Tile map");
        tiles(true);
    } else if (workspace_ == Workspace::Sprites) {
        if (guidance_)
            next_steps();
        ui::SeparatorText("Sprite objects");
        float object_height = std::max(220.f, ImGui::GetContentRegionAvail().y - 48.f);
        ImGui::BeginChild("sprite-properties", {0, object_height});
        layers(true);
        ImGui::EndChild();
        if (ui::CollapsingHeader("Sprite sheet (optional)"))
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
    ui::TextUnformatted(status_.c_str());
    ImGui::SameLine(0, 20);
    ui::TextDisabled("%d x %d | Frame %zu | %s%s", project.width, project.height, frame_ + 1,
                     project_path_.empty() ? "Untitled"
                                           : path_to_utf8(project_path_.filename()).c_str(),
                     dirty_ ? " *" : "");
    ImGui::End();
    ImGui::PopStyleVar();
}
} // namespace tzj
