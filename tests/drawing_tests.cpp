#include "core/tools.hpp"
#include <algorithm>
#include <iostream>
#include <stdexcept>
using namespace tzj;
static void check(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
static bool has(const std::vector<PixelPoint> &p, int x, int y) {
    return std::find(p.begin(), p.end(), PixelPoint{x, y}) != p.end();
}
int main() {
    try {
        auto line = drawing_pixels(DrawingShape::Line, 1, 1, 6, 3, 8, 8);
        check(line.size() == 6 && has(line, 1, 1) && has(line, 6, 3),
              "Line endpoints and continuity");
        auto clipped = drawing_pixels(DrawingShape::Line, -3, 2, 20, 2, 8, 8);
        check(clipped.size() == 8 && has(clipped, 0, 2) && has(clipped, 7, 2), "Line clipping");
        auto rect = drawing_pixels(DrawingShape::Rectangle, 5, 6, 2, 2, 8, 8);
        check(rect.size() == 14 && !has(rect, 3, 3), "Reversed outline rectangle");
        auto filled = drawing_pixels(DrawingShape::Rectangle, 2, 2, 5, 6, 8, 8, true);
        check(filled.size() == 20 && has(filled, 3, 3), "Filled rectangle");
        for (int w = 1; w <= 12; ++w)
            for (int h = 1; h <= 12; ++h) {
                auto oval = drawing_pixels(DrawingShape::Ellipse, 0, 0, w - 1, h - 1, w, h);
                auto solid = drawing_pixels(DrawingShape::Ellipse, 0, 0, w - 1, h - 1, w, h, true);
                check(!oval.empty() && solid.size() >= oval.size(),
                      "Degenerate ellipse/filled size");
                for (auto [x, y] : oval) {
                    check(has(oval, w - 1 - x, y) && has(oval, x, h - 1 - y), "Ellipse symmetry");
                    check(has(solid, x, y), "Ellipse outline missing from fill");
                }
                for (auto [x, y] : solid) {
                    check(has(solid, w - 1 - x, y) && has(solid, x, h - 1 - y),
                          "Filled ellipse symmetry");
                }
            }
        auto mirror = drawing_pixels(DrawingShape::Line, 1, 2, 1, 2, 8, 8, false, 2, true, true);
        check(mirror.size() == 16 && has(mirror, 6, 5) && has(mirror, 5, 4),
              "Thick brush symmetry");
        Project p;
        auto before = p.frames[0].pixels;
        auto attrs = p.frames[0].attributes;
        // Preview generation is read-only and uses the same coordinates as painting.
        auto points =
            drawing_pixels(DrawingShape::Rectangle, 14, 14, 18, 18, p.width, p.height, true);
        check(p.frames[0].pixels == before, "Preview changed artwork");
        History history;
        history.checkpoint(p);
        paint_pixels(p, 0, points, 3, 2);
        for (auto [x, y] : points) {
            check(p.frames[0].pixels[y * p.width + x] == 3 &&
                      p.frames[0].paint_mask[y * p.width + x] == 3,
                  "Shape pixels/paint mask");
            check(p.frames[0].attributes[(y / 16) * p.block_width() + x / 16] == 2,
                  "Palette block assignment");
        }
        check(history.undo(p) && p.frames[0].pixels == before && p.frames[0].attributes == attrs,
              "Whole shape undo");
        check(history.redo(p) && p.frames[0].pixels[14 * p.width + 14] == 3, "Shape redo");
        auto kept = p.frames[0].attributes;
        paint_pixels(p, 0, points, 0);
        check(p.frames[0].attributes == kept, "Erasing changed palette assignments");
        Sprite sprite;
        sprite.flip_x = true;
        sprite.flip_y = true;
        p.frames[0].sprites.push_back(sprite);
        auto bg = p.frames[0].pixels;
        paint_pixels(p, 0, {{0, 1}, {7, 7}, {-1, 2}, {8, 1}}, 2, 3, 0);
        check(p.frames[0].sprites[0].pixels[6 * 8 + 7] == 2 &&
                  p.frames[0].sprites[0].pixels[0] == 2,
              "Flipped sprite shape coordinates");
        check(p.frames[0].pixels == bg, "Sprite painting affected background");
        auto &s = p.frames[0].sprites[0];
        s.pixels.fill(0);
        for (int y = 0; y < 8; ++y)
            s.pixels[y * 8 + 4] = 1;
        flood_fill_sprite(p, 0, 0, 0, 0, 3); // Display left maps to storage right when flipped.
        check(s.pixels[7 * 8 + 7] == 3 && s.pixels[0] == 0 && s.pixels[4] == 1,
              "Sprite fill crossed a boundary or ignored flips");
        check(p.frames[0].pixels == bg, "Sprite fill affected background");
        std::cout << "Drawing tools passed\n";
    } catch (const std::exception &e) {
        std::cerr << e.what() << "\n";
        return 1;
    }
}
