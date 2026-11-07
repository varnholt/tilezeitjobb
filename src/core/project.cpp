#include "core/tools.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <map>
#include <nlohmann/json.hpp>
#include <queue>
#include <sstream>
#include <stdexcept>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
namespace tzj {
using Json = nlohmann::json;
static int checked_int(const Json &value, int low, int high) {
    if (!value.is_number_integer())
        throw std::runtime_error("Expected an integer in project data");
    if (value.is_number_unsigned()) {
        auto n = value.get<std::uint64_t>();
        if (n > static_cast<std::uint64_t>(high))
            throw std::runtime_error("Project number out of range");
        return static_cast<int>(n);
    }
    auto n = value.get<std::int64_t>();
    if (n < low || n > high)
        throw std::runtime_error("Project number out of range");
    return static_cast<int>(n);
}
static void byte_array(const Json &value, std::size_t size, int maximum) {
    if (!value.is_array() || value.size() != size)
        throw std::runtime_error("Invalid array length in project");
    for (const auto &n : value)
        checked_int(n, 0, maximum);
}
static void palettes_json(const Json &value) {
    if (!value.is_array() || value.size() != 4)
        throw std::runtime_error("Exactly four palettes are required");
    for (const auto &pal : value)
        byte_array(pal, 4, 63);
}
Project::Project() {
    frames.push_back(Frame{std::vector<Byte>(width * height),
                           std::vector<Byte>(block_width() * block_height()),
                           {},
                           120});
}
void Project::resize(int w, int h) {
    if (!((w == 256 && h == 240) || (w == 512 && h == 240) || (w == 256 && h == 480)))
        throw std::runtime_error("Choose 256x240, 512x240, or 256x480");
    for (auto &f : frames) {
        std::vector<Byte> p(w * h), a((w / 16) * (h / 16));
        for (int y = 0; y < std::min(h, height); ++y)
            std::copy_n(f.pixels.begin() + y * width, std::min(w, width), p.begin() + y * w);
        for (int y = 0; y < std::min(h, height) / 16; ++y)
            std::copy_n(f.attributes.begin() + y * block_width(), std::min(w, width) / 16,
                        a.begin() + y * (w / 16));
        if (!f.paint_mask.empty()) {
            std::vector<Byte> mask(w * h, 255);
            for (int y = 0; y < std::min(h, height); ++y)
                std::copy_n(f.paint_mask.begin() + y * width, std::min(w, width),
                            mask.begin() + y * w);
            f.paint_mask = std::move(mask);
        }
        for (auto &layer : f.layers) {
            if (layer.kind == LayerKind::Tilemap) {
                std::vector<int> refs(std::size_t(w / 8) * (h / 8), -1);
                for (int y = 0; y < std::min(h, height) / 8; ++y)
                    std::copy_n(layer.tile_refs.begin() + y * (width / 8), std::min(w, width) / 8,
                                refs.begin() + y * (w / 8));
                layer.tile_refs = std::move(refs);
            }
            if (layer.kind == LayerKind::Paint) {
                std::vector<Byte> resized(static_cast<std::size_t>(w) * h, 255);
                for (int y = 0; y < std::min(h, height); ++y)
                    std::copy_n(layer.pixels.begin() + y * width, std::min(w, width),
                                resized.begin() + y * w);
                layer.pixels = std::move(resized);
            }
            layer.converted.clear();
            layer.conversion_stamp = 0;
        }
        f.pixels = std::move(p);
        f.attributes = std::move(a);
    }
    width = w;
    height = h;
    for (std::size_t i = 0; i < frames.size(); ++i)
        compose_layers(i);
}
void Project::set_backdrop(Byte c) {
    if (c >= 64)
        throw std::runtime_error("NES color index out of range");
    for (auto &p : background)
        p[0] = c;
    for (auto &p : sprite_palettes)
        p[0] = c;
}
void Project::validate() const {
    const auto fail = [](bool bad, const char *s) {
        if (bad)
            throw std::runtime_error(s);
    };
    fail(!((width == 256 && height == 240) || (width == 512 && height == 240) ||
           (width == 256 && height == 480)),
         "Invalid project dimensions");
    fail(frames.empty() || frames.size() > 256, "Project requires 1 to 256 frames");
    fail(tiles.size() > 4096 || metatiles.size() > 4096, "Too many tile assets or metatiles");
    for (const auto &tile : tiles) {
        fail(tile.name.empty() || tile.name.size() > 128, "Invalid tile asset name");
        for (auto pixel : tile.pixels)
            fail(pixel > 3 && pixel != 255, "Invalid tile asset pixel");
    }
    for (const auto &meta : metatiles) {
        fail(meta.name.empty() || meta.name.size() > 128 || meta.palette < 0 || meta.palette > 3,
             "Invalid metatile");
        for (int id : meta.tiles)
            fail(id < -1 || id >= int(tiles.size()), "Invalid metatile tile reference");
    }
    fail(brushes.size() > 128 || ramps.size() > 64, "Too many saved brushes or color ramps");
    std::size_t brush_bytes = 0;
    for (const auto &brush : brushes) {
        fail(brush.name.empty() || brush.name.size() > 128 || brush.width < 1 || brush.height < 1 ||
                 brush.width > 512 || brush.height > 512 ||
                 brush.pixels.size() != std::size_t(brush.width) * brush.height,
             "Invalid saved brush");
        brush_bytes += brush.pixels.size();
        for (auto pixel : brush.pixels)
            fail(pixel > 3 && pixel != 255, "Invalid saved brush color");
    }
    fail(brush_bytes > 16 * 1024 * 1024, "Saved brush library exceeds 16 MiB");
    for (const auto &ramp : ramps) {
        fail(ramp.name.empty() || ramp.name.size() > 128, "Invalid color ramp name");
        for (auto c : ramp.colors)
            fail(c > 63, "Invalid saved ramp color");
        for (auto slot : ramp.order)
            fail(slot > 3, "Invalid saved ramp slot");
    }
    fail(font.name.empty() || font.name.size() > 128, "Invalid bitmap font name");
    for (const auto &glyph : font.glyphs)
        for (auto pixel : glyph)
            fail(pixel > 3 && pixel != 255, "Invalid bitmap font color");
    fail(std::abs(static_cast<long long>(offset_x)) > 4096 ||
             std::abs(static_cast<long long>(offset_y)) > 4096,
         "Image offset out of range");
    for (const auto &group : {background, sprite_palettes})
        for (const auto &pal : group) {
            for (auto c : pal)
                fail(c >= 64, "Invalid NES palette color");
            fail(pal[0] != background[0][0], "All subpalettes must share the backdrop");
        }
    for (const auto &im : {&reference, &sprite_sheet})
        fail((im->width == 0) != (im->height == 0) || im->width < 0 || im->height < 0 ||
                 im->width > 4096 || im->height > 4096 ||
                 im->rgba.size() != static_cast<std::size_t>(im->width) * im->height * 4,
             "Invalid embedded image");
    for (const auto &f : frames) {
        fail(f.pixels.size() != static_cast<std::size_t>(width) * height ||
                 f.attributes.size() != static_cast<std::size_t>(block_width()) * block_height(),
             "Invalid frame buffer length");
        fail(!f.paint_mask.empty() && f.paint_mask.size() != f.pixels.size(),
             "Invalid paint mask length");
        for (auto b : f.paint_mask)
            fail(b > 3 && b != 255, "Invalid paint mask value");
        fail(f.duration_ms < 10 || f.duration_ms > 60000, "Frame duration must be 10 to 60000 ms");
        fail(f.layers.size() > 64, "Too many background layers");
        fail(!f.layers.empty() && (f.active_layer < 0 || f.active_layer >= int(f.layers.size())),
             "Invalid active background layer");
        for (const auto &l : f.layers) {
            fail(l.name.size() > 4096 || l.source_path.size() > 32768,
                 "Layer text exceeds project limits");
            fail(l.kind != LayerKind::Paint && l.kind != LayerKind::Image &&
                     l.kind != LayerKind::Tilemap,
                 "Invalid layer type");
            fail(l.guide_opacity < 0 || l.guide_opacity > 100 ||
                     (l.reference_only && l.kind != LayerKind::Image),
                 "Invalid reference guide settings");
            if (l.kind == LayerKind::Paint || l.kind == LayerKind::Tilemap) {
                fail(l.pixels.size() != f.pixels.size(), "Invalid paint layer dimensions");
                for (auto c : l.pixels)
                    fail(c > 3 && c != 255, "Invalid indexed layer pixel");
                if (l.kind == LayerKind::Tilemap) {
                    fail(l.tile_refs.size() != std::size_t(width / 8) * (height / 8),
                         "Invalid tile reference map");
                    for (int id : l.tile_refs)
                        fail(id < -1 || id >= int(tiles.size()), "Invalid map tile reference");
                }
            } else {
                fail(l.source.width < 1 || l.source.height < 1 || l.source.width > 4096 ||
                         l.source.height > 4096 ||
                         l.source.rgba.size() != std::size_t(l.source.width) * l.source.height * 4,
                     "Invalid layer source image");
            }
            const auto &m = l.modifiers;
            for (int v :
                 {m.crop_x, m.crop_y, m.crop_width, m.crop_height, m.output_width, m.output_height})
                fail(v < 0 || v > 4096, "Invalid crop or resize dimensions");
            fail(m.offset_x < -4096 || m.offset_x > 4096 || m.offset_y < -4096 || m.offset_y > 4096,
                 "Invalid image layer position");
            fail(m.brightness < -100 || m.brightness > 100 || m.contrast < -100 ||
                     m.contrast > 100 || m.strength < 0 || m.strength > 100,
                 "Invalid image modifier amount");
            fail(int(m.dither) < 0 || int(m.dither) > int(DitherMode::Atkinson),
                 "Invalid dithering method");
        }
        fail(f.sprites.size() > 4096, "Too many sprite objects");
        fail(f.groups.size() > 128, "Too many metasprite groups");
        std::vector<int> group_ids;
        for (const auto &g : f.groups) {
            fail(g.id < 0 || g.id > 1000000 || g.name.empty() || g.name.size() > 128 ||
                     std::abs(static_cast<long long>(g.origin_x)) > 4096 ||
                     std::abs(static_cast<long long>(g.origin_y)) > 4096,
                 "Invalid metasprite group");
            fail(std::find(group_ids.begin(), group_ids.end(), g.id) != group_ids.end(),
                 "Duplicate metasprite ID");
            group_ids.push_back(g.id);
        }
        for (auto p : f.pixels)
            fail(p > 3, "Pixel index outside 0..3");
        for (auto p : f.attributes)
            fail(p > 3, "Attribute index outside 0..3");
        for (const auto &s : f.sprites) {
            fail(s.group < -1 || (s.group >= 0 && std::find(group_ids.begin(), group_ids.end(),
                                                            s.group) == group_ids.end()),
                 "Sprite references a missing metasprite");
            fail(s.palette < 0 || s.palette > 3, "Sprite palette outside 0..3");
            fail(std::abs(static_cast<long long>(s.x)) > 4096 ||
                     std::abs(static_cast<long long>(s.y)) > 4096,
                 "Sprite position out of range");
            for (auto p : s.pixels)
                fail(p > 3, "Sprite pixel outside 0..3");
        }
    }
}
void Project::quantize(std::size_t frame, bool auto_palettes) {
    if (!frames.at(frame).layers.empty()) {
        auto &f = frames[frame];
        int target = f.active_layer;
        if (target < 0 || f.layers.at(target).kind != LayerKind::Image) {
            target = -1;
            for (int i = int(f.layers.size()) - 1; i >= 0; --i)
                if (f.layers[i].kind == LayerKind::Image) {
                    target = i;
                    break;
                }
        }
        if (target < 0)
            throw std::runtime_error("Choose an imported image layer");
        auto &l = f.layers[target];
        if (!reference.empty() && l.source_path == image_path)
            l.source = reference;
        l.conversion_stamp = 0;
        if (auto_palettes)
            fit_layer_colors(frame, std::size_t(target));
        else
            compose_layers(frame, true);
        return;
    }
    if (reference.empty())
        throw std::runtime_error("Import a reference image first");
    auto &f = frames.at(frame);
    if (auto_palettes) {
        std::array<int, 64> histogram{};
        for (int y = 0; y < height; ++y)
            for (int x = 0; x < width; ++x) {
                auto c = reference.at(x - offset_x, y - offset_y);
                if (c.a >= 128)
                    ++histogram[nearest_nes(c)];
            }
        const Byte bg = static_cast<Byte>(std::max_element(histogram.begin(), histogram.end()) -
                                          histogram.begin());
        set_backdrop(bg);
        std::map<std::array<Byte, 3>, int> candidates;
        for (int by = 0; by < block_height(); ++by)
            for (int bx = 0; bx < block_width(); ++bx) {
                std::array<int, 64> hist{};
                for (int y = 0; y < 16; ++y)
                    for (int x = 0; x < 16; ++x) {
                        auto c = reference.at(bx * 16 + x - offset_x, by * 16 + y - offset_y);
                        if (c.a >= 128)
                            ++hist[nearest_nes(c)];
                    }
                hist[bg] = 0;
                std::array<Byte, 3> triple{};
                for (auto &c : triple) {
                    auto it = std::max_element(hist.begin(), hist.end());
                    c = *it ? static_cast<Byte>(it - hist.begin()) : bg;
                    *it = 0;
                }
                std::sort(triple.begin(), triple.end());
                ++candidates[triple];
            }
        std::vector<std::pair<std::array<Byte, 3>, int>> sorted(candidates.begin(),
                                                                candidates.end());
        std::stable_sort(sorted.begin(), sorted.end(),
                         [](const auto &a, const auto &b) { return a.second > b.second; });
        for (int i = 0; i < 4; ++i) {
            const auto &p = sorted[std::min(static_cast<std::size_t>(i), sorted.size() - 1)].first;
            background[i] = {bg, p[0], p[1], p[2]};
        }
        for (int by = 0; by < block_height(); ++by)
            for (int bx = 0; bx < block_width(); ++bx) {
                long long best = std::numeric_limits<long long>::max();
                int pal = 0;
                for (int i = 0; i < 4; ++i) {
                    long long error = 0;
                    for (int y = 0; y < 16; ++y)
                        for (int x = 0; x < 16; ++x) {
                            auto c = reference.at(bx * 16 + x - offset_x, by * 16 + y - offset_y);
                            if (c.a < 128)
                                continue;
                            auto s = nearest_slot(c, background[i], x, y, false);
                            auto n = nes_colors[background[i][s]];
                            int r = int(c.r) - n.r, g = int(c.g) - n.g, b = int(c.b) - n.b;
                            error += 3 * r * r + 6 * g * g + 2 * b * b;
                        }
                    if (error < best) {
                        best = error;
                        pal = i;
                    }
                }
                f.attributes[by * block_width() + bx] = static_cast<Byte>(pal);
            }
    }
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x)
            f.pixels[y * width + x] =
                !f.paint_mask.empty() && f.paint_mask[y * width + x] != 255
                    ? f.paint_mask[y * width + x]
                    : nearest_slot(reference.at(x - offset_x, y - offset_y),
                                   background[f.attributes[(y / 16) * block_width() + x / 16]], x,
                                   y, dither);
}
Sprite Project::make_sprite(int tx, int ty, int pal, int x, int y) const {
    if (sprite_sheet.empty())
        throw std::runtime_error("Import a sprite sheet first");
    if (tx < 0 || ty < 0 || tx * 8 + 8 > sprite_sheet.width || ty * 8 + 8 > sprite_sheet.height ||
        pal < 0 || pal > 3)
        throw std::runtime_error("Sprite tile or palette is out of range");
    Sprite s;
    s.tile_x = tx;
    s.tile_y = ty;
    s.palette = pal;
    s.x = x;
    s.y = y;
    for (int py = 0; py < 8; ++py)
        for (int px = 0; px < 8; ++px)
            s.pixels[py * 8 + px] = nearest_slot(sprite_sheet.at(tx * 8 + px, ty * 8 + py),
                                                 sprite_palettes[pal], px, py, dither, true);
    return s;
}
void Project::generate_sprites(std::size_t frame, int pal) {
    auto &f = frames.at(frame);
    std::vector<Sprite> generated;
    for (int y = 0; y < std::min(height, sprite_sheet.height) / 8; ++y)
        for (int x = 0; x < std::min(width, sprite_sheet.width) / 8; ++x) {
            auto s = make_sprite(x, y, pal, x * 8, y * 8 + 1);
            if (std::any_of(s.pixels.begin(), s.pixels.end(), [](Byte c) { return c != 0; }))
                generated.push_back(std::move(s));
        }
    f.sprites = std::move(generated);
    f.groups.clear();
}
Image Project::render(std::size_t index, bool sprites, bool hardware_limits, bool guides) const {
    Frame flattened;
    if (!frames.at(index).layers.empty())
        flattened = composed_frame(index);
    const auto &f = frames.at(index).layers.empty() ? frames.at(index) : flattened;
    Image im{width, height, std::vector<Byte>(width * height * 4)};
    auto set = [&](int x, int y, Color c) {
        const auto i = (y * width + x) * 4;
        im.rgba[i] = c.r;
        im.rgba[i + 1] = c.g;
        im.rgba[i + 2] = c.b;
        im.rgba[i + 3] = 255;
    };
    bool show_guides = guides && f.background_visible &&
                       std::any_of(f.layers.begin(), f.layers.end(),
                                   [](const auto &l) { return l.visible && l.reference_only; });
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x) {
            auto palette = background[f.attributes[(y / 16) * block_width() + x / 16]];
            Color color = nes_colors[palette[f.pixels[y * width + x]]];
            if (show_guides) {
                color = nes_colors[background[0][0]];
                for (const auto &layer : f.layers)
                    if (layer.visible) {
                        if (layer.reference_only) {
                            auto over = guide_pixel(layer, x, y);
                            color.r =
                                Byte((int(over.r) * over.a + int(color.r) * (255 - over.a)) / 255);
                            color.g =
                                Byte((int(over.g) * over.a + int(color.g) * (255 - over.a)) / 255);
                            color.b =
                                Byte((int(over.b) * over.a + int(color.b) * (255 - over.a)) / 255);
                        } else {
                            auto pixel =
                                (layer.kind == LayerKind::Image ? layer.converted : layer.pixels)
                                    .at(y * width + x);
                            if (pixel != 255)
                                color = nes_colors[palette[pixel]];
                        }
                    }
            }
            set(x, y, color);
        }
    if (sprites && f.sprites_visible) {
        // OAM priority: lower object index wins, including transparent-background priority.
        std::vector<bool> claimed(width * height);
        std::array<int, 480> scanlines{};
        int count = 0;
        for (const auto &s : f.sprites) {
            if (!sprite_visible(index, s))
                continue;
            if (hardware_limits && count++ >= 64)
                break;
            for (int y = 0; y < 8; ++y) {
                int py = s.y + y;
                if (py < 0 || py >= height)
                    continue;
                bool row = !hardware_limits || scanlines[py]++ < 8;
                if (!row)
                    continue;
                for (int x = 0; x < 8; ++x) {
                    int px = s.x + x;
                    if (px < 0 || px >= width)
                        continue;
                    Byte v = s.pixels[(s.flip_y ? 7 - y : y) * 8 + (s.flip_x ? 7 - x : x)];
                    if (!v || claimed[py * width + px])
                        continue;
                    claimed[py * width + px] = true;
                    if (s.behind && f.pixels[py * width + px])
                        continue;
                    set(px, py, nes_colors[sprite_palettes[s.palette][v]]);
                }
            }
        }
    }
    return im;
}
void paint_line(Project &p, std::size_t frame, int x0, int y0, int x1, int y1, Byte c, int size,
                int palette) {
    if (c > 3 || size < 1 || size > 32 || palette < -1 || palette > 3)
        throw std::runtime_error("Invalid brush");
    auto &f = p.frames.at(frame);
    if (f.paint_mask.empty())
        f.paint_mask.assign(f.pixels.size(), 255);
    x0 = std::clamp(x0, 0, p.width - 1);
    x1 = std::clamp(x1, 0, p.width - 1);
    y0 = std::clamp(y0, 0, p.height - 1);
    y1 = std::clamp(y1, 0, p.height - 1);
    int dx = std::abs(x1 - x0), dy = -std::abs(y1 - y0), sx = x0 < x1 ? 1 : -1,
        sy = y0 < y1 ? 1 : -1, err = dx + dy;
    for (;;) {
        for (int yy = y0 - size / 2; yy < y0 - size / 2 + size; ++yy)
            for (int xx = x0 - size / 2; xx < x0 - size / 2 + size; ++xx)
                if (xx >= 0 && yy >= 0 && xx < p.width && yy < p.height) {
                    p.write_background_pixel(frame, yy * p.width + xx, c, palette);
                }
        if (x0 == x1 && y0 == y1)
            break;
        int e = 2 * err;
        if (e >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e <= dx) {
            err += dx;
            y0 += sy;
        }
    }
    p.compose_layers(frame);
}
void flood_fill(Project &p, std::size_t frame, int x, int y, Byte c, int palette, bool erase) {
    p.compose_layers(frame);
    if (x < 0 || y < 0 || x >= p.width || y >= p.height || c > 3 || palette < -1 || palette > 3)
        return;
    auto &f = p.frames.at(frame);
    const auto source = p.active_pixels(frame);
    const Byte value = erase && !f.layers.empty() ? Byte(255) : c;
    Byte old = source[y * p.width + x],
         old_palette = f.attributes[(y / 16) * p.block_width() + x / 16];
    if (old == value && (palette < 0 || palette == old_palette))
        return;
    std::vector<bool> visited(f.pixels.size());
    std::vector<int> changed;
    std::queue<std::pair<int, int>> queue;
    queue.push({x, y});
    visited[y * p.width + x] = true;
    while (!queue.empty()) {
        auto [cx, cy] = queue.front();
        queue.pop();
        changed.push_back(cy * p.width + cx);
        for (auto [nx, ny] : {std::pair{cx - 1, cy}, std::pair{cx + 1, cy}, std::pair{cx, cy - 1},
                              std::pair{cx, cy + 1}}) {
            if (nx < 0 || ny < 0 || nx >= p.width || ny >= p.height)
                continue;
            int index = ny * p.width + nx;
            if (!visited[index] && source[index] == old &&
                f.attributes[(ny / 16) * p.block_width() + nx / 16] == old_palette) {
                visited[index] = true;
                queue.push({nx, ny});
            }
        }
    }
    if (f.paint_mask.empty())
        f.paint_mask.assign(f.pixels.size(), 255);
    for (auto index : changed) {
        p.write_background_pixel(frame, index, value, palette);
    }
    p.compose_layers(frame);
}

