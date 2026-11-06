#pragma once
#include "core/project.hpp"
namespace tzj {
struct NesScene {
    std::array<Byte, 8192> chr{};
    std::vector<Byte> nametables;
    std::array<Byte, 32> palette{};
    std::array<Byte, 256> oam{};
    int background_tiles{}, sprite_tiles{}, sprite_count{};
    bool vertical_mirroring{};
};
std::array<Byte, 16> encode_tile(const Tile &);
std::vector<std::string> hardware_errors(const Project &, std::size_t frame = 0);
NesScene compile_scene(const Project &, std::size_t frame = 0);
std::vector<Byte> build_rom(const NesScene &);
void write_binary(const std::filesystem::path &, const std::vector<Byte> &);
void export_scene(const std::filesystem::path &prefix, const Project &, std::size_t frame = 0);
} // namespace tzj
