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
    if (argc > 1 && std::string_view(argv[1]) == "--mcp-connect") {
        try {
            if (argc != 4)
                throw std::runtime_error("Usage: tilezeitjobb --mcp-connect PORT TOKEN");
            std::size_t consumed{};
            auto port = std::stoul(argv[2], &consumed);
            if (consumed != std::string(argv[2]).size() || port < 1 || port > 65535)
                throw std::runtime_error("Invalid MCP port");
            return tzj::mcp::connect_stdio(static_cast<unsigned short>(port), argv[3]);
        } catch (const std::exception &e) {
            std::cerr << e.what() << '\n';
            return 1;
        }
    }
    bool smoke = false, demo = false, start_fullscreen = false, start_mcp = false,
         nes_preview = false;
    std::filesystem::path input, screenshot;
    std::string workspace = "draw";
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--smoke-test")
            smoke = true;
        else if (arg == "--mcp")
            start_mcp = true;
        else if (arg == "--nes-preview")
            nes_preview = true;
        else if (arg == "--fullscreen")
            start_fullscreen = true;
        else if (arg == "--workspace" && i + 1 < argc)
            workspace = argv[++i];
        else if (arg == "--demo")
            demo = true;
        else if (arg == "--screenshot" && i + 1 < argc)
            screenshot = argv[++i];
        else if (arg == "--help") {
            std::cout
                << "tilezeitjobb [PROJECT.tzj] [--demo] [--fullscreen] [--workspace "
                   "draw|tiles|sprites|animate|export|brushes|text|gradients|onion] [--smoke-test] "
                   "[--screenshot FILE.png] [--mcp] [--nes-preview]\n"
                   "tilezeitjobb --mcp-connect PORT TOKEN\n";
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
    else
        glfwWindowHint(GLFW_MAXIMIZED, GLFW_TRUE);
    GLFWwindow *window =
        glfwCreateWindow(1440, 960, "tilezeitjobb | NES pixel studio", nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        return 1;
    }
    glfwSetWindowSizeLimits(window, 1040, 680, GLFW_DONT_CARE, GLFW_DONT_CARE);
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
    font.SizePixels = 17;
    io.Fonts->AddFontDefault(&font);
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");
    int result = 0;
    try {
        tzj::Editor editor(executable_directory(argv[0]) / "data");
        editor.configure_mcp(executable_directory(argv[0]) /
                             std::filesystem::path(argv[0]).filename());
        editor.set_fullscreen(start_fullscreen);
        bool fullscreen = false, was_maximized = false;
        int window_x = 0, window_y = 0, window_width = 1440, window_height = 960;
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
        editor.start_workspace(workspace);
        if (start_mcp)
            editor.start_mcp();
        if (nes_preview)
            editor.start_nes_preview();
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
            if (ImGui::IsKeyPressed(ImGuiKey_F11, false))
                editor.set_fullscreen(!editor.fullscreen());
            if (!smoke && fullscreen != editor.fullscreen()) {
                if (editor.fullscreen()) {
                    glfwGetWindowPos(window, &window_x, &window_y);
                    glfwGetWindowSize(window, &window_width, &window_height);
                    was_maximized = glfwGetWindowAttrib(window, GLFW_MAXIMIZED) == GLFW_TRUE;
                    GLFWmonitor *monitor = glfwGetPrimaryMonitor();
                    int count = 0;
                    auto monitors = glfwGetMonitors(&count);
                    long long largest = -1;
                    for (int i = 0; i < count; ++i) {
                        int x, y;
                        glfwGetMonitorPos(monitors[i], &x, &y);
                        auto *mode = glfwGetVideoMode(monitors[i]);
                        long long overlap =
                            static_cast<long long>(
                                std::max(0, std::min(window_x + window_width, x + mode->width) -
                                                std::max(window_x, x))) *
                            std::max(0, std::min(window_y + window_height, y + mode->height) -
                                            std::max(window_y, y));
                        if (overlap > largest) {
                            largest = overlap;
                            monitor = monitors[i];
                        }
                    }
                    if (auto *mode = glfwGetVideoMode(monitor))
                        glfwSetWindowMonitor(window, monitor, 0, 0, mode->width, mode->height,
                                             mode->refreshRate);
                } else {
                    glfwSetWindowMonitor(window, nullptr, window_x, window_y, window_width,
                                         window_height, GLFW_DONT_CARE);
                    if (was_maximized)
                        glfwMaximizeWindow(window);
                }
                fullscreen = editor.fullscreen();
            }
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