static std::string hex(const std::vector<Byte> &data) {
    static constexpr char digits[] = "0123456789abcdef";
    std::string s;
    s.reserve(data.size() * 2);
    for (Byte b : data) {
        s += digits[b >> 4];
        s += digits[b & 15];
    }
    return s;
}
static std::vector<Byte> unhex(const std::string &s, std::size_t expected) {
    if (s.size() != expected * 2)
        throw std::runtime_error("Invalid data length in project");
    std::vector<Byte> out;
    out.reserve(expected);
    auto n = [](char c) -> int {
        if (c >= '0' && c <= '9')
            return c - '0';
        if (c >= 'a' && c <= 'f')
            return c - 'a' + 10;
        if (c >= 'A' && c <= 'F')
            return c - 'A' + 10;
        throw std::runtime_error("Invalid hex data in project");
    };
    for (std::size_t i = 0; i < s.size(); i += 2)
        out.push_back(static_cast<Byte>((n(s[i]) << 4) | n(s[i + 1])));
    return out;
}
static Json image_json(const Image &i) {
    return Json{{"width", i.width}, {"height", i.height}, {"rgba", hex(i.rgba)}};
}
static Image read_image(const Json &j) {
    int w = checked_int(j.at("width"), 0, 4096), h = checked_int(j.at("height"), 0, 4096);
    if (w < 0 || h < 0 || w > 4096 || h > 4096)
        throw std::runtime_error("Invalid embedded image dimensions");
    return {w, h, unhex(j.at("rgba"), static_cast<std::size_t>(w) * h * 4)};
}
void save_project(const std::filesystem::path &path, const Project &input) {
    input.validate();
    auto snapshot = input;
    for (std::size_t i = 0; i < snapshot.frames.size(); ++i)
        snapshot.compose_layers(i);
    const auto &p = snapshot;
    p.validate();
    Json j{{"format", "tilezeitjobb"},
           {"version", 5},
           {"width", p.width},
           {"height", p.height},
           {"offset_x", p.offset_x},
           {"offset_y", p.offset_y},
           {"dither", p.dither},
           {"image_path", p.image_path},
           {"sprite_path", p.sprite_path},
           {"background", p.background},
           {"sprite_palettes", p.sprite_palettes},
           {"reference", image_json(p.reference)},
           {"sprite_sheet", image_json(p.sprite_sheet)},
           {"frames", Json::array()}};
    j["brushes"] = Json::array();
    for (const auto &b : p.brushes)
        j["brushes"].push_back({{"name", b.name},
                                {"width", b.width},
                                {"height", b.height},
                                {"pixels", hex(b.pixels)}});
    j["ramps"] = Json::array();
    for (const auto &r : p.ramps)
        j["ramps"].push_back({{"name", r.name}, {"colors", r.colors}, {"order", r.order}});
    j["font"] = {{"name", p.font.name}, {"glyphs", Json::array()}};
    for (const auto &g : p.font.glyphs)
        j["font"]["glyphs"].push_back(hex(std::vector<Byte>(g.begin(), g.end())));
    j["tiles"] = Json::array();
    for (const auto &tile : p.tiles)
        j["tiles"].push_back(
            {{"name", tile.name},
             {"pixels", hex(std::vector<Byte>(tile.pixels.begin(), tile.pixels.end()))}});
    j["metatiles"] = Json::array();
    for (const auto &meta : p.metatiles)
        j["metatiles"].push_back(
            {{"name", meta.name}, {"tiles", meta.tiles}, {"palette", meta.palette}});
    for (const auto &f : p.frames) {
        Json sprites = Json::array();
        for (const auto &s : f.sprites)
            sprites.push_back({{"name", s.name},
                               {"x", s.x},
                               {"y", s.y},
                               {"palette", s.palette},
                               {"tile_x", s.tile_x},
                               {"tile_y", s.tile_y},
                               {"visible", s.visible},
                               {"flip_x", s.flip_x},
                               {"flip_y", s.flip_y},
                               {"behind", s.behind},
                               {"locked", s.locked},
                               {"group", s.group},
                               {"pixels", s.pixels}});
        Json layers = Json::array();
        for (const auto &l : f.layers) {
            const auto &m = l.modifiers;
            layers.push_back({{"name", l.name},
                              {"kind", int(l.kind)},
                              {"visible", l.visible},
                              {"locked", l.locked},
                              {"effects_enabled", l.effects_enabled},
                              {"reference_only", l.reference_only},
                              {"guide_opacity", l.guide_opacity},
                              {"pixels", hex(l.pixels)},
                              {"tile_refs", l.tile_refs},
                              {"source", image_json(l.source)},
                              {"source_path", l.source_path},
                              {"modifiers",
                               {{"transform_enabled", m.transform_enabled},
                                {"color_enabled", m.color_enabled},
                                {"dither_enabled", m.dither_enabled},
                                {"crop_x", m.crop_x},
                                {"crop_y", m.crop_y},
                                {"crop_width", m.crop_width},
                                {"crop_height", m.crop_height},
                                {"output_width", m.output_width},
                                {"output_height", m.output_height},
                                {"offset_x", m.offset_x},
                                {"offset_y", m.offset_y},
                                {"brightness", m.brightness},
                                {"contrast", m.contrast},
                                {"strength", m.strength},
                                {"dither", int(m.dither)}}}});
        }
        j["frames"].push_back({{"pixels", hex(f.pixels)},
                               {"paint_mask", hex(f.paint_mask)},
                               {"attributes", f.attributes},
                               {"duration_ms", f.duration_ms},
                               {"active_layer", f.active_layer},
                               {"background_visible", f.background_visible},
                               {"background_locked", f.background_locked},
                               {"sprites_visible", f.sprites_visible},
                               {"sprites_locked", f.sprites_locked},
                               {"layers", layers},
                               {"sprites", sprites}});
        auto &groups = j["frames"].back()["groups"];
        groups = Json::array();
        for (const auto &g : f.groups)
            groups.push_back({{"id", g.id},
                              {"name", g.name},
                              {"origin_x", g.origin_x},
                              {"origin_y", g.origin_y},
                              {"visible", g.visible},
                              {"locked", g.locked}});
    }
    auto serialized = j.dump(2);
    if (serialized.size() > 192 * 1024 * 1024)
        throw std::runtime_error("Project exceeds 192 MiB; reduce embedded images or frame count");
    auto tmp = path;
    tmp += "." + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
           ".tzj.tmp";
    try {
        std::ofstream out(tmp, std::ios::binary);
        if (!out)
            throw std::runtime_error("Cannot write project");
        out << serialized;
        out.close();
        if (!out)
            throw std::runtime_error("Project write failed");
#ifdef _WIN32
        if (!MoveFileExW(tmp.c_str(), path.c_str(),
                         MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            throw std::runtime_error("Cannot replace project file");
#else
        std::filesystem::rename(tmp, path);
#endif
    } catch (...) {
        std::error_code ec;
        std::filesystem::remove(tmp, ec);
        throw;
    }
}
static Project read_legacy(const std::string &text, const std::filesystem::path &path) {
    std::map<std::string, std::string> values;
    std::istringstream input(text);
    std::string line;
    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        auto pos = line.find('=');
        if (pos == std::string::npos || line[0] == '#' || line[0] == ';')
            continue;
        auto v = line.substr(pos + 1);
        if (v.size() >= 2 && v.front() == '"' && v.back() == '"') {
            std::istringstream s(v);
            s >> std::quoted(v);
        }
        values[line.substr(0, pos)] = v;
    }
    auto number = [&](const std::string &key, int fallback = 0) {
        auto it = values.find(key);
        return it == values.end() ? fallback : std::stoi(it->second);
    };
    auto resolve = [&](std::string value) {
        for (std::size_t pos = 0; (pos = value.find("\\\\", pos)) != std::string::npos;)
            value.replace(pos, 2, "/");
        std::filesystem::path p = path_from_utf8(value);
        return p.is_absolute() ? p : path.parent_path() / p;
    };
    Project p;
    p.offset_x = number("offset_x");
    p.offset_y = number("offset_y");
    p.image_path = values["image"];
    p.sprite_path = values["sprite_image"];
    if (!p.image_path.empty()) {
        p.reference = load_image(resolve(p.image_path));
        if (p.reference.width > 256)
            p.resize(512, 240);
        else if (p.reference.height > 240)
            p.resize(256, 480);
    }
    if (!p.sprite_path.empty())
        p.sprite_sheet = load_image(resolve(p.sprite_path));
    auto read_pal = [&](const std::string &prefix, std::array<Palette, 4> &out) {
        for (int i = 0; i < std::min(4, number(prefix + "_pal_count")); ++i)
            for (int c = 0; c < 4; ++c) {
                auto v = static_cast<std::uint32_t>(std::stoull(
                    values.at(prefix + "_pal_" + std::to_string(i) + "_" + std::to_string(c))));
                out[i][c] = nearest_nes({Byte(v >> 16), Byte(v >> 8), Byte(v), 255});
            }
    };
    read_pal("nametable", p.background);
    read_pal("sprite", p.sprite_palettes);
    p.set_backdrop(p.background[0][0]);
    std::istringstream attrs(values["nametable"]);
    std::string val;
    std::size_t i = 0;
    while (std::getline(attrs, val, ';') && i < p.frames[0].attributes.size()) {
        int a = std::stoi(val);
        if (a < 0 || a > 3)
            throw std::runtime_error("Legacy attribute outside 0..3");
        p.frames[0].attributes[i++] = static_cast<Byte>(a);
    }
    if (!p.reference.empty())
        p.quantize();
    int layers = number("layer_count");
    if (layers < 0 || layers > 4096)
        throw std::runtime_error("Invalid legacy layer count");
    for (int n = 0; n < layers; ++n) {
        auto key = "layer_" + std::to_string(n);
        if (number(key + "_type") == 2) {
            auto s = p.make_sprite(number(key + "_tile_x"), number(key + "_tile_y"),
                                   number(key + "_pal"), number(key + "_screen_x"),
                                   number(key + "_screen_y"));
            s.name = values[key + "_name"];
            p.frames[0].sprites.push_back(std::move(s));
        }
    }
    p.validate();
    return p;
}
Project load_project(const std::filesystem::path &path) {
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in)
        throw std::runtime_error("Cannot open project: " + path.string());
    auto size = in.tellg();
    if (size <= 0 || size > 192 * 1024 * 1024)
        throw std::runtime_error("Invalid project size (maximum 192 MiB)");
    std::string text(static_cast<std::size_t>(size), '\0');
    in.seekg(0);
    in.read(text.data(), size);
    if (text.starts_with("\xEF\xBB\xBF"))
        text.erase(0, 3);
    auto start = text.find_first_not_of(" \t\r\n");
    if (start == std::string::npos)
        throw std::runtime_error("Empty project");
    if (text[start] != '{')
        return read_legacy(text, path);
    const auto j = Json::parse(text);
    if (j.at("format") != "tilezeitjobb" || (j.at("version") != 2 && j.at("version") != 3 &&
                                             j.at("version") != 4 && j.at("version") != 5))
        throw std::runtime_error("Unsupported project format/version");
    Project p;
    p.resize(checked_int(j.at("width"), 256, 512), checked_int(j.at("height"), 240, 480));
    p.offset_x = checked_int(j.at("offset_x"), -4096, 4096);
    p.offset_y = checked_int(j.at("offset_y"), -4096, 4096);
    p.dither = j.at("dither");
    p.image_path = j.value("image_path", "");
    p.sprite_path = j.value("sprite_path", "");
    palettes_json(j.at("background"));
    palettes_json(j.at("sprite_palettes"));
    p.background = j.at("background").get<decltype(p.background)>();
    p.sprite_palettes = j.at("sprite_palettes").get<decltype(p.sprite_palettes)>();
    p.reference = read_image(j.at("reference"));
    p.sprite_sheet = read_image(j.at("sprite_sheet"));
    if (j.contains("brushes")) {
        if (!j["brushes"].is_array() || j["brushes"].size() > 128)
            throw std::runtime_error("Invalid brush library");
        std::size_t total = 0;
        for (const auto &jb : j["brushes"]) {
            SavedBrush b;
            b.name = jb.at("name");
            b.width = checked_int(jb.at("width"), 1, 512);
            b.height = checked_int(jb.at("height"), 1, 512);
            total += std::size_t(b.width) * b.height;
            if (total > 16 * 1024 * 1024)
                throw std::runtime_error("Brush library exceeds 16 MiB");
            b.pixels = unhex(jb.at("pixels"), std::size_t(b.width) * b.height);
            p.brushes.push_back(std::move(b));
        }
    }
    if (j.contains("ramps")) {
        if (!j["ramps"].is_array() || j["ramps"].size() > 64)
            throw std::runtime_error("Invalid saved ramps");
        for (const auto &jr : j["ramps"]) {
            ColorRamp ramp;
            ramp.name = jr.at("name");
            byte_array(jr.at("colors"), 4, 63);
            byte_array(jr.at("order"), 4, 3);
            ramp.colors = jr.at("colors").get<Palette>();
            ramp.order = jr.at("order").get<std::array<Byte, 4>>();
            p.ramps.push_back(std::move(ramp));
        }
    }
    if (j.contains("font")) {
        const auto &font = j["font"];
        p.font.name = font.at("name");
        if (!font.at("glyphs").is_array() || font["glyphs"].size() != 96)
            throw std::runtime_error("Invalid bitmap font");
        for (std::size_t i = 0; i < 96; ++i) {
            auto bytes = unhex(font["glyphs"][i], 64);
            std::copy(bytes.begin(), bytes.end(), p.font.glyphs[i].begin());
        }
    }
    p.frames.clear();
    if (j.contains("tiles")) {
        if (!j["tiles"].is_array() || j["tiles"].size() > 4096)
            throw std::runtime_error("Invalid tile library");
        for (const auto &tile : j["tiles"]) {
            TileAsset asset;
            asset.name = tile.at("name");
            auto pixels = unhex(tile.at("pixels"), 64);
            std::copy(pixels.begin(), pixels.end(), asset.pixels.begin());
            p.tiles.push_back(std::move(asset));
        }
    }
    if (j.contains("metatiles")) {
        if (!j["metatiles"].is_array() || j["metatiles"].size() > 4096)
            throw std::runtime_error("Invalid metatile library");
        for (const auto &meta : j["metatiles"]) {
            Metatile asset;
            asset.name = meta.at("name");
            asset.palette = checked_int(meta.at("palette"), 0, 3);
            if (!meta.at("tiles").is_array() || meta["tiles"].size() != 4)
                throw std::runtime_error("A metatile requires four tile references");
            for (int i = 0; i < 4; ++i)
                asset.tiles[i] = checked_int(meta["tiles"][i], -1, int(p.tiles.size()) - 1);
            p.metatiles.push_back(std::move(asset));
        }
    }
    if (!j.at("frames").is_array() || j.at("frames").empty() || j.at("frames").size() > 256)
        throw std::runtime_error("Invalid frame count");
    for (const auto &jf : j.at("frames")) {
        Frame f;
        f.pixels = unhex(jf.at("pixels"), static_cast<std::size_t>(p.width) * p.height);
        auto mask = jf.value("paint_mask", std::string{});
        if (!mask.empty())
            f.paint_mask = unhex(mask, f.pixels.size());
        byte_array(jf.at("attributes"),
                   static_cast<std::size_t>(p.block_width()) * p.block_height(), 3);
        f.attributes = jf.at("attributes").get<std::vector<Byte>>();
        f.duration_ms = checked_int(jf.at("duration_ms"), 10, 60000);
        if (jf.contains("groups")) {
            if (!jf["groups"].is_array() || jf["groups"].size() > 128)
                throw std::runtime_error("Too many metasprites");
            for (const auto &group : jf["groups"]) {
                Metasprite g;
                g.id = checked_int(group.at("id"), 0, 1000000);
                g.name = group.at("name");
                g.origin_x = checked_int(group.at("origin_x"), -4096, 4096);
                g.origin_y = checked_int(group.at("origin_y"), -4096, 4096);
                g.visible = group.value("visible", true);
                g.locked = group.value("locked", false);
                f.groups.push_back(std::move(g));
            }
        }
        if (!jf.at("sprites").is_array() || jf.at("sprites").size() > 4096)
            throw std::runtime_error("Too many sprites");
        for (const auto &js : jf.at("sprites")) {
            Sprite s;
            s.name = js.at("name");
            s.x = checked_int(js.at("x"), -4096, 4096);
            s.y = checked_int(js.at("y"), -4096, 4096);
            s.palette = checked_int(js.at("palette"), 0, 3);
            s.tile_x = checked_int(js.at("tile_x"), 0, 511);
            s.tile_y = checked_int(js.at("tile_y"), 0, 511);
            s.visible = js.at("visible");
            s.flip_x = js.at("flip_x");
            s.flip_y = js.at("flip_y");
            s.behind = js.at("behind");
            s.locked = js.value("locked", false);
            s.group = checked_int(js.value("group", Json(-1)), -1, 1000000);
            byte_array(js.at("pixels"), 64, 3);
            s.pixels = js.at("pixels").get<Tile>();
            f.sprites.push_back(std::move(s));
        }
        if (jf.contains("layers")) {
            const auto &layers = jf.at("layers");
            if (!layers.is_array() || layers.size() > 64)
                throw std::runtime_error("Too many background layers");
            for (const auto &jl : layers) {
                BackgroundLayer l;
                l.name = jl.at("name");
                l.kind = static_cast<LayerKind>(checked_int(jl.at("kind"), 0, 2));
                l.visible = jl.at("visible");
                l.locked = jl.at("locked");
                l.effects_enabled = jl.value("effects_enabled", true);
                l.reference_only = jl.value("reference_only", false);
                l.guide_opacity = checked_int(jl.value("guide_opacity", Json(50)), 0, 100);
                if (l.kind == LayerKind::Paint || l.kind == LayerKind::Tilemap)
                    l.pixels = unhex(jl.at("pixels"), f.pixels.size());
                if (l.kind == LayerKind::Tilemap) {
                    if (!jl.at("tile_refs").is_array() ||
                        jl["tile_refs"].size() != std::size_t(p.width / 8) * (p.height / 8))
                        throw std::runtime_error("Invalid tile map references");
                    for (const auto &id : jl["tile_refs"])
                        l.tile_refs.push_back(checked_int(id, -1, int(p.tiles.size()) - 1));
                }
                l.source = read_image(jl.at("source"));
                l.source_path = jl.value("source_path", "");
                const auto &jm = jl.at("modifiers");
                auto &m = l.modifiers;
                m.transform_enabled = jm.at("transform_enabled");
                m.color_enabled = jm.at("color_enabled");
                m.dither_enabled = jm.at("dither_enabled");
                m.crop_x = checked_int(jm.at("crop_x"), 0, 4096);
                m.crop_y = checked_int(jm.at("crop_y"), 0, 4096);
                m.crop_width = checked_int(jm.at("crop_width"), 0, 4096);
                m.crop_height = checked_int(jm.at("crop_height"), 0, 4096);
                m.output_width = checked_int(jm.at("output_width"), 0, 4096);
                m.output_height = checked_int(jm.at("output_height"), 0, 4096);
                m.offset_x = checked_int(jm.at("offset_x"), -4096, 4096);
                m.offset_y = checked_int(jm.at("offset_y"), -4096, 4096);
                m.brightness = checked_int(jm.at("brightness"), -100, 100);
                m.contrast = checked_int(jm.at("contrast"), -100, 100);
                m.strength = checked_int(jm.at("strength"), 0, 100);
                m.dither = static_cast<DitherMode>(checked_int(jm.at("dither"), 0, 5));
                f.layers.push_back(std::move(l));
            }
            f.active_layer = checked_int(jf.at("active_layer"), -1, 63);
            f.background_visible = jf.value("background_visible", true);
            f.background_locked = jf.value("background_locked", false);
            f.sprites_visible = jf.value("sprites_visible", true);
            f.sprites_locked = jf.value("sprites_locked", false);
        }
        p.frames.push_back(std::move(f));
    }
    p.validate();
    for (std::size_t i = 0; i < p.frames.size(); ++i)
        p.compose_layers(i);
    p.validate();
    return p;
}
Project demo_project() {
    Project p;
    p.background = {{{15, 1, 33, 48}, {15, 1, 17, 48}, {15, 2, 18, 34}, {15, 12, 28, 48}}};
    p.sprite_palettes = {{{15, 17, 44, 48}, {15, 6, 22, 39}, {15, 11, 27, 48}, {15, 5, 21, 48}}};
    auto &f = p.frames[0];
    auto pixel = [&](int x, int y, int c, int palette = -1) {
        if (x < 0 || y < 0 || x >= 256 || y >= 240)
            return;
        f.pixels[y * 256 + x] = Byte(c);
        if (palette >= 0)
            f.attributes[(y / 16) * 16 + x / 16] = Byte(palette);
    };
    auto rect = [&](int x, int y, int w, int h, int c, int palette = -1) {
        for (int py = y; py < y + h; ++py)
            for (int px = x; px < x + w; ++px)
                pixel(px, py, c, palette);
    };
    // Repeated star tiles keep the scene comfortably inside one CHR pattern bank.
    for (int ty = 0; ty < 30; ++ty)
        for (int tx = 0; tx < 32; ++tx) {
            unsigned seed = unsigned(tx * 37 + ty * 71);
            if (seed % 7 == 0)
                pixel(tx * 8 + 3, ty * 8 + 4, seed % 3 == 0 ? 3 : 2);
            if (seed % 41 == 0) {
                pixel(tx * 8 + 2, ty * 8 + 4, 1);
                pixel(tx * 8 + 4, ty * 8 + 4, 1);
                pixel(tx * 8 + 3, ty * 8 + 3, 1);
                pixel(tx * 8 + 3, ty * 8 + 5, 1);
            }
        }
    rect(0, 24, 256, 64, 0);
    rect(8, 96, 80, 64, 0);
    rect(80, 112, 144, 48, 0);
    rect(64, 176, 128, 48, 0);
    // A ringed blue planet, with hard pixel shading and sparse ordered transitions.
    for (int y = 96; y < 160; ++y)
        for (int x = 16; x < 80; ++x) {
            int dx = x - 46, dy = y - 128;
            if (dx * dx + dy * dy < 26 * 26) {
                int tone = dx + dy / 2 < -5 ? 3 : dx + dy / 2 < 13 ? 2 : 1;
                if ((x + y) % 4 == 0 && tone > 1)
                    --tone;
                pixel(x, y, tone, 2);
            }
            int ring = std::abs((x - 46) * (x - 46) +
                                8 * (y - 132 + (x - 46) / 3) * (y - 132 + (x - 46) / 3) - 32 * 32);
            if (ring < 110 && (y > 128 || dx * dx + dy * dy >= 26 * 26))
                pixel(x, y, 3, 2);
        }
    // A 5x7 pixel alphabet, reused by the title and small interface lettering.
    const std::pair<char, std::array<unsigned, 7>> font[]{
        {'A', {14, 17, 17, 31, 17, 17, 17}}, {'B', {30, 17, 17, 30, 17, 17, 30}},
        {'C', {15, 16, 16, 16, 16, 16, 15}}, {'D', {30, 17, 17, 17, 17, 17, 30}},
        {'E', {31, 16, 16, 30, 16, 16, 31}}, {'F', {31, 16, 16, 30, 16, 16, 16}},
        {'G', {15, 16, 16, 23, 17, 17, 15}}, {'H', {17, 17, 17, 31, 17, 17, 17}},
        {'I', {31, 4, 4, 4, 4, 4, 31}},      {'J', {7, 2, 2, 2, 18, 18, 12}},
        {'K', {17, 18, 20, 24, 20, 18, 17}}, {'L', {16, 16, 16, 16, 16, 16, 31}},
        {'M', {17, 27, 21, 21, 17, 17, 17}}, {'N', {17, 25, 21, 19, 17, 17, 17}},
        {'O', {14, 17, 17, 17, 17, 17, 14}}, {'P', {30, 17, 17, 30, 16, 16, 16}},
        {'Q', {14, 17, 17, 17, 21, 18, 13}}, {'R', {30, 17, 17, 30, 20, 18, 17}},
        {'S', {15, 16, 16, 14, 1, 1, 30}},   {'T', {31, 4, 4, 4, 4, 4, 4}},
        {'U', {17, 17, 17, 17, 17, 17, 14}}, {'V', {17, 17, 17, 17, 17, 10, 4}},
        {'W', {17, 17, 17, 21, 21, 21, 10}}, {'X', {17, 17, 10, 4, 10, 17, 17}},
        {'Y', {17, 17, 10, 4, 4, 4, 4}},     {'Z', {31, 1, 2, 4, 8, 16, 31}},
        {'1', {4, 12, 4, 4, 4, 4, 14}},      {'2', {14, 17, 1, 2, 4, 8, 31}}};
    auto text = [&](std::string_view word, int x, int y, int scale, int c, bool italic = false) {
        for (char ch : word) {
            for (const auto &[letter, rows] : font)
                if (letter == ch)
                    for (int row = 0; row < 7; ++row)
                        for (int col = 0; col < 5; ++col)
                            if (rows[row] & (1u << (4 - col)))
                                rect(x + col * scale + (italic ? (6 - row) * scale / 3 : 0),
                                     y + row * scale, scale, scale,
                                     c < 0 ? (row < 2   ? 3
                                              : row < 5 ? 2
                                                        : 3)
                                           : c);
            x += 6 * scale;
        }
    };
    text("STARFALL", 15, 33, 4, 1, true);
    text("STARFALL", 13, 29, 4, -1, true);
    rect(20, 62, 216, 1, 2);
    rect(32, 64, 192, 1, 1);
    text("NOVA SQUADRON", 89, 74, 1, 2);
    // The main ship is background artwork; bright cockpit, hull and exhaust colors
    // are independent sprite overlays that can be moved and inspected in the editor.
    const std::array<std::pair<int, int>, 16> hull{{{88, 136},
                                                    {102, 128},
                                                    {119, 128},
                                                    {133, 117},
                                                    {157, 117},
                                                    {174, 127},
                                                    {196, 131},
                                                    {211, 136},
                                                    {195, 141},
                                                    {168, 141},
                                                    {151, 152},
                                                    {130, 152},
                                                    {124, 143},
                                                    {100, 143},
                                                    {96, 140},
                                                    {88, 140}}};
    for (int y = 112; y < 160; ++y)
        for (int x = 80; x < 224; ++x) {
            bool inside = false;
            for (std::size_t i = 0, j = hull.size() - 1; i < hull.size(); j = i++) {
                const auto [ax, ay] = hull[i];
                const auto [bx, by] = hull[j];
                if ((ay > y) != (by > y) && x < (bx - ax) * double(y - ay) / (by - ay) + ax)
                    inside = !inside;
            }
            if (inside) {
                int shade = y < 126 ? 3 : y < 135 ? 2 : 1;
                if (y == 135 || y == 140)
                    shade = 3;
                pixel(x, y, shade, 1);
            }
        }
    rect(135, 119, 20, 2, 1, 1);
    rect(137, 121, 21, 3, 2, 1);
    rect(106, 131, 20, 2, 1, 1);
    rect(109, 133, 15, 1, 3, 1);
    for (int x = 130; x < 163; x += 4)
        rect(x, 142, 2, 5, 2, 1);
    rect(167, 132, 24, 2, 3, 1);
    rect(177, 137, 21, 1, 2, 1);
    rect(95, 134, 7, 7, 1, 1);
    rect(96, 136, 6, 2, 3, 1);
    // Lower title-screen furniture stays in a separate, teal background color set.
    for (int y = 176; y < 224; ++y)
        for (int x = 64; x < 192; ++x)
            f.attributes[(y / 16) * 16 + x / 16] = 3;
    rect(72, 185, 112, 1, 2);
    rect(72, 207, 112, 1, 1);
    text("PRESS START", 95, 192, 1, 3);
    text("1 PLAYER", 78, 214, 1, 2);
    text("2 PLAYERS", 137, 214, 1, 1);
    auto sprite = [&](std::string name, int x, int y, int palette,
                      const std::array<std::string_view, 8> &rows) {
        Sprite s;
        s.name = std::move(name);
        s.x = x;
        s.y = y;
        s.palette = palette;
        s.tile_x = int(f.sprites.size());
        for (int py = 0; py < 8; ++py)
            for (int px = 0; px < 8; ++px)
                s.pixels[py * 8 + px] = Byte(rows[py][px] - '0');
        f.sprites.push_back(std::move(s));
    };
    sprite("Cockpit cyan glow", 140, 121, 0,
           {"00000000", "00022222", "00233333", "02333322", "22222111", "00000000", "00000000",
            "00000000"});
    sprite("Cockpit glass", 148, 121, 0,
           {"00000000", "22200000", "33322000", "22233220", "11122222", "00000000", "00000000",
            "00000000"});
    sprite("Hull rim light", 164, 128, 0,
           {"00000000", "00000000", "22222222", "33333333", "11111111", "00000000", "00000000",
            "00000000"});
    sprite("Hull rim extension", 172, 128, 0,
           {"00000000", "00000000", "22222222", "33333333", "11111111", "00000000", "00000000",
            "00000000"});
    sprite("Exhaust outer flame", 64, 133, 1,
           {"00000000", "00000000", "00000001", "00001112", "00112223", "00001112", "00000001",
            "00000000"});
    sprite("Exhaust middle flame", 72, 133, 1,
           {"00000000", "00001111", "11112222", "22223333", "33333333", "22223333", "11112222",
            "00001111"});
    sprite("Exhaust hot core", 80, 133, 1,
           {"00000000", "11111111", "22222222", "33333333", "33333333", "33333333", "22222222",
            "11111111"});
    sprite("Forward laser", 216, 132, 0,
           {"00000000", "00000000", "00000000", "22222222", "33333333", "22222222", "00000000",
            "00000000"});
    sprite("Laser point", 224, 132, 0,
           {"00000000", "00000000", "00000000", "22200000", "33333000", "22200000", "00000000",
            "00000000"});
    sprite("Navigation beacon", 194, 129, 1,
           {"00000000", "00000000", "00020000", "00232000", "00020000", "00000000", "00000000",
            "00000000"});
    // A smaller green fighter shows a character-sized 16x16 metasprite (four objects).
    const std::array<std::string_view, 16> fighter{
        {"0000000000000000", "0000000220000000", "0000002332000000", "0000002332000000",
         "0000022332200000", "0000222332220000", "0002222332222000", "0022222332222200",
         "0222222332222220", "2221222332221222", "0001222332221000", "0001222332221000",
         "0000222332220000", "0000011331100000", "0000011001100000", "0000000000000000"}};
    for (int ty = 0; ty < 2; ++ty)
        for (int tx = 0; tx < 2; ++tx) {
            std::array<std::string_view, 8> rows;
            for (int y = 0; y < 8; ++y)
                rows[y] = fighter[ty * 8 + y].substr(tx * 8, 8);
            sprite("Wingman " + std::to_string(ty * 2 + tx + 1), 37 + tx * 8, 175 + ty * 8, 2,
                   rows);
        }
    f.groups.push_back({0, 37, 175, "Wingman"});
    for (auto &s : f.sprites)
        if (s.name.starts_with("Wingman "))
            s.group = 0;
    p.sprite_sheet = {int(f.sprites.size()) * 8, 8,
                      std::vector<Byte>(f.sprites.size() * 8 * 8 * 4)};
    for (std::size_t i = 0; i < f.sprites.size(); ++i)
        for (int y = 0; y < 8; ++y)
            for (int x = 0; x < 8; ++x) {
                auto slot = f.sprites[i].pixels[y * 8 + x];
                auto c = nes_colors[p.sprite_palettes[f.sprites[i].palette][slot]];
                auto out = (y * p.sprite_sheet.width + int(i) * 8 + x) * 4;
                p.sprite_sheet.rgba[out] = c.r;
                p.sprite_sheet.rgba[out + 1] = c.g;
                p.sprite_sheet.rgba[out + 2] = c.b;
                p.sprite_sheet.rgba[out + 3] = slot ? 255 : 0;
            }
    auto save_brush = [&](std::string name, IndexedBrush brush) {
        for (auto &pixel : brush.pixels)
            if (!pixel)
                pixel = 255;
        p.brushes.push_back({std::move(name), brush.width, brush.height, std::move(brush.pixels)});
    };
    save_brush("Star tile", capture_brush(p, 0, 0, 0, 8, 8));
    save_brush("Planet", capture_brush(p, 0, 16, 96, 64, 64));
    save_brush("Ship hull", capture_brush(p, 0, 80, 112, 128, 48));
    if (!f.sprites.empty())
        save_brush("Sprite highlight",
                   {8, 8, {f.sprites[0].pixels.begin(), f.sprites[0].pixels.end()}});
    p.ramps = {{"Deep blue", p.background[0], {0, 1, 2, 3}},
               {"Planet blue", p.background[2], {0, 1, 2, 3}}};
    return p;
}

