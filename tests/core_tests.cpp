#include "core/nes.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace tzj;
static void check(bool ok, const char *what) {
    if (!ok)
        throw std::runtime_error(what);
}
template <class F> static void rejects(F action, const char *what) {
    bool caught = false;
    try {
        action();
    } catch (const std::exception &) {
        caught = true;
    }
    check(caught, what);
}
int main() {
    try {
        Tile tile{};
        tile[0] = 1;
        tile[1] = 2;
        tile[2] = 3;
        auto bytes = encode_tile(tile);
        check(bytes[0] == 0xa0 && bytes[8] == 0x60, "CHR bitplane order");
        tile[0] = 4;
        rejects([&] { encode_tile(tile); }, "Invalid tile accepted");
        Project p;
        auto &f = p.frames[0];
        f.attributes[0] = 0;
        f.attributes[1] = 1;
        f.attributes[16] = 2;
        f.attributes[17] = 3;
        f.attributes[14 * 16] = 3;
        auto scene = compile_scene(p);
        check(scene.nametables.size() == 1024 && scene.nametables[960] == 0xe4,
              "Attribute quadrant packing");
        check(scene.nametables[1016] == 3, "Bottom attribute row");
        check(scene.background_tiles == 1, "Duplicate tiles not deduplicated");
        p.frames[0].pixels[0] = 3;
        scene = compile_scene(p);
        check(scene.background_tiles == 2 && scene.chr[0] == 128 && scene.chr[8] == 128,
              "CHR from pixels");
        Sprite s;
        s.x = 24;
        s.y = 30;
        s.palette = 2;
        s.flip_x = true;
        s.behind = true;
        s.pixels[0] = 3;
        p.frames[0].sprites.push_back(s);
        scene = compile_scene(p);
        check(scene.oam[0] == 29 && scene.oam[2] == 98 && scene.oam[3] == 24, "OAM encoding");
        check(scene.chr[4096] == 128 && scene.chr[4104] == 128, "Sprite pattern bank");
        auto rom = build_rom(scene);
        check(rom.size() == 24592 && rom[0] == 'N' && rom[3] == 26 && rom[4] == 1 && rom[5] == 1,
              "iNES structure");
        check(rom[16 + 0x3ffc] == 0 && rom[16 + 0x3ffd] == 128, "Reset vector");
        check(std::equal(scene.chr.begin(), scene.chr.end(), rom.end() - 8192), "ROM CHR contents");
        p.resize(512, 240);
        p.frames[0].attributes[16] = 2;
        scene = compile_scene(p);
        check(scene.vertical_mirroring && scene.nametables.size() == 2048 &&
                  scene.nametables[1024 + 960] == 2,
              "Horizontal screen attributes");
        check(build_rom(scene)[6] == 1, "Vertical mirroring header");
        p.resize(256, 480);
        p.frames[0].attributes[15 * 16] = 3;
        scene = compile_scene(p);
        check(scene.nametables[1024 + 960] == 3 && !scene.vertical_mirroring,
              "Vertical screen boundary");
        auto dir = std::filesystem::current_path() / "test-output";
        std::filesystem::create_directories(dir);
        save_project(dir / "roundtrip.tzj", p);
        auto loaded = load_project(dir / "roundtrip.tzj");
        check(loaded == p, "Project roundtrip");
        save_project(dir / "roundtrip.tzj", p);
        check(load_project(dir / "roundtrip.tzj") == p, "Atomic overwrite");
        save_png(dir / "roundtrip.png", p.render());
        check(load_image(dir / "roundtrip.png") == p.render(), "PNG roundtrip");
        Project q;
        q.reference = load_image(dir / "roundtrip.png");
        q.quantize(0, true);
        q.validate();
        check(q.frames[0].pixels.size() == 61440, "Quantization dimensions");
        paint_line(q, 0, 3, 3, 3, 3, 3, 1);
        q.quantize();
        check(q.frames[0].pixels[3 * q.width + 3] == 3, "Reference conversion lost painted pixels");
        save_project(dir / "painted.tzj", q);
        check(load_project(dir / "painted.tzj") == q, "Paint mask roundtrip");
        History history;
        auto before = q;
        history.checkpoint(q);
        paint_line(q, 0, 0, 0, 40, 40, 3, 2);
        check(history.undo(q) && q == before, "Undo");
        check(history.redo(q) && q != before, "Redo");
        p.frames[0].sprites.assign(9, s);
        rejects([&] { compile_scene(p); }, "Scanline overflow accepted");
        p.frames[0].sprites.assign(65, s);
        rejects([&] { compile_scene(p); }, "OAM overflow accepted");
        p.frames[0].sprites.clear();
        p.background[0][1] = 13;
        rejects([&] { compile_scene(p); }, "Unsafe color accepted");
        p.background[0][1] = 1;
        for (int id = 0; id < 257; ++id) {
            int tx = id % 32, ty = id / 32;
            for (int bit = 0; bit < 9; ++bit)
                p.frames[0].pixels[(ty * 8 + bit / 8) * p.width + tx * 8 + bit % 8] =
                    Byte((id >> bit) & 1);
        }
        rejects([&] { compile_scene(p); }, "CHR overflow accepted");
        Project fill;
        flood_fill(fill, 0, 0, 0, 2);
        check(std::all_of(fill.frames[0].pixels.begin(), fill.frames[0].pixels.end(),
                          [](Byte b) { return b == 2; }),
              "Flood fill");
        std::ofstream legacy(dir / "legacy.tzj");
        legacy << "[General]\nnametable_pal_count=0\nsprite_pal_count=0\nnametable=1;2;3;0\nlayer_"
                  "count=0\n";
        legacy.close();
        auto old = load_project(dir / "legacy.tzj");
        check(old.frames[0].attributes[0] == 1 && old.frames[0].attributes[2] == 3,
              "Legacy INI importer");
        std::ofstream invalid(dir / "invalid.tzj");
        invalid << "{\"format\":\"tilezeitjobb\",\"version\":999}";
        invalid.close();
        rejects([&] { load_project(dir / "invalid.tzj"); }, "Future project version accepted");
        auto read_text = [](const std::filesystem::path &path) {
            std::ifstream in(path, std::ios::binary);
            return std::string(std::istreambuf_iterator<char>(in), {});
        };
        auto mutate_number = [&](const char *field, const char *number) {
            auto text = read_text(dir / "roundtrip.tzj");
            auto pos = text.find(std::string("\"") + field + "\":");
            check(pos != std::string::npos, "Mutation field missing");
            pos = text.find_first_of("0123456789-", pos + std::string(field).size() + 3);
            auto end = text.find_first_not_of("0123456789-", pos);
            text.replace(pos, end - pos, number);
            std::ofstream out(dir / "mutated.tzj");
            out << text;
            out.close();
            rejects([&] { load_project(dir / "mutated.tzj"); },
                    "Out of range JSON value wrapped into a byte or integer");
        };
        mutate_number("background", "271");
        mutate_number("attributes", "256");
        mutate_number("duration_ms", "4294967416");
        Image tiny{8, 8, std::vector<Byte>(8 * 8 * 4, 255)};
        save_png(dir / "legacy-source.png", tiny);
        std::ofstream rich_legacy(dir / "legacy-images.tzj");
        rich_legacy << "[General]\nimage=legacy-source.png\nsprite_image=legacy-source.png\nlayer_"
                       "count=1\nlayer_0_type=2\nlayer_0_name=old "
                       "sprite\nlayer_0_tile_x=0\nlayer_0_tile_y=0\nlayer_0_pal=1\nlayer_0_screen_"
                       "x=12\nlayer_0_screen_y=28\n";
        rich_legacy.close();
        auto converted = load_project(dir / "legacy-images.tzj");
        check(converted.reference == tiny && converted.sprite_sheet == tiny &&
                  converted.frames[0].sprites.size() == 1 && converted.frames[0].sprites[0].x == 12,
              "Legacy image and sprite import");
        save_project(dir / "converted.tzj", converted);
        check(load_project(dir / "converted.tzj") == converted, "Legacy conversion roundtrip");
        auto demo = demo_project();
        export_scene(dir / "demo", demo);
        save_png(dir / "demo.png", demo.render());
        save_project(dir / "demo.tzj", demo);
        Project spr;
        Sprite visible;
        visible.x = 90;
        visible.y = 100;
        for (int y = 0; y < 8; ++y)
            for (int x = 0; x < 8; ++x)
                visible.pixels[y * 8 + x] = Byte((x / 2 + y / 2) % 4);
        visible.flip_x = true;
        visible.flip_y = true;
        visible.behind = true;
        spr.frames[0].sprites.push_back(visible);
        Sprite overlap;
        overlap.x = 92;
        overlap.y = 100;
        overlap.pixels.fill(1);
        spr.frames[0].sprites.push_back(overlap);
        Sprite top;
        top.x = 15;
        top.y = 2;
        top.pixels.fill(2);
        spr.frames[0].sprites.push_back(top);
        paint_line(spr, 0, 93, 103, 97, 103, 2, 2);
        export_scene(dir / "sprite", spr);
        save_png(dir / "sprite.png", spr.render());
        save_project(dir / "sprite.tzj", spr);
        auto horizontal = demo_project();
        horizontal.resize(512, 240);
        horizontal.frames[0].attributes[16] = 2;
        export_scene(dir / "horizontal", horizontal);
        save_project(dir / "horizontal.tzj", horizontal);
        auto vertical = demo_project();
        vertical.resize(256, 480);
        vertical.frames[0].attributes[15 * 16] = 3;
        export_scene(dir / "vertical", vertical);
        save_project(dir / "vertical.tzj", vertical);
        std::cout << "All core tests passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << "\n";
        return 1;
    }
}
