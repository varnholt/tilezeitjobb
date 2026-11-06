#include "app/editor.hpp"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <imgui.h>
#include <imgui_internal.h>
#include <iostream>
#include <tuple>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
EM_JS(void, browser_pick, (int action), { Module.pickFile(action); });
EM_JS(void, browser_download, (const char *path, int bundle),
      { Module.downloadFile(UTF8ToString(path), !!bundle); });
#endif
namespace tzj {
static void focus_panel(const char *name) {
    if (auto *window = ImGui::FindWindowByName(name)) {
        if (window->DockNode && window->DockNode->TabBar)
            window->DockNode->TabBar->NextSelectedTabId = window->TabId;
        ImGui::SetWindowFocus(name);
    }
}
static ImVec4 color(Byte index) {
    auto c = nes_colors.at(index);
    return {c.r / 255.f, c.g / 255.f, c.b / 255.f, 1};
}
static ImTextureID texture_id(unsigned int id) {
    return static_cast<ImTextureID>(id);
}
static void upload(unsigned int &id, const Image &image) {
    if (image.empty()) {
        if (id)
            glDeleteTextures(1, &id);
        id = 0;
        return;
    }
    if (!id)
        glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, 0x812F);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, 0x812F);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image.width, image.height, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 image.rgba.data());
}
Editor::Editor(std::filesystem::path data_directory) : data_directory_(std::move(data_directory)) {
    browse_dir_ = std::filesystem::current_path();
}
Editor::~Editor() {
    for (auto id :
         {texture_, sheet_texture_, reference_texture_, previous_texture_, effect_texture_})
        if (id)
            glDeleteTextures(1, &id);
}
void Editor::changed() {
    dirty_ = true;
    refresh_ = true;
}
void Editor::checkpoint() {
    history_.checkpoint(project);
}
void Editor::guard(std::function<void()> action) {
    if (dirty_)
        pending_ = std::move(action);
    else
        action();
}
void Editor::request_close() {
    guard([this] { quit_ = true; });
}
void Editor::set_demo() {
    welcome_ = false;
    cancel_drawing();
    project = demo_project();
    selected_tile_ = read_tile(project, 0, 0, 0);
    tile_selected_ = true;
    selected_tile_x_ = selected_tile_y_ = 0;
    history_.clear();
    frame_ = 0;
    layer_ = -1;
    dirty_ = false;
    refresh_ = true;
}
void Editor::open(const std::filesystem::path &path) {
    auto loaded = load_project(path);
    welcome_ = false;
    cancel_drawing();
    project = std::move(loaded);
    project_path_ = path;
    frame_ = 0;
    layer_ = -1;
    selection_ = {};
    history_.clear();
    dirty_ = false;
    refresh_ = true;
    tile_selected_ = false;
    status_ = "Opened " + path_to_utf8(path.filename());
}
void Editor::drop(const std::filesystem::path &path) {
    try {
        if (path.extension() == ".tzj")
            guard([this, path] { open(path); });
        else {
            file_action_ = FileAction::Background;
            perform_file(path);
        }
    } catch (const std::exception &e) {
        error_ = e.what();
    }
}
#ifdef __EMSCRIPTEN__
void Editor::browser_file(const std::filesystem::path &path, int action) {
    cancel_drawing();
    if (action == int(FileAction::Open))
        guard([this, path] { open(path); });
    else {
        file_action_ = static_cast<FileAction>(action);
        perform_file(path);
    }
}
#endif
void Editor::file_dialog(FileAction action) {
#ifdef __EMSCRIPTEN__
    if (action == FileAction::Open || action == FileAction::Background ||
        action == FileAction::Sprites) {
        browser_pick(int(action));
        return;
    }
    file_action_ = action;
    std::filesystem::path name = action == FileAction::Export ? "scene.nes"
                                 : action == FileAction::Png  ? "scene.png"
                                 : project_path_.empty()      ? "untitled.tzj"
                                                              : project_path_.filename();
    perform_file(std::filesystem::path("/exports") / name);
    return;
#endif
    file_action_ = action;
    file_popup_ = true;
    std::string initial;
    if (action == FileAction::Save)
        initial = project_path_.empty() ? "untitled.tzj" : path_to_utf8(project_path_.filename());
    if (action == FileAction::Export)
        initial = "scene.nes";
    if (action == FileAction::Png)
        initial = "scene.png";
    std::snprintf(file_path_, sizeof(file_path_), "%s", initial.c_str());
}
void Editor::save(bool as) {
    try {
        if (as || project_path_.empty()) {
            file_dialog(FileAction::Save);
            return;
        }
        save_project(project_path_, project);
#ifdef __EMSCRIPTEN__
        browser_download(path_to_utf8(project_path_).c_str(), 0);
#endif
        dirty_ = false;
        status_ = "Project saved";
    } catch (const std::exception &e) {
        error_ = e.what();
    }
}
void Editor::perform_file(const std::filesystem::path &path) {
    try {
        switch (file_action_) {
        case FileAction::Open:
            open(path);
            break;
        case FileAction::Save:
            save_project(path, project);
#ifdef __EMSCRIPTEN__
            browser_download(path_to_utf8(path).c_str(), 0);
#endif
            project_path_ = path;
            dirty_ = false;
            status_ = "Project saved";
            if (pending_) {
                auto action = std::move(pending_);
                pending_ = {};
                action();
            }
            break;
        case FileAction::Background: {
            auto im = load_image(path);
            checkpoint();
            project.reference = std::move(im);
            project.image_path = path_to_utf8(path);
            project.quantize(frame_);
            changed();
            status_ = "Imported background; use Auto palettes to fit its colors";
            break;
        }
        case FileAction::Sprites: {
            auto im = load_image(path);
            checkpoint();
            project.sprite_sheet = std::move(im);
            project.sprite_path = path_to_utf8(path);
            tile_x_ = tile_y_ = 0;
            changed();
            status_ = "Imported sprite sheet";
            break;
        }
        case FileAction::Export: {
            auto prefix = path;
            if (prefix.extension() == ".nes")
                prefix.replace_extension();
            export_scene(prefix, project, frame_);
#ifdef __EMSCRIPTEN__
            browser_download(path_to_utf8(prefix).c_str(), 1);
#endif
            status_ = "Exported .nes, .chr, .nam, .pal, .oam and .h";
            break;
        }
        case FileAction::Png:
            save_png(path, project.render(frame_, show_sprites_, hardware_preview_));
#ifdef __EMSCRIPTEN__
            browser_download(path_to_utf8(path).c_str(), 0);
#endif
            status_ = "PNG exported";
            break;
        default:
            break;
        }
    } catch (const std::exception &e) {
        error_ = e.what();
    }
}
void Editor::refresh_textures() {
    if (!refresh_)
        return;
    auto image = project.render(frame_, show_sprites_, hardware_preview_);
    if (pal_test_)
        for (int y = 1; y < image.height; y += 2)
            for (int x = 0; x < image.width; ++x)
                for (int c = 0; c < 3; ++c)
                    image.rgba[(y * image.width + x) * 4 + c] =
                        Byte(image.rgba[(y * image.width + x) * 4 + c] * 0.8f);
    upload(texture_, image);
    upload(sheet_texture_, project.sprite_sheet);
    upload(reference_texture_, project.reference);
    if (frame_ > 0)
        upload(previous_texture_, project.render(frame_ - 1));
    else {
        Image empty;
        upload(previous_texture_, empty);
    }
    try {
        compiled_ = compile_scene(project, frame_);
        compile_error_.clear();
    } catch (const std::exception &e) {
        compiled_.reset();
        compile_error_ = e.what();
    }
    tile_catalog_ = tile_catalog(project, frame_);
    if (tile_selected_ &&
        std::none_of(tile_catalog_.begin(), tile_catalog_.end(),
                     [this](const auto &entry) { return entry.pixels == selected_tile_; })) {
        selected_tile_x_ = std::clamp(selected_tile_x_, 0, project.width / 8 - 1);
        selected_tile_y_ = std::clamp(selected_tile_y_, 0, project.height / 8 - 1);
        selected_tile_ = read_tile(project, frame_, selected_tile_x_, selected_tile_y_);
    }
    refresh_ = false;
}
void Editor::dock_layout() {
    if (!advanced_panels_)
        return;
    ImGuiID dock = ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());
    if (layout_built_)
        return;
    layout_built_ = true;
    if (!force_layout_ && ImGui::DockBuilderGetNode(dock) &&
        ImGui::DockBuilderGetNode(dock)->IsSplitNode())
        return;
    force_layout_ = false;
    ImGui::DockBuilderRemoveNode(dock);
    ImGui::DockBuilderAddNode(dock, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dock, ImGui::GetMainViewport()->WorkSize);
    ImGuiID center = dock, left, right, bottom, left_top, right_top;
    ImGui::DockBuilderSplitNode(center, ImGuiDir_Left, 0.21f, &left, &center);
    ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, 0.28f, &right, &center);
    ImGui::DockBuilderSplitNode(center, ImGuiDir_Down, 0.19f, &bottom, &center);
    ImGui::DockBuilderSplitNode(left, ImGuiDir_Up, 0.42f, &left_top, &left);
    ImGui::DockBuilderSplitNode(right, ImGuiDir_Up, 0.50f, &right_top, &right);
    ImGui::DockBuilderDockWindow("Tools", left_top);
    ImGui::DockBuilderDockWindow("Brush", left_top);
    ImGui::DockBuilderDockWindow("Palettes", left);
    ImGui::DockBuilderDockWindow("Canvas", center);
    ImGui::DockBuilderDockWindow("Timeline", bottom);
    ImGui::DockBuilderDockWindow("Layers & sprites", right_top);
    ImGui::DockBuilderDockWindow("Tiles", right_top);
    ImGui::DockBuilderDockWindow("Hardware", right);
    ImGui::DockBuilderDockWindow("Preview", right);
    ImGui::DockBuilderDockWindow("Attributes", right);
    ImGui::DockBuilderDockWindow("Sprite sheet", right);
    ImGui::DockBuilderDockWindow("Palette test effect", right);
    ImGui::DockBuilderFinish(dock);
}
void Editor::choose_workspace(Workspace workspace) {
    cancel_drawing();
    workspace_ = workspace;
    fit_countdown_ = 1;
    if (workspace == Workspace::Tiles) {
        tool_ = Tool::TileBrush;
        layer_ = -1;
        palette_group_ = 0;
        focus_panel("Tiles");
    } else if (workspace == Workspace::Sprites) {
        tool_ = Tool::Sprite;
        palette_group_ = 1;
        focus_panel("Layers & sprites");
        focus_panel("Sprite sheet");
    } else if (workspace == Workspace::Export) {
        tool_ = Tool::Pan;
        focus_panel("Hardware");
    } else {
        tool_ = Tool::Pencil;
        layer_ = -1;
        palette_group_ = 0;
        focus_panel("Tools");
        focus_panel("Preview");
    }
}
void Editor::menus() {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New", "Ctrl+N"))
                guard([this] { show_start(); });
            if (ImGui::MenuItem("Open project...", "Ctrl+O"))
                guard([this] { file_dialog(FileAction::Open); });
            if (ImGui::MenuItem("Save project", "Ctrl+S"))
                save();
            if (ImGui::MenuItem("Save as...", "Ctrl+Shift+S"))
                save(true);
            ImGui::Separator();
            if (ImGui::MenuItem("Import background..."))
                file_dialog(FileAction::Background);
            if (ImGui::MenuItem("Import sprite sheet..."))
                file_dialog(FileAction::Sprites);
            ImGui::Separator();
            if (ImGui::MenuItem("Export NES ROM + assets...", "Ctrl+E"))
                file_dialog(FileAction::Export);
            if (ImGui::MenuItem("Export PNG..."))
                file_dialog(FileAction::Png);
            ImGui::Separator();
            if (ImGui::MenuItem("Quit"))
                request_close();
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Edit")) {
            if (ImGui::MenuItem("Undo", "Ctrl+Z")) {
                if (history_.undo(project)) {
                    frame_ = std::min(frame_, project.frames.size() - 1);
                    layer_ = -1;
                    changed();
                }
            }
            if (ImGui::MenuItem("Redo", "Ctrl+Y")) {
                if (history_.redo(project)) {
                    frame_ = std::min(frame_, project.frames.size() - 1);
                    layer_ = -1;
                    changed();
                }
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Copy selection", "Ctrl+C"))
                copy_selection();
            if (ImGui::MenuItem("Cut selection", "Ctrl+X"))
                copy_selection(true);
            if (ImGui::MenuItem("Paste brush", "Ctrl+V", false, !clipboard_.empty()))
                tool_ = Tool::Stamp;
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Layout")) {
            for (auto [name, w, h] : {std::tuple{"Single screen", 256, 240},
                                      std::tuple{"Two screens horizontally", 512, 240},
                                      std::tuple{"Two screens vertically", 256, 480}}) {
                if (ImGui::MenuItem(name, nullptr, project.width == w && project.height == h)) {
                    checkpoint();
                    project.resize(w, h);
                    changed();
                }
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Fullscreen", "F11", &fullscreen_);
            ImGui::MenuItem("Workflow guidance", nullptr, &guidance_);
            if (ImGui::MenuItem("Advanced layout", nullptr, &advanced_panels_)) {
                layout_built_ = false;
                force_layout_ = true;
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Help")) {
            if (ImGui::MenuItem("Start here..."))
                show_start();
            ImGui::TextUnformatted("tilezeitjobb 2.0 - NES pixel and tile studio");
            ImGui::Separator();
            ImGui::TextUnformatted("P pencil  E eraser  F fill  A attributes  I picker");
            ImGui::TextUnformatted("S sprite  M move  R select  V stamp  T tile  H pan");
            ImGui::TextUnformatted("L line  B rectangle  O ellipse  K polyline  G polygon");
            ImGui::TextUnformatted("Shift constrains shapes; Esc cancels the preview.");
            ImGui::TextUnformatted("1..4 color slot  [ / ] brush size");
            ImGui::TextUnformatted("Middle drag pans; mouse wheel zooms.");
            ImGui::TextUnformatted("ROM export uses the current frame and initial viewport.");
            ImGui::EndMenu();
        }
        ImGui::Separator();
        ImGui::Text("%s%s",
                    project_path_.empty() ? "Untitled"
                                          : path_to_utf8(project_path_.filename()).c_str(),
                    dirty_ ? " *" : "");
        ImGui::EndMainMenuBar();
    }
    if (ImGui::GetIO().WantTextInput)
        return;
    auto &io = ImGui::GetIO();
    if (io.KeyCtrl) {
        if (ImGui::IsKeyPressed(ImGuiKey_N))
            guard([this] { show_start(); });
        if (ImGui::IsKeyPressed(ImGuiKey_S))
            save(io.KeyShift);
        if (ImGui::IsKeyPressed(ImGuiKey_O))
            guard([this] { file_dialog(FileAction::Open); });
        if (ImGui::IsKeyPressed(ImGuiKey_E))
            file_dialog(FileAction::Export);
        if (ImGui::IsKeyPressed(ImGuiKey_Z) && history_.undo(project)) {
            cancel_drawing();
            frame_ = std::min(frame_, project.frames.size() - 1);
            layer_ = -1;
            changed();
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Y) && history_.redo(project)) {
            cancel_drawing();
            frame_ = std::min(frame_, project.frames.size() - 1);
            layer_ = -1;
            changed();
        }
        if (ImGui::IsKeyPressed(ImGuiKey_C))
            copy_selection();
        if (ImGui::IsKeyPressed(ImGuiKey_X))
            copy_selection(true);
        if (ImGui::IsKeyPressed(ImGuiKey_V) && !clipboard_.empty())
            tool_ = Tool::Stamp;
        return;
    }
    for (auto [key, t] : {std::pair{ImGuiKey_P, Tool::Pencil},
                          {ImGuiKey_E, Tool::Eraser},
                          {ImGuiKey_L, Tool::Line},
                          {ImGuiKey_B, Tool::Rectangle},
                          {ImGuiKey_O, Tool::Ellipse},
                          {ImGuiKey_K, Tool::Polyline},
                          {ImGuiKey_G, Tool::Polygon},
                          {ImGuiKey_F, Tool::Fill},
                          {ImGuiKey_A, Tool::Palette},
                          {ImGuiKey_I, Tool::Picker},
                          {ImGuiKey_S, Tool::Sprite},
                          {ImGuiKey_M, Tool::Move},
                          {ImGuiKey_R, Tool::Select},
                          {ImGuiKey_V, Tool::Stamp},
                          {ImGuiKey_T, Tool::TileBrush},
                          {ImGuiKey_H, Tool::Pan}})
        if (ImGui::IsKeyPressed(key)) {
            if (t == Tool::TileBrush)
                workspace_ = Workspace::Tiles;
            else if (t == Tool::Sprite || t == Tool::Move)
                workspace_ = Workspace::Sprites;
            else if (t != Tool::Pan && workspace_ != Workspace::Sprites &&
                     workspace_ != Workspace::Animate)
                workspace_ = Workspace::Draw;
            cancel_drawing();
            tool_ = t;
        }
    for (int i = 0; i < 4; ++i)
        if (ImGui::IsKeyPressed(static_cast<ImGuiKey>(ImGuiKey_1 + i)))
            slot_ = i;
    if (ImGui::IsKeyPressed(ImGuiKey_LeftBracket))
        brush_ = std::max(1, brush_ / 2);
    if (ImGui::IsKeyPressed(ImGuiKey_RightBracket))
        brush_ = std::min(8, brush_ * 2);
}
void Editor::tools(bool embedded) {
    if (!embedded)
        ImGui::Begin("Tools");
    ImGui::SeparatorText(guidance_ ? "2. Choose a tool" : "Tools");
    int column = 0;
    for (auto [name, t] : {std::pair{"Pencil  P", Tool::Pencil},
                           {"Eraser  E", Tool::Eraser},
                           {"Line  L", Tool::Line},
                           {"Rectangle B", Tool::Rectangle},
                           {"Ellipse  O", Tool::Ellipse},
                           {"Polyline K", Tool::Polyline},
                           {"Polygon  G", Tool::Polygon},
                           {"Fill  F", Tool::Fill},
                           {"Set colors A", Tool::Palette},
                           {"Pick color  I", Tool::Picker},
                           {"Place  S", Tool::Sprite},
                           {"Move  M", Tool::Move},
                           {"Select  R", Tool::Select},
                           {"Brush  V", Tool::Stamp},
                           {"Paint tile  T", Tool::TileBrush},
                           {"Pan  H", Tool::Pan}}) {
        bool relevant = t == Tool::Pan || t == Tool::Picker;
        if (workspace_ != Workspace::Tiles)
            relevant |= t == Tool::Line || t == Tool::Rectangle || t == Tool::Ellipse ||
                        t == Tool::Polyline || t == Tool::Polygon;
        if (workspace_ == Workspace::Tiles)
            relevant |= t == Tool::TileBrush || t == Tool::Palette;
        else if (workspace_ == Workspace::Sprites)
            relevant |= t == Tool::Sprite || t == Tool::Move || t == Tool::Pencil ||
                        t == Tool::Eraser || t == Tool::Fill;
        else
            relevant |= t == Tool::Pencil || t == Tool::Eraser || t == Tool::Fill ||
                        t == Tool::Select || t == Tool::Stamp;
        if (embedded && !relevant)
            continue;
        if (column++ % 2)
            ImGui::SameLine();
        float width =
            (ImGui::GetContentRegionAvail().x - (column % 2 ? ImGui::GetStyle().ItemSpacing.x : 0));
        if (column % 2)
            width /= 2;
        std::string id = std::string("##tool-") + name;
        bool selected = ImGui::Selectable(id.c_str(), tool_ == t, 0, {width, 28});
        auto position = ImGui::GetItemRectMin();
        tool_icon(t, position.x + 4, position.y + 5);
        std::string label(name);
        auto shortcut = label.find_last_of(' ');
        if (shortcut != std::string::npos)
            label.resize(shortcut);
        while (!label.empty() && label.back() == ' ')
            label.pop_back();
        ImGui::GetWindowDrawList()->AddText({position.x + 27, position.y + 5},
                                            ImGui::GetColorU32(ImGuiCol_Text), label.c_str());
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", name);
        if (selected) {
            cancel_drawing();
            tool_ = t;
            if (t == Tool::TileBrush || t == Tool::Stamp || t == Tool::Palette) {
                layer_ = -1;
                palette_group_ = 0;
            }
        }
    }
    if (tool_ == Tool::Pencil || tool_ == Tool::Eraser || shape_tool() || path_tool()) {
        ImGui::Spacing();
        ImGui::TextUnformatted("Brush size");
        for (int size : {1, 2, 4, 8}) {
            if (size != 1)
                ImGui::SameLine();
            char label[12];
            std::snprintf(label, sizeof(label), "%d px", size);
            if (ImGui::Selectable(label, brush_ == size, 0, {45, 25}))
                brush_ = size;
        }
    }
    if (shape_tool()) {
        if (tool_ != Tool::Line)
            ImGui::Checkbox("Filled shape", &shape_filled_);
        ImGui::TextWrapped("Drag to preview; release to draw. Shift constrains; Esc cancels.");
    }
    if (path_tool()) {
        if (tool_ == Tool::Polygon)
            ImGui::Checkbox("Filled polygon", &shape_filled_);
        ImGui::TextWrapped("Click points. Enter or right-click finishes; Esc cancels.");
        if (!path_.empty()) {
            ImGui::Text("%zu points | Backspace removes last", path_.size());
            if (ImGui::Button("Finish"))
                finish_path();
            ImGui::SameLine();
            if (ImGui::Button("Cancel"))
                cancel_drawing();
        }
    }
    if (workspace_ != Workspace::Tiles) {
        ImGui::SeparatorText("Symmetry");
        ImGui::Checkbox("Mirror left / right", &mirror_x_);
        ImGui::Checkbox("Mirror top / bottom", &mirror_y_);
        ImGui::TextDisabled(layer_ >= 0 ? "Around sprite center" : "Around canvas center");
        ImGui::TextDisabled("Pencil / eraser / shapes");
    }
    if (ImGui::CollapsingHeader("Canvas options", embedded ? 0 : ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::Checkbox("Show sprites", &show_sprites_))
            refresh_ = true;
        if (ImGui::Checkbox("Preview sprite limits", &hardware_preview_))
            refresh_ = true;
        ImGui::Checkbox("Onion skin", &onion_);
        ImGui::Checkbox("Highlight active color set", &highlight_);
        ImGui::Checkbox("Reference overlay", &reference_overlay_);
        if (ImGui::Checkbox("Scanlines", &pal_test_))
            refresh_ = true;
    }
    if (!embedded) {
        ImGui::TextWrapped("%s", status_.c_str());
        ImGui::End();
    }
}

