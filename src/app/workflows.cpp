#include "app/editor.hpp"
#include "app/ui_help.hpp"
#include <algorithm>
#include <cstdio>
#include <imgui.h>

namespace tzj {
namespace {
ImU32 swatch(Byte index) {
    auto c = nes_colors.at(index);
    return IM_COL32(c.r, c.g, c.b, 255);
}
void draw_tile(ImDrawList *draw, ImVec2 origin, float scale, const Tile &tile,
               const Palette &palette) {
    for (int y = 0; y < 8; ++y)
        for (int x = 0; x < 8; ++x)
            draw->AddRectFilled({origin.x + x * scale, origin.y + y * scale},
                                {origin.x + (x + 1) * scale, origin.y + (y + 1) * scale},
                                swatch(palette[tile[y * 8 + x]]));
}
} // namespace
void Editor::transform_captured_brush(BrushTransform transform) {
    if (clipboard_.empty())
        return;
    auto result = transform_brush({clip_width_, clip_height_, clipboard_}, transform);
    clip_width_ = result.width;
    clip_height_ = result.height;
    clipboard_ = std::move(result.pixels);
    tool_ = Tool::Stamp;
    status_ = "Brush transformed; paint on the canvas to stamp it";
}
void Editor::brush_controls(bool embedded) {
    if (!embedded && !ImGui::Begin("Brush")) {
        if (!embedded)
            ImGui::End();
        return;
    }
    if (clipboard_.empty())
        ImGui::TextWrapped("Select a rectangle with R, then capture it with Ctrl+C. Paste with V.");
    else {
        ui::Text("Captured brush: %d x %d", clip_width_, clip_height_);
        if (ui::Button("Rotate left"))
            transform_captured_brush(BrushTransform::RotateCCW);
        ImGui::SameLine();
        if (ui::Button("Rotate right"))
            transform_captured_brush(BrushTransform::RotateCW);
        if (ui::Button("Flip horizontal"))
            transform_captured_brush(BrushTransform::FlipX);
        ImGui::SameLine();
        if (ui::Button("Flip vertical"))
            transform_captured_brush(BrushTransform::FlipY);
        if (ui::Button("Paint with brush")) {
            tool_ = Tool::Stamp;
            layer_ = -1;
        }
        ui::Checkbox("Slot zero is transparent", &brush_transparent_);
        ui::Checkbox("Snap stamps to 8x8 grid", &brush_snap_);
        ui::Checkbox("Wrap across canvas edges", &brush_wrap_);
        ImGui::TextWrapped("Brush pixels use the destination region's NES palette. Transformations "
                           "keep exact indexed pixels.");
        // A bounded preview samples large captured regions without drawing hundreds of thousands of
        // widgets.
        float scale = std::min(4.f, std::min(220.f / clip_width_, 130.f / clip_height_));
        auto origin = ImGui::GetCursorScreenPos();
        ImVec2 size{clip_width_ * scale, clip_height_ * scale};
        ImGui::Dummy(size);
        auto *draw = ImGui::GetWindowDrawList();
        draw->AddRectFilled(origin, {origin.x + size.x, origin.y + size.y},
                            IM_COL32(40, 44, 50, 255));
        const int step = scale < 1 ? static_cast<int>(1 / scale) + 1 : 1;
        for (int y = 0; y < clip_height_; y += step)
            for (int x = 0; x < clip_width_; x += step) {
                Byte value = clipboard_[y * clip_width_ + x];
                if (value == 0 && brush_transparent_)
                    continue;
                draw->AddRectFilled({origin.x + x * scale, origin.y + y * scale},
                                    {origin.x + std::min(x + step, clip_width_) * scale,
                                     origin.y + std::min(y + step, clip_height_) * scale},
                                    swatch(project.background[palette_][value]));
            }
    }
    if (selection_.width > 0) {
        ImGui::Separator();
        ui::Text("Selection: %d x %d", selection_.width, selection_.height);
        if (ui::Button("Capture"))
            copy_selection();
        ImGui::SameLine();
        if (ui::Button("Cut"))
            copy_selection(true);
        ImGui::SameLine();
        if (ui::Button("Deselect"))
            selection_ = {};
    }
    if (!embedded)
        ImGui::End();
}
void Editor::edit_selected_tile(const Tile &replacement) {
    std::size_t count = 1;
    if (edit_linked_tiles_)
        count = replace_tile(project, frame_, selected_tile_, replacement);
    else {
        selected_tile_x_ = std::clamp(selected_tile_x_, 0, project.width / 8 - 1);
        selected_tile_y_ = std::clamp(selected_tile_y_, 0, project.height / 8 - 1);
        stamp_tile(project, frame_, selected_tile_x_, selected_tile_y_, replacement);
    }
    selected_tile_ = replacement;
    if (count) {
        changed();
        status_ = "Updated " + std::to_string(count) + " tile instance(s)";
    } else
        status_ = "Edited the tile brush; paint it into the map with T";
}
void Editor::tiles(bool embedded) {
    if (!embedded && !ImGui::Begin("Tiles")) {
        if (!embedded)
            ImGui::End();
        return;
    }
    ui::Text("Background CHR: %zu / 256 patterns", tile_catalog_.size());
    ImGui::TextWrapped("Pick a tile; paint with T. Right-click the map to pick an existing tile.");
    ImGui::BeginChild("tile-catalog", {0, 155}, true);
    int columns = std::max(1, static_cast<int>(ImGui::GetContentRegionAvail().x / 40));
    int rows = static_cast<int>((tile_catalog_.size() + columns - 1) / columns);
    ImGuiListClipper clipper;
    clipper.Begin(rows, 48);
    while (clipper.Step())
        for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row)
            for (int column = 0; column < columns; ++column) {
                auto index = static_cast<std::size_t>(row * columns + column);
                if (index >= tile_catalog_.size())
                    break;
                if (column)
                    ImGui::SameLine();
                ImGui::PushID(static_cast<int>(index));
                auto pos = ImGui::GetCursorScreenPos();
                ui::InvisibleButton("tile", {32, 44});
                auto &entry = tile_catalog_[index];
                if (ImGui::IsItemClicked()) {
                    selected_tile_ = entry.pixels;
                    tile_selected_ = true;
                    selected_tile_x_ = entry.locations[0].first;
                    selected_tile_y_ = entry.locations[0].second;
                    tool_ = Tool::TileBrush;
                    layer_ = -1;
                    palette_group_ = 0;
                }
                auto *draw = ImGui::GetWindowDrawList();
                draw_tile(draw, pos, 4, entry.pixels, project.background[palette_]);
                if (tile_selected_ && selected_tile_ == entry.pixels)
                    draw->AddRect(pos, {pos.x + 32, pos.y + 32}, IM_COL32(255, 208, 93, 255), 0, 0,
                                  2);
                char label[12];
                std::snprintf(label, sizeof(label), "%02zX", index);
                draw->AddText({pos.x, pos.y + 32}, IM_COL32(180, 190, 200, 255), label);
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Pattern %zu / %zu map uses", index, entry.locations.size());
                ImGui::PopID();
            }
    ImGui::EndChild();
    if (tile_selected_) {
        ui::Checkbox("Edit all identical map tiles", &edit_linked_tiles_);
        ImGui::TextWrapped(
            "Linked edits affect the current frame. Each 16x16 region keeps its own palette.");
        auto origin = ImGui::GetCursorScreenPos();
        ui::InvisibleButton("edit-tile", {160, 160});
        auto *draw = ImGui::GetWindowDrawList();
        draw_tile(draw, origin, 20, selected_tile_, project.background[palette_]);
        for (int i = 0; i <= 8; ++i) {
            draw->AddLine({origin.x + i * 20, origin.y}, {origin.x + i * 20, origin.y + 160},
                          IM_COL32(90, 95, 100, 130));
            draw->AddLine({origin.x, origin.y + i * 20}, {origin.x + 160, origin.y + i * 20},
                          IM_COL32(90, 95, 100, 130));
        }
        if (ImGui::IsItemHovered() && (ImGui::IsMouseDown(0) || ImGui::IsMouseDown(1))) {
            int x = std::clamp(static_cast<int>((ImGui::GetIO().MousePos.x - origin.x) / 20), 0, 7),
                y = std::clamp(static_cast<int>((ImGui::GetIO().MousePos.y - origin.y) / 20), 0, 7);
            Tile replacement = selected_tile_;
            replacement[y * 8 + x] = static_cast<Byte>(ImGui::IsMouseDown(1) ? 0 : slot_);
            if (replacement != selected_tile_) {
                if (!tile_edit_stroke_) {
                    checkpoint();
                    tile_edit_stroke_ = true;
                }
                edit_selected_tile(replacement);
            }
        }
        if (!ImGui::IsMouseDown(0) && !ImGui::IsMouseDown(1))
            tile_edit_stroke_ = false;
        if (ui::Button("Paint tile (T)")) {
            tool_ = Tool::TileBrush;
            layer_ = -1;
        }
        ImGui::SameLine();
        if (ui::Button("Rotate tile")) {
            checkpoint();
            auto b = transform_brush({8, 8, {selected_tile_.begin(), selected_tile_.end()}},
                                     BrushTransform::RotateCW);
            Tile rotated{};
            std::copy(b.pixels.begin(), b.pixels.end(), rotated.begin());
            edit_selected_tile(rotated);
        }
    } else
        ImGui::TextWrapped(
            "Pick a map tile to edit it here, or right-click the canvas with T selected.");
    if (!embedded)
        ImGui::End();
}
} // namespace tzj
