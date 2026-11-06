#pragma once
#include "core/nes.hpp"
#include "core/tools.hpp"
#include <functional>
#include <optional>
namespace tzj {
class Editor {
  public:
    Project project;
    Editor(std::filesystem::path data_directory = {});
    ~Editor();
    bool tick();
    void open(const std::filesystem::path &);
    void drop(const std::filesystem::path &);
    void request_close();
    void set_demo();
    bool fullscreen() const { return fullscreen_; }
    void set_fullscreen(bool value) { fullscreen_ = value; }
    void set_status(std::string message) { status_ = std::move(message); }

  private:
    enum class Workspace { Draw, Tiles, Sprites, Animate, Export };
    Workspace workspace_{Workspace::Draw};
    bool guidance_{true}, advanced_panels_{}, fullscreen_{};
    void choose_workspace(Workspace);
    enum class Tool {
        Pencil,
        Eraser,
        Fill,
        Palette,
        Picker,
        Sprite,
        Move,
        Select,
        Stamp,
        TileBrush,
        Pan
    };
    enum class FileAction { None, Open, Save, Background, Sprites, Export, Png };
    History history_;
    std::size_t frame_{};
    int layer_{-1}, palette_{}, slot_{1}, brush_{1}, grid_{8}, tile_x_{}, tile_y_{};
    int start_x_{}, start_y_{}, last_x_{}, last_y_{};
    Tool tool_{Tool::Pencil};
    bool dirty_{}, refresh_{true}, quit_{}, stroke_{}, playing_{}, show_sprites_{true},
        hardware_preview_{}, onion_{}, highlight_{}, reference_overlay_{}, pal_test_{};
    bool layout_built_{}, focus_workflows_{true};
    bool watch_sources_{true};
    double next_watch_{};
    std::filesystem::path watched_background_, watched_sprites_;
    std::optional<std::filesystem::file_time_type> background_time_, sprite_time_;
    void watch_sources();
    float zoom_{2.0f};
    double next_frame_{};
    std::filesystem::path data_directory_;
    std::vector<Image> effect_images_;
    unsigned int effect_texture_{};
    bool effect_loaded_{}, focus_hardware_{true};
    unsigned int texture_{}, sheet_texture_{}, reference_texture_{}, previous_texture_{};
    std::optional<NesScene> compiled_;
    std::string compile_error_, status_{"Ready"}, error_;
    std::filesystem::path project_path_;
    std::function<void()> pending_;
    FileAction file_action_{FileAction::None};
    char file_path_[2048]{};
    std::filesystem::path browse_dir_;
    bool file_popup_{};
    struct Selection {
        int x{}, y{}, width{}, height{};
    };
    Selection selection_;
    std::vector<Byte> clipboard_;
    int clip_width_{}, clip_height_{};
    bool brush_transparent_{true}, brush_wrap_{}, brush_snap_{};
    int palette_group_{}, swap_first_{1}, swap_second_{2};
    bool preserve_palette_art_{true};
    std::optional<Palette> copied_palette_;
    std::vector<TileEntry> tile_catalog_;
    Tile selected_tile_{};
    bool tile_selected_{}, edit_linked_tiles_{true}, tile_edit_stroke_{};
    int selected_tile_x_{}, selected_tile_y_{};
    void tiles();
    void brush_controls();
    void transform_captured_brush(BrushTransform);
    void edit_selected_tile(const Tile &);
    void changed();
    void checkpoint();
    void guard(std::function<void()>);
    void file_dialog(FileAction);
    void save(bool as = false);
    void refresh_textures();
    void menus();
    void tools();
    void palettes();
    void canvas();
    void layers();
    void preview();
    void timeline();
    void hardware();
    void attributes();
    void sprite_sheet();
    void palette_effect();
    void popups();
    void copy_selection(bool cut = false);
    void apply_tool(int x, int y, bool first, bool right);
    void perform_file(const std::filesystem::path &);
    void dock_layout();
};
} // namespace tzj
