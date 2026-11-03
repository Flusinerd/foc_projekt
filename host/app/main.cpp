// FOC Studio - host tool for the FOC framework (Dear ImGui + ImPlot, GLFW, OpenGL 3).
//
//   focstudio                  normal start
//   focstudio --demo           connect to the virtual drive and run a demo profile
//   focstudio --screenshot f.ppm --frames N [--size WxH]   render N frames, save last frame, exit
#include "App.hpp"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <implot.h>

#include <GLFW/glfw3.h>

#include <chrono>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

namespace {

void glfwError(int code, const char* desc) { std::fprintf(stderr, "GLFW %d: %s\n", code, desc); }

void applyStyle(float scale)
{
    ImGui::StyleColorsDark();
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding = 4.0f;
    s.FrameRounding = 3.0f;
    s.GrabRounding = 3.0f;
    s.TabRounding = 3.0f;
    s.FramePadding = ImVec2(6, 4);
    s.ItemSpacing = ImVec2(8, 6);
    ImVec4* c = s.Colors;
    c[ImGuiCol_WindowBg] = ImVec4(0.105f, 0.11f, 0.12f, 1.0f);
    c[ImGuiCol_ChildBg] = ImVec4(0.09f, 0.095f, 0.105f, 1.0f);
    c[ImGuiCol_TitleBgActive] = ImVec4(0.16f, 0.29f, 0.48f, 1.0f);
    c[ImGuiCol_Header] = ImVec4(0.20f, 0.32f, 0.50f, 0.55f);
    c[ImGuiCol_TableRowBgAlt] = ImVec4(1, 1, 1, 0.03f);
    s.ScaleAllSizes(scale);
    ImPlot::StyleColorsDark();
}

ImFont* loadFonts(float px, ImFont** mono)
{
    ImGuiIO& io = ImGui::GetIO();
    const char* sans[] = {
        "C:\\Windows\\Fonts\\segoeui.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/TTF/DejaVuSans.ttf",
        "/System/Library/Fonts/SFNS.ttf",
    };
    const char* monos[] = {
        "C:\\Windows\\Fonts\\consola.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
        "/usr/share/fonts/TTF/DejaVuSansMono.ttf",
        "/System/Library/Fonts/Menlo.ttc",
    };
    ImFont* main = nullptr;
    for (const char* f : sans)
        if (std::ifstream(f).good() && (main = io.Fonts->AddFontFromFileTTF(f, px))) break;
    if (!main) { ImFontConfig cfg; cfg.SizePixels = px; main = io.Fonts->AddFontDefault(&cfg); }
    *mono = nullptr;
    for (const char* f : monos)
        if (std::ifstream(f).good() && (*mono = io.Fonts->AddFontFromFileTTF(f, px))) break;
    return main;
}

bool savePpm(const char* path, int w, int h)
{
    std::vector<unsigned char> px(size_t(w) * h * 3);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, px.data());
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    f << "P6\n" << w << ' ' << h << "\n255\n";
    for (int y = h - 1; y >= 0; --y) f.write(reinterpret_cast<const char*>(&px[size_t(y) * w * 3]), w * 3);
    return true;
}

} // namespace

int main(int argc, char** argv)
{
    bool demo = false;
    const char* shot = nullptr;
    const char* focus = nullptr;
    int frames = 0, width = 1600, height = 960;
    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--demo")) demo = true;
        else if (!std::strcmp(argv[i], "--screenshot") && i + 1 < argc) shot = argv[++i];
        else if (!std::strcmp(argv[i], "--frames") && i + 1 < argc) frames = std::atoi(argv[++i]);
        else if (!std::strcmp(argv[i], "--size") && i + 1 < argc) std::sscanf(argv[++i], "%dx%d", &width, &height);
        else if (!std::strcmp(argv[i], "--focus") && i + 1 < argc) focus = argv[++i];   // e.g. "Parameter"
    }

    glfwSetErrorCallback(glfwError);
    if (!glfwInit()) return 1;
    const char* glsl = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
#ifdef __APPLE__
    glsl = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
    GLFWwindow* win = glfwCreateWindow(width, height, "FOC Studio", nullptr, nullptr);
    if (!win) { glfwTerminate(); return 1; }
    glfwMakeContextCurrent(win);
    glfwSwapInterval(1);

    float scale = 1.0f;
    glfwGetWindowContentScale(win, &scale, nullptr);
    if (scale < 1.0f) scale = 1.0f;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.IniFilename = shot ? nullptr : "focstudio.ini";
    applyStyle(scale);
    ImFont* mono = nullptr;
    loadFonts(16.0f * scale, &mono);

    ImGui_ImplGlfw_InitForOpenGL(win, true);
    ImGui_ImplOpenGL3_Init(glsl);

    App app;
    app.setFont(mono);

    int frame = 0;
    while (!glfwWindowShouldClose(win) && !app.wantsQuit()) {
        glfwPollEvents();
        if (glfwGetWindowAttrib(win, GLFW_ICONIFIED)) { std::this_thread::sleep_for(std::chrono::milliseconds(20)); continue; }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        if (demo && frame == 1) app.startDemo();
        app.frame();
        if (focus && frame == 2) ImGui::SetWindowFocus(focus);
        ImGui::Render();

        int w, h;
        glfwGetFramebufferSize(win, &w, &h);
        glViewport(0, 0, w, h);
        glClearColor(0.07f, 0.075f, 0.08f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        ++frame;
        if (shot && frame >= frames) {
            const bool ok = savePpm(shot, w, h);
            std::printf("%s %s (%dx%d)\n", ok ? "saved" : "FAILED", shot, w, h);
            break;
        }
        glfwSwapBuffers(win);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();
    glfwDestroyWindow(win);
    glfwTerminate();
    return 0;
}
