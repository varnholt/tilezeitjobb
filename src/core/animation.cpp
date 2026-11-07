#include "core/nes.hpp"
#include "core/tools.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <map>
#include <nlohmann/json.hpp>
#include <stdexcept>
namespace tzj {
namespace {
std::filesystem::path asset_path(std::filesystem::path prefix, const char *extension) {
    prefix += extension;
    return prefix;
}
} // namespace
NesAnimation compile_animation(const Project &p) {
    p.validate();
    NesAnimation result;
    result.scene = compile_scene(p, 0);
    std::fill(result.scene.chr.begin() + 4096, result.scene.chr.end(), Byte{0});
    std::map<std::array<Byte, 16>, int> patterns;
    for (std::size_t frame = 0; frame < p.frames.size(); ++frame) {
        auto scene = compile_scene(p, frame);
        if (scene.nametables != result.scene.nametables || scene.palette != result.scene.palette ||
            !std::equal(scene.chr.begin(), scene.chr.begin() + 4096, result.scene.chr.begin()))
            throw std::runtime_error(
                "Animated NROM currently requires a fixed background and shared palettes; frame " +
                std::to_string(frame + 1) + " changes them");
        for (int i = 0; i < scene.sprite_count; ++i) {
            int local = scene.oam[i * 4 + 1];
            std::array<Byte, 16> encoded;
            std::copy_n(scene.chr.begin() + 4096 + local * 16, 16, encoded.begin());
            auto it = patterns.find(encoded);
            if (it == patterns.end()) {
                int id = int(patterns.size());
                if (id >= 256)
                    throw std::runtime_error(
                        "Animation uses more than 256 unique sprite patterns across its frames");
                std::copy(encoded.begin(), encoded.end(),
                          result.scene.chr.begin() + 4096 + id * 16);
                it = patterns.emplace(encoded, id).first;
            }
            scene.oam[i * 4 + 1] = Byte(it->second);
        }
        result.oam.push_back(scene.oam);
        result.ticks.push_back(std::uint16_t(
            std::clamp(std::lround(p.frames[frame].duration_ms * 60.0988 / 1000.), 1L, 65535L)));
    }
    result.scene.oam = result.oam.front();
    result.scene.sprite_tiles = int(patterns.size());
    return result;
}
void export_animation(const std::filesystem::path &prefix, const Project &p) {
    auto animation = compile_animation(p);
    auto rom = build_animation_rom(animation);
    nlohmann::json metadata{{"format", "tilezeitjobb-nes-animation"},
                            {"version", 1},
                            {"fps", 60.0988},
                            {"background", "fixed"},
                            {"frames", nlohmann::json::array()}};
    for (std::size_t i = 0; i < p.frames.size(); ++i)
        metadata["frames"].push_back({{"index", i},
                                      {"duration_ms", p.frames[i].duration_ms},
                                      {"ticks", animation.ticks[i]}});
    // Validate and construct every asset before opening output files.
    auto json = metadata.dump(2);
    std::vector<Byte> data;
    for (const auto &oam : animation.oam)
        data.insert(data.end(), oam.begin(), oam.end());
    write_binary(asset_path(prefix, ".nes"), rom);
    write_binary(asset_path(prefix, ".chr"),
                 {animation.scene.chr.begin(), animation.scene.chr.end()});
    write_binary(asset_path(prefix, ".nam"), animation.scene.nametables);
    write_binary(asset_path(prefix, ".pal"),
                 {animation.scene.palette.begin(), animation.scene.palette.end()});
    write_binary(asset_path(prefix, ".oam"), data);
    write_binary(asset_path(prefix, ".json"), {json.begin(), json.end()});
}
void export_sprite_sheet(const std::filesystem::path &prefix, const Project &p, int columns,
                         bool sprites_only, int group) {
    p.validate();
    if (columns < 1 || columns > 16 || group < -1)
        throw std::runtime_error("Sprite sheet requires 1..16 columns");
    int left = std::numeric_limits<int>::max(), top = left;
    int right = std::numeric_limits<int>::min(), bottom = right;
    bool any = false;
    if (sprites_only) {
        for (std::size_t frame = 0; frame < p.frames.size(); ++frame)
            for (const auto &s : p.frames[frame].sprites)
                if (p.sprite_visible(frame, s) && (group < 0 || s.group == group)) {
                    left = std::min(left, s.x);
                    top = std::min(top, s.y);
                    right = std::max(right, s.x + 8);
                    bottom = std::max(bottom, s.y + 8);
                    any = true;
                }
        if (!any)
            throw std::runtime_error("No visible sprites match the sprite-sheet selection");
    } else {
        left = top = 0;
        right = p.width;
        bottom = p.height;
    }
    int cell_width = right - left, cell_height = bottom - top;
    columns = std::min(columns, int(p.frames.size()));
    int rows = (int(p.frames.size()) + columns - 1) / columns;
    if (cell_width < 1 || cell_height < 1 || cell_width * columns > 4096 ||
        cell_height * rows > 4096 ||
        std::size_t(cell_width) * cell_height * columns * rows > 16 * 1024 * 1024)
        throw std::runtime_error("Sprite sheet exceeds 4096x4096 / 16 megapixels; export a smaller "
                                 "object or fewer frames");
    Image sheet{cell_width * columns, cell_height * rows,
                std::vector<Byte>(std::size_t(cell_width) * columns * cell_height * rows * 4)};
    nlohmann::json json{{"format", "tilezeitjobb-spritesheet"},
                        {"version", 1},
                        {"image", path_to_utf8(prefix.filename()) + ".png"},
                        {"cell_width", cell_width},
                        {"cell_height", cell_height},
                        {"columns", columns},
                        {"loop", true},
                        {"frames", nlohmann::json::array()}};
    for (std::size_t frame = 0; frame < p.frames.size(); ++frame) {
        Image image;
        if (!sprites_only)
            image = p.render(frame);
        std::vector<Byte> claimed(std::size_t(cell_width) * cell_height);
        int ox = int(frame % columns) * cell_width, oy = int(frame / columns) * cell_height;
        auto put = [&](int x, int y, Color color) {
            auto i = std::size_t((oy + y) * sheet.width + ox + x) * 4;
            sheet.rgba[i] = color.r;
            sheet.rgba[i + 1] = color.g;
            sheet.rgba[i + 2] = color.b;
            sheet.rgba[i + 3] = color.a;
        };
        if (sprites_only) {
            for (const auto &s : p.frames[frame].sprites)
                if (p.sprite_visible(frame, s) && (group < 0 || s.group == group))
                    for (int sy = 0; sy < 8; ++sy)
                        for (int sx = 0; sx < 8; ++sx) {
                            int x = s.x - left + sx, y = s.y - top + sy;
                            auto index = std::size_t(y) * cell_width + x;
                            Byte pixel =
                                s.pixels[(s.flip_y ? 7 - sy : sy) * 8 + (s.flip_x ? 7 - sx : sx)];
                            if (!pixel || claimed.at(index))
                                continue;
                            claimed[index] = 1;
                            put(x, y, nes_colors[p.sprite_palettes[s.palette][pixel]]);
                        }
        } else
            for (int y = 0; y < cell_height; ++y)
                for (int x = 0; x < cell_width; ++x)
                    put(x, y, image.at(x, y));
        nlohmann::json item{{"index", frame},
                            {"x", ox},
                            {"y", oy},
                            {"width", cell_width},
                            {"height", cell_height},
                            {"duration_ms", p.frames[frame].duration_ms},
                            {"source_x", left},
                            {"source_y", top},
                            {"metasprites", nlohmann::json::array()}};
        for (const auto &g : p.frames[frame].groups)
            if (group < 0 || group == g.id)
                item["metasprites"].push_back({{"id", g.id},
                                               {"name", g.name},
                                               {"origin_x", g.origin_x - left},
                                               {"origin_y", g.origin_y - top}});
        json["frames"].push_back(std::move(item));
    }
    auto png = encode_png(sheet);
    auto text = json.dump(2);
    write_binary(asset_path(prefix, ".png"), png);
    write_binary(asset_path(prefix, ".json"), {text.begin(), text.end()});
}
} // namespace tzj
