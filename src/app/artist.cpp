#include "app/editor.hpp"
#include "app/ui_help.hpp"
#include <algorithm>
#include <cstdio>
#include <imgui.h>
namespace tzj {
namespace {
template <class Brush>
void indexed_preview(const Brush &brush, const Palette &palette, float max_width = 280,
                     float max_height = 140) {
    if (brush.pixels.empty())
        return;
    float scale = std::min(8.f, std::min(max_width / brush.width, max_height / brush.height));
    auto origin = ImGui::GetCursorScreenPos();
    ImGui::Dummy({brush.width * scale, brush.height * scale});
    auto *draw = ImGui::GetWindowDrawList();
    int step = std::max(1, int(1 / scale));
    for (int y = 0; y < brush.height; y += step)
        for (int x = 0; x < brush.width; x += step) {
            auto value = brush.pixels[y * brush.width + x];
            auto shade = ((x / 4 + y / 4) & 1) ? 45 : 65;
            Color c = value == 255 ? Color{Byte(shade), Byte(shade), Byte(shade)}
                                   : nes_colors[palette[value]];
            draw->AddRectFilled({origin.x + x * scale, origin.y + y * scale},
                                {origin.x + std::min(x + step, brush.width) * scale,
                                 origin.y + std::min(y + step, brush.height) * scale},
                                IM_COL32(c.r, c.g, c.b, 255));
        }
}
} // namespace
void Editor::use_brush(const IndexedBrush &brush) {
    cancel_drawing();
    clipboard_ = brush.pixels;
    clip_width_ = brush.width;
    clip_height_ = brush.height;
    brush_transparent_ = false;
    tool_ = Tool::Stamp;
    layer_ = -1;
    palette_group_ = 0;
    status_ = "Brush ready. Click the canvas to stamp; Ctrl+Z undoes a stroke.";
}
void Editor::brush_library() {
    ImGui::SetNextWindowSize({430, 590}, ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Brush library", &show_brush_library_)) {
        ui::InputText("Brush name", asset_name_, sizeof(asset_name_));
        bool selection = selection_.width > 0 && selection_.height > 0;
        bool sprite = layer_ >= 0 && layer_ < int(project.frames[frame_].sprites.size());
        ImGui::BeginDisabled((clipboard_.empty() && !selection && !sprite) ||
                             project.brushes.size() >= 128 || !asset_name_[0]);
        if (ui::Button(sprite      ? "Save selected sprite"
                       : selection ? "Save selection as brush"
                                   : "Save captured brush")) {
            if (sprite) {
                const auto &s = project.frames[frame_].sprites[layer_];
                clip_width_ = clip_height_ = 8;
                clipboard_.resize(64);
                for (int y = 0; y < 8; ++y)
                    for (int x = 0; x < 8; ++x) {
                        auto value = s.pixels[(s.flip_y ? 7 - y : y) * 8 + (s.flip_x ? 7 - x : x)];
                        clipboard_[y * 8 + x] = value ? value : 255;
                    }
            } else if (selection)
                copy_selection();
            SavedBrush saved{asset_name_, clip_width_, clip_height_, clipboard_};
            auto candidate = project;
            candidate.brushes.push_back(saved);
            candidate.validate();
            checkpoint();
            project.brushes.push_back(std::move(saved));
            library_selection_ = int(project.brushes.size()) - 1;
            changed();
        }
        ImGui::EndDisabled();
        if (ui::Button("Import from project..."))
            file_dialog(FileAction::Brushes);
        if (library_selection_ < 0 && !project.brushes.empty())
            library_selection_ = 0;
        ImGui::BeginChild("Saved brushes", {0, 230}, ImGuiChildFlags_Borders);
        ImGuiListClipper clipper;
        clipper.Begin(int(project.brushes.size()), 58);
        while (clipper.Step())
            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
                ImGui::PushID(int(i));
                const auto &b = project.brushes[i];
                auto row = ImGui::GetCursorPosY();
                indexed_preview(b, project.background[palette_], 40, 40);
                ImGui::SameLine();
                ImGui::BeginGroup();
                if (ui::Selectable(b.name.c_str(), library_selection_ == int(i)))
                    library_selection_ = int(i);
                ui::TextDisabled("%d x %d%s", b.width, b.height,
                                 (b.width % 8 == 0 && b.height % 8 == 0) ? " / tile group" : "");
                ImGui::EndGroup();
                ImGui::SetCursorPosY(row + 58);
                ImGui::PopID();
            }
        ImGui::EndChild();
        if (library_selection_ >= 0 && library_selection_ < int(project.brushes.size())) {
            const auto &saved = project.brushes[library_selection_];
            indexed_preview(saved, project.background[palette_]);
            if (ui::Button("Use saved brush"))
                use_brush({saved.width, saved.height, saved.pixels});
            if (saved.width == 8 && saved.height == 8) {
                ImGui::SameLine();
                ImGui::BeginDisabled(project.frames[frame_].sprites_locked ||
                                     project.frames[frame_].sprites.size() >= 64);
                if (ui::Button("Create sprite")) {
                    auto pixels = saved.pixels;
                    auto name = saved.name;
                    add_blank_sprite();
                    auto &s = project.frames[frame_].sprites[layer_];
                    s.name = name;
                    for (int i = 0; i < 64; ++i)
                        s.pixels[i] = pixels[i] == 255 ? 0 : pixels[i];
                    changed();
                }
                ImGui::EndDisabled();
            }
            if (ui::Button("Rename saved brush") && asset_name_[0]) {
                checkpoint();
                project.brushes[library_selection_].name = asset_name_;
                changed();
            }
            ImGui::SameLine();
            if (ui::Button("Delete saved brush")) {
                checkpoint();
                project.brushes.erase(project.brushes.begin() + library_selection_);
                library_selection_ = -1;
                changed();
            }
        } else if (project.brushes.empty())
            ImGui::TextWrapped("Select an area with R or capture pixels with Ctrl+C, then save it "
                               "here. Brushes and tile groups are stored with the project.");
    }
    ImGui::End();
}
void Editor::bitmap_text() {
    ImGui::SetNextWindowSize({530, 690}, ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Bitmap text", &show_bitmap_text_)) {
        ImGui::InputTextMultiline("##bitmap-text", text_, sizeof(text_), {-1, 85});
        ui::help("Bitmap text input", "Printable ASCII text and newlines. Lowercase uses the "
                                      "matching glyph in the selected bitmap font.");
        ui::SliderInt("Letter spacing", &text_spacing_, 0, 16);
        ui::SliderInt("Line spacing", &text_line_gap_, 0, 32);
        ui::SliderInt("Pixel scale", &text_scale_, 1, 4);
        ui::Checkbox("Use current paint color", &text_recolor_);
        if (text_recolor_) {
            ImGui::TextUnformatted("Text color");
            auto palette =
                (palette_group_ ? project.sprite_palettes : project.background)[palette_];
            for (int i = 0; i < 4; ++i) {
                ImGui::SameLine();
                ImGui::PushID(i);
                auto color = nes_colors[palette[i]];
                if (ui::ColorButton("##text-color",
                                    {color.r / 255.f, color.g / 255.f, color.b / 255.f, 1},
                                    ImGuiColorEditFlags_NoTooltip, {24, 24}))
                    slot_ = i;
                ui::help("Text color",
                         "Choose an indexed paint color for the text. Destination background "
                         "regions supply the final NES colors when stamping.");
                ImGui::PopID();
            }
        }
        try {
            auto brush = text_brush(project.font, text_, Byte(slot_), text_spacing_, text_line_gap_,
                                    text_scale_, text_recolor_);
            indexed_preview(
                brush, (palette_group_ ? project.sprite_palettes : project.background)[palette_]);
            if (ui::Button("Use text as brush"))
                use_brush(brush);
        } catch (const std::exception &e) {
            ImGui::TextWrapped("%s", e.what());
        }
        if (ui::CollapsingHeader("Edit bitmap font")) {
            ui::Text("Font: %s", project.font.name.c_str());
            char letter[2]{char(glyph_), 0};
            if (ui::InputText("Glyph", letter, sizeof(letter)))
                glyph_ = std::clamp(int(static_cast<unsigned char>(letter[0])), 32, 126);
            const auto &glyph = project.font.glyphs[glyph_ - 32];
            auto origin = ImGui::GetCursorScreenPos();
            ui::InvisibleButton("Glyph pixels", {160, 160},
                                ImGuiButtonFlags_MouseButtonLeft |
                                    ImGuiButtonFlags_MouseButtonRight);
            if (ImGui::IsItemActivated())
                glyph_stroke_ = false;
            auto *draw = ImGui::GetWindowDrawList();
            for (int y = 0; y < 8; ++y)
                for (int x = 0; x < 8; ++x) {
                    Byte value = glyph[y * 8 + x];
                    Color c =
                        value == 255
                            ? Color{Byte(((x + y) & 1) ? 45 : 65), 55, 65}
                            : nes_colors[(palette_group_ ? project.sprite_palettes
                                                         : project.background)[palette_][value]];
                    draw->AddRectFilled({origin.x + x * 20, origin.y + y * 20},
                                        {origin.x + (x + 1) * 20 - 1, origin.y + (y + 1) * 20 - 1},
                                        IM_COL32(c.r, c.g, c.b, 255));
                }
            if (ImGui::IsItemActive() && (ImGui::IsMouseDown(0) || ImGui::IsMouseDown(1))) {
                auto position = ImGui::GetIO().MousePos;
                int x = std::clamp(int((position.x - origin.x) / 20), 0, 7),
                    y = std::clamp(int((position.y - origin.y) / 20), 0, 7);
                Byte value = ImGui::IsMouseDown(1) ? 255 : Byte(slot_);
                if (glyph[y * 8 + x] != value) {
                    if (!glyph_stroke_) {
                        checkpoint();
                        glyph_stroke_ = true;
                    }
                    project.font.glyphs[glyph_ - 32][y * 8 + x] = value;
                    changed();
                }
            }
            if (!ImGui::IsMouseDown(0) && !ImGui::IsMouseDown(1))
                glyph_stroke_ = false;
            if (ui::Button("Import font sheet..."))
                file_dialog(FileAction::FontSheet);
            ImGui::SameLine();
            if (ui::Button("Restore built-in font")) {
                checkpoint();
                project.font = default_bitmap_font();
                changed();
            }
            ImGui::TextWrapped("Font sheets use 96 row-major 8x8 cells, starting at ASCII space. "
                               "Alpha and the top-left background color become transparent.");
        }
    }
    ImGui::End();
}
void Editor::gradient_editor() {
    ImGui::SetNextWindowSize({470, 650}, ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Gradient fill", &show_gradients_)) {
        ImGui::PushItemWidth(190);
        const char *direction[]{"Horizontal", "Vertical"};
        int orientation = gradient_vertical_ ? 1 : 0;
        if (ui::Combo("Direction", &orientation, direction, 2))
            gradient_vertical_ = orientation == 1;
        const char *methods[]{"None", "Ordered 4x4", "Noise"};
        ui::Combo("Gradient dithering", &gradient_dither_, methods, 3);
        const char *slots[]{"Backdrop", "Paint color 2", "Paint color 3", "Paint color 4"};
        for (int i = 0; i < 4; ++i) {
            ImGui::PushID(i);
            int slot = gradient_order_[i];
            if (ui::Combo(i == 0 ? "Start" : i == 3 ? "End" : "Ramp stop", &slot, slots, 4))
                gradient_order_[i] = Byte(slot);
            ImGui::PopID();
        }
        auto palette = (palette_group_ ? project.sprite_palettes : project.background)[palette_];
        indexed_preview(gradient_brush(128, 48, gradient_order_, gradient_vertical_,
                                       GradientDither(gradient_dither_)),
                        palette);
        bool selection = selection_.width > 0 && selection_.height > 0 && layer_ < 0 &&
                         project.paint_layer_editable(frame_);
        ImGui::BeginDisabled(!selection);
        if (ui::Button("Fill selected area")) {
            auto brush = gradient_brush(selection_.width, selection_.height, gradient_order_,
                                        gradient_vertical_, GradientDither(gradient_dither_));
            auto candidate = project;
            stamp_brush(candidate, frame_, selection_.x, selection_.y, brush);
            checkpoint();
            project = std::move(candidate);
            changed();
        }
        ImGui::EndDisabled();
        ui::SliderInt("Brush width", &gradient_width_, 1, 256);
        ui::SliderInt("Brush height", &gradient_height_, 1, 256);
        if (ui::Button("Use gradient as brush"))
            use_brush(gradient_brush(gradient_width_, gradient_height_, gradient_order_,
                                     gradient_vertical_, GradientDither(gradient_dither_)));
        ui::SeparatorText("Saved color ramps");
        ui::InputText("Ramp name", ramp_name_, sizeof(ramp_name_));
        ImGui::BeginDisabled(!ramp_name_[0] || project.ramps.size() >= 64);
        if (ui::Button("Save current color ramp")) {
            checkpoint();
            project.ramps.push_back({ramp_name_, palette, gradient_order_});
            changed();
        }
        ImGui::EndDisabled();
        for (std::size_t i = 0; i < project.ramps.size(); ++i) {
            ImGui::PushID(int(i));
            const auto &r = project.ramps[i];
            indexed_preview(gradient_brush(96, 8, r.order, false, GradientDither::None), r.colors,
                            192, 16);
            ImGui::SameLine();
            if (ui::Button(r.name.c_str())) {
                checkpoint();
                project.set_backdrop(r.colors[0]);
                (palette_group_ ? project.sprite_palettes : project.background)[palette_] =
                    r.colors;
                gradient_order_ = r.order;
                changed();
            }
            ui::help("Apply saved color ramp",
                     "Recolor the active NES color set across all frames, including its shared "
                     "backdrop, and restore this gradient's slot order.");
            ImGui::SameLine();
            bool remove = ui::SmallButton("Delete ramp");
            ImGui::PopID();
            if (remove) {
                checkpoint();
                project.ramps.erase(project.ramps.begin() + std::ptrdiff_t(i));
                changed();
                break;
            }
        }
        ImGui::PopItemWidth();
    }
    ImGui::End();
}
void Editor::artist_windows() {
    if (show_brush_library_)
        brush_library();
    if (show_bitmap_text_)
        bitmap_text();
    if (show_gradients_)
        gradient_editor();
    if (show_onion_settings_) {
        ImGui::SetNextWindowSize({400, 0}, ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Onion skin settings", &show_onion_settings_,
                         ImGuiWindowFlags_AlwaysAutoResize)) {
            bool modified = ui::Checkbox("Onion skin enabled", &onion_);
            modified |= ui::SliderInt("Previous frames (green)", &onion_previous_, 0, 8);
            modified |= ui::SliderInt("Following frames (pink)", &onion_following_, 0, 8);
            modified |= ui::SliderInt("Ghost opacity", &onion_opacity_, 0, 100);
            modified |= ui::Checkbox("Sprites only", &onion_sprites_only_);
            if (modified)
                refresh_ = true;
        }
        ImGui::End();
    }
}
} // namespace tzj
