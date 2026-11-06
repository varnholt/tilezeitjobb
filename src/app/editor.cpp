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
namespace tzj {
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
    project = demo_project();
    history_.clear();
    frame_ = 0;
    layer_ = -1;
    dirty_ = false;
    refresh_ = true;
}
void Editor::open(const std::filesystem::path &path) {
    auto loaded = load_project(path);
    project = std::move(loaded);
    project_path_ = path;
    frame_ = 0;
    layer_ = -1;
    selection_ = {};
    history_.clear();
    dirty_ = false;
    refresh_ = true;
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
void Editor::file_dialog(FileAction action) {
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
            status_ = "Exported .nes, .chr, .nam, .pal, .oam and .h";
            break;
        }
        case FileAction::Png:
            save_png(path, project.render(frame_, show_sprites_, hardware_preview_));
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
    refresh_ = false;
}
void Editor::dock_layout() {
    ImGuiID dock = ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());
    if (layout_built_)
        return;
    layout_built_ = true;
    if (ImGui::DockBuilderGetNode(dock) && ImGui::DockBuilderGetNode(dock)->IsSplitNode())
        return;
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
    ImGui::DockBuilderDockWindow("Palettes", left);
    ImGui::DockBuilderDockWindow("Canvas", center);
    ImGui::DockBuilderDockWindow("Timeline", bottom);
    ImGui::DockBuilderDockWindow("Layers & sprites", right_top);
    ImGui::DockBuilderDockWindow("Hardware", right);
    ImGui::DockBuilderDockWindow("Preview", right);
    ImGui::DockBuilderDockWindow("Attributes", right);
    ImGui::DockBuilderDockWindow("Sprite sheet", right);
    ImGui::DockBuilderDockWindow("Palette test effect", right);
    ImGui::DockBuilderFinish(dock);
}
void Editor::menus() {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New", "Ctrl+N"))
                guard([this] {
                    project = Project{};
                    project_path_.clear();
                    frame_ = 0;
                    layer_ = -1;
                    history_.clear();
                    dirty_ = false;
                    refresh_ = true;
                });
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
        if (ImGui::BeginMenu("Help")) {
            ImGui::TextUnformatted("tilezeitjobb 2.0 - NES pixel and tile studio");
            ImGui::Separator();
            ImGui::TextUnformatted("P pencil  E eraser  F fill  A attributes  I picker");
            ImGui::TextUnformatted("S sprite  M move sprite  R select  V stamp  H pan");
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
            guard([this] {
                project = Project{};
                project_path_.clear();
                frame_ = 0;
                layer_ = -1;
                selection_ = {};
                history_.clear();
                dirty_ = false;
                refresh_ = true;
            });
        if (ImGui::IsKeyPressed(ImGuiKey_S))
            save(io.KeyShift);
        if (ImGui::IsKeyPressed(ImGuiKey_O))
            guard([this] { file_dialog(FileAction::Open); });
        if (ImGui::IsKeyPressed(ImGuiKey_E))
            file_dialog(FileAction::Export);
        if (ImGui::IsKeyPressed(ImGuiKey_Z) && history_.undo(project)) {
            frame_ = std::min(frame_, project.frames.size() - 1);
            layer_ = -1;
            changed();
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Y) && history_.redo(project)) {
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
                          {ImGuiKey_F, Tool::Fill},
                          {ImGuiKey_A, Tool::Palette},
                          {ImGuiKey_I, Tool::Picker},
                          {ImGuiKey_S, Tool::Sprite},
                          {ImGuiKey_M, Tool::Move},
                          {ImGuiKey_R, Tool::Select},
                          {ImGuiKey_V, Tool::Stamp},
                          {ImGuiKey_H, Tool::Pan}})
        if (ImGui::IsKeyPressed(key))
            tool_ = t;
    for (int i = 0; i < 4; ++i)
        if (ImGui::IsKeyPressed(static_cast<ImGuiKey>(ImGuiKey_1 + i)))
            slot_ = i;
    if (ImGui::IsKeyPressed(ImGuiKey_LeftBracket))
        brush_ = std::max(1, brush_ / 2);
    if (ImGui::IsKeyPressed(ImGuiKey_RightBracket))
        brush_ = std::min(8, brush_ * 2);
}
void Editor::tools() {
    ImGui::Begin("Tools");
    ImGui::TextUnformatted("DRAW / ARRANGE");
    ImGui::Separator();
    int i = 0;
    for (auto [name, t] : {std::pair{"Pencil  P", Tool::Pencil},
                           {"Eraser  E", Tool::Eraser},
                           {"Fill  F", Tool::Fill},
                           {"Palette  A", Tool::Palette},
                           {"Pick  I", Tool::Picker},
                           {"Sprite  S", Tool::Sprite},
                           {"Move  M", Tool::Move},
                           {"Select  R", Tool::Select},
                           {"Stamp  V", Tool::Stamp},
                           {"Pan  H", Tool::Pan}}) {
        if (i++ % 2)
            ImGui::SameLine();
        if (ImGui::Selectable(name, tool_ == t, 0, {108, 25}))
            tool_ = t;
    }
    ImGui::Separator();
    ImGui::TextUnformatted("BRUSH");
    for (int size : {1, 2, 4, 8}) {
        if (size != 1)
            ImGui::SameLine();
        char label[12];
        std::snprintf(label, sizeof(label), "%d px", size);
        if (ImGui::RadioButton(label, brush_ == size))
            brush_ = size;
    }
    if (ImGui::SliderFloat("Zoom", &zoom_, 0.5f, 12.0f, "%.1fx")) {
    }
    ImGui::SetNextItemWidth(110);
    const char *grids[]{"None", "1 pixel", "8 pixels", "16 pixels"};
    int gi = grid_ == 0 ? 0 : grid_ == 1 ? 1 : grid_ == 8 ? 2 : 3;
    if (ImGui::Combo("Grid", &gi, grids, 4))
        grid_ = gi == 0 ? 0 : gi == 1 ? 1 : gi == 2 ? 8 : 16;
    if (ImGui::Checkbox("Show sprites", &show_sprites_))
        refresh_ = true;
    if (ImGui::Checkbox("Hardware sprite limits", &hardware_preview_))
        refresh_ = true;
    ImGui::Checkbox("Previous frame onion skin", &onion_);
    ImGui::Checkbox("Highlight current palette", &highlight_);
    ImGui::Checkbox("Reference overlay", &reference_overlay_);
    if (ImGui::Checkbox("Scanline preview", &pal_test_))
        refresh_ = true;
    ImGui::Separator();
    if (!clipboard_.empty()) {
        if (ImGui::Button("Flip brush X"))
            for (int y = 0; y < clip_height_; ++y)
                std::reverse(clipboard_.begin() + y * clip_width_,
                             clipboard_.begin() + (y + 1) * clip_width_);
        ImGui::SameLine();
        if (ImGui::Button("Flip brush Y"))
            for (int y = 0; y < clip_height_ / 2; ++y)
                for (int x = 0; x < clip_width_; ++x)
                    std::swap(clipboard_[y * clip_width_ + x],
                              clipboard_[(clip_height_ - 1 - y) * clip_width_ + x]);
    }
    if (selection_.width > 0 && ImGui::Button("Clear selection"))
        selection_ = {};
    ImGui::TextWrapped("%s", status_.c_str());
    ImGui::End();
}
void Editor::palettes() {
    ImGui::Begin("Palettes");
    ImGui::TextUnformatted("NES SUBPALETTES");
    for (int group = 0; group < 2; ++group) {
        ImGui::PushID(group);
        ImGui::TextUnformatted(group ? "Sprites" : "Background");
        auto &palettes = group ? project.sprite_palettes : project.background;
        for (int row = 0; row < 4; ++row) {
            ImGui::PushID(row);
            bool active = palette_ == row && ((layer_ >= 0) == (group == 1));
            if (ImGui::RadioButton("##palette", active)) {
                palette_ = row;
                if (group == 0)
                    layer_ = -1;
            }
            for (int c = 0; c < 4; ++c) {
                ImGui::SameLine();
                ImGui::PushID(c);
                if (ImGui::ColorButton("##color", color(palettes[row][c]),
                                       ImGuiColorEditFlags_NoTooltip, {32, 25})) {
                    palette_ = row;
                    slot_ = c;
                    if (group == 0)
                        layer_ = -1;
                    ImGui::OpenPopup("Choose NES color");
                }
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Slot %d: $%02X (click to edit)", c, palettes[row][c]);
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
    ImGui::Text("Active: palette %d / slot %d", palette_, slot_);
    for (int c = 0; c < 4; ++c) {
        if (c)
            ImGui::SameLine();
        char label[12];
        std::snprintf(label, sizeof(label), "%d##slot", c + 1);
        if (ImGui::RadioButton(label, c == slot_))
            slot_ = c;
    }
    ImGui::Separator();
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
    ImGui::End();
}
void Editor::copy_selection(bool cut) {
    if (selection_.width <= 0 || selection_.height <= 0)
        return;
    clip_width_ = selection_.width;
    clip_height_ = selection_.height;
    clipboard_.resize(clip_width_ * clip_height_);
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
        if (first && !clipboard_.empty()) {
            if (f.paint_mask.empty())
                f.paint_mask.assign(f.pixels.size(), 255);
            for (int cy = 0; cy < clip_height_; ++cy)
                for (int cx = 0; cx < clip_width_; ++cx)
                    if (x + cx < project.width && y + cy < project.height) {
                        auto i = (y + cy) * project.width + x + cx;
                        f.pixels[i] = clipboard_[cy * clip_width_ + cx];
                        f.paint_mask[i] = f.pixels[i];
                    }
            changed();
        }
        return;
    }
    if (tool_ == Tool::Fill) {
        if (first) {
            flood_fill(project, frame_, x, y, paint);
            changed();
        }
        return;
    }
    if (layer_ >= 0) {
        auto &s = f.sprites.at(layer_);
        for (int dy = 0; dy < brush_; ++dy)
            for (int dx = 0; dx < brush_; ++dx) {
                int px = x - s.x + dx, py = y - s.y + dy;
                if (px >= 0 && px < 8 && py >= 0 && py < 8)
                    s.pixels[(s.flip_y ? 7 - py : py) * 8 + (s.flip_x ? 7 - px : px)] =
                        tool_ == Tool::Eraser ? 0 : paint;
            }
        changed();
        return;
    }
    paint_line(project, frame_, first ? x : last_x_, first ? y : last_y_, x, y,
               tool_ == Tool::Eraser ? 0 : paint, brush_);
    changed();
}
void Editor::canvas() {
    ImGui::Begin("Canvas");
    ImGui::Text("%d x %d  |  Frame %zu  |  %s", project.width, project.height, frame_ + 1,
                layer_ < 0 ? "Background" : project.frames[frame_].sprites.at(layer_).name.c_str());
    ImGui::SameLine();
    if (ImGui::SmallButton("Fit")) {
        auto available = ImGui::GetContentRegionAvail();
        zoom_ = std::max(
            .5f, std::min(available.x / project.width, (available.y - 30) / project.height));
    }
    ImGui::BeginChild("canvas-scroll", {0, 0}, false,
                      ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    auto origin = ImGui::GetCursorScreenPos();
    ImVec2 size{project.width * zoom_, project.height * zoom_};
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
    if (hover && io.MouseWheel != 0)
        zoom_ = std::clamp(zoom_ + (io.MouseWheel > 0 ? .5f : -.5f), .5f, 12.f);
    if (hover) {
        int x = std::clamp(int((io.MousePos.x - origin.x) / zoom_), 0, project.width - 1),
            y = std::clamp(int((io.MousePos.y - origin.y) / zoom_), 0, project.height - 1);
        draw->AddRect({origin.x + x * zoom_, origin.y + y * zoom_},
                      {origin.x + (x + brush_) * zoom_, origin.y + (y + brush_) * zoom_},
                      IM_COL32(240, 240, 240, 180));
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle) ||
            (tool_ == Tool::Pan && ImGui::IsMouseDragging(ImGuiMouseButton_Left))) {
            ImGui::SetScrollX(ImGui::GetScrollX() - io.MouseDelta.x);
            ImGui::SetScrollY(ImGui::GetScrollY() - io.MouseDelta.y);
        } else if (!playing_) {
            bool first = ImGui::IsMouseClicked(0) || ImGui::IsMouseClicked(1);
            if (first) {
                stroke_ = true;
                start_x_ = last_x_ = x;
                start_y_ = last_y_ = y;
                if (tool_ != Tool::Picker && tool_ != Tool::Select && tool_ != Tool::Pan)
                    checkpoint();
            }
            if (stroke_ && (ImGui::IsMouseDown(0) || ImGui::IsMouseDown(1))) {
                apply_tool(x, y, first, ImGui::IsMouseDown(1));
                last_x_ = x;
                last_y_ = y;
            }
        }
        ImGui::SetTooltip(
            "%d, %d | Tile %d,%d | Palette %d", x, y, x / 8, y / 8,
            project.frames[frame_].attributes[(y / 16) * project.block_width() + x / 16]);
    }
    if (!ImGui::IsMouseDown(0) && !ImGui::IsMouseDown(1))
        stroke_ = false;
    ImGui::EndChild();
    ImGui::End();
}
void Editor::layers() {
    ImGui::Begin("Layers & sprites");
    auto &f = project.frames[frame_];
    if (ImGui::Selectable("Background nametable", layer_ < 0))
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
    ImGui::Begin("Timeline");
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
void Editor::hardware() {
    if (focus_hardware_) {
        ImGui::SetNextWindowFocus();
        focus_hardware_ = false;
    }
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
void Editor::sprite_sheet() {
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
        tools();
        palettes();
        canvas();
        layers();
        timeline();
        preview();
        hardware();
        attributes();
        sprite_sheet();
        palette_effect();
        popups();
    } catch (const std::exception &e) {
        error_ = e.what();
        std::cerr << "Editor: " << e.what() << "\n";
    }
    return !quit_;
}
} // namespace tzj
