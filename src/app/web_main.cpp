#include "app/editor.hpp"
#include <GLES3/gl3.h>
#include <GLFW/glfw3.h>
#include <emscripten.h>
#include <emscripten/html5.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <iostream>
#include <memory>
namespace {
GLFWwindow *window{};
std::unique_ptr<tzj::Editor> editor;
double next_autosave{};
EM_JS(void, sync_storage, (), {
    FS.syncfs(
        false, function(error) {
            if (error)
                console.warn(error);
        });
});
EM_JS(void, web_fullscreen, (), { Module.toggleFullscreen(); });
void tick() {
    double width = 0, height = 0;
    emscripten_get_element_css_size("#canvas", &width, &height);
    int w = 0, h = 0;
    glfwGetWindowSize(window, &w, &h);
    if (w != int(width) || h != int(height))
        glfwSetWindowSize(window, int(width), int(height));
    glfwPollEvents();
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    if (editor->fullscreen()) {
        web_fullscreen();
        editor->set_fullscreen(false);
    }
    editor->tick();
    ImGui::Render();
    glfwGetFramebufferSize(window, &w, &h);
    glViewport(0, 0, w, h);
    glClearColor(.04f, .05f, .065f, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    glfwSwapBuffers(window);
    if (!editor->starting() && emscripten_get_now() > next_autosave) {
        next_autosave = emscripten_get_now() + 10000;
        try {
            tzj::save_project("/projects/autosave.tzj", editor->project);
            sync_storage();
        } catch (const std::exception &e) {
            std::cerr << e.what() << "\n";
        }
    }
}
} // namespace
extern "C" {
EMSCRIPTEN_KEEPALIVE void tzj_web_file(int action, const char *path) {
    if (editor)
        editor->browser_file(tzj::path_from_utf8(path), action);
}
EMSCRIPTEN_KEEPALIVE int tzj_web_unsaved() {
    return editor && editor->unsaved();
}
}
int main() {
    if (!glfwInit())
        return 1;
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    window = glfwCreateWindow(1280, 800, "tilezeitjobb", nullptr, nullptr);
    if (!window)
        return 1;
    glfwMakeContextCurrent(window);
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    auto &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.IniFilename = "/projects/imgui.ini";
    ImGui::StyleColorsDark();
    auto &style = ImGui::GetStyle();
    style.WindowRounding = 3;
    style.FrameRounding = 3;
    style.WindowPadding = {12, 12};
    style.FramePadding = {8, 5};
    style.Colors[ImGuiCol_WindowBg] = {.065f, .08f, .10f, 1};
    style.Colors[ImGuiCol_Header] = {.14f, .28f, .31f, 1};
    style.Colors[ImGuiCol_Button] = {.15f, .25f, .29f, 1};
    style.Colors[ImGuiCol_ButtonHovered] = {.19f, .40f, .43f, 1};
    style.Colors[ImGuiCol_CheckMark] = {.47f, .84f, .73f, 1};
    ImFontConfig font;
    font.SizePixels = 17;
    io.Fonts->AddFontDefault(&font);
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 300 es");
    editor = std::make_unique<tzj::Editor>("/data");
    editor->set_demo();
    editor->show_start();
    try {
        if (std::filesystem::exists("/projects/autosave.tzj")) {
            editor->open("/projects/autosave.tzj");
            editor->set_status("Restored browser autosave");
        }
    } catch (const std::exception &e) {
        editor->set_status(std::string("Could not restore autosave: ") + e.what());
    }
    editor->start_workspace("draw");
    emscripten_set_main_loop(tick, 0, true);
    return 0;
}
