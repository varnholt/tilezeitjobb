#include "core/project.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <tuple>
namespace tzj {
bool BackgroundLayer::operator==(const BackgroundLayer &o) const {
    return name == o.name && kind == o.kind && visible == o.visible && locked == o.locked &&
           effects_enabled == o.effects_enabled && reference_only == o.reference_only &&
           guide_opacity == o.guide_opacity && pixels == o.pixels && source == o.source &&
           source_path == o.source_path && modifiers == o.modifiers;
}
namespace {
Color source_pixel(const BackgroundLayer &layer, int x, int y) {
    const auto &m = layer.modifiers;
    x -= m.offset_x;
    y -= m.offset_y;
    const bool transform = layer.effects_enabled && m.transform_enabled;
    const int cx = transform ? m.crop_x : 0, cy = transform ? m.crop_y : 0;
    const int cw = transform && m.crop_width ? m.crop_width : layer.source.width - cx;
    const int ch = transform && m.crop_height ? m.crop_height : layer.source.height - cy;
    const int ow = transform && m.output_width ? m.output_width : cw;
    const int oh = transform && m.output_height ? m.output_height : ch;
    if (ow <= 0 || oh <= 0 || cw <= 0 || ch <= 0 || x < 0 || y < 0 || x >= ow || y >= oh)
        return {0, 0, 0, 0};
    auto c = layer.source.at(cx + int(static_cast<long long>(x) * cw / ow),
                             cy + int(static_cast<long long>(y) * ch / oh));
    if (layer.effects_enabled && m.color_enabled) {
        auto adjust = [&](Byte v) {
            return Byte(std::clamp(
                std::lround((v - 127.5) * (100 + m.contrast) / 100. + 127.5 + m.brightness * 2.55),
                0L, 255L));
        };
        c.r = adjust(c.r);
        c.g = adjust(c.g);
        c.b = adjust(c.b);
    }
    return c;
}
std::uint64_t stamp(const Project &p, const Frame &f, const BackgroundLayer &layer) {
    std::uint64_t h = 1469598103934665603ull;
    auto mix = [&](std::uint64_t v) { h = (h ^ v) * 1099511628211ull; };
    for (auto pal : p.background)
        for (auto c : pal)
            mix(c);
    for (auto c : f.attributes)
        mix(c);
    const auto &m = layer.modifiers;
    for (int v : {p.width,
                  p.height,
                  layer.source.width,
                  layer.source.height,
                  m.crop_x,
                  m.crop_y,
                  m.crop_width,
                  m.crop_height,
                  m.output_width,
                  m.output_height,
                  m.offset_x,
                  m.offset_y,
                  m.brightness,
                  m.contrast,
                  m.strength,
                  int(m.dither),
                  int(m.transform_enabled),
                  int(m.color_enabled),
                  int(m.dither_enabled),
                  int(layer.effects_enabled)})
        mix(std::uint64_t(v));
    // Source content is immutable between import/reload operations, which invalidate this cache
    // explicitly.
    return h ? h : 1;
}
int threshold(int x, int y, int n) {
    constexpr int base[2][2]{{0, 2}, {3, 1}};
    int value = 0;
    for (int bit = 0; (1 << bit) < n; ++bit)
        value = value * 4 + base[(y >> bit) & 1][(x >> bit) & 1];
    return value;
}
Byte ordered(Color c, const Palette &palette, int x, int y, int n, int strength) {
    if (!strength)
        return nearest_slot(c, palette, x, y, false);
    const auto nearest = nearest_slot(c, palette, x, y, false);
    auto distance = [](double r, double g, double b) { return 3 * r * r + 6 * g * g + 2 * b * b; };
    auto single = nes_colors[palette[nearest]];
    double best = distance(double(c.r) - single.r, double(c.g) - single.g, double(c.b) - single.b);
    Byte a = nearest, b = nearest;
    double weight = 0;
    for (int i = 0; i < 4; ++i)
        for (int j = i + 1; j < 4; ++j) {
            auto ca = nes_colors[palette[i]], cb = nes_colors[palette[j]];
            const double dr = double(cb.r) - ca.r, dg = double(cb.g) - ca.g,
                         db = double(cb.b) - ca.b;
            const double length = distance(dr, dg, db);
            if (!length)
                continue;
            double t = std::clamp(
                (3 * (c.r - ca.r) * dr + 6 * (c.g - ca.g) * dg + 2 * (c.b - ca.b) * db) / length,
                0., 1.);
            double error = distance(c.r - ca.r - t * dr, c.g - ca.g - t * dg, c.b - ca.b - t * db);
            if (error < best) {
                best = error;
                a = Byte(i);
                b = Byte(j);
                weight = t;
            }
        }
    if (a == b)
        return nearest;
    double t = weight * strength / 100. + (nearest == b ? 1. : 0.) * (100 - strength) / 100.;
    return (threshold(x, y, n) + .5) / (n * n) < t ? b : a;
}
void convert(const Project &p, const Frame &f, BackgroundLayer &layer, bool force) {
    auto signature = stamp(p, f, layer);
    if (!force && layer.conversion_stamp == signature && layer.converted.size() == f.pixels.size())
        return;
    layer.converted.assign(static_cast<std::size_t>(p.width) * p.height, 255);
    const auto &m = layer.modifiers;
    auto mode = layer.effects_enabled && m.dither_enabled ? m.dither : DitherMode::None;
    const bool diffusion = mode == DitherMode::FloydSteinberg || mode == DitherMode::Atkinson;
    std::vector<std::array<double, 3>> errors(diffusion ? layer.converted.size() : 0);
    for (int y = 0; y < p.height; ++y)
        for (int x = 0; x < p.width; ++x) {
            auto c = source_pixel(layer, x, y);
            if (c.a < 128)
                continue;
            auto index = y * p.width + x;
            const auto &pal = p.background[f.attributes[(y / 16) * p.block_width() + x / 16]];
            if (diffusion) {
                c.r = Byte(std::clamp(std::lround(c.r + errors[index][0]), 0L, 255L));
                c.g = Byte(std::clamp(std::lround(c.g + errors[index][1]), 0L, 255L));
                c.b = Byte(std::clamp(std::lround(c.b + errors[index][2]), 0L, 255L));
            }
            Byte slot = mode == DitherMode::Bayer2 || mode == DitherMode::Bayer4 ||
                                mode == DitherMode::Bayer8
                            ? ordered(c, pal, x, y,
                                      mode == DitherMode::Bayer2   ? 2
                                      : mode == DitherMode::Bayer4 ? 4
                                                                   : 8,
                                      m.strength)
                            : nearest_slot(c, pal, x, y, false);
            layer.converted[index] = slot;
            if (diffusion && m.strength) {
                auto quantized = nes_colors[pal[slot]];
                auto spread = [&](int dx, int dy, double weight) {
                    int px = x + dx, py = y + dy;
                    if (px < 0 || py < 0 || px >= p.width || py >= p.height || px / 16 != x / 16 ||
                        py / 16 != y / 16 || source_pixel(layer, px, py).a < 128)
                        return;
                    auto &e = errors[py * p.width + px];
                    const double amount = weight * m.strength / 100.;
                    e[0] += (int(c.r) - quantized.r) * amount;
                    e[1] += (int(c.g) - quantized.g) * amount;
                    e[2] += (int(c.b) - quantized.b) * amount;
                };
                if (mode == DitherMode::FloydSteinberg) {
                    spread(1, 0, 7. / 16);
                    spread(-1, 1, 3. / 16);
                    spread(0, 1, 5. / 16);
                    spread(1, 1, 1. / 16);
                } else {
                    spread(1, 0, 1. / 8);
                    spread(2, 0, 1. / 8);
                    spread(-1, 1, 1. / 8);
                    spread(0, 1, 1. / 8);
                    spread(1, 1, 1. / 8);
                    spread(0, 2, 1. / 8);
                }
            }
        }
    layer.conversion_stamp = signature;
}
void compose(const Project &p, Frame &f, bool force) {
    if (f.layers.empty())
        return;
    f.pixels.assign(static_cast<std::size_t>(p.width) * p.height, 0);
    f.paint_mask.assign(f.pixels.size(), 255);
    for (auto &layer : f.layers) {
        if (layer.kind == LayerKind::Image)
            convert(p, f, layer, force);
        if (!f.background_visible || !layer.visible || layer.reference_only)
            continue;
        const auto &pixels = layer.kind == LayerKind::Image ? layer.converted : layer.pixels;
        if (pixels.size() != f.pixels.size())
            throw std::runtime_error("Invalid layer pixel dimensions");
        for (std::size_t i = 0; i < pixels.size(); ++i)
            if (pixels[i] != 255) {
                f.pixels[i] = pixels[i];
                f.paint_mask[i] = layer.kind == LayerKind::Paint ? pixels[i] : 255;
            }
    }
}
} // namespace
Color guide_pixel(const BackgroundLayer &layer, int x, int y) {
    auto color = source_pixel(layer, x, y);
    color.a = Byte(int(color.a) * std::clamp(layer.guide_opacity, 0, 100) / 100);
    return color;
}
void Project::ensure_layers(std::size_t frame) {
    auto &f = frames.at(frame);
    if (!f.layers.empty())
        return;
    BackgroundLayer base;
    base.name = "Background artwork";
    base.pixels = f.pixels;
    if (!reference.empty()) {
        BackgroundLayer image;
        image.name = "Imported image";
        image.kind = LayerKind::Image;
        image.source = reference;
        image.source_path = image_path;
        image.modifiers.offset_x = offset_x;
        image.modifiers.offset_y = offset_y;
        image.modifiers.dither = dither ? DitherMode::Bayer4 : DitherMode::None;
        f.layers.push_back(std::move(image));
        // Preserve the exact old conversion while making it independently paintable.
        base.name = "Existing background";
    }
    f.layers.push_back(std::move(base));
    f.active_layer = int(f.layers.size()) - 1;
    compose_layers(frame);
}
void Project::compose_layers(std::size_t frame, bool force) {
    compose(*this, frames.at(frame), force);
}
Frame Project::composed_frame(std::size_t frame) const {
    auto f = frames.at(frame);
    compose(*this, f, false);
    return f;
}
bool Project::paint_layer_editable(std::size_t frame) const {
    const auto &f = frames.at(frame);
    if (f.layers.empty())
        return true;
    if (!f.background_visible || f.background_locked || f.active_layer < 0 ||
        f.active_layer >= int(f.layers.size()))
        return false;
    const auto &l = f.layers[f.active_layer];
    return l.kind == LayerKind::Paint && l.visible && !l.locked;
}
const std::vector<Byte> &Project::active_pixels(std::size_t frame) const {
    const auto &f = frames.at(frame);
    if (f.layers.empty() || f.active_layer < 0)
        return f.pixels;
    const auto &l = f.layers.at(f.active_layer);
    return l.kind == LayerKind::Image ? l.converted : l.pixels;
}
void Project::write_background_pixel(std::size_t frame, std::size_t position, Byte color,
                                     int palette) {
    auto &f = frames.at(frame);
    if (color > 3 && color != 255)
        throw std::runtime_error("Invalid layer paint color");
    if (!paint_layer_editable(frame))
        throw std::runtime_error("Choose an unlocked, visible paint layer. Use Make paintable copy "
                                 "for imported artwork.");
    if (f.layers.empty()) {
        f.pixels.at(position) = color == 255 ? 0 : color;
        if (f.paint_mask.empty())
            f.paint_mask.assign(f.pixels.size(), 255);
        f.paint_mask.at(position) = f.pixels[position];
    } else
        f.layers.at(f.active_layer).pixels.at(position) = color;
    if (palette >= 0) {
        int x = int(position % width), y = int(position / width);
        f.attributes[(y / 16) * block_width() + x / 16] = Byte(palette);
    }
}
std::size_t Project::add_paint_layer(std::size_t frame, std::string name) {
    ensure_layers(frame);
    auto &f = frames.at(frame);
    if (f.layers.size() >= 64)
        throw std::runtime_error("A frame supports at most 64 background layers");
    BackgroundLayer layer;
    layer.name = std::move(name);
    layer.pixels.assign(f.pixels.size(), 255);
    auto i = std::clamp(f.active_layer + 1, 0, int(f.layers.size()));
    f.layers.insert(f.layers.begin() + i, std::move(layer));
    f.active_layer = i;
    compose_layers(frame);
    return std::size_t(i);
}
std::size_t Project::add_image_layer(std::size_t frame, Image source, std::string name,
                                     std::string path) {
    if (source.empty())
        throw std::runtime_error("Choose a nonempty image");
    auto i = add_paint_layer(frame, std::move(name));
    auto &l = frames[frame].layers[i];
    l.kind = LayerKind::Image;
    l.pixels.clear();
    l.source = std::move(source);
    l.source_path = std::move(path);
    compose_layers(frame, true);
    return i;
}
std::size_t Project::duplicate_layer(std::size_t frame, std::size_t index, bool paintable) {
    ensure_layers(frame);
    auto &f = frames.at(frame);
    if (f.layers.size() >= 64)
        throw std::runtime_error("A frame supports at most 64 background layers");
    compose_layers(frame);
    auto copy = f.layers.at(index);
    copy.name += paintable ? " - paint copy" : " copy";
    copy.visible = true;
    copy.locked = false;
    if (paintable && copy.kind == LayerKind::Image) {
        copy.reference_only = false;
        copy.kind = LayerKind::Paint;
        copy.pixels = copy.converted;
        copy.source = {};
        copy.source_path.clear();
        copy.converted.clear();
        copy.conversion_stamp = 0;
    }
    f.layers.insert(f.layers.begin() + index + 1, std::move(copy));
    f.active_layer = int(index + 1);
    compose_layers(frame);
    return index + 1;
}
void Project::remove_layer(std::size_t frame, std::size_t index) {
    auto &f = frames.at(frame);
    if (f.layers.size() <= 1 || f.layers.at(index).locked || f.background_locked)
        throw std::runtime_error(
            "Keep at least one background layer and unlock it before deletion");
    f.layers.erase(f.layers.begin() + index);
    f.active_layer = std::min(int(index), int(f.layers.size()) - 1);
    compose_layers(frame);
}
void Project::move_layer(std::size_t frame, std::size_t index, std::size_t destination) {
    auto &f = frames.at(frame);
    if (index >= f.layers.size() || destination >= f.layers.size())
        throw std::runtime_error("Layer position outside the stack");
    if (f.background_locked || f.layers[index].locked)
        throw std::runtime_error("Unlock the layer before moving it");
    auto item = std::move(f.layers[index]);
    f.layers.erase(f.layers.begin() + index);
    f.layers.insert(f.layers.begin() + destination, std::move(item));
    f.active_layer = int(destination);
    compose_layers(frame);
}
void Project::merge_layer_down(std::size_t frame, std::size_t index) {
    auto &f = frames.at(frame);
    if (index == 0 || index >= f.layers.size())
        throw std::runtime_error("Choose a layer above another background layer");
    auto &top = f.layers[index];
    auto &bottom = f.layers[index - 1];
    if (top.reference_only || bottom.reference_only)
        throw std::runtime_error("Make a paintable copy of reference guides before merging");
    if (f.background_locked || top.locked || bottom.locked || !top.visible || !bottom.visible)
        throw std::runtime_error("Show and unlock both layers before merging");
    compose_layers(frame);
    auto pixels = bottom.kind == LayerKind::Image ? bottom.converted : bottom.pixels;
    const auto &upper = top.kind == LayerKind::Image ? top.converted : top.pixels;
    for (std::size_t i = 0; i < pixels.size(); ++i)
        if (upper[i] != 255)
            pixels[i] = upper[i];
    BackgroundLayer merged;
    merged.name = bottom.name + " + " + top.name;
    merged.pixels = std::move(pixels);
    f.layers[index - 1] = std::move(merged);
    f.layers.erase(f.layers.begin() + index);
    f.active_layer = int(index) - 1;
    compose_layers(frame);
}
void Project::clear_active_layer(std::size_t frame) {
    if (!paint_layer_editable(frame))
        throw std::runtime_error("Choose an unlocked paint layer");
    auto &f = frames.at(frame);
    if (f.layers.empty()) {
        f.paint_mask.clear();
        std::fill(f.pixels.begin(), f.pixels.end(), Byte(0));
    } else
        std::fill(f.layers.at(f.active_layer).pixels.begin(),
                  f.layers.at(f.active_layer).pixels.end(), Byte(255));
    compose_layers(frame);
}
void Project::fit_layer_colors(std::size_t frame, std::size_t index) {
    auto &f = frames.at(frame);
    const auto &l = f.layers.at(index);
    if (l.kind != LayerKind::Image)
        throw std::runtime_error("Choose an imported image layer");
    Project temp;
    temp.resize(width, height);
    temp.reference = {width, height, std::vector<Byte>(std::size_t(width) * height * 4)};
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x) {
            auto c = source_pixel(l, x, y);
            auto i = (y * width + x) * 4;
            temp.reference.rgba[i] = c.r;
            temp.reference.rgba[i + 1] = c.g;
            temp.reference.rgba[i + 2] = c.b;
            temp.reference.rgba[i + 3] = c.a;
        }
    temp.background = background;
    temp.quantize(0, true);
    background = temp.background;
    set_backdrop(background[0][0]);
    f.attributes = temp.frames[0].attributes;
    compose_layers(frame, true);
}
} // namespace tzj
