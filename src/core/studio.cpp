#include "core/tools.hpp"
#include <algorithm>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
namespace tzj {
namespace {
const Metasprite &find_group(const Frame &f, int id) {
    auto it =
        std::find_if(f.groups.begin(), f.groups.end(), [&](const auto &g) { return g.id == id; });
    if (it == f.groups.end())
        throw std::runtime_error("Metasprite group no longer exists");
    return *it;
}
void check_group(const Project &p, std::size_t frame, int id) {
    const auto &f = p.frames.at(frame);
    const auto &g = find_group(f, id);
    if (f.sprites_locked || g.locked)
        throw std::runtime_error("Unlock the metasprite before editing");
    for (const auto &s : f.sprites)
        if (s.group == id && s.locked)
            throw std::runtime_error("Unlock every member of the metasprite before editing");
}
int next_group(const Frame &f) {
    int id = 0;
    for (const auto &g : f.groups)
        id = std::max(id, g.id + 1);
    if (f.groups.size() >= 128 || id > 1000000)
        throw std::runtime_error("Too many metasprite groups");
    return id;
}
} // namespace
bool Project::sprite_visible(std::size_t frame, const Sprite &sprite) const {
    const auto &f = frames.at(frame);
    if (!f.sprites_visible || !sprite.visible)
        return false;
    return sprite.group < 0 || find_group(f, sprite.group).visible;
}
bool Project::sprite_locked(std::size_t frame, const Sprite &sprite) const {
    const auto &f = frames.at(frame);
    return f.sprites_locked || sprite.locked ||
           (sprite.group >= 0 && find_group(f, sprite.group).locked);
}
PixelRect metasprite_bounds(const Project &p, std::size_t frame, int id) {
    const auto &f = p.frames.at(frame);
    find_group(f, id);
    int left = 4096, top = 4096, right = -4096, bottom = -4096;
    for (const auto &s : f.sprites)
        if (s.group == id) {
            left = std::min(left, s.x);
            top = std::min(top, s.y);
            right = std::max(right, s.x + 8);
            bottom = std::max(bottom, s.y + 8);
        }
    if (right < left)
        return {};
    return {left, top, right - left, bottom - top};
}
int group_sprites(Project &p, std::size_t frame, const std::vector<int> &indices,
                  std::string name) {
    auto &f = p.frames.at(frame);
    if (indices.empty() || name.empty() || name.size() > 128 || f.sprites_locked)
        throw std::runtime_error("Choose sprites in an unlocked group and provide a name");
    std::set<int> chosen(indices.begin(), indices.end());
    for (int i : chosen) {
        if (i < 0 || i >= int(f.sprites.size()) || p.sprite_locked(frame, f.sprites[i]))
            throw std::runtime_error("Invalid or locked sprite selection");
        if (f.sprites[i].group >= 0)
            throw std::runtime_error(
                "Ungroup existing metasprites before regrouping their members");
    }
    int id = next_group(f), x = 4096, y = 4096;
    for (int i : chosen) {
        auto &s = f.sprites[i];
        s.group = id;
        x = std::min(x, s.x);
        y = std::min(y, s.y);
    }
    f.groups.push_back({id, x, y, std::move(name)});
    return id;
}
int create_metasprite(Project &p, std::size_t frame, int width, int height, int palette, int x,
                      int y, std::string name) {
    auto &f = p.frames.at(frame);
    if (width < 8 || height < 8 || width > 64 || height > 64 || width % 8 || height % 8 ||
        palette < 0 || palette > 3 || x < 0 || y < 1 || x + width > 256 || y + height > 240 ||
        f.sprites.size() + width * height / 64 > 4096 || f.sprites_locked || name.empty() ||
        name.size() > 128)
        throw std::runtime_error("Metasprite must be an 8..64 pixel grid inside the NES viewport");
    int id = next_group(f);
    for (int sy = 0; sy < height; sy += 8)
        for (int sx = 0; sx < width; sx += 8) {
            Sprite s;
            s.name = name + " " + std::to_string(sx / 8) + "," + std::to_string(sy / 8);
            s.x = x + sx;
            s.y = y + sy;
            s.palette = palette;
            s.group = id;
            f.sprites.push_back(std::move(s));
        }
    f.groups.push_back({id, x, y, std::move(name)});
    return id;
}
void ungroup_sprites(Project &p, std::size_t frame, int id) {
    check_group(p, frame, id);
    auto &f = p.frames[frame];
    for (auto &s : f.sprites)
        if (s.group == id)
            s.group = -1;
    std::erase_if(f.groups, [&](const auto &g) { return g.id == id; });
}
void move_metasprite(Project &p, std::size_t frame, int id, int dx, int dy) {
    check_group(p, frame, id);
    auto &f = p.frames[frame];
    const auto &group = find_group(f, id);
    if (std::abs(static_cast<long long>(group.origin_x) + dx) > 4096 ||
        std::abs(static_cast<long long>(group.origin_y) + dy) > 4096)
        throw std::runtime_error("Metasprite origin exceeds project bounds");
    for (const auto &s : f.sprites)
        if (s.group == id && (std::abs(static_cast<long long>(s.x) + dx) > 4096 ||
                              std::abs(static_cast<long long>(s.y) + dy) > 4096))
            throw std::runtime_error("Metasprite movement exceeds project bounds");
    for (auto &s : f.sprites)
        if (s.group == id) {
            s.x += dx;
            s.y += dy;
        }
    for (auto &g : f.groups)
        if (g.id == id) {
            g.origin_x += dx;
            g.origin_y += dy;
        }
}
void transform_metasprite(Project &p, std::size_t frame, int id, BrushTransform transform) {
    check_group(p, frame, id);
    if (transform != BrushTransform::FlipX && transform != BrushTransform::FlipY &&
        transform != BrushTransform::RotateCW && transform != BrushTransform::RotateCCW)
        throw std::runtime_error("Invalid metasprite transform");
    auto bounds = metasprite_bounds(p, frame, id);
    if (!bounds.width)
        return;
    auto sprites = p.frames[frame].sprites;
    for (auto &s : sprites)
        if (s.group == id) {
            int x = s.x - bounds.x, y = s.y - bounds.y;
            if (transform == BrushTransform::FlipX) {
                s.x = bounds.x + bounds.width - x - 8;
                s.flip_x = !s.flip_x;
            } else if (transform == BrushTransform::FlipY) {
                s.y = bounds.y + bounds.height - y - 8;
                s.flip_y = !s.flip_y;
            } else {
                Tile displayed;
                for (int py = 0; py < 8; ++py)
                    for (int px = 0; px < 8; ++px)
                        displayed[py * 8 + px] =
                            s.pixels[(s.flip_y ? 7 - py : py) * 8 + (s.flip_x ? 7 - px : px)];
                auto rotated =
                    transform_brush({8, 8, {displayed.begin(), displayed.end()}}, transform);
                std::copy(rotated.pixels.begin(), rotated.pixels.end(), s.pixels.begin());
                s.flip_x = s.flip_y = false;
                if (transform == BrushTransform::RotateCW) {
                    s.x = bounds.x + bounds.height - y - 8;
                    s.y = bounds.y + x;
                } else {
                    s.x = bounds.x + y;
                    s.y = bounds.y + bounds.width - x - 8;
                }
            }
        }
    for (const auto &s : sprites)
        if (s.x < -4096 || s.x > 4096 || s.y < -4096 || s.y > 4096)
            throw std::runtime_error("Transformed metasprite exceeds coordinate limits");
    p.frames[frame].sprites = std::move(sprites);
}
int duplicate_metasprite(Project &p, std::size_t frame, int id) {
    auto &f = p.frames.at(frame);
    auto group = find_group(f, id);
    int next = next_group(f);
    if (f.sprites_locked)
        throw std::runtime_error("Unlock the sprite group before duplicating");
    std::vector<Sprite> copies;
    for (auto s : f.sprites)
        if (s.group == id) {
            s.group = next;
            s.locked = false;
            copies.push_back(std::move(s));
        }
    if (f.sprites.size() + copies.size() > 4096)
        throw std::runtime_error("Too many sprites");
    f.sprites.insert(f.sprites.end(), copies.begin(), copies.end());
    group.id = next;
    group.name = group.name.substr(0, 123) + " copy";
    group.locked = false;
    f.groups.push_back(std::move(group));
    return next;
}
void delete_metasprite(Project &p, std::size_t frame, int id) {
    check_group(p, frame, id);
    auto &f = p.frames[frame];
    std::erase_if(f.sprites, [&](const auto &s) { return s.group == id; });
    std::erase_if(f.groups, [&](const auto &g) { return g.id == id; });
}
IndexedBrush resize_brush(const IndexedBrush &brush, int width, int height) {
    if (brush.width < 1 || brush.height < 1 || brush.width > 512 || brush.height > 512 ||
        brush.pixels.size() != std::size_t(brush.width) * brush.height || width < 1 || height < 1 ||
        width > 512 || height > 512)
        throw std::runtime_error("Invalid selection size");
    IndexedBrush out{width, height, std::vector<Byte>(std::size_t(width) * height)};
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x)
            out.pixels[y * width + x] =
                brush.pixels[(y * brush.height / height) * brush.width + x * brush.width / width];
    return out;
}
void apply_selection(Project &p, std::size_t frame, PixelRect source, int x, int y,
                     const IndexedBrush &result, bool copy) {
    if (!p.paint_layer_editable(frame) || source.x < 0 || source.y < 0 || source.width < 1 ||
        source.height < 1 || source.width > p.width || source.height > p.height ||
        source.x > p.width || source.y > p.height || source.x + source.width > p.width ||
        source.y + source.height > p.height || std::abs(static_cast<long long>(x)) > 4096 ||
        std::abs(static_cast<long long>(y)) > 4096)
        throw std::runtime_error("Choose an editable selection inside the canvas");
    auto checked = resize_brush(result, result.width, result.height);
    for (auto pixel : checked.pixels)
        if (pixel > 3 && pixel != 255)
            throw std::runtime_error("Invalid selection color");
    auto &f = p.frames.at(frame);
    if (!f.layers.empty() && f.layers[f.active_layer].kind == LayerKind::Tilemap) {
        auto &l = f.layers[f.active_layer];
        std::set<int> cells;
        auto include = [&](int rx, int ry, int rw, int rh) {
            int left = std::max(0, rx), top = std::max(0, ry), right = std::min(p.width, rx + rw),
                bottom = std::min(p.height, ry + rh);
            if (left >= right || top >= bottom)
                return;
            for (int ty = top / 8; ty <= (bottom - 1) / 8; ++ty)
                for (int tx = left / 8; tx <= (right - 1) / 8; ++tx) {
                    int cell = ty * (p.width / 8) + tx;
                    if (l.tile_refs.at(cell) >= 0)
                        cells.insert(cell);
                }
        };
        if (!copy)
            include(source.x, source.y, source.width, source.height);
        include(x, y, checked.width, checked.height);
        if (p.tiles.size() + cells.size() > 4096)
            throw std::runtime_error(
                "Selection needs more independent tiles than the library permits");
        for (int cell : cells) {
            Tile tile = p.tiles.at(l.tile_refs[cell]).pixels;
            int id = int(p.tiles.size());
            p.tiles.push_back({"Selection tile " + std::to_string(id), tile});
            l.tile_refs[cell] = id;
        }
    }
    if (!copy)
        for (int sy = 0; sy < source.height; ++sy)
            for (int sx = 0; sx < source.width; ++sx)
                p.write_background_pixel(
                    frame, std::size_t((source.y + sy) * p.width + source.x + sx), 255);
    stamp_brush(p, frame, x, y, checked);
}
std::vector<PixelPoint> pixel_perfect_path(const std::vector<PixelPoint> &points) {
    std::vector<PixelPoint> out;
    for (auto c : points) {
        if (!out.empty() && out.back() == c)
            continue;
        if (out.size() >= 2) {
            auto a = out[out.size() - 2], b = out.back();
            if (std::abs(a.x - c.x) == 1 && std::abs(a.y - c.y) == 1 &&
                std::abs(a.x - b.x) + std::abs(a.y - b.y) == 1 &&
                std::abs(b.x - c.x) + std::abs(b.y - c.y) == 1)
                out.pop_back();
        }
        out.push_back(c);
    }
    return out;
}
std::size_t add_tile_asset(Project &p, std::string name, const Tile &pixels) {
    if (p.tiles.size() >= 4096 || name.empty() || name.size() > 128)
        throw std::runtime_error("Invalid tile asset or library full");
    for (auto pixel : pixels)
        if (pixel > 3 && pixel != 255)
            throw std::runtime_error("Invalid indexed tile asset");
    p.tiles.push_back({std::move(name), pixels});
    return p.tiles.size() - 1;
}
void place_tile_asset(Project &p, std::size_t frame, std::size_t layer, int tx, int ty, int asset) {
    auto &f = p.frames.at(frame);
    auto &l = f.layers.at(layer);
    if (l.kind != LayerKind::Tilemap || l.locked || f.background_locked || !l.visible ||
        !f.background_visible || tx < 0 || ty < 0 || tx >= p.width / 8 || ty >= p.height / 8 ||
        asset < -1 || asset >= int(p.tiles.size()))
        throw std::runtime_error("Choose an editable tile-map cell and a valid tile asset");
    l.tile_refs.at(ty * (p.width / 8) + tx) = asset;
    p.compose_layers(frame);
}
void place_metatile(Project &p, std::size_t frame, std::size_t layer, int bx, int by,
                    std::size_t asset) {
    const auto meta = p.metatiles.at(asset);
    if (meta.palette < 0 || meta.palette > 3)
        throw std::runtime_error("Metatile palette must be 0..3");
    if (bx < 0 || by < 0 || bx >= p.block_width() || by >= p.block_height())
        throw std::runtime_error("Metatile lies outside the canvas");
    const auto &l = p.frames.at(frame).layers.at(layer);
    if (l.kind != LayerKind::Tilemap || l.locked || p.frames[frame].background_locked ||
        !l.visible || !p.frames[frame].background_visible)
        throw std::runtime_error("Select an editable tile map first");
    for (int id : meta.tiles)
        if (id < -1 || id >= int(p.tiles.size()))
            throw std::runtime_error("Invalid metatile tile reference");
    auto &f = p.frames[frame];
    for (int y = 0; y < 2; ++y)
        for (int x = 0; x < 2; ++x)
            f.layers[layer].tile_refs[(by * 2 + y) * (p.width / 8) + bx * 2 + x] =
                meta.tiles[y * 2 + x];
    f.attributes[by * p.block_width() + bx] = Byte(meta.palette);
    p.compose_layers(frame);
}
std::size_t convert_to_tilemap(Project &p, std::size_t frame, std::size_t layer) {
    p.compose_layers(frame);
    auto &f = p.frames.at(frame);
    auto &source = f.layers.at(layer);
    if (source.locked || f.background_locked || source.reference_only)
        throw std::runtime_error("Unlock an artwork layer before converting it to a tile map");
    auto pixels = source.kind == LayerKind::Image ? source.converted : source.pixels;
    std::map<Tile, int> ids;
    for (std::size_t i = 0; i < p.tiles.size(); ++i)
        ids.try_emplace(p.tiles[i].pixels, int(i));
    std::vector<TileAsset> additions;
    std::vector<int> refs;
    for (int ty = 0; ty < p.height / 8; ++ty)
        for (int tx = 0; tx < p.width / 8; ++tx) {
            Tile tile;
            for (int y = 0; y < 8; ++y)
                for (int x = 0; x < 8; ++x)
                    tile[y * 8 + x] = pixels[(ty * 8 + y) * p.width + tx * 8 + x];
            if (std::all_of(tile.begin(), tile.end(), [](Byte b) { return b == 255; })) {
                refs.push_back(-1);
                continue;
            }
            auto it = ids.find(tile);
            if (it == ids.end()) {
                int id = int(p.tiles.size() + additions.size());
                if (id >= 4096)
                    throw std::runtime_error("Tile library exceeds 4096 entries");
                additions.push_back({"Tile " + std::to_string(id), tile});
                it = ids.emplace(tile, id).first;
            }
            refs.push_back(it->second);
        }
    p.tiles.insert(p.tiles.end(), additions.begin(), additions.end());
    source.kind = LayerKind::Tilemap;
    source.tile_refs = std::move(refs);
    source.source = {};
    source.source_path.clear();
    source.converted.clear();
    p.compose_layers(frame);
    return layer;
}
} // namespace tzj