void Editor::palettes(bool embedded) {
    if (!embedded)
        ImGui::Begin("Palettes");
    ImGui::SeparatorText(guidance_ ? "1. Choose a paint color" : "Paint color");
    ImGui::TextWrapped("Click a color to paint. Right-click a color to edit it.");
    for (int group = 0; group < 2; ++group) {
        if (!advanced_panels_ && group != (workspace_ == Workspace::Sprites ? 1 : 0))
            continue;
        ImGui::PushID(group);
        ImGui::TextUnformatted(group ? "Sprites" : "Background");
        auto &palettes = group ? project.sprite_palettes : project.background;
        if (embedded && guidance_) {
            const char *sets[]{"Color set 1", "Color set 2", "Color set 3", "Color set 4"};
            if (ImGui::Combo("##active-color-set", &palette_, sets, 4)) {
                palette_group_ = group;
                if (group == 0)
                    layer_ = -1;
                else if (layer_ >= 0) {
                    checkpoint();
                    project.frames[frame_].sprites.at(layer_).palette = palette_;
                    changed();
                }
            }
        }
        for (int row = 0; row < 4; ++row) {
            if (embedded && guidance_ && row != palette_)
                continue;
            ImGui::PushID(row);
            bool active = palette_ == row && palette_group_ == group;
            char label[24];
            std::snprintf(label, sizeof(label), "Set %d##palette", row + 1);
            if (ImGui::RadioButton(label, active)) {
                palette_ = row;
                palette_group_ = group;
                if (group == 0)
                    layer_ = -1;
            }
            for (int c = 0; c < 4; ++c) {
                ImGui::SameLine();
                ImGui::PushID(c);
                if (ImGui::ColorButton("##color", color(palettes[row][c]),
                                       ImGuiColorEditFlags_NoTooltip, {32, 25})) {
                    palette_ = row;
                    palette_group_ = group;
                    slot_ = c;
                    if (group == 0)
                        layer_ = -1;
                }
                if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
                    palette_ = row;
                    palette_group_ = group;
                    slot_ = c;
                    ImGui::OpenPopup("Choose NES color");
                }
                if (active && slot_ == c)
                    ImGui::GetWindowDrawList()->AddRect(ImGui::GetItemRectMin(),
                                                        ImGui::GetItemRectMax(),
                                                        IM_COL32(255, 208, 93, 255), 0, 0, 2);
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip(
                        "Color set %d, color %d ($%02X). Click to paint; right-click to edit.",
                        row + 1, c + 1, palettes[row][c]);
                if (ImGui::BeginPopup("Choose NES color")) {
                    for (int n = 0; n < 64; ++n) {
                        if (n % 16)
                            ImGui::SameLine(0, 2);
                        ImGui::PushID(n);
                        if (ImGui::ColorButton("##nes", color(Byte(n)),
                                               ImGuiColorEditFlags_NoTooltip, {23, 23})) {
                            checkpoint();
                            if (c == 0)
                                project.set_backdrop(Byte(n));
                            else
                                palettes[row][c] = Byte(n);
                            changed();
                            ImGui::CloseCurrentPopup();
                        }
                        if (ImGui::IsItemHovered())
                            ImGui::SetTooltip("$%02X%s", n, n == 13 ? " unsafe on hardware" : "");
                        ImGui::PopID();
                    }
                    ImGui::EndPopup();
                }
                ImGui::PopID();
            }
            ImGui::PopID();
        }
        ImGui::PopID();
    }
    ImGui::Separator();
    ImGui::Text("Color set %d / paint color %d", palette_ + 1, slot_ + 1);
    ImGui::TextWrapped("Each 16x16 background block shares one color set.");
    ImGui::Separator();
    if (ImGui::CollapsingHeader("Palette operations")) {
        const char *slots[]{"1", "2", "3"};
        int a = swap_first_ - 1, b = swap_second_ - 1;
        ImGui::SetNextItemWidth(65);
        if (ImGui::Combo("First", &a, slots, 3))
            swap_first_ = a + 1;
        ImGui::SetNextItemWidth(65);
        if (ImGui::Combo("Second", &b, slots, 3))
            swap_second_ = b + 1;
        ImGui::Checkbox("Preserve existing artwork", &preserve_palette_art_);
        if (ImGui::Button("Swap slots")) {
            checkpoint();
            swap_palette_slots(project, palette_group_ == 1, palette_, swap_first_, swap_second_,
                               preserve_palette_art_);
            changed();
        }
        auto &palettes = palette_group_ ? project.sprite_palettes : project.background;
        if (ImGui::Button("Copy colors"))
            copied_palette_ = palettes[palette_];
        ImGui::SameLine();
        ImGui::BeginDisabled(!copied_palette_);
        if (ImGui::Button("Paste colors")) {
            checkpoint();
            for (int i = 1; i < 4; ++i)
                palettes[palette_][i] = (*copied_palette_)[i];
            changed();
        }
        ImGui::EndDisabled();
        ImGui::TextWrapped("Swap preserves artwork across all frames by remapping pixels. Slot "
                           "zero remains the NES backdrop/transparency.");
    }
    if (project.reference.empty()) {
        if (!embedded && ImGui::Button("Import a picture..."))
            file_dialog(FileAction::Background);
        if (!embedded)
            ImGui::End();
        return;
    }
    ImGui::TextUnformatted("IMAGE CONVERSION");
    bool dithering = project.dither;
    if (ImGui::Checkbox("Ordered dithering", &dithering)) {
        checkpoint();
        project.dither = dithering;
        changed();
    }
    if (ImGui::Button("Reapply palettes") && !project.reference.empty()) {
        checkpoint();
        project.quantize(frame_);
        changed();
    }
    ImGui::SameLine();
    if (ImGui::Button("Auto palettes") && !project.reference.empty()) {
        checkpoint();
        project.quantize(frame_, true);
        changed();
    }
    int offsets[]{project.offset_x, project.offset_y};
    if (ImGui::InputInt2("Image offset", offsets)) {
        checkpoint();
        project.offset_x = std::clamp(offsets[0], -4096, 4096);
        project.offset_y = std::clamp(offsets[1], -4096, 4096);
        changed();
    }
    if (ImGui::Button("Clear background paint") && !project.reference.empty()) {
        checkpoint();
        project.frames[frame_].paint_mask.clear();
        project.quantize(frame_);
        changed();
    }
    ImGui::Checkbox("Watch source images", &watch_sources_);
    ImGui::TextWrapped("Reapply and source changes preserve painted pixels. Clear background paint "
                       "restores the reference conversion.");
    if (!embedded)
        ImGui::End();
}
void Editor::copy_selection(bool cut) {
    if (selection_.width <= 0 || selection_.height <= 0)
        return;
    clip_width_ = selection_.width;
    clip_height_ = selection_.height;
    clipboard_ =
        capture_brush(project, frame_, selection_.x, selection_.y, clip_width_, clip_height_)
            .pixels;
    auto &f = project.frames[frame_];
    if (cut) {
        checkpoint();
        if (f.paint_mask.empty())
            f.paint_mask.assign(f.pixels.size(), 255);
    }
    for (int y = 0; y < clip_height_; ++y)
        for (int x = 0; x < clip_width_; ++x) {
            auto index = (selection_.y + y) * project.width + selection_.x + x;
            clipboard_[y * clip_width_ + x] = f.pixels[index];
            if (cut) {
                f.pixels[index] = 0;
                f.paint_mask[index] = 0;
            }
        }
    if (cut)
        changed();
    status_ = "Copied indexed pixels; V stamps the brush";
}
void Editor::apply_tool(int x, int y, bool first, bool right) {
    auto &f = project.frames[frame_];
    Byte paint = Byte(right ? 0 : slot_);
    if (tool_ == Tool::TileBrush) {
        if (right || ImGui::GetIO().KeyAlt) {
            selected_tile_ = read_tile(project, frame_, x / 8, y / 8);
            tile_selected_ = true;
            selected_tile_x_ = x / 8;
            selected_tile_y_ = y / 8;
            palette_ = f.attributes[(y / 16) * project.block_width() + x / 16];
            palette_group_ = 0;
            layer_ = -1;
            status_ = "Picked tile; left-drag paints it on the map";
        } else if (tile_selected_) {
            paint_tile_line(project, frame_, first ? x / 8 : last_x_ / 8,
                            first ? y / 8 : last_y_ / 8, x / 8, y / 8, selected_tile_);
            layer_ = -1;
            changed();
        } else
            status_ = "Choose a pattern in Tiles, or right-click a map tile";
        return;
    }
    if (tool_ == Tool::Picker) {
        palette_ = f.attributes[(y / 16) * project.block_width() + x / 16];
        slot_ = f.pixels[y * project.width + x];
        layer_ = -1;
        return;
    }
    if (tool_ == Tool::Select) {
        selection_ = {std::min(start_x_, x), std::min(start_y_, y), std::abs(start_x_ - x) + 1,
                      std::abs(start_y_ - y) + 1};
        return;
    }
    if (tool_ == Tool::Pan)
        return;
    if (tool_ == Tool::Move) {
        if (first) {
            layer_ = -1;
            for (std::size_t i = 0; i < f.sprites.size(); ++i) {
                auto &s = f.sprites[i];
                if (s.visible && x >= s.x && x < s.x + 8 && y >= s.y && y < s.y + 8) {
                    layer_ = static_cast<int>(i);
                    break;
                }
            }
        } else if (layer_ >= 0) {
            auto &s = f.sprites.at(layer_);
            s.x += x - last_x_;
            s.y += y - last_y_;
            changed();
        }
        return;
    }
    if (tool_ == Tool::Sprite) {
        if (first) {
            if (project.sprite_sheet.empty() || tile_x_ * 8 + 8 > project.sprite_sheet.width ||
                tile_y_ * 8 + 8 > project.sprite_sheet.height) {
                status_ = "Choose a complete 8x8 tile from a sprite sheet first";
                return;
            }
            auto s = project.make_sprite(tile_x_, tile_y_, palette_, x, y);
            s.name = "Sprite " + std::to_string(f.sprites.size() + 1);
            f.sprites.push_back(std::move(s));
            layer_ = static_cast<int>(f.sprites.size() - 1);
            changed();
        }
        return;
    }
    if (tool_ == Tool::Palette) {
        f.attributes[(y / 16) * project.block_width() + x / 16] = Byte(palette_);
        changed();
        return;
    }
    if (tool_ == Tool::Stamp) {
        if (!clipboard_.empty() && (first || x != last_x_ || y != last_y_)) {
            int sx = brush_snap_ ? (x / 8) * 8 : x, sy = brush_snap_ ? (y / 8) * 8 : y;
            stamp_brush(project, frame_, sx, sy, {clip_width_, clip_height_, clipboard_},
                        brush_transparent_, brush_wrap_);
            changed();
        }
        return;
    }
    if (tool_ == Tool::Fill) {
        if (first) {
            if (layer_ >= 0) {
                const auto &s = f.sprites.at(layer_);
                flood_fill_sprite(project, frame_, layer_, x - s.x, y - s.y, Byte(paint));
            } else
                flood_fill(project, frame_, x, y, paint, right ? -1 : palette_);
            changed();
        }
        return;
    }
    paint_stroke(stroke_pixels(first ? x : last_x_, first ? y : last_y_, x, y, false), right);
}
bool Editor::shape_tool() const {
    return tool_ == Tool::Line || tool_ == Tool::Rectangle || tool_ == Tool::Ellipse;
}
std::vector<PixelPoint> Editor::stroke_pixels(int x0, int y0, int x1, int y1,
                                              bool constrain) const {
    DrawingShape shape = tool_ == Tool::Rectangle ? DrawingShape::Rectangle
                         : tool_ == Tool::Ellipse ? DrawingShape::Ellipse
                                                  : DrawingShape::Line;
    if (constrain) {
        int dx = x1 - x0, dy = y1 - y0;
        if (shape == DrawingShape::Line && std::abs(dx) > 2 * std::abs(dy))
            y1 = y0;
        else if (shape == DrawingShape::Line && std::abs(dy) > 2 * std::abs(dx))
            x1 = x0;
        else {
            int side = std::max(std::abs(dx), std::abs(dy));
            x1 = x0 + (dx < 0 ? -side : side);
            y1 = y0 + (dy < 0 ? -side : side);
        }
    }
    int width = project.width, height = project.height;
    if (layer_ >= 0) {
        const auto &sprite = project.frames[frame_].sprites.at(layer_);
        x0 -= sprite.x;
        x1 -= sprite.x;
        y0 -= sprite.y;
        y1 -= sprite.y;
        width = height = 8;
    }
    return drawing_pixels(shape, x0, y0, x1, y1, width, height,
                          shape != DrawingShape::Line && shape_filled_, brush_, mirror_x_,
                          mirror_y_);
}
bool Editor::path_tool() const {
    return tool_ == Tool::Polyline || tool_ == Tool::Polygon;
}
void Editor::cancel_drawing() {
    shape_drag_ = false;
    stroke_ = false;
    path_.clear();
}
std::vector<PixelPoint> Editor::path_preview(const std::vector<PixelPoint> &world) const {
    auto points = world;
    int width = project.width, height = project.height;
    if (layer_ >= 0) {
        const auto &s = project.frames[frame_].sprites.at(layer_);
        for (auto &p : points) {
            p.x -= s.x;
            p.y -= s.y;
        }
        width = height = 8;
    }
    return path_pixels(points, width, height, tool_ == Tool::Polygon,
                       tool_ == Tool::Polygon && shape_filled_, brush_, mirror_x_, mirror_y_);
}
void Editor::finish_path() {
    if (path_.size() >= (tool_ == Tool::Polygon ? 3u : 2u)) {
        auto pixels = path_preview(path_);
        if (!pixels.empty()) {
            checkpoint();
            paint_stroke(pixels, false);
        }
        cancel_drawing();
    }
}
void Editor::paint_stroke(const std::vector<PixelPoint> &points, bool right) {
    if (points.empty())
        return;
    bool erase = right || tool_ == Tool::Eraser;
    paint_pixels(project, frame_, points, Byte(erase ? 0 : slot_), erase ? -1 : palette_, layer_);
    changed();
}
void Editor::canvas() {
    if (!advanced_panels_)
        place_panel(canvas_rect_);
    ImGui::Begin("Canvas", nullptr,
                 advanced_panels_
                     ? 0
                     : ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                           ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking);
    ImGui::Text("%s | %s", tool_label(), layer_ >= 0 ? "Selected sprite" : "Background");
    ImGui::SameLine();
    auto &colors = palette_group_ ? project.sprite_palettes : project.background;
    ImGui::ColorButton("Current paint", color(colors[palette_][slot_]),
                       ImGuiColorEditFlags_NoTooltip, {25, 21});
    ImGui::SameLine();
    ImGui::Text("Set %d / color %d", palette_ + 1, slot_ + 1);
    ImGui::SameLine();
    if (ImGui::SmallButton("Fit canvas"))
        fit_countdown_ = 0;
    ImGui::SetNextItemWidth(145);
    ImGui::SliderFloat("Zoom", &zoom_, .5f, 12.f, "%.1fx");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(90);
    const char *grids[]{"No grid", "Pixels", "8 x 8", "16 x 16"};
    int grid = grid_ == 0 ? 0 : grid_ == 1 ? 1 : grid_ == 8 ? 2 : 3;
    if (ImGui::Combo("Grid", &grid, grids, 4))
        grid_ = grid == 0 ? 0 : grid == 1 ? 1 : grid == 2 ? 8 : 16;
    ImGui::Separator();
    ImGui::BeginChild("canvas-scroll", {0, 0}, false,
                      ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    auto available = ImGui::GetContentRegionAvail();
    if (fit_countdown_ >= 0) {
        if (fit_countdown_ == 0)
            zoom_ = std::clamp(std::floor(std::min((available.x - 32) / project.width,
                                                   (available.y - 32) / project.height) *
                                          2) /
                                   2,
                               .5f, 12.f);
        --fit_countdown_;
    }
    ImVec2 size{project.width * zoom_, project.height * zoom_};
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(16.f, (available.x - size.x) / 2));
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + std::max(16.f, (available.y - size.y) / 2));
    auto origin = ImGui::GetCursorScreenPos();
    ImGui::Image(texture_id(texture_), size);
    bool hover = ImGui::IsItemHovered();
    auto *draw = ImGui::GetWindowDrawList();
    if (onion_ && previous_texture_)
        draw->AddImage(texture_id(previous_texture_), origin,
                       {origin.x + size.x, origin.y + size.y}, {0, 0}, {1, 1},
                       IM_COL32(255, 255, 255, 65));
    if (reference_overlay_ && reference_texture_)
        draw->AddImage(texture_id(reference_texture_),
                       {origin.x + project.offset_x * zoom_, origin.y + project.offset_y * zoom_},
                       {origin.x + (project.offset_x + project.reference.width) * zoom_,
                        origin.y + (project.offset_y + project.reference.height) * zoom_},
                       {0, 0}, {1, 1}, IM_COL32(255, 255, 255, 90));
    if (grid_ > 0 && (grid_ > 1 || zoom_ >= 5)) {
        for (int x = 0; x <= project.width; x += grid_)
            draw->AddLine({origin.x + x * zoom_, origin.y},
                          {origin.x + x * zoom_, origin.y + size.y}, IM_COL32(110, 135, 150, 75));
        for (int y = 0; y <= project.height; y += grid_)
            draw->AddLine({origin.x, origin.y + y * zoom_},
                          {origin.x + size.x, origin.y + y * zoom_}, IM_COL32(110, 135, 150, 75));
    }
    if (highlight_) {
        auto &attrs = project.frames[frame_].attributes;
        for (int by = 0; by < project.block_height(); ++by)
            for (int bx = 0; bx < project.block_width(); ++bx)
                if (attrs[by * project.block_width() + bx] != palette_)
                    draw->AddRectFilled(
                        {origin.x + bx * 16 * zoom_, origin.y + by * 16 * zoom_},
                        {origin.x + (bx + 1) * 16 * zoom_, origin.y + (by + 1) * 16 * zoom_},
                        IM_COL32(0, 0, 0, 130));
    }
    if (selection_.width > 0)
        draw->AddRect({origin.x + selection_.x * zoom_, origin.y + selection_.y * zoom_},
                      {origin.x + (selection_.x + selection_.width) * zoom_,
                       origin.y + (selection_.y + selection_.height) * zoom_},
                      IM_COL32(255, 208, 93, 255), 0, 0, 2);
    if (layer_ >= 0) {
        auto &s = project.frames[frame_].sprites.at(layer_);
        draw->AddRect({origin.x + s.x * zoom_, origin.y + s.y * zoom_},
                      {origin.x + (s.x + 8) * zoom_, origin.y + (s.y + 8) * zoom_},
                      IM_COL32(255, 208, 93, 255), 0, 0, 2);
    }
    auto &io = ImGui::GetIO();
    if (hover && !shape_drag_ && io.MouseWheel != 0)
        zoom_ = std::clamp(zoom_ + (io.MouseWheel > 0 ? .5f : -.5f), .5f, 12.f);
    if (hover) {
        int x = std::clamp(int((io.MousePos.x - origin.x) / zoom_), 0, project.width - 1),
            y = std::clamp(int((io.MousePos.y - origin.y) / zoom_), 0, project.height - 1);
        int cursor_x =
            tool_ == Tool::TileBrush || (tool_ == Tool::Stamp && brush_snap_) ? (x / 8) * 8 : x;
        int cursor_y =
            tool_ == Tool::TileBrush || (tool_ == Tool::Stamp && brush_snap_) ? (y / 8) * 8 : y;
        int cursor_w = tool_ == Tool::TileBrush                      ? 8
                       : tool_ == Tool::Stamp && !clipboard_.empty() ? clip_width_
                                                                     : brush_;
        int cursor_h = tool_ == Tool::TileBrush                      ? 8
                       : tool_ == Tool::Stamp && !clipboard_.empty() ? clip_height_
                                                                     : brush_;
        draw->AddRect(
            {origin.x + cursor_x * zoom_, origin.y + cursor_y * zoom_},
            {origin.x + (cursor_x + cursor_w) * zoom_, origin.y + (cursor_y + cursor_h) * zoom_},
            IM_COL32(240, 240, 240, 180));
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle) ||
            (tool_ == Tool::Pan && ImGui::IsMouseDragging(ImGuiMouseButton_Left))) {
            ImGui::SetScrollX(ImGui::GetScrollX() - io.MouseDelta.x);
            ImGui::SetScrollY(ImGui::GetScrollY() - io.MouseDelta.y);
        } else if (!playing_ && !path_tool()) {
            bool first = ImGui::IsMouseClicked(0) || ImGui::IsMouseClicked(1);
            if (first) {
                stroke_ = true;
                start_x_ = last_x_ = x;
                start_y_ = last_y_ = y;
                if (shape_tool()) {
                    shape_drag_ = true;
                    shape_right_ = ImGui::IsMouseClicked(1);
                }
                if (!shape_tool() && tool_ != Tool::Picker && tool_ != Tool::Select &&
                    tool_ != Tool::Pan &&
                    !(tool_ == Tool::TileBrush && (io.KeyAlt || ImGui::IsMouseDown(1))))
                    checkpoint();
            }
            if (stroke_ && (ImGui::IsMouseDown(0) || ImGui::IsMouseDown(1))) {
                if (!shape_tool())
                    apply_tool(x, y, first, ImGui::IsMouseDown(1));
                last_x_ = x;
                last_y_ = y;
            }
        }
        ImGui::SetTooltip(
            "%d, %d | Tile %d,%d | Palette %d", x, y, x / 8, y / 8,
            project.frames[frame_].attributes[(y / 16) * project.block_width() + x / 16]);
    }
    if (!path_.empty() && (!path_tool() || path_frame_ != frame_ || path_layer_ != layer_ ||
                           path_kind_ != tool_ || playing_))
        cancel_drawing();
    std::vector<PixelPoint> preview_points;
    bool preview_right = false;
    if (path_tool() && !playing_ && !io.WantTextInput) {
        if (ImGui::IsKeyPressed(ImGuiKey_Escape))
            cancel_drawing();
        else {
            if (ImGui::IsKeyPressed(ImGuiKey_Backspace) && !path_.empty())
                path_.pop_back();
            PixelPoint point{std::clamp(int(std::floor((io.MousePos.x - origin.x) / zoom_)), 0,
                                        project.width - 1),
                             std::clamp(int(std::floor((io.MousePos.y - origin.y) / zoom_)), 0,
                                        project.height - 1)};
            if (io.KeyShift && !path_.empty()) {
                auto last = path_.back();
                int dx = point.x - last.x, dy = point.y - last.y;
                if (std::abs(dx) > 2 * std::abs(dy))
                    point.y = last.y;
                else if (std::abs(dy) > 2 * std::abs(dx))
                    point.x = last.x;
                else {
                    int n = std::max(std::abs(dx), std::abs(dy));
                    point = {last.x + (dx < 0 ? -n : n), last.y + (dy < 0 ? -n : n)};
                }
            }
            if (hover && ImGui::IsMouseClicked(0)) {
                if (tool_ == Tool::Polygon && path_.size() >= 3 && point == path_.front())
                    finish_path();
                else if (path_.size() < 256 && (path_.empty() || point != path_.back())) {
                    if (path_.empty()) {
                        path_frame_ = frame_;
                        path_layer_ = layer_;
                        path_kind_ = tool_;
                    }
                    path_.push_back(point);
                }
            }
            if (ImGui::IsKeyPressed(ImGuiKey_Enter) || (hover && ImGui::IsMouseClicked(1)))
                finish_path();
            if (!path_.empty()) {
                auto points = path_;
                if (hover && points.size() < 256 && point != points.back())
                    points.push_back(point);
                preview_points = path_preview(points);
                for (auto p : path_)
                    draw->AddCircle(
                        {origin.x + (p.x + .5f) * zoom_, origin.y + (p.y + .5f) * zoom_}, 4,
                        IM_COL32(255, 210, 95, 255));
            }
        }
    }
    if (shape_drag_ && (!shape_tool() || ImGui::IsKeyPressed(ImGuiKey_Escape) || playing_)) {
        cancel_drawing();
        stroke_ = false;
    }
    if (shape_drag_) {
        // Keep the gesture captured when the pointer leaves the image.
        int x = int(std::floor((io.MousePos.x - origin.x) / zoom_));
        int y = int(std::floor((io.MousePos.y - origin.y) / zoom_));
        auto points = stroke_pixels(start_x_, start_y_, x, y, io.KeyShift);
        preview_points = points;
        preview_right = shape_right_;
        if (ImGui::IsMouseReleased(shape_right_ ? 1 : 0)) {
            if (!points.empty()) {
                checkpoint();
                paint_stroke(points, shape_right_);
            }
            cancel_drawing();
        }
    }
    if (!preview_points.empty()) {
        int ox = 0, oy = 0;
        const Sprite *sprite = nullptr;
        if (layer_ >= 0) {
            sprite = &project.frames[frame_].sprites.at(layer_);
            ox = sprite->x;
            oy = sprite->y;
        }
        Byte slot = Byte(preview_right ? 0 : slot_);
        for (auto [px, py] : preview_points) {
            int wx = px + ox, wy = py + oy;
            if (wx < 0 || wy < 0 || wx >= project.width || wy >= project.height)
                continue;
            Byte index =
                sprite ? project.sprite_palettes[sprite->palette][slot]
                       : project.background[preview_right
                                                ? project.frames[frame_].attributes
                                                      [(wy / 16) * project.block_width() + wx / 16]
                                                : palette_][slot];
            if (sprite && slot == 0) {
                const auto &f = project.frames[frame_];
                index =
                    project.background[f.attributes[(wy / 16) * project.block_width() + wx / 16]]
                                      [f.pixels[wy * project.width + wx]];
            }
            draw->AddRectFilled({origin.x + wx * zoom_, origin.y + wy * zoom_},
                                {origin.x + (wx + 1) * zoom_, origin.y + (wy + 1) * zoom_},
                                ImGui::ColorConvertFloat4ToU32(color(index)));
        }
    }
    if (mirror_x_ || mirror_y_) {
        float ox = 0, oy = 0, w = float(project.width), h = float(project.height);
        if (layer_ >= 0) {
            const auto &s = project.frames[frame_].sprites.at(layer_);
            ox = float(s.x);
            oy = float(s.y);
            w = h = 8;
        }
        if (mirror_x_)
            draw->AddLine({origin.x + (ox + w / 2) * zoom_, origin.y + oy * zoom_},
                          {origin.x + (ox + w / 2) * zoom_, origin.y + (oy + h) * zoom_},
                          IM_COL32(100, 220, 255, 150));
        if (mirror_y_)
            draw->AddLine({origin.x + ox * zoom_, origin.y + (oy + h / 2) * zoom_},
                          {origin.x + (ox + w) * zoom_, origin.y + (oy + h / 2) * zoom_},
                          IM_COL32(100, 220, 255, 150));
    }
    if (focus_sprite_ && layer_ >= 0) {
        const auto &s = project.frames[frame_].sprites.at(layer_);
        auto pos = ImGui::GetWindowPos();
        ImGui::SetScrollFromPosX(origin.x - pos.x + (s.x + 4) * zoom_, .5f);
        ImGui::SetScrollFromPosY(origin.y - pos.y + (s.y + 4) * zoom_, .5f);
        focus_sprite_ = false;
    }
    if (!ImGui::IsMouseDown(0) && !ImGui::IsMouseDown(1))
        stroke_ = false;
    ImGui::EndChild();
    ImGui::End();
}
void Editor::layers(bool embedded) {
    if (!embedded)
        ImGui::Begin("Layers & sprites");
    auto &f = project.frames[frame_];
    if (ImGui::Button("+ New sprite", {-1, 28}))
        add_blank_sprite();
    if (ImGui::Selectable("Background picture", layer_ < 0))
        layer_ = -1;
    ImGui::Separator();
    ImGui::BeginChild("layer-list", {0, 115}, true);
    for (std::size_t i = 0; i < f.sprites.size(); ++i) {
        ImGui::PushID(static_cast<int>(i));
        bool visible = f.sprites[i].visible;
        if (ImGui::Checkbox("##visible", &visible)) {
            checkpoint();
            f.sprites[i].visible = visible;
            changed();
        }
        ImGui::SameLine();
        if (ImGui::Selectable(f.sprites[i].name.c_str(), layer_ == static_cast<int>(i))) {
            layer_ = static_cast<int>(i);
            palette_ = f.sprites[i].palette;
            palette_group_ = 1;
        }
        ImGui::PopID();
    }
    ImGui::EndChild();
    if (ImGui::Button("Generate from sheet") && !project.sprite_sheet.empty()) {
        checkpoint();
        project.generate_sprites(frame_, palette_);
        layer_ = -1;
        changed();
    }
    if (layer_ >= 0 && static_cast<std::size_t>(layer_) < f.sprites.size()) {
        auto &s = f.sprites[layer_];
        ImGui::Separator();
        ImGui::TextUnformatted("Sprite pixels (click to paint)");
        auto origin = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("sprite-pixels", {128, 128});
        auto *draw = ImGui::GetWindowDrawList();
        for (int y = 0; y < 8; ++y)
            for (int x = 0; x < 8; ++x) {
                auto c = s.pixels[y * 8 + x];
                ImU32 rgb =
                    c ? ImGui::ColorConvertFloat4ToU32(color(project.sprite_palettes[s.palette][c]))
                      : IM_COL32(28, 34, 40, 255);
                draw->AddRectFilled({origin.x + x * 16.f, origin.y + y * 16.f},
                                    {origin.x + (x + 1) * 16.f, origin.y + (y + 1) * 16.f}, rgb);
                draw->AddRect({origin.x + x * 16.f, origin.y + y * 16.f},
                              {origin.x + (x + 1) * 16.f, origin.y + (y + 1) * 16.f},
                              IM_COL32(60, 70, 80, 255));
            }
        if (ImGui::IsItemHovered() && (ImGui::IsMouseDown(0) || ImGui::IsMouseDown(1))) {
            if (ImGui::IsMouseClicked(0) || ImGui::IsMouseClicked(1))
                checkpoint();
            int x = int((ImGui::GetIO().MousePos.x - origin.x) / 16),
                y = int((ImGui::GetIO().MousePos.y - origin.y) / 16);
            s.pixels[y * 8 + x] = Byte(ImGui::IsMouseDown(1) ? 0 : slot_);
            changed();
        }
        char name[256];
        std::snprintf(name, sizeof(name), "%s", s.name.c_str());
        if (ImGui::InputText("Name", name, sizeof(name))) {
            checkpoint();
            s.name = name;
            changed();
        }
        int position[]{s.x, s.y};
        if (ImGui::InputInt2("Position", position)) {
            checkpoint();
            s.x = std::clamp(position[0], -4096, 4096);
            s.y = std::clamp(position[1], -4096, 4096);
            changed();
        }
        int pal = s.palette;
        if (ImGui::SliderInt("Palette", &pal, 0, 3)) {
            checkpoint();
            s.palette = pal;
            palette_ = pal;
            changed();
        }
        for (auto [label, value] : {std::pair{"Flip X", &s.flip_x},
                                    {"Flip Y", &s.flip_y},
                                    {"Behind background", &s.behind}}) {
            bool v = *value;
            if (ImGui::Checkbox(label, &v)) {
                checkpoint();
                *value = v;
                changed();
            }
        }
        if (ImGui::Button("Delete sprite")) {
            checkpoint();
            f.sprites.erase(f.sprites.begin() + layer_);
            layer_ = -1;
            changed();
        } else {
            ImGui::SameLine();
            if (ImGui::Button("Up") && layer_ > 0) {
                checkpoint();
                std::swap(f.sprites[layer_], f.sprites[layer_ - 1]);
                --layer_;
                changed();
            }
            ImGui::SameLine();
            if (ImGui::Button("Down") && static_cast<std::size_t>(layer_ + 1) < f.sprites.size()) {
                checkpoint();
                std::swap(f.sprites[layer_], f.sprites[layer_ + 1]);
                ++layer_;
                changed();
            }
        }
    } else {
        ImGui::TextWrapped("Select a sprite to edit its pixels, palette, position, priority and "
                           "flipping. Earlier objects have higher OAM priority.");
    }
    if (!embedded)
        ImGui::End();
}
void Editor::preview() {
    ImGui::Begin("Preview");
    ImVec2 available = ImGui::GetContentRegionAvail();
    float scale =
        std::max(.1f, std::min(available.x / project.width, (available.y - 25) / project.height));
    ImGui::Image(texture_id(texture_), {project.width * scale, project.height * scale});
    ImGui::TextUnformatted("Nearest-neighbor / current frame");
    ImGui::End();
}
void Editor::timeline() {
    if (!advanced_panels_)
        place_panel(timeline_rect_);
    ImGui::Begin("Timeline", nullptr,
                 advanced_panels_
                     ? 0
                     : ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                           ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking);
    if (ImGui::Button(playing_ ? "Pause" : "Play")) {
        playing_ = !playing_;
        next_frame_ = ImGui::GetTime() + project.frames[frame_].duration_ms / 1000.;
    }
    ImGui::SameLine();
    if (ImGui::Button("Duplicate") && project.frames.size() < 256) {
        checkpoint();
        auto duplicate = project.frames[frame_];
        project.frames.insert(project.frames.begin() + static_cast<std::ptrdiff_t>(frame_ + 1),
                              std::move(duplicate));
        ++frame_;
        layer_ = -1;
        changed();
    }
    ImGui::SameLine();
    if (ImGui::Button("Blank frame") && project.frames.size() < 256) {
        checkpoint();
        Frame f{std::vector<Byte>(project.width * project.height),
                std::vector<Byte>(project.block_width() * project.block_height()),
                {},
                120};
        project.frames.push_back(std::move(f));
        frame_ = project.frames.size() - 1;
        layer_ = -1;
        changed();
    }
    ImGui::SameLine();
    if (ImGui::Button("Delete frame") && project.frames.size() > 1) {
        checkpoint();
        project.frames.erase(project.frames.begin() + static_cast<std::ptrdiff_t>(frame_));
        frame_ = std::min(frame_, project.frames.size() - 1);
        layer_ = -1;
        changed();
    }
    ImGui::SameLine();
    int ms = project.frames[frame_].duration_ms;
    ImGui::SetNextItemWidth(100);
    if (ImGui::InputInt("ms", &ms)) {
        checkpoint();
        project.frames[frame_].duration_ms = std::clamp(ms, 10, 60000);
        changed();
    }
    ImGui::BeginChild("frames", {0, 0}, false, ImGuiWindowFlags_HorizontalScrollbar);
    for (std::size_t i = 0; i < project.frames.size(); ++i) {
        if (i)
            ImGui::SameLine();
        char label[32];
        std::snprintf(label, sizeof(label), "%03zu\n%d ms", i + 1, project.frames[i].duration_ms);
        if (ImGui::Selectable(label, i == frame_, 0, {68, 43})) {
            frame_ = i;
            layer_ = -1;
            refresh_ = true;
        }
    }
    ImGui::EndChild();
    ImGui::End();
}
void Editor::hardware(bool embedded) {
    if (!embedded && focus_hardware_) {
        ImGui::SetNextWindowFocus();
        focus_hardware_ = false;
    }
    if (!embedded)
        ImGui::Begin("Hardware");
    ImGui::TextUnformatted("NES / NROM-128");
    ImGui::Separator();
    if (compiled_) {
        ImGui::TextColored({.45f, .85f, .64f, 1}, "Ready for ROM export");
        ImGui::Text("Background tiles  %d / 256", compiled_->background_tiles);
        ImGui::Text("Sprite tiles      %d / 256", compiled_->sprite_tiles);
        ImGui::Text("OAM sprites       %d / 64", compiled_->sprite_count);
        ImGui::Text("Nametables        %zu", compiled_->nametables.size() / 1024);
    } else {
        ImGui::TextColored({1, .55f, .4f, 1}, "Export blocked");
        ImGui::TextWrapped("%s", compile_error_.c_str());
    }
    ImGui::Separator();
    std::array<float, 240> lines{};
    for (const auto &s : project.frames[frame_].sprites)
        if (s.visible)
            for (int y = std::max(0, s.y); y < std::min(240, s.y + 8); ++y)
                ++lines[y];
    ImGui::TextUnformatted("Sprites per scanline (limit 8)");
    ImGui::PlotHistogram("##scanlines", lines.data(), 240, 0, nullptr, 0, 16, {0, 65});
    ImGui::TextWrapped("Export includes the current frame. The ROM displays the first 256x240 "
                       "viewport; a second nametable is included for use in your game.");
    ImGui::TextUnformatted("16 KiB PRG + 8 KiB CHR / mapper 0");
    if (ImGui::Button("Export ROM + assets..."))
        file_dialog(FileAction::Export);
    ImGui::Separator();
    ImGui::TextWrapped("The .nes file runs in an emulator or on a compatible flash cartridge. Raw "
                       "CHR, nametable, palette, OAM and C header files are also written.");
    if (!embedded)
        ImGui::End();
}
void Editor::attributes() {
    ImGui::Begin("Attributes");
    ImGui::TextUnformatted("16x16 background palette regions");
    ImGui::TextUnformatted("Click to assign the active palette");
    ImGui::BeginChild("attribute-grid", {0, 0}, false, ImGuiWindowFlags_HorizontalScrollbar);
    auto &attrs = project.frames[frame_].attributes;
    for (int y = 0; y < project.block_height(); ++y) {
        for (int x = 0; x < project.block_width(); ++x) {
            if (x)
                ImGui::SameLine(0, 2);
            int i = y * project.block_width() + x;
            ImGui::PushID(i);
            ImGui::PushStyleColor(ImGuiCol_Button, color(project.background[attrs[i]][2]));
            char label[4];
            std::snprintf(label, sizeof(label), "%d", attrs[i]);
            if (ImGui::Button(label, {19, 19})) {
                checkpoint();
                attrs[i] = Byte(palette_);
                changed();
            }
            ImGui::PopStyleColor();
            ImGui::PopID();
        }
    }
    ImGui::EndChild();
    ImGui::End();
}
void Editor::sprite_sheet(bool embedded) {
    if (!embedded)
        ImGui::Begin("Sprite sheet");
    if (project.sprite_sheet.empty()) {
        ImGui::TextWrapped(
            "Import a sprite sheet, select an 8x8 tile, then place it with the Sprite tool.");
        if (ImGui::Button("Import sprite sheet..."))
            file_dialog(FileAction::Sprites);
    } else {
        ImGui::Text("Tile %d,%d / palette %d", tile_x_, tile_y_, palette_);
        float scale =
            std::clamp(ImGui::GetContentRegionAvail().x / project.sprite_sheet.width, .25f, 4.f);
        ImGui::BeginChild("sheet-scroll", {0, 0}, false, ImGuiWindowFlags_HorizontalScrollbar);
        auto pos = ImGui::GetCursorScreenPos();
        ImGui::Image(texture_id(sheet_texture_),
                     {project.sprite_sheet.width * scale, project.sprite_sheet.height * scale});
        if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(0)) {
            tile_x_ = int((ImGui::GetIO().MousePos.x - pos.x) / (8 * scale));
            tile_y_ = int((ImGui::GetIO().MousePos.y - pos.y) / (8 * scale));
            tool_ = Tool::Sprite;
        }
        ImGui::GetWindowDrawList()->AddRect(
            {pos.x + tile_x_ * 8 * scale, pos.y + tile_y_ * 8 * scale},
            {pos.x + (tile_x_ + 1) * 8 * scale, pos.y + (tile_y_ + 1) * 8 * scale},
            IM_COL32(255, 208, 93, 255), 0, 0, 2);
        ImGui::EndChild();
    }
    if (!embedded)
        ImGui::End();
}
void Editor::palette_effect() {
    if (!ImGui::Begin("Palette test effect")) {
        ImGui::End();
        return;
    }
    if (!effect_loaded_) {
        effect_loaded_ = true;
        try {
            auto dir = data_directory_ / "paltesteffect";
            if (!std::filesystem::exists(dir))
                dir = std::filesystem::current_path() / "data/paltesteffect";
            std::vector<std::filesystem::path> files;
            for (const auto &entry : std::filesystem::directory_iterator(dir))
                if (entry.path().extension() == ".bmp")
                    files.push_back(entry.path());
            std::sort(files.begin(), files.end());
            for (const auto &path : files)
                effect_images_.push_back(load_image(path));
        } catch (const std::exception &e) {
            status_ = std::string("Palette test assets: ") + e.what();
        }
    }
    if (effect_images_.empty()) {
        ImGui::TextWrapped("No palette test patterns were found in the data directory.");
        ImGui::End();
        return;
    }
    int w = effect_images_[0].width, h = effect_images_[0].height;
    Image image{w, h, std::vector<Byte>(w * h * 4)};
    double frame = ImGui::GetTime() * 60;
    for (int y = 0; y < h; ++y) {
        double phase = (y / static_cast<double>(effect_images_.size())) * 3.141592653589793;
        double value = std::sin(std::cos(frame * .03 + phase * 4) * phase * .2 + frame * .02);
        auto index = std::min(effect_images_.size() - 1,
                              static_cast<std::size_t>((value + 1) * .5 * effect_images_.size()));
        for (int x = 0; x < w; ++x) {
            auto shade = effect_images_[index].at(x, y).r;
            int slot = shade < 48 ? 0 : shade < 128 ? 1 : shade < 192 ? 2 : 3;
            auto c = nes_colors[project.background[palette_][slot]];
            auto i = (y * w + x) * 4;
            image.rgba[i] = c.r;
            image.rgba[i + 1] = c.g;
            image.rgba[i + 2] = c.b;
            image.rgba[i + 3] = 255;
        }
    }
    upload(effect_texture_, image);
    ImGui::Text("Original animated palette test / palette %d", palette_);
    float scale = std::max(.5f, std::min(ImGui::GetContentRegionAvail().x / w,
                                         (ImGui::GetContentRegionAvail().y - 30) / h));
    ImGui::Image(texture_id(effect_texture_), {w * scale, h * scale});
    ImGui::TextWrapped("A palette test animation; this is separate from NES signal emulation.");
    ImGui::End();
}
void Editor::popups() {
    if (!error_.empty())
        ImGui::OpenPopup("Operation failed");
    if (ImGui::BeginPopupModal("Operation failed", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::PushTextWrapPos(520);
        ImGui::TextUnformatted(error_.c_str());
        ImGui::PopTextWrapPos();
        if (ImGui::Button("OK")) {
            error_.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    if (pending_ && file_action_ != FileAction::Save)
        ImGui::OpenPopup("Unsaved changes");
    if (ImGui::BeginPopupModal("Unsaved changes", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted("Save your changes before continuing?");
        if (ImGui::Button("Save")) {
            if (project_path_.empty()) {
                file_dialog(FileAction::Save);
                ImGui::CloseCurrentPopup();
            } else {
                save();
                if (!dirty_) {
                    auto action = std::move(pending_);
                    pending_ = {};
                    ImGui::CloseCurrentPopup();
                    action();
                }
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Discard")) {
            auto action = std::move(pending_);
            pending_ = {};
            dirty_ = false;
            ImGui::CloseCurrentPopup();
            action();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            pending_ = {};
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    if (file_popup_) {
        ImGui::OpenPopup("Choose file");
        file_popup_ = false;
    }
    ImGui::SetNextWindowSize({680, 480}, ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal("Choose file", nullptr, ImGuiWindowFlags_NoResize)) {
        ImGui::TextWrapped("%s", path_to_utf8(browse_dir_).c_str());
        if (ImGui::Button("Parent directory") && browse_dir_.has_parent_path())
            browse_dir_ = browse_dir_.parent_path();
        ImGui::SameLine();
        if (ImGui::Button("Project directory"))
            browse_dir_ = std::filesystem::current_path();
        ImGui::BeginChild("files", {0, 300}, true);
        std::error_code ec;
        std::vector<std::filesystem::directory_entry> entries;
        for (auto it = std::filesystem::directory_iterator(browse_dir_, ec);
             !ec && it != std::filesystem::directory_iterator(); it.increment(ec))
            entries.push_back(*it);
        std::sort(entries.begin(), entries.end(), [](const auto &a, const auto &b) {
            return a.path().filename() < b.path().filename();
        });
        for (const auto &entry : entries) {
            bool dir = entry.is_directory(ec);
            auto label = (dir ? "[dir] " : "") + path_to_utf8(entry.path().filename());
            if (ImGui::Selectable(label.c_str(), false, ImGuiSelectableFlags_AllowDoubleClick)) {
                if (dir && ImGui::IsMouseDoubleClicked(0)) {
                    browse_dir_ = entry.path();
                    break;
                }
                if (!dir)
                    std::snprintf(file_path_, sizeof(file_path_), "%s",
                                  path_to_utf8(entry.path().filename()).c_str());
            }
        }
        if (ec)
            ImGui::TextWrapped("Cannot list directory: %s", ec.message().c_str());
        ImGui::EndChild();
        ImGui::InputText("Path", file_path_, sizeof(file_path_));
        ImGui::TextUnformatted("Type an absolute path or a filename in the current directory.");
        if (ImGui::Button("Choose") && file_path_[0]) {
            auto path = path_from_utf8(file_path_);
            if (path.is_relative())
                path = browse_dir_ / path;
            if (std::filesystem::is_directory(path, ec)) {
                browse_dir_ = path;
                file_path_[0] = 0;
            } else {
                bool writes = file_action_ == FileAction::Save ||
                              file_action_ == FileAction::Export || file_action_ == FileAction::Png;
                if (writes && std::filesystem::exists(path)) {
                    ImGui::OpenPopup("Replace existing file?");
                } else {
                    const auto action = file_action_;
                    perform_file(path);
                    if (file_action_ == action)
                        file_action_ = FileAction::None;
                    ImGui::CloseCurrentPopup();
                }
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            if (file_action_ == FileAction::Save)
                pending_ = {};
            file_action_ = FileAction::None;
            ImGui::CloseCurrentPopup();
        }
        if (ImGui::BeginPopupModal("Replace existing file?", nullptr,
                                   ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextUnformatted("Replace the existing file and matching export assets?");
            if (ImGui::Button("Replace")) {
                auto path = path_from_utf8(file_path_);
                if (path.is_relative())
                    path = browse_dir_ / path;
                const auto action = file_action_;
                perform_file(path);
                if (file_action_ == action)
                    file_action_ = FileAction::None;
                ImGui::CloseCurrentPopup();
                file_path_[0] = 0;
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel"))
                ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }
        if (!file_path_[0] && file_action_ == FileAction::None)
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
}
void Editor::watch_sources() {
    if (!watch_sources_ || ImGui::GetTime() < next_watch_)
        return;
    next_watch_ = ImGui::GetTime() + 1.;
    auto poll = [&](const std::string &text, bool sprites) {
        if (text.empty())
            return;
        auto path = path_from_utf8(text);
        if (path.is_relative())
            path = (project_path_.empty() ? std::filesystem::current_path()
                                          : project_path_.parent_path()) /
                   path;
        auto &watched = sprites ? watched_sprites_ : watched_background_;
        auto &last = sprites ? sprite_time_ : background_time_;
        std::error_code ec;
        auto time = std::filesystem::last_write_time(path, ec);
        if (ec)
            return;
        if (path != watched || !last) {
            watched = path;
            last = time;
            return;
        }
        if (time == *last)
            return;
        try {
            auto image = load_image(path);
            auto &stored = sprites ? project.sprite_sheet : project.reference;
            if (image != stored) {
                checkpoint();
                stored = std::move(image);
                if (sprites) {
                    for (auto &frame : project.frames)
                        for (auto &sprite : frame.sprites)
                            if (sprite.tile_x * 8 + 8 <= stored.width &&
                                sprite.tile_y * 8 + 8 <= stored.height)
                                sprite.pixels = project
                                                    .make_sprite(sprite.tile_x, sprite.tile_y,
                                                                 sprite.palette, sprite.x, sprite.y)
                                                    .pixels;
                } else
                    project.quantize(frame_);
                changed();
                status_ = "Reloaded externally changed source image";
            }
            last = time;
        } catch (const std::exception &e) {
            status_ = std::string("Source reload pending: ") + e.what();
        }
    };
    poll(project.image_path, false);
    poll(project.sprite_path, true);
}
bool Editor::tick() {
    try {
        if (!ImGui::IsMouseDown(0) && !ImGui::IsMouseDown(1))
            tile_edit_stroke_ = false;
        watch_sources();
        if (selection_.x + selection_.width > project.width ||
            selection_.y + selection_.height > project.height)
            selection_ = {};
        if (playing_ && project.frames.size() > 1 && ImGui::GetTime() >= next_frame_) {
            frame_ = (frame_ + 1) % project.frames.size();
            layer_ = -1;
            next_frame_ = ImGui::GetTime() + project.frames[frame_].duration_ms / 1000.;
            refresh_ = true;
        }
        if (layer_ >= static_cast<int>(project.frames[frame_].sprites.size()))
            layer_ = -1;
        menus();
        dock_layout();
        refresh_textures();
        if (advanced_panels_) {
            tools();
            palettes();
            canvas();
            layers();
            timeline();
            preview();
            hardware();
            attributes();
            sprite_sheet();
            tiles();
            brush_controls();
            palette_effect();
        } else
            guided_workspace();
        focus_workflows_ = false;
        popups();
        start_screen();
    } catch (const std::exception &e) {
        error_ = e.what();
        std::cerr << "Editor: " << e.what() << "\n";
    }
    return !quit_;
}
} // namespace tzj
