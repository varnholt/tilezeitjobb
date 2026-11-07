#include "core/tools.hpp"
#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>
namespace tzj {
BitmapFont default_bitmap_font() {
    BitmapFont result;
    for (auto &glyph : result.glyphs)
        glyph.fill(255);
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
        {'0', {14, 17, 19, 21, 25, 17, 14}}, {'1', {4, 12, 4, 4, 4, 4, 14}},
        {'2', {14, 17, 1, 2, 4, 8, 31}},     {'3', {30, 1, 1, 14, 1, 1, 30}},
        {'4', {2, 6, 10, 18, 31, 2, 2}},     {'5', {31, 16, 16, 30, 1, 1, 30}},
        {'6', {14, 16, 16, 30, 17, 17, 14}}, {'7', {31, 1, 2, 4, 8, 8, 8}},
        {'8', {14, 17, 17, 14, 17, 17, 14}}, {'9', {14, 17, 17, 15, 1, 1, 14}},
        {'.', {0, 0, 0, 0, 0, 6, 6}},        {',', {0, 0, 0, 0, 6, 6, 4}},
        {':', {0, 6, 6, 0, 6, 6, 0}},        {'!', {4, 4, 4, 4, 4, 0, 4}},
        {'?', {14, 17, 1, 2, 4, 0, 4}},      {'-', {0, 0, 0, 31, 0, 0, 0}},
        {'+', {0, 4, 4, 31, 4, 4, 0}},       {'/', {1, 2, 2, 4, 8, 8, 16}},
        {'(', {2, 4, 8, 8, 8, 4, 2}},        {')', {8, 4, 2, 2, 2, 4, 8}},
        {'_', {0, 0, 0, 0, 0, 0, 31}},       {'=', {0, 0, 31, 0, 31, 0, 0}},
        {'*', {0, 21, 14, 31, 14, 21, 0}},   {'%', {17, 2, 4, 8, 16, 17, 0}}};
    for (const auto &[ch, rows] : font)
        for (int y = 0; y < 7; ++y)
            for (int x = 0; x < 5; ++x)
                if (rows[y] & (1u << (4 - x)))
                    result.glyphs[unsigned(ch) - 32][y * 8 + x + 1] = 1;
    for (int ch = 'a'; ch <= 'z'; ++ch)
        result.glyphs[ch - 32] = result.glyphs[ch - 'a' + 'A' - 32];
    for (int ch = 33; ch < 127; ++ch)
        if (std::all_of(result.glyphs[ch - 32].begin(), result.glyphs[ch - 32].end(),
                        [](Byte p) { return p == 255; }))
            result.glyphs[ch - 32] = result.glyphs['?' - 32];
    return result;
}
IndexedBrush text_brush(const BitmapFont &font, std::string_view text, Byte color, int spacing,
                        int line_gap, int scale, bool recolor) {
    if (text.empty() || text.size() > 1024 || color > 3 || spacing < 0 || spacing > 16 ||
        line_gap < 0 || line_gap > 32 || scale < 1 || scale > 4)
        throw std::runtime_error("Invalid bitmap text settings");
    int columns = 0, longest = 0, lines = 1;
    for (unsigned char c : text) {
        if (c == '\n') {
            longest = std::max(longest, columns);
            columns = 0;
            ++lines;
        } else {
            if (c < 32 || c > 126)
                throw std::runtime_error("Bitmap text supports printable ASCII and newlines");
            ++columns;
        }
    }
    longest = std::max(longest, columns);
    int width = std::max(1, longest * (8 + spacing) - spacing) * scale;
    int height = (lines * (8 + line_gap) - line_gap) * scale;
    if (width > 512 || height > 512)
        throw std::runtime_error("Text brush exceeds 512 x 512 pixels");
    IndexedBrush brush{width, height, std::vector<Byte>(std::size_t(width) * height, 255)};
    int ox = 0, oy = 0;
    for (unsigned char c : text) {
        if (c == '\n') {
            ox = 0;
            oy += (8 + line_gap) * scale;
            continue;
        }
        const auto &glyph = font.glyphs[c - 32];
        for (int y = 0; y < 8; ++y)
            for (int x = 0; x < 8; ++x) {
                Byte value = glyph[y * 8 + x];
                if (value == 255)
                    continue;
                if (value > 3)
                    throw std::runtime_error("Invalid bitmap font color slot");
                for (int sy = 0; sy < scale; ++sy)
                    for (int sx = 0; sx < scale; ++sx)
                        brush.pixels[(oy + y * scale + sy) * width + ox + x * scale + sx] =
                            recolor ? color : value;
            }
        ox += (8 + spacing) * scale;
    }
    return brush;
}
BitmapFont font_from_sheet(const Image &sheet, const Palette &palette, bool opaque_background) {
    if (sheet.empty() || sheet.width % 8 || sheet.height % 8 ||
        (sheet.width / 8) * (sheet.height / 8) < 96)
        throw std::runtime_error(
            "Font sheet requires at least 96 row-major 8x8 cells, starting at ASCII space");
    for (auto color : palette)
        if (color > 63)
            throw std::runtime_error("Invalid font palette");
    BitmapFont font;
    font.name = "Imported 8x8";
    const auto background = sheet.at(0, 0);
    if (opaque_background)
        for (std::size_t i = 3; i < sheet.rgba.size(); i += 4)
            if (sheet.rgba[i] < 255) {
                opaque_background = false;
                break;
            }
    for (int i = 0; i < 96; ++i)
        for (int y = 0; y < 8; ++y)
            for (int x = 0; x < 8; ++x) {
                auto c = sheet.at((i % (sheet.width / 8)) * 8 + x, (i / (sheet.width / 8)) * 8 + y);
                bool transparent = c.a < 128 || (opaque_background && c.r == background.r &&
                                                 c.g == background.g && c.b == background.b);
                font.glyphs[i][y * 8 + x] =
                    transparent ? 255 : nearest_slot(c, palette, x, y, false);
            }
    return font;
}
IndexedBrush gradient_brush(int width, int height, const std::array<Byte, 4> &order, bool vertical,
                            GradientDither dither) {
    if (width < 1 || height < 1 || width > 512 || height > 512 || int(dither) < 0 ||
        int(dither) > 2)
        throw std::runtime_error("Invalid gradient dimensions or dithering");
    for (auto slot : order)
        if (slot > 3)
            throw std::runtime_error("Gradient slots must be 0..3");
    constexpr int bayer[]{0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5};
    IndexedBrush brush{width, height, std::vector<Byte>(std::size_t(width) * height)};
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x) {
            double t = 3. * (vertical ? y : x) / std::max(1, (vertical ? height : width) - 1);
            int low = std::min(3, int(t)), high = std::min(3, low + 1);
            double threshold = .5;
            if (dither == GradientDither::Ordered)
                threshold = (bayer[(y & 3) * 4 + (x & 3)] + .5) / 16.;
            if (dither == GradientDither::Noise) {
                std::uint32_t seed =
                    std::uint32_t(x) * 374761393u + std::uint32_t(y) * 668265263u + 0x9e3779b9u;
                seed = (seed ^ (seed >> 13)) * 1274126177u;
                seed ^= seed >> 16;
                threshold = (seed & 0xffff) / 65536.;
            }
            brush.pixels[y * width + x] = order[t - low > threshold ? high : low];
        }
    return brush;
}
std::vector<PixelPoint> symmetric_pixels(const std::vector<PixelPoint> &points, int width,
                                         int height, float cx, float cy, bool mx, bool my,
                                         int sectors) {
    if (width < 1 || height < 1 || width > 512 || height > 512 || sectors < 1 || sectors > 16 ||
        !std::isfinite(cx) || !std::isfinite(cy) || std::abs(cx) > 4096 || std::abs(cy) > 4096)
        throw std::runtime_error("Invalid symmetry settings");
    std::vector<Byte> mask(std::size_t(width) * height);
    for (auto [x, y] : points) {
        if (x < 0 || y < 0 || x >= width || y >= height)
            continue;
        for (int fx = 0; fx <= (mx ? 1 : 0); ++fx)
            for (int fy = 0; fy <= (my ? 1 : 0); ++fy)
                for (int n = 0; n < sectors; ++n) {
                    double dx = (fx ? -1 : 1) * (x - cx), dy = (fy ? -1 : 1) * (y - cy),
                           angle = 2 * std::numbers::pi * n / sectors;
                    int px = int(std::lround(cx + dx * std::cos(angle) - dy * std::sin(angle)));
                    int py = int(std::lround(cy + dx * std::sin(angle) + dy * std::cos(angle)));
                    if (px >= 0 && py >= 0 && px < width && py < height)
                        mask[py * width + px] = 1;
                }
    }
    std::vector<PixelPoint> result;
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x)
            if (mask[y * width + x])
                result.push_back({x, y});
    return result;
}
Tile wrapped_tile_line(const Tile &tile, int x0, int y0, int x1, int y1, Byte color) {
    if (color > 3)
        throw std::runtime_error("Invalid tile color");
    Tile result = tile;
    for (auto [x, y] : drawing_pixels(DrawingShape::Line, x0, y0, x1, y1, 24, 24))
        result[(y % 8) * 8 + x % 8] = color;
    return result;
}
Image onion_overlay(const Project &p, std::size_t frame, int previous, int following, int opacity,
                    bool sprites_only) {
    if (frame >= p.frames.size() || previous < 0 || previous > 8 || following < 0 ||
        following > 8 || opacity < 0 || opacity > 100)
        throw std::runtime_error("Invalid onion skin settings");
    Image out{p.width, p.height, std::vector<Byte>(std::size_t(p.width) * p.height * 4)};
    auto current = p.render(frame);
    for (int distance = std::max(previous, following); distance >= 1; --distance)
        for (int direction : {-1, 1}) {
            if (distance > (direction < 0 ? previous : following))
                continue;
            int target = int(frame) + direction * distance;
            if (target < 0 || target >= int(p.frames.size()))
                continue;
            auto image = p.render(std::size_t(target));
            std::vector<Byte> coverage(std::size_t(p.width) * p.height, sprites_only ? 0 : 1);
            const auto &f = p.frames[target];
            if (sprites_only && f.sprites_visible)
                for (const auto &s : f.sprites)
                    if (s.visible)
                        for (int sy = 0; sy < 8; ++sy)
                            for (int sx = 0; sx < 8; ++sx) {
                                int x = s.x + sx, y = s.y + sy;
                                if (x < 0 || y < 0 || x >= p.width || y >= p.height)
                                    continue;
                                if (s.pixels[(s.flip_y ? 7 - sy : sy) * 8 +
                                             (s.flip_x ? 7 - sx : sx)])
                                    coverage[y * p.width + x] = 1;
                            }
            const Color tint = direction < 0 ? Color{70, 220, 150} : Color{255, 95, 125};
            for (int y = 0; y < p.height; ++y)
                for (int x = 0; x < p.width; ++x) {
                    auto i = std::size_t(y * p.width + x);
                    auto c = image.at(x, y);
                    if (!coverage[i] || c == current.at(x, y) ||
                        (!sprites_only && c == nes_colors[p.background[0][0]]))
                        continue;
                    out.rgba[i * 4] = tint.r;
                    out.rgba[i * 4 + 1] = tint.g;
                    out.rgba[i * 4 + 2] = tint.b;
                    out.rgba[i * 4 + 3] = Byte(opacity * 255 / (100 * distance));
                }
        }
    return out;
}
} // namespace tzj
