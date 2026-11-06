#include "core/project.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <map>
#include <nlohmann/json.hpp>
#include <queue>
#include <sstream>
#include <stdexcept>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
namespace tzj {
using Json = nlohmann::json;
static int checked_int(const Json &value, int low, int high) {
    if (!value.is_number_integer())
        throw std::runtime_error("Expected an integer in project data");
    if (value.is_number_unsigned()) {
        auto n = value.get<std::uint64_t>();
        if (n > static_cast<std::uint64_t>(high))
            throw std::runtime_error("Project number out of range");
        return static_cast<int>(n);
    }
    auto n = value.get<std::int64_t>();
    if (n < low || n > high)
        throw std::runtime_error("Project number out of range");
    return static_cast<int>(n);
}
static void byte_array(const Json &value, std::size_t size, int maximum) {
    if (!value.is_array() || value.size() != size)
        throw std::runtime_error("Invalid array length in project");
    for (const auto &n : value)
        checked_int(n, 0, maximum);
}
static void palettes_json(const Json &value) {
    if (!value.is_array() || value.size() != 4)
        throw std::runtime_error("Exactly four palettes are required");
    for (const auto &pal : value)
        byte_array(pal, 4, 63);
}
Project::Project() {
    frames.push_back(Frame{std::vector<Byte>(width * height),
                           std::vector<Byte>(block_width() * block_height()),
                           {},
                           120});
}
void Project::resize(int w, int h) {
    if (!((w == 256 && h == 240) || (w == 512 && h == 240) || (w == 256 && h == 480)))
        throw std::runtime_error("Choose 256x240, 512x240, or 256x480");
    for (auto &f : frames) {
        std::vector<Byte> p(w * h), a((w / 16) * (h / 16));
        for (int y = 0; y < std::min(h, height); ++y)
            std::copy_n(f.pixels.begin() + y * width, std::min(w, width), p.begin() + y * w);
        for (int y = 0; y < std::min(h, height) / 16; ++y)
            std::copy_n(f.attributes.begin() + y * block_width(), std::min(w, width) / 16,
                        a.begin() + y * (w / 16));
        if (!f.paint_mask.empty()) {
            std::vector<Byte> mask(w * h, 255);
            for (int y = 0; y < std::min(h, height); ++y)
                std::copy_n(f.paint_mask.begin() + y * width, std::min(w, width),
                            mask.begin() + y * w);
            f.paint_mask = std::move(mask);
        }
        f.pixels = std::move(p);
        f.attributes = std::move(a);
    }
    width = w;
    height = h;
}
void Project::set_backdrop(Byte c) {
    if (c >= 64)
        throw std::runtime_error("NES color index out of range");
    for (auto &p : background)
        p[0] = c;
    for (auto &p : sprite_palettes)
        p[0] = c;
}
void Project::validate() const {
    const auto fail = [](bool bad, const char *s) {
        if (bad)
            throw std::runtime_error(s);
    };
    fail(!((width == 256 && height == 240) || (width == 512 && height == 240) ||
           (width == 256 && height == 480)),
         "Invalid project dimensions");
    fail(frames.empty() || frames.size() > 256, "Project requires 1 to 256 frames");
    fail(std::abs(static_cast<long long>(offset_x)) > 4096 ||
             std::abs(static_cast<long long>(offset_y)) > 4096,
         "Image offset out of range");
    for (const auto &group : {background, sprite_palettes})
        for (const auto &pal : group) {
            for (auto c : pal)
                fail(c >= 64, "Invalid NES palette color");
            fail(pal[0] != background[0][0], "All subpalettes must share the backdrop");
        }
    for (const auto &im : {&reference, &sprite_sheet})
        fail((im->width == 0) != (im->height == 0) || im->width < 0 || im->height < 0 ||
                 im->width > 4096 || im->height > 4096 ||
                 im->rgba.size() != static_cast<std::size_t>(im->width) * im->height * 4,
             "Invalid embedded image");
    for (const auto &f : frames) {
        fail(f.pixels.size() != static_cast<std::size_t>(width) * height ||
                 f.attributes.size() != static_cast<std::size_t>(block_width()) * block_height(),
             "Invalid frame buffer length");
        fail(!f.paint_mask.empty() && f.paint_mask.size() != f.pixels.size(),
             "Invalid paint mask length");
        for (auto b : f.paint_mask)
            fail(b > 3 && b != 255, "Invalid paint mask value");
        fail(f.duration_ms < 10 || f.duration_ms > 60000, "Frame duration must be 10 to 60000 ms");
        fail(f.sprites.size() > 4096, "Too many sprite objects");
        for (auto p : f.pixels)
            fail(p > 3, "Pixel index outside 0..3");
        for (auto p : f.attributes)
            fail(p > 3, "Attribute index outside 0..3");
        for (const auto &s : f.sprites) {
            fail(s.palette < 0 || s.palette > 3, "Sprite palette outside 0..3");
            fail(std::abs(static_cast<long long>(s.x)) > 4096 ||
                     std::abs(static_cast<long long>(s.y)) > 4096,
                 "Sprite position out of range");
            for (auto p : s.pixels)
                fail(p > 3, "Sprite pixel outside 0..3");
        }
    }
}
void Project::quantize(std::size_t frame, bool auto_palettes) {
    if (reference.empty())
        throw std::runtime_error("Import a reference image first");
    auto &f = frames.at(frame);
    if (auto_palettes) {
        std::array<int, 64> histogram{};
        for (int y = 0; y < height; ++y)
            for (int x = 0; x < width; ++x) {
                auto c = reference.at(x - offset_x, y - offset_y);
                if (c.a >= 128)
                    ++histogram[nearest_nes(c)];
            }
        const Byte bg = static_cast<Byte>(std::max_element(histogram.begin(), histogram.end()) -
                                          histogram.begin());
        set_backdrop(bg);
        std::map<std::array<Byte, 3>, int> candidates;
        for (int by = 0; by < block_height(); ++by)
            for (int bx = 0; bx < block_width(); ++bx) {
                std::array<int, 64> hist{};
                for (int y = 0; y < 16; ++y)
                    for (int x = 0; x < 16; ++x) {
                        auto c = reference.at(bx * 16 + x - offset_x, by * 16 + y - offset_y);
                        if (c.a >= 128)
                            ++hist[nearest_nes(c)];
                    }
                hist[bg] = 0;
                std::array<Byte, 3> triple{};
                for (auto &c : triple) {
                    auto it = std::max_element(hist.begin(), hist.end());
                    c = *it ? static_cast<Byte>(it - hist.begin()) : bg;
                    *it = 0;
                }
                std::sort(triple.begin(), triple.end());
                ++candidates[triple];
            }
        std::vector<std::pair<std::array<Byte, 3>, int>> sorted(candidates.begin(),
                                                                candidates.end());
        std::stable_sort(sorted.begin(), sorted.end(),
                         [](const auto &a, const auto &b) { return a.second > b.second; });
        for (int i = 0; i < 4; ++i) {
            const auto &p = sorted[std::min(static_cast<std::size_t>(i), sorted.size() - 1)].first;
            background[i] = {bg, p[0], p[1], p[2]};
        }
        for (int by = 0; by < block_height(); ++by)
            for (int bx = 0; bx < block_width(); ++bx) {
                long long best = std::numeric_limits<long long>::max();
                int pal = 0;
                for (int i = 0; i < 4; ++i) {
                    long long error = 0;
                    for (int y = 0; y < 16; ++y)
                        for (int x = 0; x < 16; ++x) {
                            auto c = reference.at(bx * 16 + x - offset_x, by * 16 + y - offset_y);
                            if (c.a < 128)
                                continue;
                            auto s = nearest_slot(c, background[i], x, y, false);
                            auto n = nes_colors[background[i][s]];
                            int r = int(c.r) - n.r, g = int(c.g) - n.g, b = int(c.b) - n.b;
                            error += 3 * r * r + 6 * g * g + 2 * b * b;
                        }
                    if (error < best) {
                        best = error;
                        pal = i;
                    }
                }
                f.attributes[by * block_width() + bx] = static_cast<Byte>(pal);
            }
    }
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x)
            f.pixels[y * width + x] =
                !f.paint_mask.empty() && f.paint_mask[y * width + x] != 255
                    ? f.paint_mask[y * width + x]
                    : nearest_slot(reference.at(x - offset_x, y - offset_y),
                                   background[f.attributes[(y / 16) * block_width() + x / 16]], x,
                                   y, dither);
}
Sprite Project::make_sprite(int tx, int ty, int pal, int x, int y) const {
    if (sprite_sheet.empty())
        throw std::runtime_error("Import a sprite sheet first");
    if (tx < 0 || ty < 0 || tx * 8 + 8 > sprite_sheet.width || ty * 8 + 8 > sprite_sheet.height ||
        pal < 0 || pal > 3)
        throw std::runtime_error("Sprite tile or palette is out of range");
    Sprite s;
    s.tile_x = tx;
    s.tile_y = ty;
    s.palette = pal;
    s.x = x;
    s.y = y;
    for (int py = 0; py < 8; ++py)
        for (int px = 0; px < 8; ++px)
            s.pixels[py * 8 + px] = nearest_slot(sprite_sheet.at(tx * 8 + px, ty * 8 + py),
                                                 sprite_palettes[pal], px, py, dither, true);
    return s;
}
void Project::generate_sprites(std::size_t frame, int pal) {
    auto &f = frames.at(frame);
    std::vector<Sprite> generated;
    for (int y = 0; y < std::min(height, sprite_sheet.height) / 8; ++y)
        for (int x = 0; x < std::min(width, sprite_sheet.width) / 8; ++x) {
            auto s = make_sprite(x, y, pal, x * 8, y * 8 + 1);
            if (std::any_of(s.pixels.begin(), s.pixels.end(), [](Byte c) { return c != 0; }))
                generated.push_back(std::move(s));
        }
    f.sprites = std::move(generated);
}
Image Project::render(std::size_t index, bool sprites, bool hardware_limits) const {
    const auto &f = frames.at(index);
    Image im{width, height, std::vector<Byte>(width * height * 4)};
    auto set = [&](int x, int y, Color c) {
        const auto i = (y * width + x) * 4;
        im.rgba[i] = c.r;
        im.rgba[i + 1] = c.g;
        im.rgba[i + 2] = c.b;
        im.rgba[i + 3] = 255;
    };
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x)
            set(x, y,
                nes_colors[background[f.attributes[(y / 16) * block_width() + x / 16]]
                                     [f.pixels[y * width + x]]]);
    if (sprites) {
        // OAM priority: lower object index wins, including transparent-background priority.
        std::vector<bool> claimed(width * height);
        std::array<int, 480> scanlines{};
        int count = 0;
        for (const auto &s : f.sprites) {
            if (!s.visible)
                continue;
            if (hardware_limits && count++ >= 64)
                break;
            for (int y = 0; y < 8; ++y) {
                int py = s.y + y;
                if (py < 0 || py >= height)
                    continue;
                bool row = !hardware_limits || scanlines[py]++ < 8;
                if (!row)
                    continue;
                for (int x = 0; x < 8; ++x) {
                    int px = s.x + x;
                    if (px < 0 || px >= width)
                        continue;
                    Byte v = s.pixels[(s.flip_y ? 7 - y : y) * 8 + (s.flip_x ? 7 - x : x)];
                    if (!v || claimed[py * width + px])
                        continue;
                    claimed[py * width + px] = true;
                    if (s.behind && f.pixels[py * width + px])
                        continue;
                    set(px, py, nes_colors[sprite_palettes[s.palette][v]]);
                }
            }
        }
    }
    return im;
}
void paint_line(Project &p, std::size_t frame, int x0, int y0, int x1, int y1, Byte c, int size,
                int palette) {
    if (c > 3 || size < 1 || size > 32 || palette < -1 || palette > 3)
        throw std::runtime_error("Invalid brush");
    auto &f = p.frames.at(frame);
    if (f.paint_mask.empty())
        f.paint_mask.assign(f.pixels.size(), 255);
    x0 = std::clamp(x0, 0, p.width - 1);
    x1 = std::clamp(x1, 0, p.width - 1);
    y0 = std::clamp(y0, 0, p.height - 1);
    y1 = std::clamp(y1, 0, p.height - 1);
    int dx = std::abs(x1 - x0), dy = -std::abs(y1 - y0), sx = x0 < x1 ? 1 : -1,
        sy = y0 < y1 ? 1 : -1, err = dx + dy;
    for (;;) {
        for (int yy = y0 - size / 2; yy < y0 - size / 2 + size; ++yy)
            for (int xx = x0 - size / 2; xx < x0 - size / 2 + size; ++xx)
                if (xx >= 0 && yy >= 0 && xx < p.width && yy < p.height) {
                    f.pixels[yy * p.width + xx] = c;
                    f.paint_mask[yy * p.width + xx] = c;
                    if (palette >= 0)
                        f.attributes[(yy / 16) * p.block_width() + xx / 16] =
                            static_cast<Byte>(palette);
                }
        if (x0 == x1 && y0 == y1)
            break;
        int e = 2 * err;
        if (e >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}
void flood_fill(Project &p, std::size_t frame, int x, int y, Byte c, int palette) {
    if (x < 0 || y < 0 || x >= p.width || y >= p.height || c > 3 || palette < -1 || palette > 3)
        return;
    auto &f = p.frames.at(frame);
    Byte old = f.pixels[y * p.width + x],
         old_palette = f.attributes[(y / 16) * p.block_width() + x / 16];
    if (old == c && (palette < 0 || palette == old_palette))
        return;
    std::vector<bool> visited(f.pixels.size());
    std::vector<int> changed;
    std::queue<std::pair<int, int>> queue;
    queue.push({x, y});
    visited[y * p.width + x] = true;
    while (!queue.empty()) {
        auto [cx, cy] = queue.front();
        queue.pop();
        changed.push_back(cy * p.width + cx);
        for (auto [nx, ny] : {std::pair{cx - 1, cy}, std::pair{cx + 1, cy}, std::pair{cx, cy - 1},
                              std::pair{cx, cy + 1}}) {
            if (nx < 0 || ny < 0 || nx >= p.width || ny >= p.height)
                continue;
            int index = ny * p.width + nx;
            if (!visited[index] && f.pixels[index] == old &&
                f.attributes[(ny / 16) * p.block_width() + nx / 16] == old_palette) {
                visited[index] = true;
                queue.push({nx, ny});
            }
        }
    }
    if (f.paint_mask.empty())
        f.paint_mask.assign(f.pixels.size(), 255);
    for (auto index : changed) {
        f.pixels[index] = c;
        f.paint_mask[index] = c;
        if (palette >= 0)
            f.attributes[(index / p.width / 16) * p.block_width() + (index % p.width) / 16] =
                static_cast<Byte>(palette);
    }
}

static std::string hex(const std::vector<Byte> &data) {
    static constexpr char digits[] = "0123456789abcdef";
    std::string s;
    s.reserve(data.size() * 2);
    for (Byte b : data) {
        s += digits[b >> 4];
        s += digits[b & 15];
    }
    return s;
}
static std::vector<Byte> unhex(const std::string &s, std::size_t expected) {
    if (s.size() != expected * 2)
        throw std::runtime_error("Invalid data length in project");
    std::vector<Byte> out;
    out.reserve(expected);
    auto n = [](char c) -> int {
        if (c >= '0' && c <= '9')
            return c - '0';
        if (c >= 'a' && c <= 'f')
            return c - 'a' + 10;
        if (c >= 'A' && c <= 'F')
            return c - 'A' + 10;
        throw std::runtime_error("Invalid hex data in project");
    };
    for (std::size_t i = 0; i < s.size(); i += 2)
        out.push_back(static_cast<Byte>((n(s[i]) << 4) | n(s[i + 1])));
    return out;
}
static Json image_json(const Image &i) {
    return Json{{"width", i.width}, {"height", i.height}, {"rgba", hex(i.rgba)}};
}
static Image read_image(const Json &j) {
    int w = checked_int(j.at("width"), 0, 4096), h = checked_int(j.at("height"), 0, 4096);
    if (w < 0 || h < 0 || w > 4096 || h > 4096)
        throw std::runtime_error("Invalid embedded image dimensions");
    return {w, h, unhex(j.at("rgba"), static_cast<std::size_t>(w) * h * 4)};
}
void save_project(const std::filesystem::path &path, const Project &p) {
    p.validate();
    Json j{{"format", "tilezeitjobb"},
           {"version", 2},
           {"width", p.width},
           {"height", p.height},
           {"offset_x", p.offset_x},
           {"offset_y", p.offset_y},
           {"dither", p.dither},
           {"image_path", p.image_path},
           {"sprite_path", p.sprite_path},
           {"background", p.background},
           {"sprite_palettes", p.sprite_palettes},
           {"reference", image_json(p.reference)},
           {"sprite_sheet", image_json(p.sprite_sheet)},
           {"frames", Json::array()}};
    for (const auto &f : p.frames) {
        Json sprites = Json::array();
        for (const auto &s : f.sprites)
            sprites.push_back({{"name", s.name},
                               {"x", s.x},
                               {"y", s.y},
                               {"palette", s.palette},
                               {"tile_x", s.tile_x},
                               {"tile_y", s.tile_y},
                               {"visible", s.visible},
                               {"flip_x", s.flip_x},
                               {"flip_y", s.flip_y},
                               {"behind", s.behind},
                               {"pixels", s.pixels}});
        j["frames"].push_back({{"pixels", hex(f.pixels)},
                               {"paint_mask", hex(f.paint_mask)},
                               {"attributes", f.attributes},
                               {"duration_ms", f.duration_ms},
                               {"sprites", sprites}});
    }
    auto serialized = j.dump(2);
    if (serialized.size() > 192 * 1024 * 1024)
        throw std::runtime_error("Project exceeds 192 MiB; reduce embedded images or frame count");
    auto tmp = path;
    tmp += "." + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
           ".tzj.tmp";
    try {
        std::ofstream out(tmp, std::ios::binary);
        if (!out)
            throw std::runtime_error("Cannot write project");
        out << serialized;
        out.close();
        if (!out)
            throw std::runtime_error("Project write failed");
#ifdef _WIN32
        if (!MoveFileExW(tmp.c_str(), path.c_str(),
                         MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            throw std::runtime_error("Cannot replace project file");
#else
        std::filesystem::rename(tmp, path);
#endif
    } catch (...) {
        std::error_code ec;
        std::filesystem::remove(tmp, ec);
        throw;
    }
}
static Project read_legacy(const std::string &text, const std::filesystem::path &path) {
    std::map<std::string, std::string> values;
    std::istringstream input(text);
    std::string line;
    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        auto pos = line.find('=');
        if (pos == std::string::npos || line[0] == '#' || line[0] == ';')
            continue;
        auto v = line.substr(pos + 1);
        if (v.size() >= 2 && v.front() == '"' && v.back() == '"') {
            std::istringstream s(v);
            s >> std::quoted(v);
        }
        values[line.substr(0, pos)] = v;
    }
    auto number = [&](const std::string &key, int fallback = 0) {
        auto it = values.find(key);
        return it == values.end() ? fallback : std::stoi(it->second);
    };
    auto resolve = [&](std::string value) {
        for (std::size_t pos = 0; (pos = value.find("\\\\", pos)) != std::string::npos;)
            value.replace(pos, 2, "/");
        std::filesystem::path p = path_from_utf8(value);
        return p.is_absolute() ? p : path.parent_path() / p;
    };
    Project p;
    p.offset_x = number("offset_x");
    p.offset_y = number("offset_y");
    p.image_path = values["image"];
    p.sprite_path = values["sprite_image"];
    if (!p.image_path.empty()) {
        p.reference = load_image(resolve(p.image_path));
        if (p.reference.width > 256)
            p.resize(512, 240);
        else if (p.reference.height > 240)
            p.resize(256, 480);
    }
    if (!p.sprite_path.empty())
        p.sprite_sheet = load_image(resolve(p.sprite_path));
    auto read_pal = [&](const std::string &prefix, std::array<Palette, 4> &out) {
        for (int i = 0; i < std::min(4, number(prefix + "_pal_count")); ++i)
            for (int c = 0; c < 4; ++c) {
                auto v = static_cast<std::uint32_t>(std::stoull(
                    values.at(prefix + "_pal_" + std::to_string(i) + "_" + std::to_string(c))));
                out[i][c] = nearest_nes({Byte(v >> 16), Byte(v >> 8), Byte(v), 255});
            }
    };
    read_pal("nametable", p.background);
    read_pal("sprite", p.sprite_palettes);
    p.set_backdrop(p.background[0][0]);
    std::istringstream attrs(values["nametable"]);
    std::string val;
    std::size_t i = 0;
    while (std::getline(attrs, val, ';') && i < p.frames[0].attributes.size()) {
        int a = std::stoi(val);
        if (a < 0 || a > 3)
            throw std::runtime_error("Legacy attribute outside 0..3");
        p.frames[0].attributes[i++] = static_cast<Byte>(a);
    }
    if (!p.reference.empty())
        p.quantize();
    int layers = number("layer_count");
    if (layers < 0 || layers > 4096)
        throw std::runtime_error("Invalid legacy layer count");
    for (int n = 0; n < layers; ++n) {
        auto key = "layer_" + std::to_string(n);
        if (number(key + "_type") == 2) {
            auto s = p.make_sprite(number(key + "_tile_x"), number(key + "_tile_y"),
                                   number(key + "_pal"), number(key + "_screen_x"),
                                   number(key + "_screen_y"));
            s.name = values[key + "_name"];
            p.frames[0].sprites.push_back(std::move(s));
        }
    }
    p.validate();
    return p;
}
Project load_project(const std::filesystem::path &path) {
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in)
        throw std::runtime_error("Cannot open project: " + path.string());
    auto size = in.tellg();
    if (size <= 0 || size > 192 * 1024 * 1024)
        throw std::runtime_error("Invalid project size (maximum 192 MiB)");
    std::string text(static_cast<std::size_t>(size), '\0');
    in.seekg(0);
    in.read(text.data(), size);
    if (text.starts_with("\xEF\xBB\xBF"))
        text.erase(0, 3);
    auto start = text.find_first_not_of(" \t\r\n");
    if (start == std::string::npos)
        throw std::runtime_error("Empty project");
    if (text[start] != '{')
        return read_legacy(text, path);
    const auto j = Json::parse(text);
    if (j.at("format") != "tilezeitjobb" || j.at("version") != 2)
        throw std::runtime_error("Unsupported project format/version");
    Project p;
    p.resize(checked_int(j.at("width"), 256, 512), checked_int(j.at("height"), 240, 480));
    p.offset_x = checked_int(j.at("offset_x"), -4096, 4096);
    p.offset_y = checked_int(j.at("offset_y"), -4096, 4096);
    p.dither = j.at("dither");
    p.image_path = j.value("image_path", "");
    p.sprite_path = j.value("sprite_path", "");
    palettes_json(j.at("background"));
    palettes_json(j.at("sprite_palettes"));
    p.background = j.at("background").get<decltype(p.background)>();
    p.sprite_palettes = j.at("sprite_palettes").get<decltype(p.sprite_palettes)>();
    p.reference = read_image(j.at("reference"));
    p.sprite_sheet = read_image(j.at("sprite_sheet"));
    p.frames.clear();
    if (!j.at("frames").is_array() || j.at("frames").empty() || j.at("frames").size() > 256)
        throw std::runtime_error("Invalid frame count");
    for (const auto &jf : j.at("frames")) {
        Frame f;
        f.pixels = unhex(jf.at("pixels"), static_cast<std::size_t>(p.width) * p.height);
        auto mask = jf.value("paint_mask", std::string{});
        if (!mask.empty())
            f.paint_mask = unhex(mask, f.pixels.size());
        byte_array(jf.at("attributes"),
                   static_cast<std::size_t>(p.block_width()) * p.block_height(), 3);
        f.attributes = jf.at("attributes").get<std::vector<Byte>>();
        f.duration_ms = checked_int(jf.at("duration_ms"), 10, 60000);
        if (!jf.at("sprites").is_array() || jf.at("sprites").size() > 4096)
            throw std::runtime_error("Too many sprites");
        for (const auto &js : jf.at("sprites")) {
            Sprite s;
            s.name = js.at("name");
            s.x = checked_int(js.at("x"), -4096, 4096);
            s.y = checked_int(js.at("y"), -4096, 4096);
            s.palette = checked_int(js.at("palette"), 0, 3);
            s.tile_x = checked_int(js.at("tile_x"), 0, 511);
            s.tile_y = checked_int(js.at("tile_y"), 0, 511);
            s.visible = js.at("visible");
            s.flip_x = js.at("flip_x");
            s.flip_y = js.at("flip_y");
            s.behind = js.at("behind");
            byte_array(js.at("pixels"), 64, 3);
            s.pixels = js.at("pixels").get<Tile>();
            f.sprites.push_back(std::move(s));
        }
        p.frames.push_back(std::move(f));
    }
    p.validate();
    return p;
}
Project demo_project() {
    Project p;
    auto &f = p.frames[0];
    for (int y = 0; y < p.height; ++y)
        for (int x = 0; x < p.width; ++x) {
            f.attributes[(y / 16) * 16 + x / 16] = Byte((x / 64 + y / 64) % 4);
            f.pixels[y * p.width + x] = Byte(((x / 4) ^ (y / 4)) & 3);
        }
    return p;
}
void History::checkpoint(const Project &p) {
    if (!past_.empty() && past_.back() == p)
        return;
    past_.push_back(p);
    future_.clear();
    std::size_t bytes = 0;
    for (auto it = past_.rbegin(); it != past_.rend(); ++it) {
        bytes += it->reference.rgba.size() + it->sprite_sheet.rgba.size();
        for (const auto &f : it->frames)
            bytes += f.pixels.size() + f.paint_mask.size() + f.attributes.size() +
                     f.sprites.size() * sizeof(Sprite);
    }
    while (past_.size() > 1 && (past_.size() > 32 || bytes > 64 * 1024 * 1024)) {
        const auto &old = past_.front();
        bytes -= old.reference.rgba.size() + old.sprite_sheet.rgba.size();
        for (const auto &f : old.frames)
            bytes -= f.pixels.size() + f.paint_mask.size() + f.attributes.size() +
                     f.sprites.size() * sizeof(Sprite);
        past_.erase(past_.begin());
    }
}
bool History::undo(Project &p) {
    if (past_.empty())
        return false;
    future_.push_back(std::move(p));
    p = std::move(past_.back());
    past_.pop_back();
    return true;
}
bool History::redo(Project &p) {
    if (future_.empty())
        return false;
    past_.push_back(std::move(p));
    p = std::move(future_.back());
    future_.pop_back();
    return true;
}
} // namespace tzj
