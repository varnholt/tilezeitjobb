#include "app/editor.hpp"
#include "app/ui_help.hpp"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cmath>
#include <imgui.h>
namespace tzj {
void Editor::upload_emulator() {
    Image image{256, 240, std::vector<Byte>(256 * 240 * 4)};
    const auto &pixels = emulator_->pixels();
    for (std::size_t i = 0; i < pixels.size(); ++i) {
        auto color = nes_colors[pixels[i]];
        image.rgba[i * 4] = color.r;
        image.rgba[i * 4 + 1] = color.g;
        image.rgba[i * 4 + 2] = color.b;
        image.rgba[i * 4 + 3] = 255;
    }
    if (!emulator_texture_)
        glGenTextures(1, &emulator_texture_);
    glBindTexture(GL_TEXTURE_2D, emulator_texture_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 240, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 image.rgba.data());
}
void Editor::boot_emulator() {
    try {
        auto rom = build_rom(compile_scene(project, frame_));
        auto candidate = std::make_unique<emu::Nes>();
        candidate->load(rom);
        // Let the cartridge's own startup routine wait for vblank and upload its scene.
        for (int i = 0; i < 8; ++i)
            candidate->run_frame();
        emulator_ = std::move(candidate);
        emulator_rom_ = std::move(rom);
        emulator_revision_ = render_revision_;
        emulator_error_.clear();
        emulator_running_ = true;
        next_emulator_frame_ = ImGui::GetTime() + 1.0 / 60.0988;
        upload_emulator();
    } catch (const std::exception &e) {
        emulator_error_ = e.what();
        emulator_revision_ = render_revision_;
        // Keep the last successfully booted ROM and its framebuffer available.
    }
}
void Editor::emulator_window() {
    if (!show_emulator_)
        return;
    ImGui::SetNextWindowSize({570, std::min(730.f, ImGui::GetMainViewport()->WorkSize.y - 50)},
                             ImGuiCond_FirstUseEver);
    if (ImGui::Begin("NES preview", &show_emulator_)) {
        if (ui::Button("Run current artwork"))
            boot_emulator();
        ImGui::SameLine();
        ImGui::BeginDisabled(!emulator_);
        if (ui::Button(emulator_running_ ? "Pause" : "Resume")) {
            emulator_running_ = !emulator_running_;
            next_emulator_frame_ = ImGui::GetTime();
        }
        ImGui::SameLine();
        bool step = ui::Button("Step frame");
        ImGui::SameLine();
        bool reset = ui::Button("Reset ROM");
        ImGui::EndDisabled();
        ui::Checkbox("Rebuild after edits", &emulator_auto_);
        bool drawing = stroke_ || shape_drag_ || tile_edit_stroke_ || !path_.empty() ||
                       ImGui::IsMouseDown(0) || ImGui::IsMouseDown(1);
        if (emulator_ && emulator_revision_ != render_revision_ && compiled_ &&
            build_rom(*compiled_) == emulator_rom_)
            emulator_revision_ = render_revision_;
        if (emulator_auto_ && emulator_revision_ != render_revision_ && !drawing &&
            ImGui::GetTime() - emulator_changed_at_ > .25) {
            if (compiled_ && !emulator_rom_.empty() && build_rom(*compiled_) == emulator_rom_)
                emulator_revision_ = render_revision_;
            else {
                const bool was_running = emulator_running_, had_rom = bool(emulator_);
                boot_emulator();
                if (had_rom)
                    emulator_running_ = was_running;
            }
        }
        try {
            if (emulator_) {
                bool update = false;
                if (reset) {
                    emulator_->reset();
                    for (int i = 0; i < 8; ++i)
                        emulator_->run_frame();
                    update = true;
                    next_emulator_frame_ = ImGui::GetTime() + 1.0 / 60.0988;
                }
                if (step) {
                    emulator_running_ = false;
                    emulator_->run_frame();
                    update = true;
                }
                if (emulator_running_ && ImGui::GetTime() >= next_emulator_frame_) {
                    int count = 0;
                    do {
                        emulator_->run_frame();
                        next_emulator_frame_ += 1.0 / 60.0988;
                    } while (++count < 3 && ImGui::GetTime() >= next_emulator_frame_);
                    if (count == 3)
                        next_emulator_frame_ = ImGui::GetTime() + 1.0 / 60.0988;
                    update = true;
                }
                if (update)
                    upload_emulator();
            }
        } catch (const std::exception &e) {
            emulator_error_ = e.what();
            emulator_running_ = false;
        }
        if (!emulator_error_.empty()) {
            ImGui::TextWrapped("Cannot rebuild: %s", emulator_error_.c_str());
            if (emulator_)
                ui::TextDisabled("Showing the last working ROM.");
        } else if (emulator_revision_ != render_revision_ && emulator_)
            ui::TextDisabled(emulator_auto_ ? "Artwork changed. Rebuild pending."
                                            : "Artwork changed. Run again to rebuild the ROM.");
        if (emulator_) {
            ui::Text("NROM / NTSC | Frame %llu | %s",
                     static_cast<unsigned long long>(emulator_->frames()),
                     emulator_running_ ? "Running" : "Paused");
            auto available = ImGui::GetContentRegionAvail();
            int scale =
                std::max(1, int(std::floor(std::min(available.x / 256.f,
                                                    std::max(240.f, available.y - 36) / 240.f))));
            ImGui::Image(static_cast<ImTextureID>(emulator_texture_),
                         {256.f * scale, 240.f * scale});
            ui::help("Emulated screen", "The framebuffer comes from executing the exported ROM. "
                                        "Nearest-neighbor integer scaling keeps NES pixels sharp.");
        } else
            ImGui::TextWrapped("Run the current artwork to boot its exported ROM here.");
        ImGui::TextDisabled("Artwork preview: mapper 0, no audio or input.");
    }
    ImGui::End();
}
} // namespace tzj
