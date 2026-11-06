#include "core/tools.hpp"
#include <algorithm>
#include <cmath>
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
std::vector<PixelPoint> drawing_pixels(DrawingShape shape, int x0, int y0, int x1, int y1,
                                       int width, int height, bool filled, int size, bool mirror_x,
                                       bool mirror_y) {
    if (width < 1 || height < 1 || width > 512 || height > 512 || size < 1 || size > 32)
        throw std::runtime_error("Invalid drawing dimensions");
    // Endpoints may lie outside the target (e.g. dragging beyond a selected sprite).
    x0 = std::clamp(x0, -1024, 1024);
    x1 = std::clamp(x1, -1024, 1024);
    y0 = std::clamp(y0, -1024, 1024);
    y1 = std::clamp(y1, -1024, 1024);
    std::vector<Byte> mask(static_cast<std::size_t>(width) * height);
    auto dot = [&](int x, int y) {
        for (int dy = 0; dy < size; ++dy)
            for (int dx = 0; dx < size; ++dx) {
                int px = x + dx, py = y + dy;
                if (px < 0 || py < 0 || px >= width || py >= height)
                    continue;
                mask[py * width + px] = 1;
                if (mirror_x)
                    mask[py * width + width - 1 - px] = 1;
                if (mirror_y)
                    mask[(height - 1 - py) * width + px] = 1;
                if (mirror_x && mirror_y)
                    mask[(height - 1 - py) * width + width - 1 - px] = 1;
            }
    };
    auto line = [&](int ax, int ay, int bx, int by) {
        int dx = std::abs(bx - ax), sx = ax < bx ? 1 : -1;
        int dy = -std::abs(by - ay), sy = ay < by ? 1 : -1, error = dx + dy;
        for (;;) {
            dot(ax, ay);
            if (ax == bx && ay == by)
                break;
            int twice = 2 * error;
            if (twice >= dy) {
                error += dy;
                ax += sx;
            }
            if (twice <= dx) {
                error += dx;
                ay += sy;
            }
        }
    };
    if (shape == DrawingShape::Line)
        line(x0, y0, x1, y1);
    else {
        int left = std::min(x0, x1), right = std::max(x0, x1);
        int top = std::min(y0, y1), bottom = std::max(y0, y1);
        if (shape == DrawingShape::Rectangle) {
            if (filled) {
                for (int y = top; y <= bottom; ++y)
                    line(left, y, right, y);
            } else {
                line(left, top, right, top);
                line(left, bottom, right, bottom);
                line(left, top, left, bottom);
                line(right, top, right, bottom);
            }
        } else if (left == right || top == bottom)
            line(left, top, right, bottom);
        else {
            // Integer midpoint ellipse, including its tips for narrow bounding boxes.
            long long a = right - left, b = bottom - top, odd = b & 1;
            long long dx = 4 * (1 - a) * b * b, dy = 4 * (odd + 1) * a * a;
            long long error = dx + dy + odd * a * a;
            int low = top + static_cast<int>((b + 1) / 2), high = low - static_cast<int>(odd);
            const long long aa = 8 * a * a, bb = 8 * b * b;
            auto row = [&](int l, int r, int y) {
                if (filled)
                    line(l, y, r, y);
                else {
                    dot(l, y);
                    dot(r, y);
                }
            };
            do {
                row(left, right, low);
                row(left, right, high);
                long long twice = 2 * error;
                if (twice <= dy) {
                    ++low;
                    --high;
                    error += dy += aa;
                }
                if (twice >= dx || 2 * error > dy) {
                    ++left;
                    --right;
                    error += dx += bb;
                }
            } while (left <= right);
            while (low - high < b) {
                row(left - 1, right + 1, low++);
                row(left - 1, right + 1, high--);
            }
        }
    }
    std::vector<PixelPoint> result;
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x)
            if (mask[y * width + x])
                result.push_back({x, y});
    return result;
}
std::vector<PixelPoint> path_pixels(const std::vector<PixelPoint> &vertices, int width, int height,
                                    bool closed, bool filled, int size, bool mirror_x,
                                    bool mirror_y) {
    if (vertices.size() > 256)
        throw std::runtime_error("A path supports at most 256 points");
    // Validate dimensions even for an empty path.
    auto result = drawing_pixels(DrawingShape::Line, -1024, -1024, -1024, -1024, width, height,
                                 false, size, mirror_x, mirror_y);
    std::vector<Byte> mask(static_cast<std::size_t>(width) * height);
    auto segment = [&](PixelPoint a, PixelPoint b) {
        for (auto [x, y] : drawing_pixels(DrawingShape::Line, a.x, a.y, b.x, b.y, width, height,
                                          false, size, mirror_x, mirror_y))
            mask[y * width + x] = 1;
    };
    for (std::size_t i = 0; i < vertices.size(); ++i) {
        if (i)
            segment(vertices[i - 1], vertices[i]);
        else
            segment(vertices[0], vertices[0]);
    }
    if (closed && vertices.size() > 2) {
        segment(vertices.back(), vertices.front());
        if (filled) {
            for (int y = 0; y < height; ++y) {
                std::vector<double> crossings;
                const double scan = y + .5;
                for (std::size_t i = 0; i < vertices.size(); ++i) {
                    auto a = vertices[i], b = vertices[(i + 1) % vertices.size()];
                    if ((a.y <= scan && b.y > scan) || (b.y <= scan && a.y > scan))
                        crossings.push_back(a.x + (scan - a.y) * (double(b.x) - a.x) /
                                                      (double(b.y) - a.y));
                }
                std::sort(crossings.begin(), crossings.end());
                for (std::size_t i = 1; i < crossings.size(); i += 2) {
                    int left = std::clamp(int(std::ceil(crossings[i - 1])), 0, width - 1);
                    int right = std::clamp(int(std::floor(crossings[i])), -1, width - 1);
                    if (crossings[i] < 0 || crossings[i - 1] >= width)
                        continue;
                    for (int x = left; x <= right; ++x) {
                        mask[y * width + x] = 1;
                        if (mirror_x)
                            mask[y * width + width - 1 - x] = 1;
                        if (mirror_y)
                            mask[(height - 1 - y) * width + x] = 1;
                        if (mirror_x && mirror_y)
                            mask[(height - 1 - y) * width + width - 1 - x] = 1;
                    }
                }
            }
        }
    }
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x)
            if (mask[y * width + x])
                result.push_back({x, y});
    return result;
}
void flood_fill_sprite(Project &p, std::size_t frame, std::size_t sprite, int x, int y,
                       Byte color) {
    if (color > 3)
        throw std::runtime_error("Invalid NES drawing color");
    auto &s = p.frames.at(frame).sprites.at(sprite);
    if (x < 0 || x >= 8 || y < 0 || y >= 8)
        return;
    if (s.flip_x)
        x = 7 - x;
    if (s.flip_y)
        y = 7 - y;
    Byte old = s.pixels[y * 8 + x];
    if (old == color)
        return;
    std::vector<PixelPoint> pending{{x, y}};
    s.pixels[y * 8 + x] = color;
    for (std::size_t i = 0; i < pending.size(); ++i) {
        auto point = pending[i];
        for (auto [dx, dy] : {PixelPoint{1, 0}, {-1, 0}, {0, 1}, {0, -1}}) {
            int px = point.x + dx, py = point.y + dy;
            if (px < 0 || py < 0 || px >= 8 || py >= 8 || s.pixels[py * 8 + px] != old)
                continue;
            s.pixels[py * 8 + px] = color;
            pending.push_back({px, py});
        }
    }
}
void paint_pixels(Project &p, std::size_t frame, const std::vector<PixelPoint> &points, Byte color,
                  int palette, int sprite) {
    if (color > 3 || palette < -1 || palette > 3 || sprite < -1)
        throw std::runtime_error("Invalid NES drawing color or layer");
    auto &f = p.frames.at(frame);
    if (sprite >= 0) {
        auto &s = f.sprites.at(sprite);
        for (auto [x, y] : points) {
            if (x < 0 || x >= 8 || y < 0 || y >= 8)
                continue;
            s.pixels[(s.flip_y ? 7 - y : y) * 8 + (s.flip_x ? 7 - x : x)] = color;
        }
    } else {
        ensure_mask(f);
        for (auto [x, y] : points) {
            if (x < 0 || x >= p.width || y < 0 || y >= p.height)
                continue;
            f.pixels[y * p.width + x] = color;
            f.paint_mask[y * p.width + x] = color;
            if (palette >= 0)
                f.attributes[(y / 16) * p.block_width() + x / 16] = Byte(palette);
        }
    }
}
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
