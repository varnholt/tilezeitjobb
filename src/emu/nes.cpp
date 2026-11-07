#include "emu/nes.hpp"
#include <algorithm>
#include <stdexcept>

namespace tzj::emu {
void Nes::load(std::span<const Byte> rom) {
    if (rom.size() < 16 || rom[0] != 'N' || rom[1] != 'E' || rom[2] != 'S' || rom[3] != 0x1a)
        throw std::runtime_error("NES preview requires an iNES ROM");
    if ((rom[7] & 0x0c) == 8 || (rom[6] >> 4) || (rom[7] & 0xf0) || (rom[6] & 0x08) || (rom[7] & 3))
        throw std::runtime_error("NES preview supports standard mapper-0 ROMs only");
    if ((rom[4] != 1 && rom[4] != 2) || rom[5] > 1)
        throw std::runtime_error("NROM requires 16/32 KiB PRG and at most 8 KiB CHR");
    const std::size_t trainer = (rom[6] & 4) ? 512 : 0, offset = 16 + trainer,
                      prg_size = rom[4] * 16384;
    if (rom.size() < offset + prg_size + rom[5] * 8192)
        throw std::runtime_error("Truncated NES ROM");
    std::vector<Byte> prg(rom.begin() + offset, rom.begin() + offset + prg_size);
    prg_ = std::move(prg);
    vertical_ = (rom[6] & 1) != 0;
    chr_ram_ = rom[5] == 0;
    chr_.fill(0);
    if (!chr_ram_)
        std::copy_n(rom.begin() + offset + prg_size, 8192, chr_.begin());
    reset();
    if (trainer)
        std::copy_n(rom.begin() + 16, 512, cart_ram_.begin() + 0x1000);
}
void Nes::reset() {
    if (!loaded())
        return;
    cpu_ = {};
    ram_.fill(0);
    cart_ram_.fill(0);
    nametables_.fill(0);
    palette_.fill(15);
    if (chr_ram_)
        chr_.fill(0);
    oam_.fill(255);
    pixels_.fill(15);
    control_ = mask_ = status_ = oam_address_ = read_buffer_ = open_bus_ = 0;
    address_ = temporary_ = 0;
    scroll_x_ = scroll_y_ = dot_ = stall_ = sprite_count_ = 0;
    scanline_ = 261;
    frames_ = 0;
    nmi_ = nmi_line_ = latch_ = false;
    cpu_.pc = read_word(0xfffc);
    cpu_.cycles = 7;
    for (int i = 0; i < 21; ++i)
        ppu_tick();
}
Byte Nes::read(Word address) {
    if (address < 0x2000)
        return ram_[address & 0x7ff];
    if (address < 0x4000) {
        switch (address & 7) {
        case 2: {
            Byte value = Byte((status_ & 0xe0) | (open_bus_ & 0x1f));
            status_ &= 0x7f;
            latch_ = false;
            update_nmi();
            open_bus_ = value;
            return value;
        }
        case 4:
            return open_bus_ = oam_[oam_address_];
        case 7: {
            auto addr = Word(address_ & 0x3fff);
            Byte value = read_buffer_;
            read_buffer_ = vram(addr);
            if (addr >= 0x3f00) {
                value = Byte((vram(addr) & 0x3f) | (open_bus_ & 0xc0));
                read_buffer_ = vram(addr - 0x1000);
            }
            address_ = Word((address_ + ((control_ & 4) ? 32 : 1)) & 0x7fff);
            return open_bus_ = value;
        }
        default:
            return open_bus_;
        }
    }
    if (address >= 0x6000 && address < 0x8000)
        return cart_ram_[address - 0x6000];
    if (address >= 0x8000)
        return prg_[(address - 0x8000) % prg_.size()];
    // No audio or input in the artwork ROM preview. Unconnected controllers are released.
    return address == 0x4016 || address == 0x4017 ? 0x40 : 0;
}
void Nes::write(Word address, Byte value) {
    if (address < 0x2000) {
        ram_[address & 0x7ff] = value;
        return;
    }
    if (address < 0x4000) {
        open_bus_ = value;
        switch (address & 7) {
        case 0:
            control_ = value;
            temporary_ = Word((temporary_ & 0xf3ff) | ((value & 3) << 10));
            update_nmi();
            break;
        case 1:
            mask_ = value;
            break;
        case 3:
            oam_address_ = value;
            break;
        case 4:
            oam_[oam_address_++] = value;
            break;
        case 5:
            if (!latch_) {
                scroll_x_ = value;
                temporary_ = Word((temporary_ & 0x7fe0) | (value >> 3));
            } else {
                scroll_y_ = value;
                temporary_ =
                    Word((temporary_ & 0x0c1f) | ((value & 7) << 12) | ((value & 0xf8) << 2));
            }
            latch_ = !latch_;
            break;
        case 6:
            if (!latch_)
                temporary_ = Word((temporary_ & 0xff) | ((value & 0x3f) << 8));
            else {
                temporary_ = Word((temporary_ & 0x7f00) | value);
                address_ = temporary_;
            }
            latch_ = !latch_;
            break;
        case 7:
            ppu_write(address_, value);
            address_ = Word((address_ + ((control_ & 4) ? 32 : 1)) & 0x7fff);
            break;
        default:
            break;
        }
        return;
    }
    if (address == 0x4014) {
        for (int i = 0; i < 256; ++i)
            oam_[oam_address_++] = read(Word((unsigned(value) << 8) | i));
        stall_ += 513 + int(cpu_.cycles & 1);
    } else if (address >= 0x6000 && address < 0x8000)
        cart_ram_[address - 0x6000] = value;
}
Word Nes::read_word(Word address) {
    Byte low = read(address);
    return Word(low | (read(Word(address + 1)) << 8));
}
int Nes::step_instruction() {
    if (!loaded())
        throw std::runtime_error("Load a ROM before running NES preview");
    int cycles = cpu_step();
    cycles += stall_;
    stall_ = 0;
    cpu_.cycles += cycles;
    for (int i = 0; i < cycles * 3; ++i)
        ppu_tick();
    return cycles;
}
void Nes::run_frame() {
    auto target = frames_ + 1;
    for (int instructions = 0; instructions < 40000; ++instructions) {
        step_instruction();
        if (frames_ >= target)
            return;
    }
    throw std::runtime_error("NES preview exceeded its frame execution budget");
}
} // namespace tzj::emu
