#include "core/project.hpp"
#define STB_IMAGE_IMPLEMENTATION
#define STBI_FAILURE_USERMSG
#include <stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <stb_image_write.h>
#include <stdexcept>
namespace tzj {
const std::array<Color, 64> nes_colors{
    {{84, 84, 84},    {0, 30, 116},    {8, 16, 144},    {48, 0, 136},    {68, 0, 100},
     {92, 0, 48},     {84, 4, 0},      {60, 24, 0},     {32, 42, 0},     {8, 58, 0},
     {0, 64, 0},      {0, 60, 0},      {0, 50, 60},     {0, 0, 0},       {0, 0, 0},
     {0, 0, 0},       {152, 150, 152}, {8, 76, 196},    {48, 50, 236},   {92, 30, 228},
     {136, 20, 176},  {160, 20, 100},  {152, 34, 32},   {120, 60, 0},    {84, 90, 0},
     {40, 114, 0},    {8, 124, 0},     {0, 118, 40},    {0, 102, 120},   {0, 0, 0},
     {0, 0, 0},       {0, 0, 0},       {236, 238, 236}, {76, 154, 236},  {120, 124, 236},
     {176, 98, 236},  {228, 84, 236},  {236, 88, 180},  {236, 106, 100}, {212, 136, 32},
     {160, 170, 0},   {116, 196, 0},   {76, 208, 32},   {56, 204, 108},  {56, 180, 204},
     {60, 60, 60},    {0, 0, 0},       {0, 0, 0},       {236, 238, 236}, {168, 204, 236},
     {188, 188, 236}, {212, 178, 236}, {236, 174, 236}, {236, 174, 212}, {236, 180, 176},
     {228, 196, 144}, {204, 210, 120}, {180, 222, 120}, {168, 226, 144}, {152, 226, 180},
     {160, 214, 228}, {160, 162, 160}, {0, 0, 0},       {0, 0, 0}}};
Color Image::at(int x, int y) const {
    if (x < 0 || y < 0 || x >= width || y >= height)
        return {0, 0, 0, 0};
    const auto i = (static_cast<std::size_t>(y) * width + x) * 4;
    return {rgba.at(i), rgba.at(i + 1), rgba.at(i + 2), rgba.at(i + 3)};
}
Image load_image(const std::filesystem::path &path) {
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in)
        throw std::runtime_error("Cannot open image: " + path.string());
    const auto size = in.tellg();
    if (size <= 0 || size > 64 * 1024 * 1024)
        throw std::runtime_error("Image file must be between 1 byte and 64 MiB");
    std::vector<Byte> bytes(static_cast<std::size_t>(size));
    in.seekg(0);
    in.read(reinterpret_cast<char *>(bytes.data()), size);
    int w = 0, h = 0, channels = 0;
    if (!stbi_info_from_memory(bytes.data(), static_cast<int>(bytes.size()), &w, &h, &channels) ||
        w < 1 || h < 1 || w > 4096 || h > 4096)
        throw std::runtime_error("Unsupported image or dimensions (maximum 4096 x 4096)");
    auto *pixels =
        stbi_load_from_memory(bytes.data(), static_cast<int>(bytes.size()), &w, &h, &channels, 4);
    if (!pixels)
        throw std::runtime_error(std::string("Image decode failed: ") + stbi_failure_reason());
    Image image{w, h, {}};
    try {
        image.rgba.assign(pixels, pixels + static_cast<std::size_t>(w) * h * 4);
    } catch (...) {
        stbi_image_free(pixels);
        throw;
    }
    stbi_image_free(pixels);
    return image;
}
void save_png(const std::filesystem::path &path, const Image &image) {
    if (image.empty())
        throw std::runtime_error("Cannot save an empty image");
    std::ofstream out(path, std::ios::binary);
    if (!out)
        throw std::runtime_error("Cannot write PNG: " + path.string());
    const auto callback = [](void *ctx, void *bytes, int size) {
        static_cast<std::ofstream *>(ctx)->write(static_cast<char *>(bytes), size);
    };
    if (!stbi_write_png_to_func(callback, &out, image.width, image.height, 4, image.rgba.data(),
                                image.width * 4) ||
        !out)
        throw std::runtime_error("PNG write failed");
}
std::vector<Byte> encode_png(const Image &image) {
    if (image.width < 1 || image.height < 1 || image.width > 4096 || image.height > 4096 ||
        image.rgba.size() != std::size_t(image.width) * image.height * 4)
        throw std::runtime_error("Invalid image dimensions or pixel buffer");
    std::vector<Byte> bytes;
    const auto callback = [](void *context, void *data, int size) {
        auto &out = *static_cast<std::vector<Byte> *>(context);
        const auto *start = static_cast<const Byte *>(data);
        out.insert(out.end(), start, start + size);
    };
    if (!stbi_write_png_to_func(callback, &bytes, image.width, image.height, 4,
                               image.rgba.data(), image.width * 4))
        throw std::runtime_error("PNG encoding failed");
    return bytes;
}
static int distance(Color a, Color b) {
    int r = int(a.r) - b.r, g = int(a.g) - b.g, bl = int(a.b) - b.b;
    return r * r * 3 + g * g * 6 + bl * bl * 2;
}
Byte nearest_nes(Color c) {
    int best = std::numeric_limits<int>::max();
    Byte index = 15;
    for (int i = 0; i < 64; ++i) {
        if ((i & 15) == 13 || (i & 15) == 14 || ((i & 15) == 15 && i != 15))
            continue;
        auto error = distance(c, nes_colors[i]);
        if (error < best) {
            best = error;
            index = static_cast<Byte>(i);
        }
    }
    return index;
}
Byte nearest_slot(Color c, const Palette &p, int x, int y, bool dither, bool transparent) {
    if (c.a < 128)
        return 0;
    if (transparent && distance(c, nes_colors[p[0]]) == 0)
        return 0;
    int best = std::numeric_limits<int>::max();
    Byte slot = 1;
    const int start = transparent ? 1 : 0;
    for (int i = start; i < 4; ++i) {
        const int err = distance(c, nes_colors[p[i]]);
        if (err < best) {
            best = err;
            slot = static_cast<Byte>(i);
        }
    }
    if (dither)
        for (int i = start; i < 4; ++i)
            for (int j = i + 1; j < 4; ++j) {
                const auto a = nes_colors[p[i]], b = nes_colors[p[j]];
                Color blend{Byte((int(a.r) + b.r) / 2), Byte((int(a.g) + b.g) / 2),
                            Byte((int(a.b) + b.b) / 2)};
                const int err = distance(c, blend);
                if (err < best) {
                    best = err;
                    slot = static_cast<Byte>(((x + y) & 1) ? i : j);
                }
            }
    return slot;
}
} // namespace tzj
