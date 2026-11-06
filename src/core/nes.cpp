#include "core/nes.hpp"
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <map>
#include <sstream>
#include <stdexcept>
namespace tzj {
std::array<Byte, 16> encode_tile(const Tile &tile) {
    std::array<Byte, 16> out{};
    for (int y = 0; y < 8; ++y)
        for (int x = 0; x < 8; ++x) {
            Byte c = tile[y * 8 + x];
            if (c > 3)
                throw std::runtime_error("Tile is not 2bpp");
            out[y] |= (c & 1) << (7 - x);
            out[y + 8] |= ((c >> 1) & 1) << (7 - x);
        }
    return out;
}
std::vector<std::string> hardware_errors(const Project &p, std::size_t frame) {
    p.validate();
    const auto &f = p.frames.at(frame);
    std::vector<std::string> errors;
    for (const auto &group : {p.background, p.sprite_palettes})
        for (const auto &pal : group)
            if (std::find(pal.begin(), pal.end(), 0x0d) != pal.end()) {
                errors.push_back("Color $0D is unsafe on some real consoles; use $0F for black.");
                goto palettes_done;
            }
palettes_done:
    int count = 0;
    std::array<int, 240> lines{};
    bool position_error = false;
    for (const auto &s : f.sprites)
        if (s.visible) {
            ++count;
            if (s.x < 0 || s.x > 255 || s.y < 1 || s.y > 239)
                position_error = true;
            for (int y = std::max(0, s.y); y < std::min(240, s.y + 8); ++y)
                ++lines[y];
        }
    if (count > 64)
        errors.push_back("NES OAM supports at most 64 visible sprites.");
    if (position_error)
        errors.push_back("ROM sprites must be inside the initial viewport: X 0..255, Y 1..239.");
    if (*std::max_element(lines.begin(), lines.end()) > 8)
        errors.push_back("More than 8 sprites overlap a scanline; hardware would drop sprites.");
    return errors;
}
NesScene compile_scene(const Project &p, std::size_t frame) {
    auto errors = hardware_errors(p, frame);
    if (!errors.empty())
        throw std::runtime_error(errors.front());
    const auto &f = p.frames.at(frame);
    NesScene scene;
    scene.oam.fill(255);
    scene.vertical_mirroring = p.width == 512;
    const int pages = (p.width / 256) * (p.height / 240);
    scene.nametables.resize(pages * 1024);
    std::map<std::array<Byte, 16>, int> bgtiles, sptiles;
    auto add = [&](const Tile &tile, bool sprite) {
        auto encoded = encode_tile(tile);
        auto &map = sprite ? sptiles : bgtiles;
        auto it = map.find(encoded);
        if (it != map.end())
            return it->second;
        const int id = static_cast<int>(map.size());
        if (id >= 256)
            throw std::runtime_error(
                sprite
                    ? "More than 256 unique sprite tiles; reduce the tiles or use a banked mapper."
                    : "More than 256 unique background tiles; reduce the tiles or use a banked "
                      "mapper.");
        map[encoded] = id;
        std::copy(encoded.begin(), encoded.end(),
                  scene.chr.begin() + (sprite ? 4096 : 0) + id * 16);
        return id;
    };
    for (int page = 0; page < pages; ++page) {
        int ox = p.width == 512 ? page * 256 : 0, oy = p.height == 480 ? page * 240 : 0;
        for (int ty = 0; ty < 30; ++ty)
            for (int tx = 0; tx < 32; ++tx) {
                Tile tile{};
                for (int y = 0; y < 8; ++y)
                    for (int x = 0; x < 8; ++x)
                        tile[y * 8 + x] = f.pixels[(oy + ty * 8 + y) * p.width + ox + tx * 8 + x];
                scene.nametables[page * 1024 + ty * 32 + tx] = static_cast<Byte>(add(tile, false));
            }
        for (int ay = 0; ay < 8; ++ay)
            for (int ax = 0; ax < 8; ++ax) {
                Byte packed = 0;
                for (int qy = 0; qy < 2; ++qy)
                    for (int qx = 0; qx < 2; ++qx) {
                        int bx = ax * 2 + qx, by = ay * 2 + qy;
                        if (by < 15) {
                            int global_y = oy / 16 + by, global_x = ox / 16 + bx;
                            packed |= f.attributes[global_y * p.block_width() + global_x]
                                      << (qy * 4 + qx * 2);
                        }
                    }
                scene.nametables[page * 1024 + 960 + ay * 8 + ax] = packed;
            }
    }
    int index = 0;
    for (const auto &s : f.sprites)
        if (s.visible) {
            const int tile = add(s.pixels, true);
            scene.oam[index * 4] = static_cast<Byte>(s.y - 1);
            scene.oam[index * 4 + 1] = static_cast<Byte>(tile);
            scene.oam[index * 4 + 2] = static_cast<Byte>(
                s.palette | (s.behind ? 32 : 0) | (s.flip_x ? 64 : 0) | (s.flip_y ? 128 : 0));
            scene.oam[index * 4 + 3] = static_cast<Byte>(s.x);
            ++index;
        }
    for (int i = 0; i < 4; ++i)
        for (int c = 0; c < 4; ++c) {
            scene.palette[i * 4 + c] = p.background[i][c];
            scene.palette[16 + i * 4 + c] = p.sprite_palettes[i][c];
        }
    scene.background_tiles = static_cast<int>(bgtiles.size());
    scene.sprite_tiles = static_cast<int>(sptiles.size());
    scene.sprite_count = index;
    return scene;
}
// Small relocatable 6502 emitter. No external assembler is required to export a ROM.
class Assembler {
    struct Fix {
        std::size_t offset;
        std::string label;
        bool relative;
    };
    std::map<std::string, int> labels;
    std::vector<Fix> fixes;

