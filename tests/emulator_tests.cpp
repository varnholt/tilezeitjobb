#include "core/nes.hpp"
#include "emu/nes.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <tuple>
using namespace tzj;
namespace {
void check(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
template <class F> void rejects(F action, const char *message) {
    bool rejected = false;
    try {
        action();
    } catch (const std::exception &) {
        rejected = true;
    }
    check(rejected, message);
}
std::vector<Byte> rom_with(std::initializer_list<Byte> program) {
    std::vector<Byte> rom(16 + 16384 + 8192, 0);
    rom[0] = 'N';
    rom[1] = 'E';
    rom[2] = 'S';
    rom[3] = 0x1a;
    rom[4] = 1;
    rom[5] = 1;
    std::fill(rom.begin() + 16, rom.begin() + 16 + 16384, 0xea);
    std::copy(program.begin(), program.end(), rom.begin() + 16);
    for (int offset : {0x3ffa, 0x3ffc, 0x3ffe}) {
        rom[16 + offset] = 0;
        rom[16 + offset + 1] = 0x80;
    }
    return rom;
}
void boot_compare(Project project, const std::string &label) {
    const auto scene = compile_scene(project);
    const auto rom = build_rom(scene);
    auto console = std::make_unique<emu::Nes>();
    console->load(rom);
    for (int i = 0; i < 12; ++i)
        console->run_frame();
    check(console->ppu_control() == 0x88, "ROM did not execute its PPU setup");
    // Every byte uploaded by the cartridge must land in its own PPU address space.
    for (std::size_t i = 0; i < scene.nametables.size(); ++i) {
        int page = int(i / 1024), address = page == 0                  ? 0x2000
                                            : scene.vertical_mirroring ? 0x2400
                                                                       : 0x2800;
        check(console->vram(emu::Word(address + (i % 1024))) == scene.nametables[i],
              "ROM nametable upload mismatch");
    }
    const auto image = project.render(0, true, true);
    const auto &pixels = console->pixels();
    for (int y = 0; y < 240; ++y)
        for (int x = 0; x < 256; ++x) {
            auto expected = image.at(x, y), actual = nes_colors[pixels[y * 256 + x]];
            if (actual != expected)
                throw std::runtime_error(label + ": pixel mismatch at " + std::to_string(x) + "," +
                                         std::to_string(y));
        }
    auto first = pixels;
    for (int i = 0; i < 8; ++i)
        console->run_frame();
    check(first == console->pixels(), "NMI refresh changed a static scene");
    console->reset();
    for (int i = 0; i < 12; ++i)
        console->run_frame();
    check(first == console->pixels(), "Reset did not reproduce the booted ROM");
    std::cout << "PASS " << label << ": ROM boot, 61440 pixels, nametables, NMI and reset\n";
}
} // namespace
int main() {
    try {
        auto console = std::make_unique<emu::Nes>();
        rejects([&] { console->run_frame(); }, "Empty console executed");
        auto invalid = rom_with({0xea});
        invalid[6] = 0x10;
        rejects([&] { console->load(invalid); }, "Unsupported mapper accepted");
        invalid = rom_with({0xea});
        invalid.resize(100);
        rejects([&] { console->load(invalid); }, "Truncated ROM accepted");
        invalid = rom_with({0xea});
        invalid[7] = 8;
        rejects([&] { console->load(invalid); }, "Unsupported NES2 header accepted");
        // Binary arithmetic, overflow, stack flags, decimal-disabled NES behavior and RAM mirrors.
        auto program =
            rom_with({0xa9, 0x7f, 0x18, 0x69, 0x01, 0x85, 0x10, 0x08, 0x68, 0x85, 0x11, 0x38,
                      0xa9, 0,    0xe9, 1,    0x85, 0x12, 0xf8, 0xa9, 9,    0x18, 0x69, 1,
                      0x85, 0x13, 0xa9, 0x55, 0x8d, 0x14, 0x08, 0x4c, 0x1f, 0x80});
        console->load(program);
        for (int i = 0; i < 22; ++i)
            console->step_instruction();
        check(console->ram(0x10) == 0x80 && (console->ram(0x11) & 0xc1) == 0xc0,
              "ADC/overflow/carry or PHP/PLA failed");
        check(console->ram(0x12) == 0xff && console->ram(0x13) == 10,
              "SBC or decimal-disabled arithmetic failed");
        check(console->ram(0x14) == 0x55, "CPU internal RAM mirroring failed");
        program = rom_with({0x20, 0x08, 0x80, 0x85, 0x20, 0x4c, 0x05, 0x80, 0xa9, 0x42, 0x60});
        console->load(program);
        for (int i = 0; i < 4; ++i)
            console->step_instruction();
        check(console->ram(0x20) == 0x42 && console->cpu().sp == 0xfd,
              "JSR/RTS stack behavior failed");
        program = rom_with({0x6c, 0xff, 0x81});
        program[16 + 0x1ff] = 0x34;
        program[16 + 0x100] = 0x82;
        program[16 + 0x200] = 0x90;
        console->load(program);
        console->step_instruction();
        check(console->cpu().pc == 0x8234, "6502 indirect-JMP page wrapping failed");
        program = rom_with({0xea});
        program[16 + 0x3ffc] = 0xfc;
        program[16 + 0xfc] = 0xd0;
        program[16 + 0xfd] = 0x7f;
        console->load(program);
        check(console->step_instruction() == 4 && console->cpu().pc == 0x817d,
              "Branch page-cross timing failed");
        // All 151 documented opcodes must decode; unsupported opcodes stop with a clear error.
        constexpr Byte official[]{
            0x00, 0x01, 0x05, 0x06, 0x08, 0x09, 0x0a, 0x0d, 0x0e, 0x10, 0x11, 0x15, 0x16, 0x18,
            0x19, 0x1d, 0x1e, 0x20, 0x21, 0x24, 0x25, 0x26, 0x28, 0x29, 0x2a, 0x2c, 0x2d, 0x2e,
            0x30, 0x31, 0x35, 0x36, 0x38, 0x39, 0x3d, 0x3e, 0x40, 0x41, 0x45, 0x46, 0x48, 0x49,
            0x4a, 0x4c, 0x4d, 0x4e, 0x50, 0x51, 0x55, 0x56, 0x58, 0x59, 0x5d, 0x5e, 0x60, 0x61,
            0x65, 0x66, 0x68, 0x69, 0x6a, 0x6c, 0x6d, 0x6e, 0x70, 0x71, 0x75, 0x76, 0x78, 0x79,
            0x7d, 0x7e, 0x81, 0x84, 0x85, 0x86, 0x88, 0x8a, 0x8c, 0x8d, 0x8e, 0x90, 0x91, 0x94,
            0x95, 0x96, 0x98, 0x99, 0x9a, 0x9d, 0xa0, 0xa1, 0xa2, 0xa4, 0xa5, 0xa6, 0xa8, 0xa9,
            0xaa, 0xac, 0xad, 0xae, 0xb0, 0xb1, 0xb4, 0xb5, 0xb6, 0xb8, 0xb9, 0xba, 0xbc, 0xbd,
            0xbe, 0xc0, 0xc1, 0xc4, 0xc5, 0xc6, 0xc8, 0xc9, 0xca, 0xcc, 0xcd, 0xce, 0xd0, 0xd1,
            0xd5, 0xd6, 0xd8, 0xd9, 0xdd, 0xde, 0xe0, 0xe1, 0xe4, 0xe5, 0xe6, 0xe8, 0xe9, 0xea,
            0xec, 0xed, 0xee, 0xf0, 0xf1, 0xf5, 0xf6, 0xf8, 0xf9, 0xfd, 0xfe};
        static_assert(std::size(official) == 151);
        for (auto op : official) {
            console->load(rom_with({op, 0, 0}));
            check(console->step_instruction() >= 2, "Invalid CPU cycle count");
        }
        console->load(rom_with({0x02}));
        rejects([&] { console->step_instruction(); }, "Unsupported opcode silently executed");
        // Test PPUDATA's delayed buffer, status latch reset and palette aliases via real code.
        console->load(rom_with({0xa9, 0x20, 0x8d, 6,    0x20, 0xa9, 0,    0x8d, 6,    0x20, 0xa9,
                                0x42, 0x8d, 7,    0x20, 0xa9, 0x20, 0x8d, 6,    0x20, 0xa9, 0,
                                0x8d, 6,    0x20, 0xad, 7,    0x20, 0x85, 0x30, 0xad, 7,    0x20,
                                0x85, 0x31, 0xa9, 0x3f, 0x8d, 6,    0x20, 0xa9, 0x10, 0x8d, 6,
                                0x20, 0xa9, 0x21, 0x8d, 7,    0x20, 0x4c, 0x36, 0x80}));
        for (int i = 0; i < 22; ++i)
            console->step_instruction();
        check(console->ram(0x30) == 0 && console->ram(0x31) == 0x42, "PPUDATA read buffer failed");
        check(console->vram(0x3f00) == 0x21 && console->vram(0x3f10) == 0x21,
              "Palette mirroring failed");
        // NMI must execute cartridge code and return with a balanced interrupt stack.
        program = rom_with({0xa9, 0x80, 0x8d, 0, 0x20, 0x4c, 5, 0x80});
        program[16 + 0x100] = 0xe6;
        program[16 + 0x101] = 0x40;
        program[16 + 0x102] = 0x40;
        program[16 + 0x3ffa] = 0;
        program[16 + 0x3ffb] = 0x81;
        console->load(program);
        for (int i = 0; i < 6; ++i)
            console->run_frame();
        check(console->ram(0x40) >= 4 && console->cpu().sp == 0xfd,
              "NMI/RTI did not execute or restore the stack");
        // Distinguish the two cartridge nametable wiring configurations.
        for (int vertical = 0; vertical < 2; ++vertical) {
            program = rom_with(
                {0xa9, 0x20, 0x8d, 6, 0x20, 0xa9, 0, 0x8d, 6, 0x20, 0xa9, 0x11, 0x8d, 7, 0x20,
                 0xa9, 0x24, 0x8d, 6, 0x20, 0xa9, 0, 0x8d, 6, 0x20, 0xa9, 0x22, 0x8d, 7, 0x20,
                 0xa9, 0x28, 0x8d, 6, 0x20, 0xa9, 0, 0x8d, 6, 0x20, 0xa9, 0x33, 0x8d, 7, 0x20});
            program[6] = Byte(vertical);
            console->load(program);
            for (int i = 0; i < 18; ++i)
                console->step_instruction();
            check(console->vram(0x2000) == (vertical ? 0x33 : 0x22) &&
                      console->vram(0x2400) == 0x22 && console->vram(0x2800) == 0x33,
                  "Nametable wiring/mirroring failed");
        }
        boot_compare(demo_project(), "STARFALL");
        for (const auto &[name, w, h] :
             {std::tuple{"horizontal", 512, 240}, std::tuple{"vertical", 256, 480}}) {
            Project p;
            p.resize(w, h);
            for (int y = 0; y < h; ++y)
                for (int x = 0; x < w; ++x)
                    p.frames[0].pixels[y * w + x] = Byte(((x / 8) + (y / 8)) & 3);
            for (std::size_t i = 0; i < p.frames[0].attributes.size(); ++i)
                p.frames[0].attributes[i] = Byte(i & 3);
            p.frames[0].pixels[h > 240 ? 240 * w : 256] = 3;
            boot_compare(p, name);
        }
        // Independent checked expectations: only the first eight sprites on a scanline draw.
        Project p;
        p.sprite_palettes[0] = {15, 0x16, 0x27, 0x30};
        for (int i = 0; i < 9; ++i) {
            Sprite sprite;
            sprite.x = i * 16;
            sprite.y = 40;
            sprite.pixels.fill(1);
            p.frames[0].sprites.push_back(sprite);
        }
        // Overflow scenes are intentionally invalid exports: construct cartridge data directly.
        auto valid = p;
        valid.frames[0].sprites.pop_back();
        auto scene = compile_scene(valid);
        scene.oam[32] = 39;
        scene.oam[33] = scene.oam[1];
        scene.oam[34] = 0;
        scene.oam[35] = 128;
        console->load(build_rom(scene));
        for (int i = 0; i < 8; ++i)
            console->run_frame();
        check(console->pixels()[40 * 256 + 112] == 0x16 && console->pixels()[40 * 256 + 128] == 15,
              "Eight-sprites-per-scanline limit failed");
        for (int i = 0; i < 30000 && !(console->ppu_status() & 0x20); ++i)
            console->step_instruction();
        check(console->ppu_status() & 0x20, "Sprite overflow flag was never set");
        // Sprite priority, flips and transparency, plus a hidden image/paint composition.
        Project sprites;
        auto &f = sprites.frames[0];
        f.pixels[41 * 256 + 21] = 1;
        Sprite first;
        first.x = 20;
        first.y = 40;
        first.palette = 1;
        first.behind = true;
        first.pixels[9] = 2;
        Sprite second = first;
        second.behind = false;
        second.palette = 2;
        f.sprites = {first, second};
        Sprite flipped;
        flipped.x = 80;
        flipped.y = 80;
        flipped.flip_x = true;
        flipped.flip_y = true;
        flipped.pixels[0] = 3;
        f.sprites.push_back(flipped);
        sprites.ensure_layers(0);
        auto upper = sprites.add_paint_layer(0, "Overlay");
        sprites.frames[0].layers[upper].pixels[10 * 256 + 10] = 3;
        sprites.compose_layers(0);
        boot_compare(sprites, "sprite priority and layers");
        std::cout << "CPU, PPU registers, NROM boot and rendering tests passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
