#include "core/nes.hpp"
#include <algorithm>
#include <map>
namespace tzj {
std::vector<NesIssue> hardware_issues(const Project &p, std::size_t frame) {
    p.validate();
    const auto f = p.composed_frame(frame);
    std::vector<NesIssue> issues;
    auto add = [&](std::string code, std::string message, int sprite, int x, int y, int w, int h,
                   int palette = -1, bool sprite_palette = false) {
        issues.push_back({std::move(code), std::move(message), int(frame), sprite, x, y, w, h,
                          palette, sprite_palette});
    };
    for (int group = 0; group < 2; ++group)
        for (int palette = 0; palette < 4; ++palette) {
            const auto &colors = (group ? p.sprite_palettes : p.background)[palette];
            if (std::find(colors.begin(), colors.end(), 0x0d) == colors.end())
                continue;
            int x = 0, y = 0, sprite = -1;
            if (group) {
                for (std::size_t i = 0; i < f.sprites.size(); ++i)
                    if (p.sprite_visible(frame, f.sprites[i]) && f.sprites[i].palette == palette) {
                        sprite = int(i);
                        x = f.sprites[i].x;
                        y = f.sprites[i].y;
                        break;
                    }
            } else
                for (int by = 0; by < p.block_height(); ++by) {
                    bool found = false;
                    for (int bx = 0; bx < p.block_width(); ++bx)
                        if (f.attributes[by * p.block_width() + bx] == palette) {
                            x = bx * 16;
                            y = by * 16;
                            found = true;
                            break;
                        }
                    if (found)
                        break;
                }
            add("unsafe_color", "Color $0D is unsafe on some real consoles; use $0F for black.",
                sprite, x, y, group ? 8 : 16, group ? 8 : 16, palette, group != 0);
        }
    int count = 0;
    std::array<int, 240> scanlines{};
    std::map<std::array<Byte, 16>, int> sprite_patterns;
    bool sprite_pattern_warning = false;
    for (std::size_t i = 0; i < f.sprites.size(); ++i) {
        const auto &s = f.sprites[i];
        if (!p.sprite_visible(frame, s))
            continue;
        ++count;
        if (s.x < 0 || s.x > 255 || s.y < 1 || s.y > 239)
            add("sprite_position",
                "ROM sprites must be inside the initial viewport: X 0..255, Y 1..239.", int(i), s.x,
                s.y, 8, 8);
        for (int y = std::max(0, s.y); y < std::min(240, s.y + 8); ++y)
            ++scanlines[y];
        sprite_patterns.try_emplace(encode_tile(s.pixels), int(i));
        if (!sprite_pattern_warning && sprite_patterns.size() > 256) {
            add("sprite_patterns",
                "More than 256 unique sprite tiles; reduce the tiles or use a banked mapper.",
                int(i), s.x, s.y, 8, 8);
            sprite_pattern_warning = true;
        }
        if (count == 65)
            add("sprite_count", "NES OAM supports at most 64 visible sprites.", int(i), s.x, s.y, 8,
                8);
    }
    for (int y = 0; y < 240;) {
        if (scanlines[y] <= 8) {
            ++y;
            continue;
        }
        int start = y, maximum = 0;
        while (y < 240 && scanlines[y] > 8)
            maximum = std::max(maximum, scanlines[y++]);
        add("sprite_scanline",
            std::to_string(maximum) + " sprites overlap scanlines " + std::to_string(start) + ".." +
                std::to_string(y - 1) + "; NES displays only eight.",
            -1, 0, start, 256, y - start);
    }
    std::map<std::array<Byte, 16>, int> background_patterns;
    bool warned = false;
    for (int ty = 0; ty < p.height / 8 && !warned; ++ty)
        for (int tx = 0; tx < p.width / 8; ++tx) {
            Tile tile;
            for (int y = 0; y < 8; ++y)
                for (int x = 0; x < 8; ++x)
                    tile[y * 8 + x] = f.pixels[(ty * 8 + y) * p.width + tx * 8 + x];
            background_patterns.try_emplace(encode_tile(tile), int(background_patterns.size()));
            if (background_patterns.size() > 256) {
                add("background_patterns",
                    "More than 256 unique background tiles; reduce the tiles or use a banked "
                    "mapper.",
                    -1, tx * 8, ty * 8, 8, 8);
                warned = true;
                break;
            }
        }
    return issues;
}
} // namespace tzj