  public:
    std::vector<Byte> code;
    void emit(std::initializer_list<int> bytes) {
        for (auto b : bytes)
            code.push_back(static_cast<Byte>(b));
    }
    void label(const std::string &name) { labels[name] = 0x8000 + static_cast<int>(code.size()); }
    void branch(int opcode, const std::string &name) {
        emit({opcode, 0});
        fixes.push_back({code.size() - 1, name, true});
    }
    void jump(int opcode, const std::string &name) {
        emit({opcode, 0, 0});
        fixes.push_back({code.size() - 2, name, false});
    }
    void resolve() {
        for (const auto &f : fixes) {
            int address = labels.at(f.label);
            if (f.relative) {
                int delta = address - (0x8000 + static_cast<int>(f.offset) + 1);
                if (delta < -128 || delta > 127)
                    throw std::runtime_error("ROM branch out of range");
                code[f.offset] = static_cast<Byte>(delta);
            } else {
                code[f.offset] = Byte(address);
                code[f.offset + 1] = Byte(address >> 8);
            }
        }
    }
    int address(const std::string &name) const { return labels.at(name); }
};
std::vector<Byte> build_rom(const NesScene &s) {
    if (s.nametables.size() != 1024 && s.nametables.size() != 2048)
        throw std::runtime_error("Invalid ROM nametables");
    constexpr int nt_address = 0x8400, pal_address = 0x8c00, oam_address = 0x8c20;
    Assembler a;
    a.label("reset");
    a.emit({0x78, 0xd8, 0xa2, 0x40, 0x8e, 0x17, 0x40, 0xa2, 0xff, 0x9a, 0xe8, 0x8e,
            0x00, 0x20, 0x8e, 0x01, 0x20, 0x8e, 0x10, 0x40, 0xad, 0x02, 0x20});
    a.label("wait1");
    a.emit({0x2c, 0x02, 0x20});
    a.branch(0x10, "wait1");
    a.emit({0xa9, 0, 0xaa});
    a.label("clear");
    for (int page = 0; page < 8; ++page)
        a.emit({0x9d, 0, page});
    a.emit({0xe8});
    a.branch(0xd0, "clear");
    a.emit({0xa9, 0xff, 0xa2, 0});
    a.label("hide");
    a.emit({0x9d, 0, 2, 0xe8});
    a.branch(0xd0, "hide");
    a.label("wait2");
    a.emit({0x2c, 0x02, 0x20});
    a.branch(0x10, "wait2");
    auto ppu_address = [&](int address) {
        a.emit({0xa9, address >> 8, 0x8d, 6, 0x20, 0xa9, address & 255, 0x8d, 6, 0x20});
    };
    for (int page = 0; page < static_cast<int>(s.nametables.size()) / 1024; ++page) {
        ppu_address(page == 0 ? 0x2000 : (s.vertical_mirroring ? 0x2400 : 0x2800));
        for (int bank = 0; bank < 4; ++bank) {
            int address = nt_address + page * 1024 + bank * 256;
            auto loop = "nt" + std::to_string(page) + std::to_string(bank);
            a.emit({0xa2, 0});
            a.label(loop);
            a.emit({0xbd, address & 255, address >> 8, 0x8d, 7, 0x20, 0xe8});
            a.branch(0xd0, loop);
        }
    }
    ppu_address(0x3f00);
    a.emit({0xa2, 0});
    a.label("pal");
    a.emit({0xbd, pal_address & 255, pal_address >> 8, 0x8d, 7, 0x20, 0xe8, 0xe0, 32});
    a.branch(0xd0, "pal");
    a.emit({0xa2, 0});
    a.label("oam");
    a.emit({0xbd, oam_address & 255, oam_address >> 8, 0x9d, 0, 2, 0xe8});
    a.branch(0xd0, "oam");
    // Wait until the next vblank before enabling rendering and DMA.
    a.label("wait3");
    a.emit({0x2c, 2, 0x20});
    a.branch(0x10, "wait3");
    a.emit({0xa9, 0,    0x8d, 3,    0x20, 0xa9, 2,    0x8d, 0x14, 0x40, 0xa9, 0,    0x8d, 5,
            0x20, 0x8d, 5,    0x20, 0xa9, 0x88, 0x8d, 0,    0x20, 0xa9, 0x1e, 0x8d, 1,    0x20});
    a.label("forever");
    a.jump(0x4c, "forever");
    // NMI refreshes OAM each frame (important for real console OAM decay).
    a.label("nmi");
    a.emit({0x48, 0xa9, 0,    0x8d, 3,    0x20, 0xa9, 2,    0x8d, 0x14, 0x40, 0xad,
            2,    0x20, 0xa9, 0,    0x8d, 5,    0x20, 0x8d, 5,    0x20, 0x68, 0x40});
    a.label("irq");
    a.emit({0x40});
    a.resolve();
    if (a.code.size() > 1024)
        throw std::runtime_error("ROM code overlaps data");
    std::vector<Byte> rom(16 + 16384 + 8192, 0);
    rom[0] = 'N';
    rom[1] = 'E';
    rom[2] = 'S';
    rom[3] = 0x1a;
    rom[4] = 1;
    rom[5] = 1;
    rom[6] = s.vertical_mirroring ? 1 : 0;
    std::fill(rom.begin() + 16, rom.begin() + 16 + 16384, Byte{0xea});
    std::copy(a.code.begin(), a.code.end(), rom.begin() + 16);
    std::copy(s.nametables.begin(), s.nametables.end(), rom.begin() + 16 + nt_address - 0x8000);
    std::copy(s.palette.begin(), s.palette.end(), rom.begin() + 16 + pal_address - 0x8000);
    std::copy(s.oam.begin(), s.oam.end(), rom.begin() + 16 + oam_address - 0x8000);
    int v = 16 + 16384 - 6;
    for (const auto &name : {"nmi", "reset", "irq"}) {
        int address = a.address(name);
        rom[v++] = Byte(address);
        rom[v++] = Byte(address >> 8);
    }
    std::copy(s.chr.begin(), s.chr.end(), rom.begin() + 16 + 16384);
    return rom;
}
void write_binary(const std::filesystem::path &path, const std::vector<Byte> &data) {
    std::ofstream out(path, std::ios::binary);
    if (!out)
        throw std::runtime_error("Cannot write: " + path.string());
    out.write(reinterpret_cast<const char *>(data.data()),
              static_cast<std::streamsize>(data.size()));
    out.close();
    if (!out)
        throw std::runtime_error("Write failed: " + path.string());
}
void export_scene(const std::filesystem::path &prefix, const Project &p, std::size_t frame) {
    auto scene = compile_scene(p, frame);
    auto rom = build_rom(scene);
    auto path = [&](const char *ext) {
        auto result = prefix;
        result += ext;
        return result;
    };
    write_binary(path(".chr"), {scene.chr.begin(), scene.chr.end()});
    write_binary(path(".nam"), scene.nametables);
    write_binary(path(".pal"), {scene.palette.begin(), scene.palette.end()});
    write_binary(path(".oam"), {scene.oam.begin(), scene.oam.end()});
    write_binary(path(".nes"), rom);
    std::ofstream header(path(".h"));
    if (!header)
        throw std::runtime_error("Cannot write C header");
    header << "#pragma once\n/* NES scene data: CHR, nametables including attributes, palette and "
              "OAM. */\n";
    auto array = [&](const char *name, const auto &bytes) {
        header << "static const unsigned char " << name << "[" << std::dec << bytes.size()
               << "] = {\n";
        int i = 0;
        for (Byte b : bytes) {
            if (i++ % 16 == 0)
                header << "  ";
            header << "0x" << std::hex << std::setw(2) << std::setfill('0') << int(b) << ",";
            if (i % 16 == 0)
                header << "\n";
        }
        header << "\n};\n";
    };
    array("tzj_chr", scene.chr);
    array("tzj_nametables", scene.nametables);
    array("tzj_palette", scene.palette);
    array("tzj_oam", scene.oam);
    header.close();
    if (!header)
        throw std::runtime_error("C header write failed");
}
} // namespace tzj
