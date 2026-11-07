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
void check(bool v, const char *message) {
    if (!v)
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
        auto font = default_bitmap_font();
        auto text = text_brush(font, "PLAYER 1234567890!?", 2);
        check(text.width == 19 * 8 && text.height == 8, "Bitmap text dimensions incorrect");
        check(std::count(text.pixels.begin(), text.pixels.end(), 2) > 100,
              "Built-in glyphs missing");
        check(std::all_of(text.pixels.begin(), text.pixels.end(),
                          [](Byte v) { return v == 2 || v == 255; }),
              "Text transparency/recolor failed");
        auto two_lines = text_brush(font, "AB\nC", 3, 1, 2, 2);
        check(two_lines.width == 34 && two_lines.height == 36,
              "Text spacing/scale/newlines failed");
        font.glyphs['A' - 32].fill(255);
        font.glyphs['A' - 32][0] = 3;
        auto custom = text_brush(font, "A", 1, 0, 0, 1, false);
        check(custom.pixels[0] == 3 && custom.pixels[1] == 255,
              "Custom colored bitmap font lost its pixels");
        rejects([&] { text_brush(font, "\xc3\xa6", 1); }, "Unsupported text accepted");
        rejects([&] { text_brush(font, std::string(100, 'A'), 1); }, "Oversized text accepted");
        Image sheet{128, 48, std::vector<Byte>(128 * 48 * 4)};
        for (std::size_t i = 3; i < sheet.rgba.size(); i += 4)
            sheet.rgba[i] = 255;
        // ASCII A is glyph 33: cell (1,2) on a sixteen-column sheet.
        auto at = std::size_t((16 * 128 + 8) * 4);
        sheet.rgba[at] = 255;
        sheet.rgba[at + 1] = 255;
        sheet.rgba[at + 2] = 255;
        auto imported = font_from_sheet(sheet, {15, 1, 33, 48});
        check(imported.glyphs['A' - 32][0] == 3 && imported.glyphs[0][0] == 255,
              "Font sheet cell order or background failed");
        Image alpha_sheet{128, 48, std::vector<Byte>(128 * 48 * 4)};
        alpha_sheet.rgba[at + 3] = 255;
        check(font_from_sheet(alpha_sheet, {15, 1, 33, 48}).glyphs['A' - 32][0] == 0,
              "Opaque black ink was removed from an alpha font sheet");
        rejects([&] { font_from_sheet(Image{8, 8, std::vector<Byte>(256)}, {15, 1, 33, 48}); },
                "Undersized font sheet accepted");
        for (auto dither : {GradientDither::None, GradientDither::Ordered, GradientDither::Noise}) {
            auto gradient = gradient_brush(32, 16, {3, 2, 1, 0}, false, dither);
            check(gradient.pixels[0] == 3 && gradient.pixels[31] == 0,
                  "Gradient endpoints incorrect");
            check(std::all_of(gradient.pixels.begin(), gradient.pixels.end(),
                              [](Byte p) { return p <= 3; }),
                  "Gradient left NES slots");
            check(gradient == gradient_brush(32, 16, {3, 2, 1, 0}, false, dither),
                  "Gradient is not deterministic");
        }
        rejects([&] { gradient_brush(0, 16, {0, 1, 2, 3}, true, GradientDither::None); },
                "Invalid gradient accepted");
        auto radial = symmetric_pixels({{5, 3}}, 9, 9, 3, 3, false, false, 4);
        check(radial.size() == 4 &&
                  std::find(radial.begin(), radial.end(), PixelPoint{3, 5}) != radial.end(),
              "Adjustable radial symmetry failed");
        auto reflected = symmetric_pixels({{1, 2}}, 8, 8, 2, 2, true, false);
        check(reflected.size() == 2 && std::find(reflected.begin(), reflected.end(),
                                                 PixelPoint{3, 2}) != reflected.end(),
              "Custom mirror center failed");
        rejects([&] { symmetric_pixels({}, 8, 8, 0, 0, false, false, 99); },
                "Invalid radial count accepted");
        auto tile = wrapped_tile_line(Tile{}, 7, 5, 9, 5, 3);
        check(tile[5 * 8 + 7] == 3 && tile[5 * 8] == 3 && tile[5 * 8 + 1] == 3,
              "Seamless tile stroke did not wrap");
        Project p;
        p.ensure_layers(0);
        IndexedBrush group{16, 16, std::vector<Byte>(256, 2)};
        p.brushes.push_back({"Platform", 16, 16, group.pixels});
        p.ramps.push_back({"Blue ramp", {15, 1, 33, 48}, {0, 2, 1, 3}});
        p.font = font;
        stamp_brush(p, 0, 16, 16, group);
        check(p.frames[0].pixels[16 * 256 + 16] == 2 && p.frames[0].pixels[31 * 256 + 31] == 2,
              "Multi-tile group stamping failed");
        History history;
        history.checkpoint(p);
        p.brushes.push_back({"Letter", 8, 8, custom.pixels});
        check(history.undo(p) && p.brushes.size() == 1, "Saved brush change did not undo");
        const auto before = compile_scene(p);
        auto guide = p.add_image_layer(0, Image{1, 1, {255, 0, 0, 255}}, "Reference");
        p.frames[0].layers[guide].reference_only = true;
        p.frames[0].layers[guide].guide_opacity = 50;
        p.compose_layers(0);
        check(compile_scene(p).chr == before.chr &&
                  compile_scene(p).nametables == before.nametables,
              "Reference guide leaked into NES export");
        check(p.render().at(0, 0) == nes_colors[15], "Reference guide leaked into artwork export");
        check(p.render(0, true, false, true).at(0, 0).r == 127,
              "Reference opacity composition incorrect");
        auto upper = p.add_paint_layer(0, "Corrections");
        p.write_background_pixel(0, 0, 1);
        p.compose_layers(0);
        check(p.render(0, true, false, true).at(0, 0) == nes_colors[p.background[0][1]],
              "Paint above guide did not cover it");
        auto baked = p.duplicate_layer(0, guide, true);
        check(!p.frames[0].layers[baked].reference_only &&
                  p.frames[0].layers[baked].kind == LayerKind::Paint,
              "Baked guide remained non-exported");
        p.frames[0].layers[baked].visible = false;
        std::filesystem::create_directories("test-output");
        auto path = std::filesystem::path("test-output/artist.tzj");
        save_project(path, p);
        check(load_project(path) == p,
              "Saved brushes, ramps, font or guide metadata did not roundtrip");
        nlohmann::json json;
        {
            std::ifstream in(path);
            in >> json;
        }
        json["version"] = 3;
        json.erase("brushes");
        json.erase("ramps");
        json.erase("font");
        for (auto &l : json["frames"][0]["layers"]) {
            l.erase("reference_only");
            l.erase("guide_opacity");
        }
        auto old = std::filesystem::path("test-output/artist-v3.tzj");
        {
            std::ofstream out(old);
            out << json;
        }
        auto migrated = load_project(old);
        check(migrated.brushes.empty() && migrated.font == default_bitmap_font(),
              "Version 3 did not supply default assets");
        Project animated;
        Sprite sprite;
        sprite.x = 0;
        sprite.y = 8;
        sprite.pixels.fill(1);
        animated.frames[0].sprites.push_back(sprite);
        for (int i = 1; i < 4; ++i) {
            auto f = animated.frames[0];
            f.sprites[0].x = i * 16;
            animated.frames.push_back(f);
        }
        auto onion = onion_overlay(animated, 1, 1, 1, 40, true);
        check(onion.at(0, 8).g > onion.at(0, 8).r && onion.at(32, 8).r > onion.at(32, 8).g,
              "Previous/following onion tints incorrect");
        check(onion.at(16, 8).a == 0, "Current sprite ghosted onto itself");
        auto falloff = onion_overlay(animated, 3, 3, 0, 60, true);
        check(falloff.at(32, 8).a > falloff.at(16, 8).a && falloff.at(16, 8).a > falloff.at(0, 8).a,
              "Onion opacity does not fall off by distance");
        std::cout << "Artist tools: brush groups, fonts, gradients, symmetry, seamless tiles, "
                     "guides, onion skin and project migration passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
