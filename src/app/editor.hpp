#pragma once
#include "core/nes.hpp"
#include "core/tools.hpp"
#include "emu/nes.hpp"
#include <functional>
#include <map>
#include <memory>
#include <optional>
#ifndef __EMSCRIPTEN__
#include "mcp/server.hpp"
#endif
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
    void show_start() { welcome_ = true; }
    bool starting() const { return welcome_; }
    bool unsaved() const { return dirty_; }
#ifdef __EMSCRIPTEN__
    void browser_file(const std::filesystem::path &, int action);
#endif
    void start_workspace(std::string_view name);
    bool fullscreen() const { return fullscreen_; }
    void set_fullscreen(bool value) { fullscreen_ = value; }
    void set_status(std::string message) { status_ = std::move(message); }
    void start_nes_preview() {
        show_emulator_ = true;
        boot_emulator();
    }
#ifndef __EMSCRIPTEN__
    void configure_mcp(std::filesystem::path executable) {
        mcp_executable_ = std::move(executable);
    }
    void start_mcp();
#endif

  private:
#ifndef __EMSCRIPTEN__
    std::unique_ptr<mcp::Server> mcp_server_;
    std::filesystem::path mcp_executable_;
    bool show_mcp_{};
    void mcp_window();
    void poll_mcp();
#endif
    enum class Workspace { Draw, Tiles, Sprites, Animate, Export };
    bool show_tiles_{}, show_timeline_{}, show_hardware_{};
    bool show_emulator_{}, emulator_running_{}, emulator_auto_{true};
    std::unique_ptr<emu::Nes> emulator_;
    unsigned int emulator_texture_{};
    std::vector<Byte> emulator_rom_;
    std::string emulator_error_;
    std::uint64_t render_revision_{}, emulator_revision_{};
    double next_emulator_frame_{}, emulator_changed_at_{};
    void emulator_window();
    void boot_emulator();
    void upload_emulator();
    bool guidance_{true}, advanced_panels_{}, fullscreen_{};
    bool preview_native_size_{true};
    void choose_workspace(Workspace);
    struct PanelRect {
        float x{}, y{}, width{}, height{};
    };
    PanelRect paint_rect_, canvas_rect_, inspector_rect_, timeline_rect_;
    bool force_layout_{};
    int fit_countdown_{3};
    void guided_workspace();
    void place_panel(const PanelRect &);
    const char *tool_label() const;
    void new_project();
    void info_windows();
    bool show_project_info_{}, show_drawing_guide_{}, show_scene_preview_{}, show_brush_settings_{};
    void start_screen();
    void add_blank_sprite();
    bool welcome_{true}, focus_sprite_{};
    enum class Tool {
        Pencil,
        Eraser,
        Line,
        Rectangle,
        Ellipse,
        Polyline,
        Polygon,
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
    enum class FileAction {
        None,
        Open,
        Save,
        Background,
        Sprites,
        Export,
        Png,
        Brushes,
        FontSheet
    };
    bool show_brush_library_{}, show_bitmap_text_{}, show_gradients_{}, show_onion_settings_{};
    int library_selection_{-1}, glyph_{'A'}, text_spacing_{}, text_line_gap_{}, text_scale_{1};
    char asset_name_[129]{"New brush"}, ramp_name_[129]{"New color ramp"}, text_[1025]{"PLAYER 1"};
    bool text_recolor_{true}, glyph_stroke_{}, gradient_vertical_{true};
    int gradient_width_{32}, gradient_height_{32}, gradient_dither_{1};
    std::array<Byte, 4> gradient_order_{0, 1, 2, 3};
    int onion_previous_{1}, onion_following_{1}, onion_opacity_{35};
    bool onion_sprites_only_{true};
    bool custom_symmetry_{};
    float symmetry_x_{127.5f}, symmetry_y_{119.5f};
    int symmetry_sectors_{1};
    bool seamless_tiles_{true};
    int tile_last_x_{}, tile_last_y_{};
    void artist_windows();
    void brush_library();
    void bitmap_text();
    void gradient_editor();
    void use_brush(const IndexedBrush &);
    History history_;
    std::size_t frame_{};
    int layer_{-1}, palette_{}, slot_{1}, brush_{1}, grid_{8}, tile_x_{}, tile_y_{};
    int start_x_{}, start_y_{}, last_x_{}, last_y_{};
    Tool tool_{Tool::Pencil};
    bool shape_filled_{}, mirror_x_{}, mirror_y_{}, shape_drag_{}, shape_right_{};
    std::vector<PixelPoint> stroke_pixels(int x0, int y0, int x1, int y1, bool constrain) const;
    bool shape_tool() const;
    bool path_tool() const;
    void tool_icon(Tool, float x, float y) const;
    void cancel_drawing();
    void finish_path();
    std::vector<PixelPoint> path_preview(const std::vector<PixelPoint> &) const;
    std::vector<PixelPoint> path_;
    Tool path_kind_{Tool::Polyline};
    std::size_t path_frame_{};
    int path_layer_{-1};
    void paint_stroke(const std::vector<PixelPoint> &, bool right);
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
    void tiles(bool embedded = false, bool *open = nullptr);
    void brush_controls(bool embedded = false, bool *open = nullptr);
    void transform_captured_brush(BrushTransform);
    void edit_selected_tile(const Tile &);
    void changed();
    void checkpoint();
    void guard(std::function<void()>);
    void file_dialog(FileAction);
    void save(bool as = false);
    void refresh_textures();
    void menus();
    void tools(bool embedded = false);
    void palettes(bool embedded = false);
    void palette_editor();
    bool palette_editor_open_{};
    int palette_edit_group_{}, palette_edit_index_{};
    void canvas();
    void layers(bool embedded = false);
    void layer_stack();
    void layer_properties();
    void layer_action(int action, int index, bool sprite);
    bool target_editable(bool warn = true);
    std::optional<std::filesystem::file_time_type> layer_watch_time_;
    double next_layer_watch_{};
    std::map<std::string, std::filesystem::file_time_type> layer_source_times_;
    void preview(bool *open = nullptr);
    void timeline(bool *open = nullptr);
    void hardware(bool embedded = false, bool *open = nullptr);
    void attributes();
    void sprite_sheet(bool embedded = false);
    void palette_effect();
    void popups();
    void copy_selection(bool cut = false);
    void apply_tool(int x, int y, bool first, bool right);
    void perform_file(const std::filesystem::path &);
    void dock_layout();
};
} // namespace tzj
