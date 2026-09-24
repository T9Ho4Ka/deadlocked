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
#include "net/radar_frame.hpp"
#include "overlay/esp.hpp"
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
    radar_.stop();
    if (overlay_context_ != nullptr) {
        ImGui::SetCurrentContext(overlay_context_);
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext(overlay_context_);
        overlay_context_ = nullptr;
        ImGui::SetCurrentContext(settings_context_);
    }
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

    // x11 first: the overlay has to place its own window over the game, and wayland does
    // not let a client do that. under a wayland session this goes through xwayland, which
    // is also what the rust client relies on.
    glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);
    bool initialised = glfwInit() == GLFW_TRUE;
    if (!initialised) {
        std::fprintf(stderr, "x11 is unavailable, the overlay will not be able to follow "
                             "the game window\n");
        glfwInitHint(GLFW_PLATFORM, GLFW_ANY_PLATFORM);
        initialised = glfwInit() == GLFW_TRUE;
    }
    if (!initialised) {
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

    // the mouse is optional: without it the esp still works, only the aim does not
    std::string reason;
    if (!os::uinput_available(reason)) {
        std::fprintf(stderr, "no mouse control: %s\n", reason.c_str());
    } else if (const std::string error = mouse_.open(); !error.empty()) {
        std::fprintf(stderr, "no mouse control: %s\n", error.c_str());
    } else {
        std::fprintf(stderr, "virtual mouse ready\n");
    }

    state_.available_configs = config::available_configs();
    state_.current_config = config::config_path();
    state_.config = config::load();
    std::fprintf(stderr, "config: %s\n", state_.current_config.c_str());
    load_fonts(state_.fonts, ui::body_size * state_.config.theme.text_scale);
    ui::apply(state_.config.theme, state_.config.accent_color);

    ImGui_ImplGlfw_InitForOpenGL(window_, true);
    ImGui_ImplOpenGL3_Init("#version 330");
    settings_context_ = ImGui::GetCurrentContext();

    // the overlay gets a window and an imgui context of its own. it is optional: without a
    // compositor or an x11 display there is no overlay, but the settings still work.
    if (overlay_.create()) {
        overlay_context_ = ImGui::CreateContext();
        ImGui::SetCurrentContext(overlay_context_);
        ImGui::GetIO().IniFilename = nullptr;
        ImGui::GetIO().MouseDrawCursor = false;
        ImGui::GetIO().Fonts->AddFontDefault();
        ImGui_ImplGlfw_InitForOpenGL(overlay_.handle(), false);
        ImGui_ImplOpenGL3_Init("#version 330");
        ImGui::SetCurrentContext(settings_context_);
        // the model renderer draws with raw gl into the overlay's context, so it is
        // created there and is optional like the overlay itself
        if (const std::string error = models_.create(); !error.empty()) {
            std::fprintf(stderr, "no player models: %s\n", error.c_str());
        }
        std::fprintf(stderr, "overlay window ready\n");
    }

    updates_.start();
    radar_.start();
    radar_.configure(state_.config.radar.enabled, state_.config.radar.url,
                     state_.config.radar_uuid);

    std::fprintf(stderr, "window ready, close it to quit\n");
    return true;
}

void App::run() {
    while (glfwWindowShouldClose(window_) == GLFW_FALSE) {
        glfwPollEvents();
        if (glfwGetWindowAttrib(window_, GLFW_ICONIFIED) != 0) {
            continue;
        }

        update_game();
        draw_overlay();

        glfwMakeContextCurrent(window_);
        ImGui::SetCurrentContext(settings_context_);
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
            if (!config::save_to(state_.config, state_.current_config)) {
                std::fprintf(stderr, "could not write %s\n", state_.current_config.c_str());
            }
        }
    }
}

void App::update_game() {
    // attaching reads the whole schema, so a failed attempt is not retried every frame
    constexpr auto attach_interval = std::chrono::seconds(2);
    const auto now = std::chrono::steady_clock::now();

    if (game_.has_value() && !game_->running()) {
        std::fprintf(stderr, "the game went away\n");
        game_.reset();
        snapshot_.clear();
    }

    if (!game_.has_value()) {
        if (now - last_attach_ < attach_interval) {
            return;
        }
        last_attach_ = now;
        game_ = cs2::Game::attach();
        if (game_.has_value()) {
            std::fprintf(stderr, "attached to cs2, pid %d\n", game_->process().pid());
        }
        return;
    }

    game_->tick();
    features_.run(*game_, state_.config, mouse_);
    game_->build_snapshot(snapshot_);
    trails_.update(snapshot_);

    // the radar thread sends whatever the newest frame is, so encoding one it never gets
    // to is only wasted when the link is slower than the game
    radar_.configure(state_.config.radar.enabled, state_.config.radar.url,
                     state_.config.radar_uuid);
    if (state_.config.radar.enabled && snapshot_.in_game) {
        radar_.submit(net::encode_radar_frame(snapshot_));
    }
}

void App::draw_overlay() {
    if (overlay_context_ == nullptr || !overlay_.alive()) {
        return;
    }

    // the overlay follows the game's window, and hides itself when there is nothing to draw
    overlay_.follow(snapshot_.window_position, snapshot_.window_size);

    ImGui::SetCurrentContext(overlay_context_);
    overlay_.begin_frame();
    if (snapshot_.in_game && features_.esp_enabled(state_.config)) {
        const overlay::FeatureState features{features_.aimbot_active(),
                                             features_.triggerbot_active()};
        overlay::draw_esp(snapshot_, state_.config, features, trails_);

        // the models go through gl directly, so they are drawn before imgui's own pass
        if (state_.config.player.draw_model != config::DrawMode::None && models_.ready()) {
            const config::PlayerConfig& player = state_.config.player;
            for (const cs2::PlayerData& data : snapshot_.players) {
                models_.draw(data.model_name, data.skeleton, snapshot_.view_matrix,
                             player.model_visible_color, player.model_invisible_color,
                             player.model_mode);
            }
        }
    }
    overlay_.end_frame();
    ImGui::SetCurrentContext(settings_context_);
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

    state_.radar_status = radar_.status();
    state_.update = updates_.result();

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
