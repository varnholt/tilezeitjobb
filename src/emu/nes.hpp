#pragma once
#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace tzj::emu {
using Byte = std::uint8_t;
using Word = std::uint16_t;
// A small NTSC NROM preview core. It executes ROM code; it does not read editor data.
// No APU, unofficial opcodes, mapper switching or cycle-exact PPU fetch pipeline.
class Nes {
  public:
    struct Cpu {
        Word pc{};
        Byte a{}, x{}, y{}, sp{0xfd}, status{0x24};
        std::uint64_t cycles{};
    };
    void load(std::span<const Byte> rom);
    void reset();
    int step_instruction();
    void run_frame();
    bool loaded() const { return !prg_.empty(); }
    const Cpu &cpu() const { return cpu_; }
    std::uint64_t frames() const { return frames_; }
    const std::array<Byte, 256 * 240> &pixels() const { return pixels_; }
    // Read-only inspection for tests/debugging, with no register side effects.
    Byte ram(Word address) const { return ram_[address & 0x7ff]; }
    Byte vram(Word address) const;
    Byte ppu_control() const { return control_; }
    Byte ppu_status() const { return status_; }

  private:
    Cpu cpu_;
    std::vector<Byte> prg_;
    std::array<Byte, 8192> chr_{};
    std::array<Byte, 2048> ram_{}, nametables_{};
    std::array<Byte, 8192> cart_ram_{};
    std::array<Byte, 32> palette_{};
    std::array<Byte, 256> oam_{};
    std::array<Byte, 256 * 240> pixels_{};
    bool vertical_{}, chr_ram_{}, nmi_{}, nmi_line_{}, latch_{};
    Byte control_{}, mask_{}, status_{}, oam_address_{}, read_buffer_{}, open_bus_{};
    Word address_{}, temporary_{};
    int scroll_x_{}, scroll_y_{}, dot_{}, scanline_{261}, stall_{};
    std::array<int, 8> line_sprites_{};
    int sprite_count_{};
    std::uint64_t frames_{};
    Byte read(Word);
    void write(Word, Byte);
    Word read_word(Word);
    Byte fetch();
    Word fetch_word();
    void push(Byte);
    Byte pop();
    void flags(Byte);
    void flag(Byte, bool);
    void compare(Byte, Byte);
    void add(Byte);
    void interrupt(Word, bool);
    int cpu_step();
    int nametable_index(Word) const;
    void ppu_write(Word, Byte);
    void ppu_tick();
    void update_nmi();
    void evaluate_sprites();
    void render_pixel(int, int);
};
} // namespace tzj::emu
