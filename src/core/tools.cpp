#include "core/tools.hpp"
#include <algorithm>
#include <cstdlib>
#include <map>
#include <stdexcept>
namespace tzj {
namespace {
void check_tile(const Tile &tile) {
    for (auto value : tile)
        if (value > 3)
            throw std::runtime_error("A NES tile requires color slots 0..3");
}
void check_brush(const IndexedBrush &brush) {
    if (brush.width < 1 || brush.height < 1 || brush.width > 512 || brush.height > 512 ||
        brush.pixels.size() != static_cast<std::size_t>(brush.width) * brush.height)
        throw std::runtime_error("Invalid indexed brush dimensions");
    for (auto value : brush.pixels)
        if (value > 3)
            throw std::runtime_error("Invalid brush pixel");
}
void ensure_mask(Frame &frame) {
    if (frame.paint_mask.empty())
        frame.paint_mask.assign(frame.pixels.size(), 255);
}
} // namespace
Tile read_tile(const Project &project, std::size_t frame, int tx, int ty) {
    if (tx < 0 || ty < 0 || tx >= project.width / 8 || ty >= project.height / 8)
        throw std::runtime_error("Tile coordinates outside the map");
    const auto &pixels = project.frames.at(frame).pixels;
    Tile result{};
    for (int y = 0; y < 8; ++y)
        for (int x = 0; x < 8; ++x)
            result[y * 8 + x] = pixels[(ty * 8 + y) * project.width + tx * 8 + x];
    return result;
}
std::vector<TileEntry> tile_catalog(const Project &project, std::size_t frame) {
    std::vector<TileEntry> entries;
    std::map<Tile, std::size_t> lookup;
    for (int ty = 0; ty < project.height / 8; ++ty)
        for (int tx = 0; tx < project.width / 8; ++tx) {
            auto tile = read_tile(project, frame, tx, ty);
            auto [it, inserted] = lookup.try_emplace(tile, entries.size());
            if (inserted)
                entries.push_back({tile, {}});
            entries[it->second].locations.push_back({tx, ty});
        }
    return entries;
}
void stamp_tile(Project &project, std::size_t frame, int tx, int ty, const Tile &tile) {
    check_tile(tile);
    if (tx < 0 || ty < 0 || tx >= project.width / 8 || ty >= project.height / 8)
        throw std::runtime_error("Tile coordinates outside the map");
    auto &f = project.frames.at(frame);
    ensure_mask(f);
    for (int y = 0; y < 8; ++y)
        for (int x = 0; x < 8; ++x) {
            auto index = (ty * 8 + y) * project.width + tx * 8 + x;
            f.pixels[index] = tile[y * 8 + x];
            f.paint_mask[index] = tile[y * 8 + x];
        }
}
std::size_t replace_tile(Project &project, std::size_t frame, const Tile &original,
                         const Tile &replacement) {
    check_tile(original);
    check_tile(replacement);
    std::vector<std::pair<int, int>> locations;
    for (int ty = 0; ty < project.height / 8; ++ty)
        for (int tx = 0; tx < project.width / 8; ++tx)
            if (read_tile(project, frame, tx, ty) == original)
                locations.push_back({tx, ty});
    for (auto [tx, ty] : locations)
        stamp_tile(project, frame, tx, ty, replacement);
    return locations.size();
}
void paint_tile_line(Project &p, std::size_t frame, int x0, int y0, int x1, int y1,
                     const Tile &tile) {
    check_tile(tile);
    x0 = std::clamp(x0, 0, p.width / 8 - 1);
    x1 = std::clamp(x1, 0, p.width / 8 - 1);
    y0 = std::clamp(y0, 0, p.height / 8 - 1);
    y1 = std::clamp(y1, 0, p.height / 8 - 1);
    int dx = std::abs(x1 - x0), dy = -std::abs(y1 - y0), sx = x0 < x1 ? 1 : -1,
        sy = y0 < y1 ? 1 : -1, error = dx + dy;
    for (;;) {
        stamp_tile(p, frame, x0, y0, tile);
        if (x0 == x1 && y0 == y1)
            break;
        int twice = error * 2;
        if (twice >= dy) {
            error += dy;
            x0 += sx;
        }
        if (twice <= dx) {
            error += dx;
            y0 += sy;
        }
    }
}
IndexedBrush capture_brush(const Project &project, std::size_t frame, int x, int y, int w, int h) {
    if (w < 1 || h < 1 || w > project.width || h > project.height || x < 0 || y < 0 ||
        x > project.width - w || y > project.height - h)
        throw std::runtime_error("Selection outside the canvas");
    const auto &f = project.frames.at(frame);
    IndexedBrush result{w, h, std::vector<Byte>(w * h)};
    for (int row = 0; row < h; ++row)
        std::copy_n(f.pixels.begin() + (y + row) * project.width + x, w,
                    result.pixels.begin() + row * w);
    return result;
}
IndexedBrush transform_brush(const IndexedBrush &brush, BrushTransform transform) {
    check_brush(brush);
    bool rotate = transform == BrushTransform::RotateCW || transform == BrushTransform::RotateCCW;
    IndexedBrush result{rotate ? brush.height : brush.width, rotate ? brush.width : brush.height,
                        std::vector<Byte>(brush.pixels.size())};
    for (int y = 0; y < brush.height; ++y)
        for (int x = 0; x < brush.width; ++x) {
            int nx = x, ny = y;
            switch (transform) {
            case BrushTransform::FlipX:
                nx = brush.width - 1 - x;
                break;
            case BrushTransform::FlipY:
                ny = brush.height - 1 - y;
                break;
            case BrushTransform::RotateCW:
                nx = brush.height - 1 - y;
                ny = x;
                break;
            case BrushTransform::RotateCCW:
                nx = y;
                ny = brush.width - 1 - x;
                break;
            }
            result.pixels[ny * result.width + nx] = brush.pixels[y * brush.width + x];
        }
    return result;
}
void stamp_brush(Project &p, std::size_t frame, int x, int y, const IndexedBrush &brush,
                 bool transparent, bool wrap) {
    check_brush(brush);
    auto &f = p.frames.at(frame);
    ensure_mask(f);
    for (int by = 0; by < brush.height; ++by)
        for (int bx = 0; bx < brush.width; ++bx) {
            Byte c = brush.pixels[by * brush.width + bx];
            if (transparent && c == 0)
                continue;
            long long px = static_cast<long long>(x) + bx, py = static_cast<long long>(y) + by;
            if (wrap) {
                px = (px % p.width + p.width) % p.width;
                py = (py % p.height + p.height) % p.height;
            }
            if (px < 0 || py < 0 || px >= p.width || py >= p.height)
                continue;
            auto index = static_cast<std::size_t>(py) * p.width + static_cast<std::size_t>(px);
            f.pixels[index] = c;
            f.paint_mask[index] = c;
        }
}
void swap_palette_slots(Project &project, bool sprites, int palette, int a, int b, bool preserve) {
    if (palette < 0 || palette > 3 || a < 1 || a > 3 || b < 1 || b > 3)
        throw std::runtime_error("Swap nonzero NES slots 1..3; slot 0 is reserved");
    if (a == b)
        return;
    auto &palettes = sprites ? project.sprite_palettes : project.background;
    std::swap(palettes[palette][a], palettes[palette][b]);
    if (!preserve)
        return;
    auto remap = [=](Byte c) {
        return c == a ? static_cast<Byte>(b) : c == b ? static_cast<Byte>(a) : c;
    };
    for (auto &f : project.frames) {
        if (sprites) {
            for (auto &sprite : f.sprites)
                if (sprite.palette == palette)
                    for (auto &c : sprite.pixels)
                        c = remap(c);
        } else
            for (int y = 0; y < project.height; ++y)
                for (int x = 0; x < project.width; ++x)
                    if (f.attributes[(y / 16) * project.block_width() + x / 16] == palette) {
                        auto i = y * project.width + x;
                        f.pixels[i] = remap(f.pixels[i]);
                        if (!f.paint_mask.empty())
                            f.paint_mask[i] = remap(f.paint_mask[i]);
                    }
    }
}
} // namespace tzj
