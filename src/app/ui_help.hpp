#pragma once
#include <cstdarg>
#include <imgui.h>
namespace tzj::ui {
void help(const char *label, const char *fallback = nullptr);
inline void TextUnformatted(const char *text, const char *end = nullptr) {
    ImGui::TextUnformatted(text, end);
    help(text);
}

inline void Text(const char *format, ...) {
    va_list args;
    va_start(args, format);
    ImGui::TextV(format, args);
    va_end(args);
    help(format,
         "This readout describes the current project or selection and updates as you edit.");
}
inline void TextDisabled(const char *format, ...) {
    va_list args;
    va_start(args, format);
    ImGui::TextDisabledV(format, args);
    va_end(args);
    help(format, "Additional information about the current tool, view or project.");
}

inline bool Button(const char *label, ImVec2 size = {0, 0}) {
    bool r = ImGui::Button(label, size);
    help(label);
    return r;
}
inline bool SmallButton(const char *label) {
    bool r = ImGui::SmallButton(label);
    help(label);
    return r;
}
inline bool Checkbox(const char *label, bool *v) {
    bool r = ImGui::Checkbox(label, v);
    help(label);
    return r;
}
inline bool Selectable(const char *label, bool selected = false, ImGuiSelectableFlags flags = 0,
                       ImVec2 size = {0, 0}) {
    bool r = ImGui::Selectable(label, selected, flags, size);
    help(label,
         "Select this entry to make it active. Painting and editing use the active selection.");
    return r;
}
inline bool SliderInt(const char *label, int *v, int min, int max, const char *format = "%d",
                      ImGuiSliderFlags flags = 0) {
    bool r = ImGui::SliderInt(label, v, min, max, format, flags);
    help(label);
    return r;
}
inline bool SliderFloat(const char *label, float *v, float min, float max,
                        const char *format = "%.3f", ImGuiSliderFlags flags = 0) {
    bool r = ImGui::SliderFloat(label, v, min, max, format, flags);
    help(label);
    return r;
}
inline bool InputInt(const char *label, int *v, int step = 1, int fast = 100,
                     ImGuiInputTextFlags flags = 0) {
    bool r = ImGui::InputInt(label, v, step, fast, flags);
    help(label);
    return r;
}
inline bool InputInt2(const char *label, int *v, ImGuiInputTextFlags flags = 0) {
    bool r = ImGui::InputInt2(label, v, flags);
    help(label);
    return r;
}
inline bool InputText(const char *label, char *v, size_t size, ImGuiInputTextFlags flags = 0,
                      ImGuiInputTextCallback callback = nullptr, void *data = nullptr) {
    bool r = ImGui::InputText(label, v, size, flags, callback, data);
    help(label);
    return r;
}
inline bool Combo(const char *label, int *v, const char *const items[], int count,
                  int height = -1) {
    bool r = ImGui::Combo(label, v, items, count, height);
    help(label);
    return r;
}
inline bool RadioButton(const char *label, bool active) {
    bool r = ImGui::RadioButton(label, active);
    help(label, "Choose this color set. A background area shares one set of four colors; a sprite "
                "uses three colors plus transparency.");
    return r;
}
inline bool ColorButton(const char *label, ImVec4 color, ImGuiColorEditFlags flags = 0,
                        ImVec2 size = {0, 0}) {
    bool r = ImGui::ColorButton(label, color, flags, size);
    help(label);
    return r;
}
inline bool CollapsingHeader(const char *label, ImGuiTreeNodeFlags flags = 0) {
    bool r = ImGui::CollapsingHeader(label, flags);
    help(label);
    return r;
}
inline bool MenuItem(const char *label, const char *shortcut = nullptr, bool selected = false,
                     bool enabled = true) {
    bool r = ImGui::MenuItem(label, shortcut, selected, enabled);
    help(label);
    return r;
}
inline bool MenuItem(const char *label, const char *shortcut, bool *selected, bool enabled = true) {
    bool r = ImGui::MenuItem(label, shortcut, selected, enabled);
    help(label);
    return r;
}
inline bool InvisibleButton(const char *label, ImVec2 size, ImGuiButtonFlags flags = 0) {
    bool r = ImGui::InvisibleButton(label, size, flags);
    help(label);
    return r;
}
inline void SeparatorText(const char *label) {
    ImGui::SeparatorText(label);
    help(label);
}
inline void Image(ImTextureID texture, ImVec2 size, ImVec2 uv0 = {0, 0}, ImVec2 uv1 = {1, 1},
                  ImVec4 tint = {1, 1, 1, 1}, ImVec4 border = {0, 0, 0, 0}) {
    ImGui::Image(texture, size, uv0, uv1, tint, border);
    help("Image preview");
}
} // namespace tzj::ui
