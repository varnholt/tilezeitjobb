#pragma once
#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace tzj {
using Byte = std::uint8_t;
inline std::filesystem::path path_from_utf8(std::string_view text) {
    return std::filesystem::path(std::u8string(text.begin(), text.end()));
}
inline std::string path_to_utf8(const std::filesystem::path &path) {
    auto text = path.u8string();
    return std::string(text.begin(), text.end());
}
using Tile = std::array<Byte, 64>;
using Palette = std::array<Byte, 4>;
struct Color {
    Byte r{}, g{}, b{}, a{255};
    bool operator==(const Color &) const = default;
};
extern const std::array<Color, 64> nes_colors;
struct Image {
    int width{}, height{};
    std::vector<Byte> rgba;
    bool empty() const { return rgba.empty(); }
    Color at(int x, int y) const;
    bool operator==(const Image &) const = default;
};
Image load_image(const std::filesystem::path &);
void save_png(const std::filesystem::path &, const Image &);
std::vector<Byte> encode_png(const Image &);
Byte nearest_nes(Color);
Byte nearest_slot(Color, const Palette &, int x, int y, bool dither, bool transparent = false);
enum class LayerKind { Paint, Image };
enum class DitherMode { None, Bayer2, Bayer4, Bayer8, FloydSteinberg, Atkinson };
struct ImageModifiers {
    bool transform_enabled{true}, color_enabled{true}, dither_enabled{true};
    int crop_x{}, crop_y{}, crop_width{}, crop_height{};
    int output_width{}, output_height{}, offset_x{}, offset_y{};
    int brightness{}, contrast{}, strength{100};
    DitherMode dither{DitherMode::Bayer4};
    bool operator==(const ImageModifiers &) const = default;
};
struct BackgroundLayer {
    std::string name{"Paint layer"};
    LayerKind kind{LayerKind::Paint};
    bool visible{true}, locked{}, effects_enabled{true};
    // Paint pixels: 0..3 are opaque NES slots, 255 is transparent.
    std::vector<Byte> pixels;
    Image source;
    std::string source_path;
    ImageModifiers modifiers;
    // Derived pixels and signature are caches, not project content.
    std::vector<Byte> converted;
    std::uint64_t conversion_stamp{};
    bool operator==(const BackgroundLayer &other) const;
};
struct Sprite {
    std::string name{"Sprite"};
    int x{}, y{1}, palette{}, tile_x{}, tile_y{};
    bool visible{true}, flip_x{}, flip_y{}, behind{}, locked{};
    Tile pixels{};
    bool operator==(const Sprite &) const = default;
};
struct Frame {
    std::vector<Byte> pixels, attributes;
    std::vector<Sprite> sprites;
    int duration_ms{120};
    std::vector<Byte> paint_mask;        // 255: reference pixel; 0..3: painted color slot.
    std::vector<BackgroundLayer> layers; // Bottom to top; sprite objects stay above this group.
    int active_layer{-1};
    bool background_visible{true}, background_locked{}, sprites_visible{true}, sprites_locked{};
    bool operator==(const Frame &) const = default;
};
struct Project {
    int width{256}, height{240}, offset_x{}, offset_y{};
    bool dither{true};
    std::string image_path, sprite_path;
    Image reference, sprite_sheet;
    std::array<Palette, 4> background{
        {{15, 1, 33, 48}, {15, 6, 22, 38}, {15, 9, 25, 41}, {15, 12, 28, 44}}};
    std::array<Palette, 4> sprite_palettes{
        {{15, 22, 39, 48}, {15, 1, 33, 48}, {15, 9, 25, 41}, {15, 6, 22, 38}}};
    std::vector<Frame> frames;
    Project();
    int block_width() const { return width / 16; }
    int block_height() const { return height / 16; }
    void resize(int w, int h);
    void validate() const;
    void quantize(std::size_t frame = 0, bool auto_palettes = false);
    Sprite make_sprite(int tx, int ty, int palette, int x, int y) const;
    void generate_sprites(std::size_t frame, int palette);
    Image render(std::size_t frame = 0, bool sprites = true, bool hardware_limits = false) const;
    void set_backdrop(Byte color);
    void ensure_layers(std::size_t frame);
    void compose_layers(std::size_t frame, bool force = false);
    Frame composed_frame(std::size_t frame) const;
    std::size_t add_paint_layer(std::size_t frame, std::string name = "Paint layer");
    std::size_t add_image_layer(std::size_t frame, Image, std::string name,
                                std::string source_path = {});
    std::size_t duplicate_layer(std::size_t frame, std::size_t layer, bool paintable = false);
    void merge_layer_down(std::size_t frame, std::size_t layer);
    void remove_layer(std::size_t frame, std::size_t layer);
    void move_layer(std::size_t frame, std::size_t layer, std::size_t destination);
    void fit_layer_colors(std::size_t frame, std::size_t layer);
    bool paint_layer_editable(std::size_t frame) const;
    const std::vector<Byte> &active_pixels(std::size_t frame) const;
    void write_background_pixel(std::size_t frame, std::size_t position, Byte color,
                                int palette = -1);
    void clear_active_layer(std::size_t frame);
    bool operator==(const Project &) const = default;
};
void paint_line(Project &, std::size_t frame, int x0, int y0, int x1, int y1, Byte color, int size,
                int palette = -1);
void flood_fill(Project &, std::size_t frame, int x, int y, Byte color, int palette = -1,
                bool erase = false);
void save_project(const std::filesystem::path &, const Project &);
Project load_project(const std::filesystem::path &);
Project demo_project();
class History {
    std::vector<Project> past_, future_;

  public:
    void checkpoint(const Project &);
    bool undo(Project &);
    bool redo(Project &);
    void clear() {
        past_.clear();
        future_.clear();
    }
};
} // namespace tzj
