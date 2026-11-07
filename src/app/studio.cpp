#include "app/editor.hpp"
#include "app/ui_help.hpp"
#include <algorithm>
#include <cstdio>
#include <imgui.h>

namespace tzj {
const Metasprite *Editor::selected_group() const {
    const auto &f = project.frames.at(frame_);
    if (group_ < 0 || layer_ < 0 || layer_ >= int(f.sprites.size()) ||
        f.sprites[layer_].group != group_)
        return nullptr;
    auto it = std::ranges::find(f.groups, group_, &Metasprite::id);
    return it == f.groups.end() ? nullptr : &*it;
}
PixelRect Editor::target_bounds() const {
    if (selected_group())
        return metasprite_bounds(project, frame_, group_);
    if (layer_ >= 0) {
        const auto &s = project.frames[frame_].sprites.at(layer_);
        return {s.x, s.y, 8, 8};
    }
    return {0, 0, project.width, project.height};
}
void Editor::group_action(int action) {
    try {
        auto candidate = project;
        int id = selected_group() ? group_ : -1;
        if (action == 0)
            id = create_metasprite(candidate, frame_, 16, 16, palette_, 120, 112, "Character");
        else if (action == 1) {
            auto indices = selected_sprites_;
            if (indices.empty() && layer_ >= 0)
                indices.push_back(layer_);
            id = group_sprites(candidate, frame_, indices, "Metasprite");
        } else if (id < 0)
            return;
        else if (action == 2) {
            ungroup_sprites(candidate, frame_, id);
            id = -1;
        } else if (action == 3)
            id = duplicate_metasprite(candidate, frame_, id);
        else if (action == 4) {
            delete_metasprite(candidate, frame_, id);
            id = -1;
        } else
            transform_metasprite(candidate, frame_, id,
                                 action == 5   ? BrushTransform::FlipX
                                 : action == 6 ? BrushTransform::FlipY
                                 : action == 7 ? BrushTransform::RotateCW
                                               : BrushTransform::RotateCCW);
        cancel_drawing();
        checkpoint();
        project.frames[frame_] = std::move(candidate.frames[frame_]);
        group_ = id;
        selected_sprites_.clear();
        if (id >= 0) {
            const auto &sprites = project.frames[frame_].sprites;
            auto it = std::ranges::find(sprites, id, &Sprite::group);
            layer_ = int(it - sprites.begin());
            palette_group_ = 1;
            tool_ = Tool::Pencil;
            focus_sprite_ = true;
        } else
            layer_ = -1;
        changed();
    } catch (const std::exception &e) {
        error_ = e.what();
    }
}
void Editor::group_properties() {
    const auto *selected = selected_group();
    if (!selected)
        return;
    ui::SeparatorText("Metasprite properties");
    auto bounds = target_bounds();
    ui::Text("%d x %d pixels", bounds.width, bounds.height);
    char name[129];
    std::snprintf(name, sizeof(name), "%s", selected->name.c_str());
    ImGui::SetNextItemWidth(std::max(100.f, ImGui::GetContentRegionAvail().x - 126));
    if (ui::InputText("Name", name, sizeof(name)) && name[0]) {
        checkpoint();
        auto &groups = project.frames[frame_].groups;
        std::ranges::find(groups, group_, &Metasprite::id)->name = name;
        changed();
    }
    ImGui::BeginDisabled(!target_editable(false));
    int position[]{bounds.x, bounds.y};
    if (ui::InputInt2("Position", position)) {
        auto candidate = project;
        move_metasprite(candidate, frame_, group_, position[0] - bounds.x, position[1] - bounds.y);
        checkpoint();
        project.frames[frame_] = std::move(candidate.frames[frame_]);
        changed();
    }
    selected = selected_group();
    int origin[]{selected->origin_x, selected->origin_y};
    if (ui::InputInt2("Origin", origin)) {
        checkpoint();
        auto &g = *std::ranges::find(project.frames[frame_].groups, group_, &Metasprite::id);
        g.origin_x = std::clamp(origin[0], -4096, 4096);
        g.origin_y = std::clamp(origin[1], -4096, 4096);
        changed();
    }
    ui::help("Origin", "Character anchor in canvas coordinates, saved in sprite-sheet metadata. "
                       "Moving the character moves its anchor.");
    if (ui::Button("Flip H"))
        group_action(5);
    ImGui::SameLine();
    if (ui::Button("Flip V"))
        group_action(6);
    if (ui::Button("Rotate left"))
        group_action(8);
    ImGui::SameLine();
    if (ui::Button("Rotate right"))
        group_action(7);
    if (ui::Button("Ungroup", {-1, 0}))
        group_action(2);
    ImGui::EndDisabled();
}
void Editor::begin_selection_transform(bool copy) {
    if (selection_.width <= 0 || layer_ >= 0 || !target_editable())
        return;
    cancel_drawing();
    floating_ = FloatingSelection{{selection_.x, selection_.y, selection_.width, selection_.height},
                                  capture_brush(project, frame_, selection_.x, selection_.y,
                                                selection_.width, selection_.height),
                                  selection_.x,
                                  selection_.y,
                                  copy};
    floating_->unscaled = floating_->brush;
    tool_ = Tool::Select;
    update_selection_preview();
}
void Editor::update_selection_preview() {
    if (!floating_)
        return;
    auto candidate = project;
    const auto &s = *floating_;
    apply_selection(candidate, frame_, s.source, s.x, s.y, s.brush, s.copy);
    selection_preview_ = std::move(candidate);
    selection_ = {s.x, s.y, s.brush.width, s.brush.height};
    refresh_ = true;
}
void Editor::commit_selection() {
    if (!floating_ || !selection_preview_)
        return;
    auto candidate = std::move(*selection_preview_);
    checkpoint();
    floating_.reset();
    selection_preview_.reset();
    project = std::move(candidate);
    changed();
}
void Editor::selection_window() {
    if (!floating_)
        return;
    bool open = true;
    ImGui::SetNextWindowSize({390, 0}, ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Transform selection", &open, ImGuiWindowFlags_AlwaysAutoResize)) {
        auto &s = *floating_;
        bool update = false;
        int pos[]{s.x, s.y}, size[]{s.brush.width, s.brush.height};
        if (ui::InputInt2("Position", pos)) {
            s.x = std::clamp(pos[0], -512, 512);
            s.y = std::clamp(pos[1], -512, 512);
            update = true;
        }
        if (ui::InputInt2("Size", size)) {
            s.brush =
                resize_brush(s.unscaled, std::clamp(size[0], 1, 512), std::clamp(size[1], 1, 512));
            update = true;
        }
        ui::help("Size", "Nearest-neighbor scaling preserves indexed pixel colors. Drag the "
                         "lower-right corner on the canvas to resize.");
        for (auto [label, transform] : {std::pair{"Flip H", BrushTransform::FlipX},
                                        {"Flip V", BrushTransform::FlipY},
                                        {"Rotate left", BrushTransform::RotateCCW},
                                        {"Rotate right", BrushTransform::RotateCW}}) {
            if (ui::Button(label)) {
                s.brush = transform_brush(s.brush, transform);
                s.unscaled = transform_brush(s.unscaled, transform);
                update = true;
            }
            ImGui::SameLine();
        }
        ImGui::NewLine();
        update |= ui::Checkbox("Keep original (copy)", &s.copy);
        if (update)
            update_selection_preview();
        bool apply = ui::Button("Apply (Enter)");
        ImGui::SameLine();
        bool cancel = ui::Button("Cancel (Esc)");
        if (apply)
            commit_selection();
        else if (cancel)
            open = false;
    }
    ImGui::End();
    if (!open)
        cancel_drawing();
}
void Editor::tilemap_window() {
    if (!show_tilemap_)
        return;
    ImGui::SetNextWindowSize({510, 600}, ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Tile map library", &show_tilemap_)) {
        auto &f = project.frames[frame_];
        bool map = layer_ < 0 && f.active_layer >= 0 &&
                   f.layers[f.active_layer].kind == LayerKind::Tilemap;
        ImGui::BeginDisabled(f.background_locked);
        if (ui::Button("New tile map")) {
            checkpoint();
            project.add_tilemap_layer(frame_);
            layer_ = -1;
            palette_group_ = 0;
            changed();
            map = true;
        }
        ImGui::SameLine();
        ImGui::BeginDisabled(map || f.active_layer < 0);
        if (ui::Button("Convert selected layer")) {
            auto candidate = project;
            convert_to_tilemap(candidate, frame_, std::size_t(f.active_layer));
            checkpoint();
            project = std::move(candidate);
            layer_ = -1;
            changed();
        }
        ImGui::EndDisabled();
        ImGui::EndDisabled();
        ui::help("Convert selected layer",
                 "Replace pixels with references to deduplicated 8x8 tiles. Editing a library tile "
                 "changes every instance. Undo restores the original layer.");
        // Reacquire frame references after replacing the project.
        if (ui::Button("Capture canvas tile")) {
            auto tile = read_tile(project, frame_, selected_tile_x_, selected_tile_y_);
            checkpoint();
            map_tile_ =
                int(add_tile_asset(project, "Tile " + std::to_string(project.tiles.size()), tile));
            changed();
        }
        ImGui::SameLine();
        if (ui::Button("New empty tile")) {
            Tile tile;
            tile.fill(255);
            checkpoint();
            map_tile_ =
                int(add_tile_asset(project, "Tile " + std::to_string(project.tiles.size()), tile));
            changed();
        }
        ui::Checkbox("Stamp 16x16 metatiles", &map_use_metatile_);
        if (ui::Button("Use tile brush (T)")) {
            cancel_drawing();
            tool_ = Tool::TileBrush;
            layer_ = -1;
            palette_group_ = 0;
        }
        ui::help("Use tile brush",
                 "On a tile-map layer, left drag places references. Right click erases a cell; Alt "
                 "click picks a tile reference. Pencil edits linked tile instances.");
        ImGui::BeginChild("tile-assets", {0, 130}, true);
        for (int i = 0; i < int(project.tiles.size()); ++i) {
            ImGui::PushID(i);
            if (i % 8)
                ImGui::SameLine();
            auto pos = ImGui::GetCursorScreenPos();
            if (ui::Selectable(("##asset" + std::to_string(i)).c_str(), map_tile_ == i, 0,
                               {42, 56}))
                map_tile_ = i;
            auto *d = ImGui::GetWindowDrawList();
            for (int y = 0; y < 8; ++y)
                for (int x = 0; x < 8; ++x) {
                    auto pixel = project.tiles[i].pixels[y * 8 + x];
                    auto c = pixel == 255 ? Color{55, 61, 68, 255}
                                          : nes_colors[project.background[palette_][pixel]];
                    d->AddRectFilled({pos.x + x * 5, pos.y + y * 5},
                                     {pos.x + x * 5 + 5, pos.y + y * 5 + 5},
                                     IM_COL32(c.r, c.g, c.b, 255));
                }
            ui::help(project.tiles[i].name.c_str(),
                     "Select this reusable tile. Edit below to update all map instances.");
            auto number = std::to_string(i);
            d->AddText({pos.x + 2, pos.y + 40}, IM_COL32(200, 210, 220, 255), number.c_str());
            ImGui::PopID();
        }
        ImGui::EndChild();
        if (map_tile_ >= 0 && map_tile_ < int(project.tiles.size())) {
            auto &asset = project.tiles[map_tile_];
            char name[129];
            std::snprintf(name, sizeof(name), "%s", asset.name.c_str());
            if (ui::InputText("Tile name", name, sizeof(name)) && name[0]) {
                checkpoint();
                asset.name = name;
                changed();
            }
            auto pos = ImGui::GetCursorScreenPos();
            ImGui::InvisibleButton("tile-pixels", {160, 160},
                                   ImGuiButtonFlags_MouseButtonLeft |
                                       ImGuiButtonFlags_MouseButtonRight);
            bool hover = ImGui::IsItemHovered();
            auto &io = ImGui::GetIO();
            if (hover && (ImGui::IsMouseDown(0) || ImGui::IsMouseDown(1))) {
                if (!tile_edit_stroke_) {
                    checkpoint();
                    tile_edit_stroke_ = true;
                }
                int x = std::clamp(int((io.MousePos.x - pos.x) / 20), 0, 7),
                    y = std::clamp(int((io.MousePos.y - pos.y) / 20), 0, 7);
                asset.pixels[y * 8 + x] = ImGui::IsMouseDown(1) ? 255 : Byte(slot_);
                for (std::size_t fi = 0; fi < project.frames.size(); ++fi)
                    project.compose_layers(fi);
                changed();
            }
            auto *d = ImGui::GetWindowDrawList();
            for (int y = 0; y < 8; ++y)
                for (int x = 0; x < 8; ++x) {
                    Byte pixel = asset.pixels[y * 8 + x];
                    auto c = pixel == 255 ? Color{55, 61, 68, 255}
                                          : nes_colors[project.background[palette_][pixel]];
                    ImVec2 a{pos.x + x * 20, pos.y + y * 20}, b{a.x + 20, a.y + 20};
                    d->AddRectFilled(a, b, IM_COL32(c.r, c.g, c.b, 255));
                    d->AddRect(a, b, IM_COL32(90, 95, 100, 255));
                }
            ui::help("Tile pixels",
                     "Left drag paints the chosen slot; right drag makes pixels transparent. Every "
                     "referencing map cell updates, across frames.");
        }
        ImGui::BeginDisabled(project.metatiles.size() >= 4096);
        if (ui::Button("New metatile")) {
            checkpoint();
            project.metatiles.push_back(
                {"Metatile", {map_tile_, map_tile_, map_tile_, map_tile_}, palette_});
            map_metatile_ = int(project.metatiles.size()) - 1;
            changed();
        }
        ImGui::EndDisabled();
        if (ImGui::BeginCombo("Metatile",
                              map_metatile_ >= 0 && map_metatile_ < int(project.metatiles.size())
                                  ? project.metatiles[map_metatile_].name.c_str()
                                  : "Choose...")) {
            for (int i = 0; i < int(project.metatiles.size()); ++i) {
                ImGui::PushID(i);
                if (ui::Selectable(project.metatiles[i].name.c_str(), map_metatile_ == i))
                    map_metatile_ = i;
                ImGui::PopID();
            }
            ImGui::EndCombo();
        }
        ui::help("Metatile",
                 "Choose a saved 16x16 block to edit or place with the tile-map brush.");
        if (map_metatile_ >= 0 && map_metatile_ < int(project.metatiles.size())) {
            auto &m = project.metatiles[map_metatile_];
            char name[129];
            std::snprintf(name, sizeof(name), "%s", m.name.c_str());
            if (ui::InputText("Metatile name", name, sizeof(name)) && name[0]) {
                checkpoint();
                m.name = name;
                changed();
            }
            auto pos = ImGui::GetCursorScreenPos();
            ImGui::InvisibleButton("metatile-quadrants", {128, 128},
                                   ImGuiButtonFlags_MouseButtonLeft |
                                       ImGuiButtonFlags_MouseButtonRight);
            if (ImGui::IsItemHovered() && (ImGui::IsMouseClicked(0) || ImGui::IsMouseClicked(1))) {
                int x = std::clamp(int((ImGui::GetIO().MousePos.x - pos.x) / 64), 0, 1);
                int y = std::clamp(int((ImGui::GetIO().MousePos.y - pos.y) / 64), 0, 1);
                if (ImGui::IsMouseClicked(1) ||
                    (map_tile_ >= 0 && map_tile_ < int(project.tiles.size()))) {
                    checkpoint();
                    m.tiles[y * 2 + x] = ImGui::IsMouseClicked(1) ? -1 : map_tile_;
                    changed();
                }
            }
            auto *draw = ImGui::GetWindowDrawList();
            for (int y = 0; y < 16; ++y)
                for (int x = 0; x < 16; ++x) {
                    int id = m.tiles[(y / 8) * 2 + x / 8];
                    Byte pixel = id >= 0 ? project.tiles.at(id).pixels[(y % 8) * 8 + x % 8] : 255;
                    auto c = pixel == 255 ? Color{55, 61, 68, 255}
                                          : nes_colors[project.background[m.palette][pixel]];
                    draw->AddRectFilled({pos.x + x * 8, pos.y + y * 8},
                                        {pos.x + x * 8 + 8, pos.y + y * 8 + 8},
                                        IM_COL32(c.r, c.g, c.b, 255));
                }
            for (int y = 0; y < 2; ++y)
                for (int x = 0; x < 2; ++x)
                    draw->AddRect({pos.x + x * 64, pos.y + y * 64},
                                  {pos.x + x * 64 + 64, pos.y + y * 64 + 64},
                                  IM_COL32(140, 160, 170, 255));
            ui::help("Metatile quadrants",
                     "Click a quadrant to assign the selected library tile. Right-click clears it. "
                     "Each 16x16 metatile shares one NES color set.");
            int ids[4];
            std::copy(m.tiles.begin(), m.tiles.end(), ids);
            if (ImGui::InputInt4("TL / TR / BL / BR", ids)) {
                checkpoint();
                for (int i = 0; i < 4; ++i)
                    m.tiles[i] = std::clamp(ids[i], -1, int(project.tiles.size()) - 1);
                changed();
            }
            ui::help("Metatile tile IDs",
                     "Library tile IDs for top-left, top-right, bottom-left and bottom-right. -1 "
                     "means transparent. You can also click the preview quadrants to assign the "
                     "selected tile.");
            int pal = m.palette;
            if (ui::SliderInt("Metatile color set", &pal, 0, 3)) {
                checkpoint();
                m.palette = pal;
                changed();
            }
        }
    }
    ImGui::End();
}
} // namespace tzj
