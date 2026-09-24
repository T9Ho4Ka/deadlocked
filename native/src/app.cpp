#include "app.hpp"

#include <cstdio>
#include <filesystem>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <GLFW/glfw3.h>

#include "ui/sidebar.hpp"
#include "ui/tabs.hpp"
#include "ui/theme.hpp"
#include "ui/widgets.hpp"

namespace dl {
namespace {

constexpr int window_width = 750;
constexpr int window_height = 450;

void glfw_error(int code, const char* description) {
    std::fprintf(stderr, "glfw error %d: %s\n", code, description);
}

const char* platform_name(int platform) {
    switch (platform) {
        case GLFW_PLATFORM_WAYLAND: return "wayland";
        case GLFW_PLATFORM_X11: return "x11";
        case GLFW_PLATFORM_NULL: return "null";
        default: return "unknown";
    }
}

/// The rust client ships the fonts, so reuse them rather than duplicating the assets. All
/// of them are loaded once, so switching fonts later is just a PushFont and never an atlas
/// rebuild. Any that cannot be read fall back to the built in font.
void load_fonts(std::array<ImFont*, config::font_count>& fonts, float size) {
    ImGuiIO& io = ImGui::GetIO();
    ImFont* fallback = io.Fonts->AddFontDefault();

    for (std::size_t i = 0; i < config::font_count; ++i) {
        fonts[i] = fallback;
#ifdef DL_ASSETS_DIR
        const std::filesystem::path path =
            std::filesystem::path(DL_ASSETS_DIR) / std::string(config::font_files[i]);
        std::error_code error;
        if (!std::filesystem::exists(path, error)) {
            std::fprintf(stderr, "font %s missing, using the built in one\n", path.c_str());
            continue;
        }
        if (ImFont* loaded = io.Fonts->AddFontFromFileTTF(path.c_str(), size);
            loaded != nullptr) {
            fonts[i] = loaded;
        }
#else
        (void)size;
#endif
    }
}

}  // namespace

App::~App() {
    if (window_ != nullptr) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        glfwDestroyWindow(window_);
        glfwTerminate();
    }
}

bool App::init() {
    glfwSetErrorCallback(glfw_error);
    if (glfwInit() == GLFW_FALSE) {
        std::fprintf(stderr, "could not initialise glfw\n");
        return false;
    }

    const int platform = glfwGetPlatform();
    std::fprintf(stderr, "glfw %s, platform: %s\n", glfwGetVersionString(),
                 platform_name(platform));
    if (platform == GLFW_PLATFORM_NULL) {
        // glfw creates windows on the null platform happily, they are just never shown,
        // which looks exactly like the program hanging with no output
        std::fprintf(stderr,
                     "no display backend: glfw fell back to the null platform, so the window\n"
                     "would never appear. is DISPLAY or WAYLAND_DISPLAY set?\n");
        glfwTerminate();
        return false;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    window_ = glfwCreateWindow(window_width, window_height, "deadlocked", nullptr, nullptr);
    if (window_ == nullptr) {
        std::fprintf(stderr, "could not create a window, is a display available?\n");
        glfwTerminate();
        return false;
    }
    glfwMakeContextCurrent(window_);
    glfwSwapInterval(1);
    std::fprintf(stderr, "gl %s on %s\n",
                 reinterpret_cast<const char*>(glGetString(GL_VERSION)),
                 reinterpret_cast<const char*>(glGetString(GL_RENDERER)));

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr;

    state_.config = config::load();
    std::fprintf(stderr, "config: %s\n", config::config_path().c_str());
    load_fonts(state_.fonts, ui::body_size * state_.config.theme.text_scale);
    ui::apply(state_.config.theme, state_.config.accent_color);

    ImGui_ImplGlfw_InitForOpenGL(window_, true);
    ImGui_ImplOpenGL3_Init("#version 330");
    std::fprintf(stderr, "window ready, close it to quit\n");
    return true;
}

void App::run() {
    while (glfwWindowShouldClose(window_) == GLFW_FALSE) {
        glfwPollEvents();
        if (glfwGetWindowAttrib(window_, GLFW_ICONIFIED) != 0) {
            continue;
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        frame();

        ImGui::Render();
        int width = 0;
        int height = 0;
        glfwGetFramebufferSize(window_, &width, &height);
        glViewport(0, 0, width, height);
        const ui::Color clear = state_.config.theme.palette().backdrop;
        glClearColor(clear.r / 255.0f, clear.g / 255.0f, clear.b / 255.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window_);

        if (state_.config_dirty) {
            state_.config_dirty = false;
            if (!config::save(state_.config)) {
                std::fprintf(stderr, "could not write %s\n", config::config_path().c_str());
            }
        }
    }
}

void App::frame() {
    if (state_.style_dirty) {
        state_.style_dirty = false;
        ui::apply(state_.config.theme, state_.config.accent_color);
    }

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ui::paint_gradient(state_.config.theme, viewport->WorkPos,
                       ImVec2(viewport->WorkPos.x + viewport->WorkSize.x,
                              viewport->WorkPos.y + viewport->WorkSize.y));

    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    constexpr ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    // 0.0f keeps the size the style already resolved, only the typeface changes
    ImGui::PushFont(state_.fonts[static_cast<std::size_t>(state_.config.font)], 0.0f);
    if (ImGui::Begin("##root", nullptr, flags)) {
        ui::draw_sidebar(state_);
        ImGui::SameLine();
        if (ImGui::BeginChild("##content", ImVec2(0.0f, 0.0f))) {
            ui::draw_tab(state_);
        }
        ImGui::EndChild();
    }
    ImGui::End();

    if (ui::text_settings_popup(state_.config.hud.overlay_text, state_.text_popup)) {
        state_.config_dirty = true;
    }

    ImGui::PopFont();
    ImGui::PopStyleVar();
}

}  // namespace dl