void History::checkpoint(const Project &p) {
    if (!past_.empty() && past_.back() == p)
        return;
    past_.push_back(p);
    future_.clear();
    std::size_t bytes = 0;
    for (auto it = past_.rbegin(); it != past_.rend(); ++it) {
        bytes += it->reference.rgba.size() + it->sprite_sheet.rgba.size();
        bytes += sizeof(BitmapFont) + it->ramps.size() * sizeof(ColorRamp);
        bytes += it->tiles.size() * sizeof(TileAsset) + it->metatiles.size() * sizeof(Metatile);
        for (const auto &brush : it->brushes)
            bytes += brush.pixels.size() + brush.name.size();
        for (const auto &f : it->frames)
            bytes += f.pixels.size() + f.paint_mask.size() + f.attributes.size() +
                     f.sprites.size() * sizeof(Sprite) + f.groups.size() * sizeof(Metasprite);
        for (const auto &f : it->frames)
            for (const auto &l : f.layers)
                bytes += l.pixels.size() + l.converted.size() + l.source.rgba.size() +
                         l.tile_refs.size() * sizeof(int);
    }
    while (past_.size() > 1 && (past_.size() > 32 || bytes > 64 * 1024 * 1024)) {
        const auto &old = past_.front();
        bytes -= old.reference.rgba.size() + old.sprite_sheet.rgba.size();
        bytes -= sizeof(BitmapFont) + old.ramps.size() * sizeof(ColorRamp);
        bytes -= old.tiles.size() * sizeof(TileAsset) + old.metatiles.size() * sizeof(Metatile);
        for (const auto &brush : old.brushes)
            bytes -= brush.pixels.size() + brush.name.size();
        for (const auto &f : old.frames)
            bytes -= f.pixels.size() + f.paint_mask.size() + f.attributes.size() +
                     f.sprites.size() * sizeof(Sprite) + f.groups.size() * sizeof(Metasprite);
        for (const auto &f : old.frames)
            for (const auto &l : f.layers)
                bytes -= l.pixels.size() + l.converted.size() + l.source.rgba.size() +
                         l.tile_refs.size() * sizeof(int);
        past_.erase(past_.begin());
    }
}
bool History::undo(Project &p) {
    if (past_.empty())
        return false;
    future_.push_back(std::move(p));
    p = std::move(past_.back());
    past_.pop_back();
    return true;
}
bool History::redo(Project &p) {
    if (future_.empty())
        return false;
    past_.push_back(std::move(p));
    p = std::move(future_.back());
    future_.pop_back();
    return true;
}
} // namespace tzj
