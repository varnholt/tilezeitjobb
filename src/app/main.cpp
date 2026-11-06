#include "app/editor.hpp"
#ifdef _WIN32
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

#include <GLFW/glfw3.h>
#include <algorithm>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <iostream>
#include <string>
static std::filesystem::path executable_directory(const char *argv0) {
#ifdef _WIN32
    std::wstring buffer(32768, L'\0');
    auto length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (length > 0 && length < buffer.size()) {
        buffer.resize(length);
        return std::filesystem::path(buffer).parent_path();
    }
#elif defined(__APPLE__)
    std::uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    std::string buffer(size, '\0');
    if (_NSGetExecutablePath(buffer.data(), &size) == 0)
        return std::filesystem::weakly_canonical(buffer.c_str()).parent_path();
#else
    std::error_code ec;
    auto path = std::filesystem::read_symlink("/proc/self/exe", ec);
    if (!ec)
        return path.parent_path();
#endif
    return std::filesystem::absolute(argv0).parent_path();
}
static void glfw_error(int code, const char *message) {
    std::cerr << "GLFW " << code << ": " << message << "\n";
}
int main(int argc, char **argv) {
    bool smoke = false, demo = false;
    std::filesystem::path input, screenshot;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--smoke-test")
            smoke = true;
        else if (arg == "--demo")
            demo = true;
        else if (arg == "--screenshot" && i + 1 < argc)
            screenshot = argv[++i];
        else if (arg == "--help") {
            std::cout
                << "tilezeitjobb [PROJECT.tzj] [--demo] [--smoke-test] [--screenshot FILE.png]\n";
            return 0;
        } else
            input = argv[i];
    }
    glfwSetErrorCallback(glfw_error);
    if (!glfwInit())
        return 1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    if (smoke)
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    GLFWwindow *window =
        glfwCreateWindow(1440, 960, "tilezeitjobb | NES pixel studio", nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    auto &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable | ImGuiConfigFlags_NavEnableKeyboard;
    if (smoke)
        io.IniFilename = nullptr;
    ImGui::StyleColorsDark();
    auto &style = ImGui::GetStyle();
    style.WindowRounding = 3;
    style.FrameRounding = 3;
    style.GrabRounding = 3;
    style.WindowPadding = {12, 12};
    style.FramePadding = {8, 5};
    style.Colors[ImGuiCol_WindowBg] = {.065f, .08f, .10f, 1};
    style.Colors[ImGuiCol_Header] = {.14f, .28f, .31f, 1};
    style.Colors[ImGuiCol_HeaderHovered] = {.18f, .42f, .44f, 1};
    style.Colors[ImGuiCol_Button] = {.15f, .25f, .29f, 1};
    style.Colors[ImGuiCol_ButtonHovered] = {.19f, .40f, .43f, 1};
    style.Colors[ImGuiCol_CheckMark] = {.47f, .84f, .73f, 1};
    style.Colors[ImGuiCol_SliderGrab] = {.47f, .84f, .73f, 1};
    ImFontConfig font;
    font.SizePixels = 15;
    io.Fonts->AddFontDefault(&font);
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");
    int result = 0;
    try {
        tzj::Editor editor(executable_directory(argv[0]) / "data");
        glfwSetWindowUserPointer(window, &editor);
        glfwSetDropCallback(window, [](GLFWwindow *w, int count, const char **paths) {
            if (count > 0)
                static_cast<tzj::Editor *>(glfwGetWindowUserPointer(w))
                    ->drop(tzj::path_from_utf8(paths[0]));
        });
        if (demo)
            editor.set_demo();
        if (!input.empty())
            editor.open(input);
        int frames = 0;
        bool running = true;
        while (running) {
            glfwPollEvents();
            if (glfwWindowShouldClose(window)) {
                glfwSetWindowShouldClose(window, false);
                editor.request_close();
            }
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();
            running = editor.tick();
            ImGui::Render();
            int width, height;
            glfwGetFramebufferSize(window, &width, &height);
            glViewport(0, 0, width, height);
            glClearColor(.04f, .05f, .065f, 1);
            glClear(GL_COLOR_BUFFER_BIT);
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            if (++frames == 8 && !screenshot.empty()) {
                tzj::Image image{width, height, std::vector<tzj::Byte>(width * height * 4)};
                glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, image.rgba.data());
                for (int y = 0; y < height / 2; ++y)
                    for (int x = 0; x < width * 4; ++x)
                        std::swap(image.rgba[y * width * 4 + x],
                                  image.rgba[(height - 1 - y) * width * 4 + x]);
                tzj::save_png(screenshot, image);
            }
            glfwSwapBuffers(window);
            if (smoke && frames >= 8)
                break;
        }
    } catch (const std::exception &e) {
        std::cerr << e.what() << "\n";
        result = 1;
    }
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return result;
}
