#include "core/nes.hpp"
#include "core/tools.hpp"
#include <algorithm>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace tzj;
static void check(bool condition, const char *message) {
    if (!condition)
        throw std::runtime_error(message);
}
template <class F> static void rejects(F action, const char *message) {
    bool failed = false;
    try {
        action();
    } catch (const std::exception &) {
        failed = true;
    }
    check(failed, message);
}
int main() {
    try {
        Project p;
        p.resize(512, 240);
        auto catalog = tile_catalog(p, 0);
        check(catalog.size() == 1 && catalog[0].locations.size() == 1920, "Blank tile usage/dedup");
        Tile pattern{};
        pattern[0] = 1;
        pattern[7] = 2;
        pattern[63] = 3;
        stamp_tile(p, 0, 0, 0, pattern);
        stamp_tile(p, 0, 40, 8, pattern);
        p.frames[0].attributes[0] = 1;
        p.frames[0].attributes[4 * p.block_width() + 20] = 2;
        auto attributes = p.frames[0].attributes;
        catalog = tile_catalog(p, 0);
        check(catalog.size() == 2 && catalog[0].locations.size() == 2,
              "Identical patterns with different palettes did not share a tile");
        Tile edited = pattern;
        edited[12] = 3;
        History history;
        history.checkpoint(p);
        auto before = p;
        check(replace_tile(p, 0, pattern, edited) == 2, "Linked replacement count");
        check(read_tile(p, 0, 0, 0) == edited && read_tile(p, 0, 40, 8) == edited,
              "Linked map edit did not update both screens");
        check(p.frames[0].attributes == attributes, "Linked edit changed attribute palettes");
        check(p.frames[0].paint_mask[1 * p.width + 4] == 3,
              "Linked edit not protected from source reload");
        check(history.undo(p) && p == before, "Linked tile undo");
        check(history.redo(p), "Linked tile redo");
        check(compile_scene(p).background_tiles == 2, "Edited map CHR export differs from catalog");
        check(replace_tile(p, 0, edited, Tile{}) == 2 && tile_catalog(p, 0).size() == 1,
              "Merging a tile did not deduplicate");
        paint_tile_line(p, 0, 0, 0, 31, 29, pattern);
        check(read_tile(p, 0, 31, 29) == pattern && read_tile(p, 0, 15, 14) == pattern,
              "Tile line gaps/endpoints");
        rejects([&] { stamp_tile(p, 0, 64, 0, pattern); }, "Out of range tile coordinate accepted");
        Tile invalid{};
        invalid[3] = 4;
        rejects([&] { replace_tile(p, 0, pattern, invalid); }, "Invalid linked tile accepted");

        Project paint;
        paint_line(paint, 0, 15, 0, 16, 0, 2, 1, 2);
        check(paint.frames[0].attributes[0] == 2 && paint.frames[0].attributes[1] == 2 &&
                  paint.frames[0].attributes[2] == 0,
              "Drawing did not apply the chosen color set precisely");
        auto paint_image = paint.render();
        auto desired = nes_colors[paint.background[2][2]];
        check(paint_image.at(15, 0) == desired, "Painted color differs from chosen swatch");
        Project fill_color;
        flood_fill(fill_color, 0, 0, 0, 0, 2);
        check(std::all_of(fill_color.frames[0].attributes.begin(),
                          fill_color.frames[0].attributes.end(), [](Byte b) { return b == 2; }),
              "Same-slot fill did not apply the new color set");
        IndexedBrush brush{3, 2, {0, 1, 2, 3, 2, 1}};
        auto cw = transform_brush(brush, BrushTransform::RotateCW);
        check(cw == IndexedBrush{2, 3, {3, 0, 2, 1, 1, 2}}, "Non-square clockwise rotation");
        check(transform_brush(cw, BrushTransform::RotateCCW) == brush, "Inverse brush rotation");
        auto rotated = brush;
        for (int i = 0; i < 4; ++i)
            rotated = transform_brush(rotated, BrushTransform::RotateCW);
        check(rotated == brush, "Four rotations did not restore brush");
        check(transform_brush(brush, BrushTransform::FlipX) ==
                  IndexedBrush{3, 2, {2, 1, 0, 1, 2, 3}},
              "Brush X flip");
        check(transform_brush(brush, BrushTransform::FlipY) ==
                  IndexedBrush{3, 2, {3, 2, 1, 0, 1, 2}},
              "Brush Y flip");
        IndexedBrush wide{512, 1, std::vector<Byte>(512, 2)};
        auto tall = transform_brush(wide, BrushTransform::RotateCW);
        check(transform_brush(tall, BrushTransform::RotateCCW) == wide,
              "Rotated wide canvas brush rejected");
        Project stamps;
        stamps.frames[0].pixels.assign(256 * 240, 3);
        stamp_brush(stamps, 0, 255, 239, brush, true, true);
        check(stamps.frames[0].pixels[239 * 256 + 255] == 3,
              "Transparent brush overwrote slot zero");
        check(stamps.frames[0].pixels[239 * 256] == 1 &&
                  stamps.frames[0].pixels[239 * 256 + 1] == 2 && stamps.frames[0].pixels[0] == 2,
              "Wrapped brush edges/corner");
        stamp_brush(stamps, 0, -1, -1, brush, false, false);
        check(stamps.frames[0].pixels[0] == 2 && stamps.frames[0].pixels[1] == 1,
              "Clipped negative brush coordinates");
        stamp_brush(stamps, 0, std::numeric_limits<int>::max(), std::numeric_limits<int>::max(),
                    brush, false, false);
        rejects([&] { stamp_brush(stamps, 0, 0, 0, {3, 2, {0, 1}}); }, "Bad brush length accepted");
        auto captured = capture_brush(stamps, 0, 0, 0, 2, 1);
        check(captured.pixels == std::vector<Byte>{2, 1}, "Selection capture");
        rejects([&] { capture_brush(stamps, 0, 255, 239, 3, 2); },
                "Out of bounds selection accepted");

        Project colors = demo_project();
        paint_line(colors, 0, 2, 2, 2, 2, 1, 1);
        colors.frames.push_back(colors.frames[0]);
        Sprite sprite;
        sprite.palette = 2;
        sprite.x = 20;
        sprite.y = 25;
        sprite.pixels.fill(1);
        sprite.pixels[2] = 0;
        colors.frames[0].sprites.push_back(sprite);
        colors.frames[1].sprites.push_back(sprite);
        auto original = colors;
        std::vector<Image> images;
        for (std::size_t f = 0; f < colors.frames.size(); ++f)
            images.push_back(colors.render(f));
        swap_palette_slots(colors, false, 0, 1, 3, true);
        swap_palette_slots(colors, true, 2, 1, 2, true);
        for (std::size_t f = 0; f < colors.frames.size(); ++f)
            check(colors.render(f) == images[f], "Palette swap changed artwork");
        check(colors.frames[0].paint_mask[2 * colors.width + 2] == 3,
              "Palette swap missed paint overrides");
        check(colors.frames[0].sprites.back().pixels[2] == 0, "Palette swap lost sprite transparency");
        check(colors.background[0][0] == original.background[0][0],
              "Palette swap modified backdrop");
        swap_palette_slots(colors, false, 0, 1, 3, true);
        swap_palette_slots(colors, true, 2, 1, 2, true);
        check(colors == original, "Palette swap inverse");
        rejects([&] { swap_palette_slots(colors, false, 0, 0, 1); },
                "Reserved backdrop slot reordered");
        rejects([&] { swap_palette_slots(colors, true, 4, 1, 2); }, "Invalid palette reordered");
        swap_palette_slots(colors, false, 0, 1, 2, false);
        check(colors.frames == original.frames, "Recolor-only swap remapped pixels");
        auto workflow = original;
        swap_palette_slots(workflow, false, 0, 1, 3, true);
        swap_palette_slots(workflow, true, 2, 1, 2, true);
        auto bank = tile_catalog(workflow, 0);
        auto edited_tile = bank[0].pixels;
        edited_tile[8] = 2;
        replace_tile(workflow, 0, bank[0].pixels, edited_tile);
        auto dir = std::filesystem::current_path() / "test-output";
        std::filesystem::create_directories(dir);
        export_scene(dir / "workflow", workflow);
        save_project(dir / "workflow.tzj", workflow);
        std::cout << "All editing workflow tests passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << "\n";
        return 1;
    }
}
