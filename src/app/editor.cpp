#include "app/editor.hpp"
#include "app/ui_help.hpp"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <imgui.h>
#include <imgui_internal.h>
#include <iostream>
#include <tuple>
#include <utility>
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
    project.ensure_layers(0);
}
Editor::~Editor() {
#ifndef __EMSCRIPTEN__
    if (mcp_server_)
        mcp_server_->stop();
#endif
    for (auto id : {texture_, sheet_texture_, reference_texture_, previous_texture_,
                    effect_texture_, emulator_texture_})
        if (id)
            glDeleteTextures(1, &id);
}
void Editor::changed() {
    project.compose_layers(frame_);
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
    project.ensure_layers(0);
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
    for (std::size_t i = 0; i < project.frames.size(); ++i)
        project.ensure_layers(i);
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
    if (action == std::to_underlying(FileAction::Open))
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
        action == FileAction::Sprites || action == FileAction::Brushes ||
        action == FileAction::FontSheet) {
        browser_pick(std::to_underlying(action));
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
            project.reference = im;
            project.image_path = path_to_utf8(path);
            project.add_image_layer(frame_, std::move(im), path_to_utf8(path.filename()),
                                    project.image_path);
            project.add_paint_layer(frame_, "Paint corrections");
            layer_ = -1;
            palette_group_ = 0;
            tool_ = Tool::Pencil;
            changed();
            status_ = "Imported as a live image layer. Select its row to adjust conversion, or "
                      "paint on the corrections layer.";
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
        case FileAction::Brushes: {
            auto imported = load_project(path);
            auto candidate = project;
            candidate.brushes.insert(candidate.brushes.end(), imported.brushes.begin(),
                                     imported.brushes.end());
            candidate.ramps.insert(candidate.ramps.end(), imported.ramps.begin(),
                                   imported.ramps.end());
            candidate.validate();
            checkpoint();
            project = std::move(candidate);
            library_selection_ = -1;
            changed();
            status_ = "Imported saved brushes and color sets.";
            break;
        }
        case FileAction::FontSheet: {
            auto font =
                font_from_sheet(load_image(path), (palette_group_ ? project.sprite_palettes
                                                                  : project.background)[palette_]);
            checkpoint();
            project.font = std::move(font);
            changed();
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
void Editor::palette_editor() {
    if (palette_editor_open_) {
        ImGui::OpenPopup("Color set editor");
        palette_editor_open_ = false;
    }
    ImGui::SetNextWindowSize({520, 0}, ImGuiCond_Appearing);
    if (!ImGui::BeginPopupModal("Color set editor", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        return;
    ImGui::TextWrapped("Change the colors used by your artwork. These changes affect every frame.");
    const char *groups[]{"Background colors", "Sprite colors"};
    const char *sets[]{"Color set 1", "Color set 2", "Color set 3", "Color set 4"};
    ui::Combo("Artwork", &palette_edit_group_, groups, 2);
    ui::Combo("Color set", &palette_edit_index_, sets, 4);
    auto &colors = palette_edit_group_ ? project.sprite_palettes : project.background;
    for (int slot = 0; slot < 4; ++slot) {
        ImGui::PushID(slot);
        if (slot)
            ImGui::SameLine();
        ImGui::BeginDisabled(palette_edit_group_ == 1 && slot == 0);
        if (ui::ColorButton("Edit NES color", color(colors[palette_edit_index_][slot]), 0,
                            {64, 40}))
            ImGui::OpenPopup("Choose color");
        ImGui::EndDisabled();
        if (palette_edit_group_ == 1 && slot == 0)
            ui::help("Sprite transparency",
                     "The first sprite slot is reserved for transparency. It erases pixels instead "
                     "of assigning an opaque NES color.");
        if (ImGui::BeginPopup("Choose color")) {
            for (int n = 0; n < 64; ++n) {
                if (n % 8)
                    ImGui::SameLine(0, 3);
                ImGui::PushID(n);
                if (ui::ColorButton("NES color", color(Byte(n)), 0, {28, 28})) {
                    checkpoint();
                    if (slot == 0)
                        project.set_backdrop(Byte(n));
                    else
                        colors[palette_edit_index_][slot] = Byte(n);
                    changed();
                    ImGui::CloseCurrentPopup();
                }
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("NES color $%02X%s", n,
                                      n == 13 ? " - unsafe on real hardware" : "");
                ImGui::PopID();
            }
            ImGui::EndPopup();
        }
        ImGui::PopID();
    }
    ImGui::TextWrapped(
        palette_edit_group_
            ? "Sprites use three paint colors; the first slot is transparent."
            : "Background areas use four colors; the first is shared by the whole scene.");
    ui::SeparatorText("Reorder paint slots");
    const char *slots[]{"1", "2", "3"};
    int a = swap_first_ - 1, b = swap_second_ - 1;
    ImGui::SetNextItemWidth(65);
    if (ui::Combo("First", &a, slots, 3))
        swap_first_ = a + 1;
    ImGui::SetNextItemWidth(65);
    if (ui::Combo("Second", &b, slots, 3))
        swap_second_ = b + 1;
    ui::Checkbox("Preserve existing artwork", &preserve_palette_art_);
    if (ui::Button("Swap slots")) {
        checkpoint();
        swap_palette_slots(project, palette_edit_group_ == 1, palette_edit_index_, swap_first_,
                           swap_second_, preserve_palette_art_);
        changed();
    }
    auto &palettes = palette_edit_group_ ? project.sprite_palettes : project.background;
    if (ui::Button("Copy colors"))
        copied_palette_ = palettes[palette_edit_index_];
    ImGui::SameLine();
    ImGui::BeginDisabled(!copied_palette_);
    if (ui::Button("Paste colors")) {
        checkpoint();
        for (int i = 1; i < 4; ++i)
            palettes[palette_edit_index_][i] = (*copied_palette_)[i];
        changed();
    }
    ImGui::EndDisabled();
    ImGui::TextWrapped("Swap preserves artwork across all frames by remapping pixels. Slot "
                       "zero remains the NES backdrop/transparency.");
    if (ui::Button("Done", {-1, 28}))
        ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
}
void Editor::refresh_textures() {
    if (!refresh_)
        return;
    auto image = project.render(frame_, show_sprites_, hardware_preview_, true);
    if (pal_test_)
        for (int y = 1; y < image.height; y += 2)
            for (int x = 0; x < image.width; ++x)
                for (int c = 0; c < 3; ++c)
                    image.rgba[(y * image.width + x) * 4 + c] =
                        Byte(image.rgba[(y * image.width + x) * 4 + c] * 0.8f);
    upload(texture_, image);
    upload(sheet_texture_, project.sprite_sheet);
    upload(reference_texture_, project.reference);
    if (onion_)
        upload(previous_texture_, onion_overlay(project, frame_, onion_previous_, onion_following_,
                                                onion_opacity_, onion_sprites_only_));
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
    ++render_revision_;
    emulator_changed_at_ = ImGui::GetTime();
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
    fit_countdown_ = 1;
    if (workspace == Workspace::Tiles) {
        show_tiles_ = true;
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
        show_hardware_ = true;
        tool_ = Tool::Pan;
        focus_panel("Hardware");
    } else {
        if (workspace == Workspace::Animate)
            show_timeline_ = true;
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
            if (ui::MenuItem("New", "Ctrl+N"))
                guard([this] { show_start(); });
            if (ui::MenuItem("Open project...", "Ctrl+O"))
                guard([this] { file_dialog(FileAction::Open); });
            if (ui::MenuItem("Save project", "Ctrl+S"))
                save();
            if (ui::MenuItem("Save as...", "Ctrl+Shift+S"))
                save(true);
            ImGui::Separator();
            if (ui::MenuItem("Import background..."))
                file_dialog(FileAction::Background);
            if (ui::MenuItem("Import sprite sheet..."))
                file_dialog(FileAction::Sprites);
            if (ui::MenuItem("Import brushes and color sets..."))
                file_dialog(FileAction::Brushes);
            ImGui::Separator();
            if (ui::MenuItem("Export NES ROM + assets...", "Ctrl+E"))
                file_dialog(FileAction::Export);
            if (ui::MenuItem("Export PNG..."))
                file_dialog(FileAction::Png);
            ImGui::Separator();
            if (ui::MenuItem("Project information..."))
                show_project_info_ = true;
            if (ui::MenuItem("Quit"))
                request_close();
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Edit")) {
            if (ui::MenuItem("Undo", "Ctrl+Z")) {
                if (history_.undo(project)) {
                    frame_ = std::min(frame_, project.frames.size() - 1);
                    layer_ = -1;
                    changed();
                }
            }
            if (ui::MenuItem("Redo", "Ctrl+Y")) {
                if (history_.redo(project)) {
                    frame_ = std::min(frame_, project.frames.size() - 1);
                    layer_ = -1;
                    changed();
                }
            }
            ImGui::Separator();
            if (ui::MenuItem("Copy selection", "Ctrl+C"))
                copy_selection();
            if (ui::MenuItem("Cut selection", "Ctrl+X"))
                copy_selection(true);
            if (ui::MenuItem("Paste brush", "Ctrl+V", false, !clipboard_.empty()))
                tool_ = Tool::Stamp;
            ImGui::Separator();
            if (ui::MenuItem("Select area", "R")) {
                tool_ = Tool::Select;
                layer_ = -1;
            }
            if (ui::MenuItem("Deselect", nullptr, false, selection_.width > 0))
                selection_ = {};
            if (ImGui::BeginMenu("Captured brush", !clipboard_.empty())) {
                if (ui::MenuItem("Rotate left"))
                    transform_captured_brush(BrushTransform::RotateCCW);
                if (ui::MenuItem("Rotate right"))
                    transform_captured_brush(BrushTransform::RotateCW);
                if (ui::MenuItem("Flip horizontal"))
                    transform_captured_brush(BrushTransform::FlipX);
                if (ui::MenuItem("Flip vertical"))
                    transform_captured_brush(BrushTransform::FlipY);
                if (ui::MenuItem("Brush settings..."))
                    show_brush_settings_ = true;
                ImGui::EndMenu();
            }
            if (ui::MenuItem("Edit color sets...")) {
                palette_edit_group_ = palette_group_;
                palette_edit_index_ = palette_;
                palette_editor_open_ = true;
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Layers")) {
            auto &f = project.frames[frame_];
            const bool sprite = layer_ >= 0;
            const int selected = sprite ? layer_ : f.active_layer;
            if (ui::MenuItem("New paint layer", nullptr, false, !f.background_locked)) {
                checkpoint();
                project.add_paint_layer(frame_);
                layer_ = -1;
                palette_group_ = 0;
                tool_ = Tool::Pencil;
                changed();
            }
            if (ui::MenuItem("Import image layer...", nullptr, false, !f.background_locked))
                file_dialog(FileAction::Background);
            if (ui::MenuItem("New sprite", nullptr, false, !f.sprites_locked))
                add_blank_sprite();
            ImGui::Separator();
            if (ui::MenuItem("Duplicate layer", nullptr, false, selected >= 0))
                layer_action(0, selected, sprite);
            bool image = !sprite && selected >= 0 && f.layers.at(selected).kind == LayerKind::Image;
            if (ui::MenuItem("Make paintable copy", nullptr, false, image))
                layer_action(1, selected, false);
            if (ui::MenuItem("Merge down", nullptr, false, !sprite && selected > 0))
                layer_action(3, selected, false);
            if (ui::MenuItem("Delete layer", nullptr, false,
                             selected >= 0 && (sprite || f.layers.size() > 1)))
                layer_action(2, selected, sprite);
            ImGui::Separator();
            if (ui::MenuItem("Move up", nullptr, false,
                             selected >= 0 &&
                                 (sprite ? selected > 0 : selected + 1 < int(f.layers.size()))))
                layer_action(4, selected, sprite);
            if (ui::MenuItem("Move down", nullptr, false,
                             selected >= 0 &&
                                 (sprite ? selected + 1 < int(f.sprites.size()) : selected > 0)))
                layer_action(5, selected, sprite);
            if (ui::MenuItem("Clear layer", nullptr, false,
                             !sprite && project.paint_layer_editable(frame_))) {
                checkpoint();
                project.clear_active_layer(frame_);
                changed();
            }
            if (ui::MenuItem("Fit NES colors to this layer", nullptr, false, image)) {
                checkpoint();
                project.fit_layer_colors(frame_, std::size_t(f.active_layer));
                changed();
            }
            if (ui::MenuItem("Generate sprites from sheet", nullptr, false,
                             !project.sprite_sheet.empty() && !f.sprites_locked)) {
                checkpoint();
                project.generate_sprites(frame_, palette_);
                layer_ = -1;
                changed();
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Layout")) {
            for (auto [name, w, h] : {std::tuple{"Single screen", 256, 240},
                                      std::tuple{"Two screens horizontally", 512, 240},
                                      std::tuple{"Two screens vertically", 256, 480}}) {
                if (ui::MenuItem(name, nullptr, project.width == w && project.height == h)) {
                    checkpoint();
                    project.resize(w, h);
                    changed();
                }
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View")) {
            ui::MenuItem("Fullscreen", "F11", &fullscreen_);
            ui::MenuItem("Compact color sets", nullptr, &guidance_);
            ui::MenuItem("Scene preview", nullptr, &show_scene_preview_);
            ui::MenuItem("Brush settings", nullptr, &show_brush_settings_);
            ui::MenuItem("Tile editor", nullptr, &show_tiles_);
            if (ui::MenuItem("Animation timeline", nullptr, &show_timeline_) && !show_timeline_)
                playing_ = false;
            ui::MenuItem("NES limits", nullptr, &show_hardware_);
            ui::MenuItem("NES preview", nullptr, &show_emulator_);
            if (ui::MenuItem("Onion skin", nullptr, &onion_))
                refresh_ = true;
            ui::MenuItem("Onion skin settings...", nullptr, &show_onion_settings_);
            if (ui::MenuItem("Advanced layout", nullptr, &advanced_panels_)) {
                layout_built_ = false;
                force_layout_ = true;
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Tools")) {
            ui::MenuItem("Brush library", nullptr, &show_brush_library_);
            ui::MenuItem("Bitmap text", nullptr, &show_bitmap_text_);
            ui::MenuItem("Gradient fill", nullptr, &show_gradients_);
#ifndef __EMSCRIPTEN__
            ImGui::Separator();
            if (ui::MenuItem("MCP server...", nullptr, mcp_server_ && mcp_server_->running()))
                show_mcp_ = true;
            ui::help("MCP server",
                     "Start or stop a local MCP connection to this editor. Connected clients can "
                     "inspect, edit, preview and export the open artwork.");
#endif
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Help")) {
            if (ui::MenuItem("Drawing guide"))
                show_drawing_guide_ = true;
            if (ui::MenuItem("Start here..."))
                guard([this] { show_start(); });
            ui::TextUnformatted("tilezeitjobb 2.0 - NES pixel and tile studio");
            ImGui::Separator();
            ui::TextUnformatted("P pencil  E eraser  F fill  A attributes  I picker");
            ui::TextUnformatted("S sprite  M move  R select  V stamp  T tile  H pan");
            ui::TextUnformatted("L line  B rectangle  O ellipse  K polyline  G polygon");
            ui::TextUnformatted("Shift constrains shapes; Esc cancels the preview.");
            ui::TextUnformatted("1..4 color slot  [ / ] brush size");
            ui::TextUnformatted("Middle drag pans; mouse wheel zooms.");
            ui::TextUnformatted("ROM export uses the current frame and initial viewport.");
            ImGui::EndMenu();
        }
        ImGui::Separator();
        ui::Text("%s%s",
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
            if (t == Tool::TileBrush) {
                show_tiles_ = true;
                layer_ = -1;
                palette_group_ = 0;
            }
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
    ui::SeparatorText("Tools");
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
        relevant |= t == Tool::Line || t == Tool::Rectangle || t == Tool::Ellipse ||
                    t == Tool::Polyline || t == Tool::Polygon || t == Tool::Pencil ||
                    t == Tool::Eraser || t == Tool::Fill;
        if (layer_ >= 0)
            relevant |= t == Tool::Sprite || t == Tool::Move;
        else
            relevant |=
                t == Tool::Select || t == Tool::Stamp || t == Tool::TileBrush || t == Tool::Palette;
        if (embedded && !relevant)
            continue;
        if (column++ % 2)
            ImGui::SameLine();
        float width =
            (ImGui::GetContentRegionAvail().x - (column % 2 ? ImGui::GetStyle().ItemSpacing.x : 0));
        if (column % 2)
            width /= 2;
        std::string id = std::string("##tool-") + name;
        bool selected = ui::Selectable(id.c_str(), tool_ == t, 0, {width, 28});
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
        if (selected) {
            cancel_drawing();
            tool_ = t;
            if (t == Tool::TileBrush)
                show_tiles_ = true;
            if (t == Tool::TileBrush || t == Tool::Stamp || t == Tool::Palette) {
                layer_ = -1;
                palette_group_ = 0;
            }
        }
    }
    if (tool_ == Tool::Pencil || tool_ == Tool::Eraser || shape_tool() || path_tool()) {
        ImGui::Spacing();
        ui::TextUnformatted("Brush size");
        for (int size : {1, 2, 4, 8}) {
            if (size != 1)
                ImGui::SameLine();
            char label[12];
            std::snprintf(label, sizeof(label), "%d px", size);
            if (ui::Selectable(label, brush_ == size, 0, {45, 25}))
                brush_ = size;
        }
    }
    if (shape_tool()) {
        if (tool_ != Tool::Line)
            ui::Checkbox("Filled shape", &shape_filled_);
    }
    if (path_tool()) {
        if (tool_ == Tool::Polygon)
            ui::Checkbox("Filled polygon", &shape_filled_);
        if (!path_.empty()) {
            ui::Text("%zu points | Backspace removes last", path_.size());
            if (ui::Button("Finish"))
                finish_path();
            ImGui::SameLine();
            if (ui::Button("Cancel"))
                cancel_drawing();
        }
    }
    if (tool_ != Tool::TileBrush) {
        ui::SeparatorText("Symmetry");
        ui::Checkbox("Mirror left / right", &mirror_x_);
        ui::Checkbox("Mirror top / bottom", &mirror_y_);
        if (ui::CollapsingHeader("Symmetry settings")) {
            ui::Checkbox("Custom center", &custom_symmetry_);
            if (custom_symmetry_) {
                ui::SliderFloat("Center X", &symmetry_x_, 0,
                                float(layer_ >= 0 ? 7 : project.width - 1), "%.1f");
                ui::SliderFloat("Center Y", &symmetry_y_, 0,
                                float(layer_ >= 0 ? 7 : project.height - 1), "%.1f");
            }
            ui::SliderInt("Radial copies", &symmetry_sectors_, 1, 16);
        }
    }
    if (ui::CollapsingHeader("Canvas options", embedded ? 0 : ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ui::Checkbox("Show sprites", &show_sprites_))
            refresh_ = true;
        if (ui::Checkbox("Preview sprite limits", &hardware_preview_))
            refresh_ = true;
        if (ui::Checkbox("Onion skin", &onion_))
            refresh_ = true;
        ui::Checkbox("Highlight active color set", &highlight_);
        ui::Checkbox("Reference overlay", &reference_overlay_);
        if (ui::Checkbox("Scanlines", &pal_test_))
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
    ui::SeparatorText("Paint color");
    for (int group = 0; group < 2; ++group) {
        if (!advanced_panels_ && group != (layer_ >= 0 ? 1 : 0))
            continue;
        ImGui::PushID(group);
        ui::TextUnformatted(group ? "Sprites" : "Background");
        auto &palettes = group ? project.sprite_palettes : project.background;
        if (embedded && guidance_) {
            const char *sets[]{"Color set 1", "Color set 2", "Color set 3", "Color set 4"};
            if (ui::Combo("##active-color-set", &palette_, sets, 4)) {
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
            if (ui::RadioButton(label, active)) {
                palette_ = row;
                palette_group_ = group;
                if (group == 0)
                    layer_ = -1;
            }
            for (int c = 0; c < 4; ++c) {
                ImGui::SameLine();
                ImGui::PushID(c);
                if (ui::ColorButton("##color", color(palettes[row][c]),
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
                        if (ui::ColorButton("##nes", color(Byte(n)), ImGuiColorEditFlags_NoTooltip,
                                            {23, 23})) {
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
    ui::Text("Color set %d / paint color %d", palette_ + 1, slot_ + 1);
    ImGui::Separator();
    if (ui::Button("Edit color sets...", {-1, 28})) {
        palette_edit_group_ = palette_group_;
        palette_edit_index_ = palette_;
        palette_editor_open_ = true;
    }
    if (!project.frames[frame_].layers.empty()) {
        if (!embedded)
            ImGui::End();
        return;
    }
    if (project.reference.empty()) {
        if (!embedded && ui::Button("Import a picture..."))
            file_dialog(FileAction::Background);
        if (!embedded)
            ImGui::End();
        return;
    }
    ui::TextUnformatted("IMAGE CONVERSION");
    bool dithering = project.dither;
    if (ui::Checkbox("Ordered dithering", &dithering)) {
        checkpoint();
        project.dither = dithering;
        changed();
    }
    if (ui::Button("Reapply palettes") && !project.reference.empty()) {
        checkpoint();
        project.quantize(frame_);
        changed();
    }
    ImGui::SameLine();
    if (ui::Button("Auto palettes") && !project.reference.empty()) {
        checkpoint();
        project.quantize(frame_, true);
        changed();
    }
    int offsets[]{project.offset_x, project.offset_y};
    if (ui::InputInt2("Image offset", offsets)) {
        checkpoint();
        project.offset_x = std::clamp(offsets[0], -4096, 4096);
        project.offset_y = std::clamp(offsets[1], -4096, 4096);
        changed();
    }
    if (ui::Button("Clear background paint") && !project.reference.empty()) {
        checkpoint();
        project.frames[frame_].paint_mask.clear();
        project.quantize(frame_);
        changed();
    }
    ui::Checkbox("Watch source images", &watch_sources_);
    ImGui::TextWrapped("Reapply and source changes preserve painted pixels. Clear background paint "
                       "restores the reference conversion.");
    if (!embedded)
        ImGui::End();
}
void Editor::copy_selection(bool cut) {
    if (cut && !target_editable())
        return;
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
            if (cut)
                project.write_background_pixel(frame_, index, 255);
        }
    if (cut)
        changed();
    status_ = "Copied indexed pixels; V stamps the brush";
}
void Editor::apply_tool(int x, int y, bool first, bool right) {
    auto &f = project.frames[frame_];
    Byte paint = Byte(right ? 0 : slot_);
    if (tool_ == Tool::TileBrush) {
        if (!right && !ImGui::GetIO().KeyAlt && !target_editable())
            return;
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
        } else if (layer_ >= 0 && target_editable()) {
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
        if (!target_editable())
            return;
        f.attributes[(y / 16) * project.block_width() + x / 16] = Byte(palette_);
        changed();
        return;
    }
    if (tool_ == Tool::Stamp) {
        if (!target_editable())
            return;
        if (!clipboard_.empty() && (first || x != last_x_ || y != last_y_)) {
            int sx = brush_snap_ ? (x / 8) * 8 : x, sy = brush_snap_ ? (y / 8) * 8 : y;
            stamp_brush(project, frame_, sx, sy, {clip_width_, clip_height_, clipboard_},
                        brush_transparent_, brush_wrap_);
            changed();
        }
        return;
    }
    if (tool_ == Tool::Fill) {
        if (!target_editable())
            return;
        if (first) {
            if (layer_ >= 0) {
                const auto &s = f.sprites.at(layer_);
                flood_fill_sprite(project, frame_, layer_, x - s.x, y - s.y, Byte(paint));
            } else
                flood_fill(project, frame_, x, y, paint, right ? -1 : palette_, right);
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
    return symmetric_pixels(drawing_pixels(shape, x0, y0, x1, y1, width, height,
                                           shape != DrawingShape::Line && shape_filled_, brush_),
                            width, height, custom_symmetry_ ? symmetry_x_ : (width - 1) * .5f,
                            custom_symmetry_ ? symmetry_y_ : (height - 1) * .5f, mirror_x_,
                            mirror_y_, symmetry_sectors_);
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
    return symmetric_pixels(path_pixels(points, width, height, tool_ == Tool::Polygon,
                                        tool_ == Tool::Polygon && shape_filled_, brush_),
                            width, height, custom_symmetry_ ? symmetry_x_ : (width - 1) * .5f,
                            custom_symmetry_ ? symmetry_y_ : (height - 1) * .5f, mirror_x_,
                            mirror_y_, symmetry_sectors_);
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
    if (points.empty() || !target_editable())
        return;
    bool erase = right || tool_ == Tool::Eraser;
    paint_pixels(project, frame_, points, Byte(erase ? 0 : slot_), erase ? -1 : palette_, layer_,
                 erase);
    changed();
}
void Editor::canvas() {
    if (!advanced_panels_)
        place_panel(canvas_rect_);
    ImGui::Begin("Canvas", nullptr,
                 advanced_panels_
                     ? 0
                     : ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                           ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking |
                           ImGuiWindowFlags_NoBringToFrontOnFocus);
    ui::Text(
        "%s | %s", tool_label(),
        layer_ >= 0
            ? project.frames[frame_].sprites.at(layer_).name.c_str()
            : project.frames[frame_].layers.at(project.frames[frame_].active_layer).name.c_str());
    ImGui::SameLine();
    auto &colors = palette_group_ ? project.sprite_palettes : project.background;
    ui::ColorButton("Current paint", color(colors[palette_][slot_]), ImGuiColorEditFlags_NoTooltip,
                    {25, 21});
    ImGui::SameLine();
    ui::Text("Set %d / color %d", palette_ + 1, slot_ + 1);
    ImGui::SameLine();
    if (ui::SmallButton("Fit canvas"))
        fit_countdown_ = 0;
    ImGui::SetNextItemWidth(145);
    ui::SliderFloat("Zoom", &zoom_, .5f, 12.f, "%.1fx");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(90);
    const char *grids[]{"No grid", "Pixels", "8 x 8", "16 x 16"};
    int grid = grid_ == 0 ? 0 : grid_ == 1 ? 1 : grid_ == 8 ? 2 : 3;
    if (ui::Combo("Grid", &grid, grids, 4))
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
    ui::Image(texture_id(texture_), size);
    bool hover = ImGui::IsItemHovered();
    auto *draw = ImGui::GetWindowDrawList();
    if (onion_ && previous_texture_)
        draw->AddImage(texture_id(previous_texture_), origin,
                       {origin.x + size.x, origin.y + size.y}, {0, 0}, {1, 1},
                       IM_COL32(255, 255, 255, 255));
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
            bool paint_target = tool_ != Tool::Pan && tool_ != Tool::Picker &&
                                tool_ != Tool::Select && tool_ != Tool::Move &&
                                tool_ != Tool::Sprite &&
                                !(tool_ == Tool::TileBrush && (io.KeyAlt || ImGui::IsMouseDown(1)));
            if (first && paint_target && !target_editable())
                first = false;
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
    if (mirror_x_ || mirror_y_ || symmetry_sectors_ > 1) {
        float ox = 0, oy = 0, w = float(project.width), h = float(project.height);
        if (layer_ >= 0) {
            const auto &s = project.frames[frame_].sprites.at(layer_);
            ox = float(s.x);
            oy = float(s.y);
            w = h = 8;
        }
        float cx = custom_symmetry_ ? symmetry_x_ + .5f : w / 2,
              cy = custom_symmetry_ ? symmetry_y_ + .5f : h / 2;
        if (mirror_x_)
            draw->AddLine({origin.x + (ox + cx) * zoom_, origin.y + oy * zoom_},
                          {origin.x + (ox + cx) * zoom_, origin.y + (oy + h) * zoom_},
                          IM_COL32(100, 220, 255, 150));
        if (mirror_y_)
            draw->AddLine({origin.x + ox * zoom_, origin.y + (oy + cy) * zoom_},
                          {origin.x + (ox + w) * zoom_, origin.y + (oy + cy) * zoom_},
                          IM_COL32(100, 220, 255, 150));
        draw->AddCircle({origin.x + (ox + cx) * zoom_, origin.y + (oy + cy) * zoom_}, 4,
                        IM_COL32(255, 205, 90, 220));
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
    if (!embedded)
        layer_stack();
    if (layer_ >= 0 && static_cast<std::size_t>(layer_) < f.sprites.size()) {
        auto &s = f.sprites[layer_];
        ImGui::Separator();
        ui::TextUnformatted("Sprite pixels (click to paint)");
        auto origin = ImGui::GetCursorScreenPos();
        ui::InvisibleButton("sprite-pixels", {128, 128});
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
        if (ImGui::IsItemHovered() && (ImGui::IsMouseDown(0) || ImGui::IsMouseDown(1)) &&
            target_editable()) {
            if (ImGui::IsMouseClicked(0) || ImGui::IsMouseClicked(1))
                checkpoint();
            int x = int((ImGui::GetIO().MousePos.x - origin.x) / 16),
                y = int((ImGui::GetIO().MousePos.y - origin.y) / 16);
            s.pixels[y * 8 + x] = Byte(ImGui::IsMouseDown(1) ? 0 : slot_);
            changed();
        }
        char name[256];
        std::snprintf(name, sizeof(name), "%s", s.name.c_str());
        if (ui::InputText("Name", name, sizeof(name))) {
            checkpoint();
            s.name = name;
            changed();
        }
        ImGui::BeginDisabled(s.locked || f.sprites_locked);
        int position[]{s.x, s.y};
        if (ui::InputInt2("Position", position)) {
            checkpoint();
            s.x = std::clamp(position[0], -4096, 4096);
            s.y = std::clamp(position[1], -4096, 4096);
            changed();
        }
        int pal = s.palette;
        if (ui::SliderInt("Palette", &pal, 0, 3)) {
            checkpoint();
            s.palette = pal;
            palette_ = pal;
            changed();
        }
        for (auto [label, value] : {std::pair{"Flip X", &s.flip_x},
                                    {"Flip Y", &s.flip_y},
                                    {"Behind background", &s.behind}}) {
            bool v = *value;
            if (ui::Checkbox(label, &v)) {
                checkpoint();
                *value = v;
                changed();
            }
        }
        ImGui::EndDisabled();
    } else {
        ImGui::TextWrapped("Select a sprite to edit its pixels, palette, position, priority and "
                           "flipping. Earlier objects have higher OAM priority.");
    }
    if (!embedded)
        ImGui::End();
}
void Editor::preview(bool *open) {
    ImGui::SetNextWindowSize({300, 350}, ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Preview", open, ImGuiWindowFlags_HorizontalScrollbar)) {
        ImGui::End();
        return;
    }
    ui::Checkbox("Native size (1:1)", &preview_native_size_);
    ImVec2 available = ImGui::GetContentRegionAvail();
    float scale =
        std::max(.1f, std::min(available.x / project.width, (available.y - 25) / project.height));
    if (preview_native_size_)
        scale = 1;
    ui::Image(texture_id(texture_), {project.width * scale, project.height * scale});
    ui::TextUnformatted("Nearest-neighbor / current frame");
    ImGui::End();
}
void Editor::timeline(bool *open) {
    if (!advanced_panels_)
        place_panel(timeline_rect_);
    ImGui::Begin("Timeline", open,
                 advanced_panels_
                     ? 0
                     : ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                           ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking |
                           ImGuiWindowFlags_NoBringToFrontOnFocus);
    if (ui::Button(playing_ ? "Pause" : "Play")) {
        playing_ = !playing_;
        next_frame_ = ImGui::GetTime() + project.frames[frame_].duration_ms / 1000.;
    }
    ImGui::SameLine();
    if (ui::Button("Duplicate") && project.frames.size() < 256) {
        checkpoint();
        auto duplicate = project.frames[frame_];
        project.frames.insert(project.frames.begin() + static_cast<std::ptrdiff_t>(frame_ + 1),
                              std::move(duplicate));
        ++frame_;
        layer_ = -1;
        changed();
    }
    ImGui::SameLine();
    if (ui::Button("Blank frame") && project.frames.size() < 256) {
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
    if (ui::Button("Delete frame") && project.frames.size() > 1) {
        checkpoint();
        project.frames.erase(project.frames.begin() + static_cast<std::ptrdiff_t>(frame_));
        frame_ = std::min(frame_, project.frames.size() - 1);
        layer_ = -1;
        changed();
    }
    ImGui::SameLine();
    int ms = project.frames[frame_].duration_ms;
    ImGui::SetNextItemWidth(100);
    if (ui::InputInt("ms", &ms)) {
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
        if (ui::Selectable(label, i == frame_, 0, {68, 43})) {
            frame_ = i;
            layer_ = -1;
            refresh_ = true;
        }
    }
    ImGui::EndChild();
    ImGui::End();
}
void Editor::hardware(bool embedded, bool *open) {
    if (!embedded && focus_hardware_) {
        ImGui::SetNextWindowFocus();
        focus_hardware_ = false;
    }
    if (!embedded && !ImGui::Begin("Hardware", open)) {
        ImGui::End();
        return;
    }
    ui::TextUnformatted("NES / NROM-128");
    ImGui::Separator();
    if (compiled_) {
        ImGui::TextColored({.45f, .85f, .64f, 1}, "Ready for ROM export");
        ui::Text("Background tiles  %d / 256", compiled_->background_tiles);
        ui::Text("Sprite tiles      %d / 256", compiled_->sprite_tiles);
        ui::Text("OAM sprites       %d / 64", compiled_->sprite_count);
        ui::Text("Nametables        %zu", compiled_->nametables.size() / 1024);
    } else {
        ImGui::TextColored({1, .55f, .4f, 1}, "Export blocked");
        ImGui::TextWrapped("%s", compile_error_.c_str());
    }
    ImGui::Separator();
    std::array<float, 240> lines{};
    for (const auto &s : project.frames[frame_].sprites)
        if (s.visible && project.frames[frame_].sprites_visible)
            for (int y = std::max(0, s.y); y < std::min(240, s.y + 8); ++y)
                ++lines[y];
    ui::TextUnformatted("Sprites per scanline (limit 8)");
    ImGui::PlotHistogram("##scanlines", lines.data(), 240, 0, nullptr, 0, 16, {0, 65});
    ImGui::TextWrapped("Export includes the current frame. The ROM displays the first 256x240 "
                       "viewport; a second nametable is included for use in your game.");
    ui::TextUnformatted("16 KiB PRG + 8 KiB CHR / mapper 0");
    if (ui::Button("Export ROM + assets..."))
        file_dialog(FileAction::Export);
    ImGui::Separator();
    ImGui::TextWrapped("The .nes file runs in an emulator or on a compatible flash cartridge. Raw "
                       "CHR, nametable, palette, OAM and C header files are also written.");
    if (!embedded)
        ImGui::End();
}
void Editor::attributes() {
    ImGui::Begin("Attributes");
    ui::TextUnformatted("16x16 background palette regions");
    ui::TextUnformatted("Click to assign the active palette");
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
            if (ui::Button(label, {19, 19})) {
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
        if (ui::Button("Import sprite sheet..."))
            file_dialog(FileAction::Sprites);
    } else {
        ui::Text("Tile %d,%d / palette %d", tile_x_, tile_y_, palette_);
        float scale =
            std::clamp(ImGui::GetContentRegionAvail().x / project.sprite_sheet.width, .25f, 4.f);
        ImGui::BeginChild("sheet-scroll", {0, 0}, false, ImGuiWindowFlags_HorizontalScrollbar);
        auto pos = ImGui::GetCursorScreenPos();
        ui::Image(texture_id(sheet_texture_),
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
    ui::Text("Original animated palette test / palette %d", palette_);
    float scale = std::max(.5f, std::min(ImGui::GetContentRegionAvail().x / w,
                                         (ImGui::GetContentRegionAvail().y - 30) / h));
    ui::Image(texture_id(effect_texture_), {w * scale, h * scale});
    ImGui::TextWrapped("A palette test animation; this is separate from NES signal emulation.");
    ImGui::End();
}
void Editor::popups() {
    if (!error_.empty())
        ImGui::OpenPopup("Operation failed");
    if (ImGui::BeginPopupModal("Operation failed", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::PushTextWrapPos(520);
        ui::TextUnformatted(error_.c_str());
        ImGui::PopTextWrapPos();
        if (ui::Button("OK")) {
            error_.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    if (pending_ && file_action_ != FileAction::Save)
        ImGui::OpenPopup("Unsaved changes");
    if (ImGui::BeginPopupModal("Unsaved changes", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ui::TextUnformatted("Save your changes before continuing?");
        if (ui::Button("Save")) {
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
        if (ui::Button("Discard")) {
            auto action = std::move(pending_);
            pending_ = {};
            dirty_ = false;
            ImGui::CloseCurrentPopup();
            action();
        }
        ImGui::SameLine();
        if (ui::Button("Cancel")) {
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
        if (ui::Button("Parent directory") && browse_dir_.has_parent_path())
            browse_dir_ = browse_dir_.parent_path();
        ImGui::SameLine();
        if (ui::Button("Project directory"))
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
            if (ui::Selectable(label.c_str(), false, ImGuiSelectableFlags_AllowDoubleClick)) {
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
        ui::InputText("Path", file_path_, sizeof(file_path_));
        ui::TextUnformatted("Type an absolute path or a filename in the current directory.");
        if (ui::Button("Choose") && file_path_[0]) {
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
        if (ui::Button("Cancel")) {
            if (file_action_ == FileAction::Save)
                pending_ = {};
            file_action_ = FileAction::None;
            ImGui::CloseCurrentPopup();
        }
        if (ImGui::BeginPopupModal("Replace existing file?", nullptr,
                                   ImGuiWindowFlags_AlwaysAutoResize)) {
            ui::TextUnformatted("Replace the existing file and matching export assets?");
            if (ui::Button("Replace")) {
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
            if (ui::Button("Cancel"))
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
    bool image_layers = false;
    std::map<std::string, Image> reloads;
    for (const auto &f : project.frames)
        for (const auto &layer : f.layers) {
            if (layer.kind != LayerKind::Image)
                continue;
            image_layers = true;
            if (layer.source_path.empty())
                continue;
            auto path = path_from_utf8(layer.source_path);
            if (path.is_relative())
                path = (project_path_.empty() ? std::filesystem::current_path()
                                              : project_path_.parent_path()) /
                       path;
            std::error_code ec;
            auto time = std::filesystem::last_write_time(path, ec);
            if (ec)
                continue;
            auto [it, first] = layer_source_times_.try_emplace(layer.source_path, time);
            if (first || it->second == time || reloads.contains(layer.source_path))
                continue;
            try {
                reloads.emplace(layer.source_path, load_image(path));
                it->second = time;
            } catch (const std::exception &e) {
                status_ = std::string("Source reload pending: ") + e.what();
            }
        }
    if (!reloads.empty()) {
        checkpoint();
        for (auto &f : project.frames)
            for (auto &layer : f.layers) {
                auto it = reloads.find(layer.source_path);
                if (layer.kind == LayerKind::Image && it != reloads.end()) {
                    layer.source = it->second;
                    layer.conversion_stamp = 0;
                }
            }
        if (auto it = reloads.find(project.image_path); it != reloads.end())
            project.reference = it->second;
        for (std::size_t i = 0; i < project.frames.size(); ++i)
            project.compose_layers(i);
        changed();
        status_ = "Reloaded image layers; paint layers and baked copies were preserved.";
    }
    if (!image_layers)
        poll(project.image_path, false);
    poll(project.sprite_path, true);
}
bool Editor::tick() {
    ImGuiErrorRecoveryState ui_state;
    ImGui::ErrorRecoveryStoreState(&ui_state);
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
        project.ensure_layers(frame_);
#ifndef __EMSCRIPTEN__
        poll_mcp();
#endif
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
        info_windows();
#ifndef __EMSCRIPTEN__
        mcp_window();
#endif
        popups();
        start_screen();
        palette_editor();
    } catch (const std::exception &e) {
        auto &io = ImGui::GetIO();
        const bool assert_errors = io.ConfigErrorRecoveryEnableAssert;
        io.ConfigErrorRecoveryEnableAssert = false;
        ImGui::ErrorRecoveryTryToRecoverState(&ui_state);
        io.ConfigErrorRecoveryEnableAssert = assert_errors;
        error_ = e.what();
        std::cerr << "Editor: " << e.what() << "\n";
    }
    return !quit_;
}
} // namespace tzj
