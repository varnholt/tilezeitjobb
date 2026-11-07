#include "core/nes.hpp"
#include "core/tools.hpp"
#include <filesystem>
#include <iostream>
#include <stdexcept>
using namespace tzj;
void check(bool v, const char *text) {
    if (!v)
        throw std::runtime_error(text);
}
template <class F> void rejects(F call, const char *text) {
    bool failed = false;
    try {
        call();
    } catch (const std::exception &) {
        failed = true;
    }
    check(failed, text);
}
int main() {
    try {
        Project p;
        p.ensure_layers(0);
        check(p.frames[0].layers.size() == 1 && p.frames[0].pixels[0] == 0,
              "Migration of blank frame");
        paint_pixels(p, 0, {{0, 0}}, 1, 0);
        auto base = p;
        auto upper = p.add_paint_layer(0, "Corrections");
        paint_pixels(p, 0, {{0, 0}, {1, 0}}, 2, 0);
        check(p.frames[0].layers[0].pixels[0] == 1 && p.frames[0].pixels[0] == 2,
              "Paint changed underlying layer");
        paint_pixels(p, 0, {{0, 0}}, 0, -1, -1, true);
        check(p.frames[0].layers[upper].pixels[0] == 255 && p.frames[0].pixels[0] == 1,
              "Erase did not reveal layer below");
        p.frames[0].layers[upper].visible = false;
        auto hidden = p.composed_frame(0);
        check(hidden.pixels[1] == 0, "Hide layer did not affect composition");
        auto hidden_export = compile_scene(p);
        p.frames[0].layers[upper].visible = true;
        check(compile_scene(p).chr != hidden_export.chr, "Visibility did not affect NES export");
        p.compose_layers(0);
        p.frames[0].layers[upper].locked = true;
        auto locked = p;
        rejects([&] { paint_pixels(p, 0, {{0, 0}}, 3); }, "Paint accepted locked layer");
        check(p == locked, "Locked paint modified document");
        p.frames[0].layers[upper].locked = false;
        auto copied = p.duplicate_layer(0, upper);
        paint_pixels(p, 0, {{2, 0}}, 3);
        check(p.frames[0].layers[upper].pixels[2] == 255 &&
                  p.frames[0].layers[copied].pixels[2] == 3,
              "Duplicate was not independent");
        auto merged_before = p.render();
        p.merge_layer_down(0, copied);
        check(p.render() == merged_before, "Merge changed visible picture");
        History history;
        history.checkpoint(p);
        p.remove_layer(0, upper);
        check(history.undo(p) && p.render() == merged_before, "Layer operation undo");
        Image image{32, 16, std::vector<Byte>(32 * 16 * 4)};
        for (int y = 0; y < 16; ++y)
            for (int x = 0; x < 32; ++x) {
                auto i = (y * 32 + x) * 4;
                image.rgba[i] = Byte(x * 8);
                image.rgba[i + 1] = Byte(x * 8);
                image.rgba[i + 2] = Byte(x * 8);
                image.rgba[i + 3] = x == 0 ? 0 : 255;
            }
        auto imported = p.add_image_layer(0, image, "Original", "example.png");
        for (auto mode : {DitherMode::None, DitherMode::Bayer2, DitherMode::Bayer4,
                          DitherMode::Bayer8, DitherMode::FloydSteinberg, DitherMode::Atkinson}) {
            p.frames[0].layers[imported].modifiers.dither = mode;
            p.compose_layers(0);
            for (auto v : p.frames[0].layers[imported].converted)
                check(v <= 3 || v == 255, "Conversion produced invalid slot");
            check(p.frames[0].layers[imported].converted[0] == 255 &&
                      p.frames[0].layers[imported].converted[33] == 255,
                  "Source transparency/bounds lost");
        }
        auto source_before = p.frames[0].layers[imported].source;
        auto processed = p.frames[0].layers[imported].converted;
        auto baked = p.duplicate_layer(0, imported, true);
        check(p.frames[0].layers[baked].kind == LayerKind::Paint &&
                  p.frames[0].layers[baked].pixels == processed,
              "Paint copy differs from processed image");
        paint_pixels(p, 0, {{1, 0}}, 3);
        check(p.frames[0].layers[imported].source == source_before &&
                  p.frames[0].layers[imported].converted == processed,
              "Painting baked copy changed source");
        auto variant = p.duplicate_layer(0, imported, false);
        p.frames[0].layers[variant].modifiers.brightness = 100;
        p.compose_layers(0);
        check(p.frames[0].layers[imported].converted == processed &&
                  p.frames[0].layers[variant].converted != processed,
              "Modifier variation changed original");
        p.frames[0].layers[variant].modifiers.offset_x = 48;
        p.frames[0].layers[variant].modifiers.output_width = 16;
        p.frames[0].layers[variant].modifiers.crop_x = 8;
        p.frames[0].layers[variant].modifiers.crop_width = 16;
        p.compose_layers(0);
        check(p.frames[0].layers[variant].converted[48] <= 3 &&
                  p.frames[0].layers[variant].converted[64] == 255,
              "Crop/resize/position");
        p.frames[0].layers[variant].visible = false;
        p.compose_layers(0);
        Sprite s;
        s.pixels[0] = 3;
        s.x = 24;
        s.y = 24;
        p.frames[0].sprites.push_back(s);
        check(compile_scene(p).sprite_count == 1, "Visible sprite missing");
        p.frames[0].sprites_visible = false;
        check(compile_scene(p).sprite_count == 0, "Hidden sprite group was exported");
        p.frames[0].background_visible = false;
        p.compose_layers(0);
        for (auto c : p.frames[0].pixels)
            check(c == 0, "Hidden background group was composited");
        auto path = std::filesystem::temp_directory_path() / "tilezeitjobb-layers-test.tzj";
        save_project(path, p);
        auto loaded = load_project(path);
        check(p == loaded, "Version 3 layer round trip");
        check(p.render() == loaded.render() && compile_scene(p).chr == compile_scene(loaded).chr,
              "Reload changed render/export");
        std::filesystem::remove(path);
        p.move_layer(0, variant, 0);
        check(p.frames[0].layers[0].name.find("copy") != std::string::npos, "Layer order move");
        p.resize(512, 240);
        p.validate();
        check(p.frames[0].layers[0].source == source_before, "Canvas resize changed source");
        Project legacy;
        legacy.reference = image;
        legacy.quantize();
        auto exact = legacy.render();
        legacy.ensure_layers(0);
        check(legacy.render() == exact, "Migration changed legacy artwork");
        std::cout << "Layer stack, conversions, isolation, migration, persistence and NES export "
                     "passed\n";
    } catch (const std::exception &e) {
        std::cerr << e.what() << "\n";
        return 1;
    }
}
