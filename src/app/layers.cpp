#include "app/editor.hpp"
#include "app/ui_help.hpp"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <imgui.h>
namespace tzj {
namespace {
bool layer_switch(const char *id, bool value, bool lock) {
    bool pressed = ui::InvisibleButton(id, {22, 34});
    auto p = ImGui::GetItemRectMin();
    auto *d = ImGui::GetWindowDrawList();
    auto color = value ? IM_COL32(140, 225, 207, 255) : IM_COL32(105, 119, 132, 255);
    if (lock) {
        d->AddRect({p.x + 5, p.y + 15}, {p.x + 17, p.y + 25}, color, 2, 0, 1.5f);
        if (value) {
            d->PathArcTo({p.x + 11, p.y + 15}, 4, 3.14159f, 6.28318f, 12);
            d->PathStroke(color, 0, 1.5f);
        } else {
            d->AddLine({p.x + 7, p.y + 15}, {p.x + 7, p.y + 9}, color, 1.5f);
            d->AddLine({p.x + 7, p.y + 9}, {p.x + 14, p.y + 9}, color, 1.5f);
        }
        ui::help("Layer lock", "Lock this layer to prevent painting, movement and deletion. "
                               "Visibility can still be changed.");
    } else {
        d->AddEllipse({p.x + 11, p.y + 18}, {8, 5}, color, 0, 0, 1.4f);
        if (value)
            d->AddCircleFilled({p.x + 11, p.y + 18}, 2.5f, color);
        else
            d->AddLine({p.x + 3, p.y + 26}, {p.x + 19, p.y + 10}, color, 1.5f);
        ui::help(
            "Layer visibility",
            "Show or hide this layer. Only visible content contributes to the exported NES scene.");
    }
    return pressed;
}
void thumbnail(const Project &p, const Frame &f, const BackgroundLayer *layer,
               const Sprite *sprite) {
    auto pos = ImGui::GetCursorScreenPos();
    ImGui::Dummy({30, 34});
    auto *d = ImGui::GetWindowDrawList();
    constexpr int side = 28;
    for (int y = 0; y < side; ++y)
        for (int x = 0; x < side; ++x) {
            Byte value = 255;
            int sx = x * p.width / side, sy = y * p.height / side, pal = 0;
            if (sprite) {
                value = sprite->pixels[(y * 8 / side) * 8 + x * 8 / side];
                pal = sprite->palette;
                if (value == 0)
                    value = 255;
            } else if (layer) {
                const auto &pixels =
                    layer->kind == LayerKind::Image ? layer->converted : layer->pixels;
                auto i = sy * p.width + sx;
                if (std::size_t(i) < pixels.size())
                    value = pixels[i];
                pal = f.attributes[(sy / 16) * p.block_width() + sx / 16];
            }
            auto shade = ((x / 4 + y / 4) & 1) ? 45 : 62;
            ImU32 c = IM_COL32(shade, shade + 3, shade + 7, 255);
            if (value != 255) {
                auto color = nes_colors[(sprite ? p.sprite_palettes : p.background)[pal][value]];
                c = IM_COL32(color.r, color.g, color.b, 255);
            }
            d->AddRectFilled({pos.x + x, pos.y + 3 + y}, {pos.x + x + 1, pos.y + 4 + y}, c);
        }
    d->AddRect({pos.x, pos.y + 3}, {pos.x + side, pos.y + 3 + side}, IM_COL32(90, 107, 121, 255));
    ui::help("Layer thumbnail",
             "A thumbnail of this layer alone. Checkerboard squares indicate transparent pixels.");
}
struct StackMove {
    int frame, kind, index;
};
} // namespace
bool Editor::target_editable(bool warn) {
    const auto &f = project.frames[frame_];
    bool editable = project.paint_layer_editable(frame_);
    if (layer_ >= 0 && layer_ < int(f.sprites.size())) {
        const auto &s = f.sprites[layer_];
        editable = f.sprites_visible && !f.sprites_locked && s.visible && !s.locked;
    }
    if (!editable && warn)
        status_ = layer_ >= 0 ? "Show and unlock the selected sprite before editing."
                              : "Select a visible, unlocked paint layer. For an imported image, "
                                "choose Make paintable copy.";
    return editable;
}
void Editor::layer_action(int action, int index, bool sprite) {
    try {
        cancel_drawing();
        auto &f = project.frames[frame_];
        if (sprite) {
            if (index < 0 || index >= int(f.sprites.size()))
                return;
            if (f.sprites_locked || (f.sprites[index].locked && action != 0)) {
                status_ = "Unlock the sprite before changing it.";
                return;
            }
            checkpoint();
            if (action == 0) {
                auto copy = f.sprites[index];
                copy.name += " copy";
                copy.locked = false;
                copy.visible = true;
                f.sprites.insert(f.sprites.begin() + index, std::move(copy));
                layer_ = index;
            } else if (action == 2) {
                f.sprites.erase(f.sprites.begin() + index);
                layer_ = -1;
            } else if (action == 4 && index > 0) {
                std::swap(f.sprites[index], f.sprites[index - 1]);
                layer_ = index - 1;
            } else if (action == 5 && index + 1 < int(f.sprites.size())) {
                std::swap(f.sprites[index], f.sprites[index + 1]);
                layer_ = index + 1;
            }
        } else {
            checkpoint();
            if (action == 0 || action == 1)
                project.duplicate_layer(frame_, std::size_t(index), action == 1);
            else if (action == 2)
                project.remove_layer(frame_, std::size_t(index));
            else if (action == 3)
                project.merge_layer_down(frame_, std::size_t(index));
            else if (action == 4 && index + 1 < int(f.layers.size()))
                project.move_layer(frame_, index, index + 1);
            else if (action == 5 && index > 0)
                project.move_layer(frame_, index, index - 1);
            layer_ = -1;
            palette_group_ = 0;
        }
        changed();
    } catch (const std::exception &e) {
        error_ = e.what();
    }
}
void Editor::layer_stack() {
    auto &f = project.frames[frame_];
    project.ensure_layers(frame_);
    ui::SeparatorText("Layers");
    int action = -1, action_index = -1;
    bool action_sprite = false;
    std::optional<std::pair<StackMove, StackMove>> move;
    float height = std::clamp(ImGui::GetContentRegionAvail().y * .46f, 180.f, 360.f);
    ImGui::BeginChild("scene-layer-stack", {0, height}, true);
    auto row = [&](int index, bool sprite) {
        ImGui::PushID(sprite ? 1000 + index : index);
        bool visible = sprite ? f.sprites[index].visible : f.layers[index].visible;
        bool locked = sprite ? f.sprites[index].locked : f.layers[index].locked;
        if (layer_switch("##eye", visible, false)) {
            checkpoint();
            if (sprite)
                f.sprites[index].visible = !visible;
            else
                f.layers[index].visible = !visible;
            changed();
        }
        ImGui::SameLine(0, 2);
        if (layer_switch("##lock", locked, true)) {
            checkpoint();
            if (sprite)
                f.sprites[index].locked = !locked;
            else
                f.layers[index].locked = !locked;
            changed();
        }
        ImGui::SameLine(0, 3);
        thumbnail(project, f, sprite ? nullptr : &f.layers[index],
                  sprite ? &f.sprites[index] : nullptr);
        ImGui::SameLine(0, 5);
        const auto &name = sprite ? f.sprites[index].name : f.layers[index].name;
        const bool selected = sprite ? layer_ == index : layer_ < 0 && f.active_layer == index;
        auto pos = ImGui::GetCursorScreenPos();
        bool picked = ui::Selectable((name + "##layer-row").c_str(), selected, 0,
                                     {ImGui::GetContentRegionAvail().x, 36});
        auto *d = ImGui::GetWindowDrawList();
        const char *type =
            sprite ? (f.sprites[index].behind ? "Sprite / behind background" : "Sprite object")
            : f.layers[index].kind == LayerKind::Image ? "Image / live modifiers"
                                                       : "Paint / indexed pixels";
        d->AddText(nullptr, 12, {pos.x, pos.y + 20}, IM_COL32(140, 157, 172, 255), type);
        if (picked) {
            cancel_drawing();
            if (sprite) {
                layer_ = index;
                palette_group_ = 1;
                palette_ = f.sprites[index].palette;
                tool_ = Tool::Pencil;
            } else {
                layer_ = -1;
                f.active_layer = index;
                palette_group_ = 0;
                if (tool_ == Tool::Sprite || tool_ == Tool::Move)
                    tool_ = Tool::Pencil;
            }
        }
        if (ImGui::BeginDragDropSource()) {
            StackMove payload{int(frame_), sprite ? 1 : 0, index};
            ImGui::SetDragDropPayload("TZJ_LAYER", &payload, sizeof(payload));
            ImGui::TextUnformatted(name.c_str());
            ImGui::EndDragDropSource();
        }
        if (ImGui::BeginDragDropTarget()) {
            if (const auto *payload = ImGui::AcceptDragDropPayload("TZJ_LAYER")) {
                auto from = *static_cast<const StackMove *>(payload->Data);
                if (from.frame == int(frame_) && from.kind == (sprite ? 1 : 0) &&
                    from.index != index)
                    move = std::pair{from, StackMove{int(frame_), sprite ? 1 : 0, index}};
            }
            ImGui::EndDragDropTarget();
        }
        if (ImGui::BeginPopupContextItem("Layer actions")) {
            if (ui::MenuItem("Duplicate layer")) {
                action = 0;
                action_index = index;
                action_sprite = sprite;
            }
            if (!sprite && f.layers[index].kind == LayerKind::Image &&
                ui::MenuItem("Make paintable copy")) {
                action = 1;
                action_index = index;
            }
            if (!sprite && ui::MenuItem("Merge down", nullptr, false, index > 0)) {
                action = 3;
                action_index = index;
            }
            if (ui::MenuItem("Move up", nullptr, false,
                             sprite ? index > 0 : index + 1 < int(f.layers.size()))) {
                action = 4;
                action_index = index;
                action_sprite = sprite;
            }
            if (ui::MenuItem("Move down", nullptr, false,
                             sprite ? index + 1 < int(f.sprites.size()) : index > 0)) {
                action = 5;
                action_index = index;
                action_sprite = sprite;
            }
            if (ui::MenuItem("Delete layer", nullptr, false, sprite || f.layers.size() > 1)) {
                action = 2;
                action_index = index;
                action_sprite = sprite;
            }
            ImGui::EndPopup();
        }
        ImGui::PopID();
    };
    // Collapse large sprite groups initially, keeping the background visible beneath them.
    ImGui::PushID("sprite-group");
    if (layer_switch("##eye", f.sprites_visible, false)) {
        checkpoint();
        f.sprites_visible = !f.sprites_visible;
        changed();
    }
    ImGui::SameLine(0, 2);
    if (layer_switch("##lock", f.sprites_locked, true)) {
        checkpoint();
        f.sprites_locked = !f.sprites_locked;
        changed();
    }
    ImGui::SameLine(0, 5);
    bool sprites_open =
        ImGui::TreeNodeEx("sprite-group",
                          (f.sprites.size() <= 4 ? ImGuiTreeNodeFlags_DefaultOpen : 0) |
                              ImGuiTreeNodeFlags_SpanAvailWidth,
                          "Sprites (%zu)", f.sprites.size());
    if (sprites_open) {
        for (int i = 0; i < int(f.sprites.size()); ++i)
            row(i, true);
        ImGui::TreePop();
    }
    ImGui::PopID();
    ImGui::PushID("background-group");
    if (layer_switch("##eye", f.background_visible, false)) {
        checkpoint();
        f.background_visible = !f.background_visible;
        changed();
    }
    ImGui::SameLine(0, 2);
    if (layer_switch("##lock", f.background_locked, true)) {
        checkpoint();
        f.background_locked = !f.background_locked;
        changed();
    }
    ImGui::SameLine(0, 5);
    bool background_open = ImGui::TreeNodeEx(
        "background-group", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth,
        "Background (%zu)", f.layers.size());
    if (background_open) {
        for (int i = int(f.layers.size()) - 1; i >= 0; --i)
            row(i, false);
        ImGui::TreePop();
    }
    ImGui::PopID();
    ImGui::EndChild();
    if (move) {
        try {
            checkpoint();
            auto [from, to] = *move;
            if (from.kind == 0) {
                project.move_layer(frame_, from.index, to.index);
                layer_ = -1;
            } else if (!f.sprites_locked && !f.sprites[from.index].locked) {
                auto s = std::move(f.sprites[from.index]);
                f.sprites.erase(f.sprites.begin() + from.index);
                f.sprites.insert(f.sprites.begin() + to.index, std::move(s));
                layer_ = to.index;
            }
            changed();
        } catch (const std::exception &e) {
            error_ = e.what();
        }
    }
    if (action >= 0)
        layer_action(action, action_index, action_sprite);
    ImGui::BeginDisabled(f.background_locked);
    if (ui::Button("+ Paint")) {
        checkpoint();
        project.add_paint_layer(frame_);
        layer_ = -1;
        palette_group_ = 0;
        tool_ = Tool::Pencil;
        changed();
    }
    ImGui::SameLine();
    if (ui::Button("+ Image"))
        file_dialog(FileAction::Background);
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(f.sprites_locked);
    if (ui::Button("+ Sprite"))
        add_blank_sprite();
    ImGui::EndDisabled();
}
void Editor::layer_properties() {
    auto &f = project.frames[frame_];
    if (f.active_layer < 0 || f.active_layer >= int(f.layers.size()))
        return;
    const int index = f.active_layer;
    auto &l = f.layers[index];
    ui::SeparatorText("Selected layer");
    char name[256];
    std::snprintf(name, sizeof(name), "%s", l.name.c_str());
    if (ui::InputText("Layer name", name, sizeof(name))) {
        checkpoint();
        l.name = name;
        changed();
    }
    if (!l.visible || !f.background_visible) {
        ui::TextDisabled("Hidden layer");
        ui::help("Hidden layer", "Show this layer with its eye control before painting.");
    }
    if (l.locked || f.background_locked) {
        ui::TextDisabled("Locked layer");
        ui::help("Locked layer", "Unlock this layer before editing its pixels or modifiers.");
    }
    if (l.kind == LayerKind::Image) {
        ui::TextDisabled("Image / live modifiers");
        ui::help("Image / live modifiers",
                 "The original image stays intact. Modifiers generate its NES conversion; Make "
                 "paintable copy captures that result as editable pixels.");
        ui::Text("Source: %d x %d", l.source.width, l.source.height);
        ImGui::BeginDisabled(l.locked || f.background_locked);
        bool effects = l.effects_enabled;
        if (ui::Checkbox("Enable modifiers", &effects)) {
            checkpoint();
            l.effects_enabled = effects;
            changed();
        }
        auto next = l.modifiers;
        bool modified = false;
        modified |= ui::Checkbox("Dithering", &next.dither_enabled);
        if (next.dither_enabled) {
            const char *methods[]{"None",        "Ordered 2x2",     "Ordered 4x4",
                                  "Ordered 8x8", "Floyd-Steinberg", "Atkinson"};
            int mode = int(next.dither);
            if (ui::Combo("Method", &mode, methods, 6)) {
                next.dither = static_cast<DitherMode>(mode);
                modified = true;
            }
            modified |= ui::SliderInt("Dither strength", &next.strength, 0, 100);
        }
        if (ui::CollapsingHeader("Color adjustments")) {
            modified |= ui::Checkbox("Color adjustment", &next.color_enabled);
            modified |= ui::SliderInt("Brightness", &next.brightness, -100, 100);
            modified |= ui::SliderInt("Contrast", &next.contrast, -100, 100);
        }
        if (ui::CollapsingHeader("Crop / resize / position")) {
            modified |= ui::Checkbox("Crop and resize", &next.transform_enabled);
            int crop[]{next.crop_x, next.crop_y};
            if (ui::InputInt2("Crop origin", crop)) {
                next.crop_x = std::clamp(crop[0], 0, 4096);
                next.crop_y = std::clamp(crop[1], 0, 4096);
                modified = true;
            }
            int size[]{next.crop_width, next.crop_height};
            if (ui::InputInt2("Crop size", size)) {
                next.crop_width = std::clamp(size[0], 0, 4096);
                next.crop_height = std::clamp(size[1], 0, 4096);
                modified = true;
            }
            int output[]{next.output_width, next.output_height};
            if (ui::InputInt2("Output size", output)) {
                next.output_width = std::clamp(output[0], 0, 4096);
                next.output_height = std::clamp(output[1], 0, 4096);
                modified = true;
            }
            int offset[]{next.offset_x, next.offset_y};
            if (ui::InputInt2("Layer position", offset)) {
                next.offset_x = std::clamp(offset[0], -4096, 4096);
                next.offset_y = std::clamp(offset[1], -4096, 4096);
                modified = true;
            }
            ImGui::TextWrapped(
                "Zero size keeps original dimensions. Resize preserves hard pixel edges.");
        }
        if (modified) {
            checkpoint();
            l.modifiers = next;
            l.conversion_stamp = 0;
            changed();
        }
        if (ui::Button("Fit NES colors to this layer")) {
            checkpoint();
            project.fit_layer_colors(frame_, std::size_t(index));
            changed();
        }
        ImGui::EndDisabled();

    } else {
        ui::TextDisabled("Paint / transparent pixels");
        ui::help("Paint / transparent pixels",
                 "Paint on the selected layer. Erasing reveals layers below. Layer operations are "
                 "in the Layers menu or the row's context menu.");
    }
}
} // namespace tzj
