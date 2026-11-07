#pragma once
#include "core/project.hpp"
#include <utility>

namespace tzj {
struct TileEntry {
    Tile pixels{};
    std::vector<std::pair<int, int>> locations;
};
// Identical bit patterns share an exported CHR tile regardless of display palette.
std::vector<TileEntry> tile_catalog(const Project &, std::size_t frame);
Tile read_tile(const Project &, std::size_t frame, int tx, int ty);
void stamp_tile(Project &, std::size_t frame, int tx, int ty, const Tile &);
std::size_t replace_tile(Project &, std::size_t frame, const Tile &original,
                         const Tile &replacement);
void paint_tile_line(Project &, std::size_t frame, int tx0, int ty0, int tx1, int ty1,
                     const Tile &);
struct IndexedBrush {
    int width{}, height{};
    std::vector<Byte> pixels;
    bool operator==(const IndexedBrush &) const = default;
};
enum class DrawingShape { Line, Rectangle, Ellipse };
struct PixelPoint {
    int x{}, y{};
    bool operator==(const PixelPoint &) const = default;
};
// Returns clipped, unique pixels, shared by the live preview and the committed stroke.
std::vector<PixelPoint> drawing_pixels(DrawingShape, int x0, int y0, int x1, int y1, int width,
                                       int height, bool filled = false, int size = 1,
                                       bool mirror_x = false, bool mirror_y = false);
std::vector<PixelPoint> path_pixels(const std::vector<PixelPoint> &, int width, int height,
                                    bool closed = false, bool filled = false, int size = 1,
                                    bool mirror_x = false, bool mirror_y = false);
void flood_fill_sprite(Project &, std::size_t frame, std::size_t sprite, int x, int y, Byte color);
// Sprite points use displayed coordinates; flips are applied when writing storage.
void paint_pixels(Project &, std::size_t frame, const std::vector<PixelPoint> &, Byte color,
                  int palette = -1, int sprite = -1, bool erase = false);
enum class BrushTransform { FlipX, FlipY, RotateCW, RotateCCW };
IndexedBrush capture_brush(const Project &, std::size_t frame, int x, int y, int width, int height);
IndexedBrush transform_brush(const IndexedBrush &, BrushTransform);
void stamp_brush(Project &, std::size_t frame, int x, int y, const IndexedBrush &,
                 bool transparent_zero = false, bool wrap = false);
// Only slots 1..3 may be reordered: slot zero is backdrop/transparency on NES.
void swap_palette_slots(Project &, bool sprites, int palette, int first, int second,
                        bool preserve_artwork = true);
} // namespace tzj
