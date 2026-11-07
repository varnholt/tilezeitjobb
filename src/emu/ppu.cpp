#include "emu/nes.hpp"
namespace tzj::emu {
int Nes::nametable_index(Word address) const {
    int offset = (address - 0x2000) & 0xfff, table = offset / 0x400;
    return ((vertical_ ? table & 1 : table >> 1) * 0x400) + (offset & 0x3ff);
}
Byte Nes::vram(Word address) const {
    address &= 0x3fff;
    if (address < 0x2000)
        return chr_[address];
    if (address < 0x3f00)
        return nametables_[nametable_index(address)];
    int offset = address & 0x1f;
    if ((offset & 0x13) == 0x10)
        offset -= 16;
    return palette_[offset];
}
void Nes::ppu_write(Word address, Byte value) {
    address &= 0x3fff;
    if (address < 0x2000) {
        if (chr_ram_)
            chr_[address] = value;
    } else if (address < 0x3f00)
        nametables_[nametable_index(address)] = value;
    else {
        int offset = address & 0x1f;
        if ((offset & 0x13) == 0x10)
            offset -= 16;
        palette_[offset] = value & 0x3f;
    }
}
void Nes::update_nmi() {
    bool line = (status_ & 0x80) && (control_ & 0x80);
    if (line && !nmi_line_)
        nmi_ = true;
    nmi_line_ = line;
}
void Nes::evaluate_sprites() {
    sprite_count_ = 0;
    if (!(mask_ & 0x18))
        return;
    int height = (control_ & 0x20) ? 16 : 8;
    for (int i = 0; i < 64; ++i) {
        int top = int(oam_[i * 4]) + 1;
        if (scanline_ < top || scanline_ >= top + height)
            continue;
        if (sprite_count_ == 8) {
            status_ |= 0x20;
            break;
        }
        line_sprites_[sprite_count_++] = i;
    }
}
void Nes::render_pixel(int x, int y) {
    Byte background = 0, background_palette = 0;
    if ((mask_ & 8) && (x >= 8 || (mask_ & 2))) {
        int sx = x + scroll_x_ + ((control_ & 1) ? 256 : 0),
            sy = y + scroll_y_ + ((control_ & 2) ? 240 : 0);
        int table = ((sx / 256) & 1) | (((sy / 240) & 1) << 1), tx = (sx % 256) / 8,
            ty = (sy % 240) / 8;
        Word base = Word(0x2000 + table * 0x400);
        int tile = vram(Word(base + ty * 32 + tx)), row = (sy % 240) & 7, bit = 7 - (sx & 7);
        Word pattern = Word(((control_ & 0x10) ? 0x1000 : 0) + tile * 16 + row);
        background = Byte(((vram(pattern) >> bit) & 1) | (((vram(pattern + 8) >> bit) & 1) << 1));
        auto attribute = vram(Word(base + 0x3c0 + (ty / 4) * 8 + tx / 4));
        background_palette = Byte((attribute >> (((ty & 2) << 1) | (tx & 2))) & 3);
    }
    Byte color =
        background ? vram(Word(0x3f00 + background_palette * 4 + background)) : vram(0x3f00);
    if ((mask_ & 0x10) && (x >= 8 || (mask_ & 4))) {
        int height = (control_ & 0x20) ? 16 : 8;
        for (int n = 0; n < sprite_count_; ++n) {
            int i = line_sprites_[n], left = oam_[i * 4 + 3], sx = x - left;
            if (sx < 0 || sx >= 8)
                continue;
            int sy = y - int(oam_[i * 4]) - 1;
            Byte attr = oam_[i * 4 + 2], tile = oam_[i * 4 + 1];
            if (attr & 0x40)
                sx = 7 - sx;
            if (attr & 0x80)
                sy = height - 1 - sy;
            int bank = (control_ & 8) ? 0x1000 : 0;
            if (height == 16) {
                bank = (tile & 1) * 0x1000;
                tile = Byte((tile & 0xfe) + (sy / 8));
            }
            Word pattern = Word(bank + tile * 16 + (sy & 7));
            int bit = 7 - sx;
            Byte pixel =
                Byte(((vram(pattern) >> bit) & 1) | (((vram(pattern + 8) >> bit) & 1) << 1));
            if (!pixel)
                continue;
            if (i == 0 && background && x != 255)
                status_ |= 0x40;
            if (!background || !(attr & 0x20))
                color = vram(Word(0x3f10 + (attr & 3) * 4 + pixel));
            break; // Lower OAM index wins, including a sprite behind opaque background.
        }
    }
    pixels_[y * 256 + x] = Byte(color & ((mask_ & 1) ? 0x30 : 0x3f));
}
void Nes::ppu_tick() {
    if (scanline_ == 261 && dot_ == 1) {
        status_ &= 0x1f;
        update_nmi();
    }
    if (scanline_ == 241 && dot_ == 1) {
        status_ |= 0x80;
        update_nmi();
    }
    if (scanline_ < 240) {
        if (dot_ == 1)
            evaluate_sprites();
        if (dot_ >= 1 && dot_ <= 256)
            render_pixel(dot_ - 1, scanline_);
    }
    // NTSC skips the last pre-render dot on odd frames when rendering is enabled.
    if (scanline_ == 261 && dot_ == 339 && (frames_ & 1) && (mask_ & 0x18)) {
        dot_ = 0;
        scanline_ = 0;
        ++frames_;
        return;
    }
    if (++dot_ == 341) {
        dot_ = 0;
        if (++scanline_ == 262) {
            scanline_ = 0;
            ++frames_;
        }
    }
}
} // namespace tzj::emu
