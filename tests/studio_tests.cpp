#include "core/nes.hpp"
#include "core/tools.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <stdexcept>
using namespace tzj;
namespace {
void check(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
template <class F> void rejects(F fn, const char *message) {
    bool failed = false;
    try {
        fn();
    } catch (const std::exception &) {
        failed = true;
    }
    check(failed, message);
}
} // namespace
int main() {
    try {
        Project p;
        p.ensure_layers(0);
        int group = create_metasprite(p, 0, 16, 16, 1, 32, 40, "Hero");
        check(p.frames[0].sprites.size() == 4 &&
                  metasprite_bounds(p, 0, group) == PixelRect{32, 40, 16, 16},
              "Metasprite grid construction failed");
        p.frames[0].sprites[0].pixels[0] = 3;
        auto original = p.render();
        transform_metasprite(p, 0, group, BrushTransform::FlipX);
        check(p.frames[0].sprites[0].x == 40 && p.frames[0].sprites[0].flip_x,
              "Metasprite flip did not move and flip members");
        transform_metasprite(p, 0, group, BrushTransform::FlipX);
        check(p.render() == original, "Double metasprite flip changed artwork");
        for (int i = 0; i < 4; ++i)
            transform_metasprite(p, 0, group, BrushTransform::RotateCW);
        check(p.render() == original, "Four metasprite rotations changed artwork");
        move_metasprite(p, 0, group, 8, 4);
        check(metasprite_bounds(p, 0, group) == PixelRect{40, 44, 16, 16},
              "Metasprite movement failed");
        p.frames[0].groups[0].visible = false;
        check(compile_scene(p).sprite_count == 0, "Hidden metasprite leaked into ROM export");
        p.frames[0].groups[0].visible = true;
        p.frames[0].groups[0].locked = true;
        rejects([&] { move_metasprite(p, 0, group, 1, 0); }, "Locked group moved");
        rejects([&] { paint_pixels(p, 0, {{0, 0}}, 1, 0, 0); }, "Locked group member was painted");
        p.frames[0].groups[0].locked = false;
        int copy = duplicate_metasprite(p, 0, group);
        check(p.frames[0].sprites.size() == 8, "Metasprite duplication failed");
        delete_metasprite(p, 0, copy);
        check(p.frames[0].sprites.size() == 4, "Metasprite deletion removed wrong members");
        ungroup_sprites(p, 0, group);
        check(p.frames[0].groups.empty() && p.frames[0].sprites[0].group == -1, "Ungroup failed");
        group = group_sprites(p, 0, {0, 1, 2, 3}, "Hero");
        p.validate();
        Project map;
        map.ensure_layers(0);
        map.frames[0].layers[0].pixels[0] = 1;
        map.frames[0].layers[0].pixels[8] = 1;
        map.compose_layers(0);
        auto pixels = map.render();
        convert_to_tilemap(map, 0, 0);
        check(map.render() == pixels && map.frames[0].layers[0].kind == LayerKind::Tilemap,
              "Converting paint to tile references changed artwork");
        auto &layer = map.frames[0].layers[0];
        check(layer.tile_refs[0] == layer.tile_refs[1], "Identical tiles did not share an asset");
        map.write_background_pixel(0, 0, 3);
        int shared = layer.tile_refs[0];
        map.frames.push_back(map.frames[0]);
        map.tiles[shared].pixels[0] = 2;
        check(capture_brush(map, 1, 0, 0, 1, 1).pixels[0] == 2,
              "Capturing a map frame used stale tile pixels");
        map.tiles[shared].pixels[0] = 3;
        map.compose_layers(0);
        check(map.frames[0].pixels[0] == 3 && map.frames[0].pixels[8] == 3,
              "Referenced tile edits did not update all instances");
        auto transparent = Tile{};
        transparent.fill(255);
        auto asset = add_tile_asset(map, "Floating tile", transparent);
        map.tiles[asset].pixels[0] = 2;
        map.metatiles.push_back({"Platform", {int(asset), int(asset), int(asset), int(asset)}, 2});
        place_metatile(map, 0, 0, 2, 2, 0);
        check(map.frames[0].pixels[32 * 256 + 32] == 2 && map.frames[0].attributes[2 * 16 + 2] == 2,
              "Metatile stamping or palette assignment failed");
        auto before = map.render();
        map.frames[0].attributes[0] = 0;
        map.frames[0].attributes[1] = 1;
        map.frames[0].layers[0].tile_refs[2] = map.frames[0].layers[0].tile_refs[0];
        map.compose_layers(0);
        before = map.render();
        swap_palette_slots(map, false, 0, 1, 3, true);
        check(map.render() == before, "Palette remapping broke tile instances in other color sets");
        map.resize(512, 240);
        check(map.frames[0].layers[0].tile_refs.size() == 64 * 30,
              "Tile references did not resize with the canvas");
        map.validate();
        Project selection;
        selection.ensure_layers(0);
        paint_pixels(selection, 0, {{1, 1}, {2, 1}}, 2, 0);
        auto brush = capture_brush(selection, 0, 1, 1, 2, 1);
        auto scaled = resize_brush(brush, 4, 2);
        History history;
        history.checkpoint(selection);
        apply_selection(selection, 0, {1, 1, 2, 1}, 10, 12, scaled);
        check(selection.frames[0].layers[0].pixels[257] == 255 &&
                  selection.frames[0].pixels[12 * 256 + 10] == 2,
              "Selection move/scale failed");
        check(history.undo(selection) && selection.frames[0].pixels[257] == 2,
              "Selection transform was not one undo operation");
        auto path = pixel_perfect_path({{0, 0}, {1, 0}, {1, 1}, {2, 1}, {2, 2}});
        check(path == std::vector<PixelPoint>{{0, 0}, {1, 1}, {2, 2}},
              "Pixel-perfect pencil did not remove staircase corners");
        std::filesystem::create_directories("test-output");
        Project sheet;
        int character = create_metasprite(sheet, 0, 16, 16, 0, 32, 40, "Player");
        sheet.frames[0].sprites[0].pixels[0] = 1;
        sheet.frames.push_back(sheet.frames[0]);
        sheet.frames[1].duration_ms = 250;
        move_metasprite(sheet, 1, character, 8, 0);
        export_sprite_sheet("test-output/player-sheet", sheet, 2, true, character);
        auto image = load_image("test-output/player-sheet.png");
        check(image.width == 48 && image.height == 16 && image.at(0, 0).a == 255 &&
                  image.at(1, 0).a == 0,
              "Sprite sheet crop or transparency failed");
        nlohmann::json metadata;
        std::ifstream("test-output/player-sheet.json") >> metadata;
        check(metadata["frames"][1]["duration_ms"] == 250 &&
                  metadata["frames"][1]["metasprites"][0]["origin_x"] == 8,
              "Sprite-sheet timing or character origin was lost");
        sheet.frames[0].sprites[0].x = 600;
        sheet.frames[0].sprites[0].y = 300;
        auto issues = hardware_issues(sheet);
        auto outside = std::ranges::find(issues, std::string("sprite_position"), &NesIssue::code);
        check(outside != issues.end() && outside->sprite == 0 && outside->x == 600 &&
                  outside->y == 300,
              "NES diagnostics did not locate the offending sprite");
        sheet.frames[1].sprites_visible = false;
        for (auto &s : sheet.frames[0].sprites) {
            s.x += 600;
            s.y += 300;
        }
        export_sprite_sheet("test-output/offscreen-sheet", sheet, 1);
        auto offscreen = load_image("test-output/offscreen-sheet.png");
        check(offscreen.width < 1024, "Offscreen sprite sheet incorrectly included canvas origin");
        save_project("test-output/studio-groups.tzj", p);
        check(load_project("test-output/studio-groups.tzj") == p,
              "Metasprite metadata did not roundtrip");
        save_project("test-output/studio-map.tzj", map);
        check(load_project("test-output/studio-map.tzj") == map,
              "Tile assets or tile map references did not roundtrip");
        std::cout << "Studio model: metasprites, selections, referenced tile maps, metatiles, "
                     "palette remapping, pencil cleanup and persistence passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
