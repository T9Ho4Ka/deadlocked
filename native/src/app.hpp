#pragma once

#include <array>
#include <string_view>
#include <utility>

#include "config/config.hpp"

struct GLFWwindow;
struct ImFont;

namespace dl {

enum class Tab : int {
    Aimbot,
    Player,
    Hud,
    Grenades,
    Unsafe,
    Radar,
    Config,
    Application,
};

inline constexpr std::array<std::pair<Tab, std::string_view>, 8> tabs{{
    {Tab::Aimbot, "Aimbot"},
    {Tab::Player, "Player"},
    {Tab::Hud, "Hud"},
    {Tab::Grenades, "Grenades"},
    {Tab::Unsafe, "Unsafe"},
    {Tab::Radar, "Radar"},
    {Tab::Config, "Config"},
    {Tab::Application, "Application"},
}};

inline constexpr float sidebar_width = 176.0f;

/// Everything the ui draws from. The game side of this, status and frame times included,
/// is still stubbed until the cs2 layer is ported.
struct AppState {
    config::Config config;
    Tab tab = Tab::Aimbot;
    /// set by any control that changes the theme, consumed once per frame
    bool style_dirty = false;
    /// set by any control that changes the config, triggers a save
    bool config_dirty = false;
    /// key of the text settings popup that is open, empty when none is
    std::string text_popup;
    /// one entry per config::Font, all loaded up front so switching needs no atlas rebuild
    std::array<ImFont*, config::font_count> fonts{};

    void mark_style_changed() {
        style_dirty = true;
        config_dirty = true;
    }
};

class App {
public:
    App() = default;
    App(const App&) = delete;
    App& operator=(const App&) = delete;
    ~App();

    /// Returns false and prints the reason when the window or gl context cannot be created.
    bool init();
    void run();

private:
    void frame();

    GLFWwindow* window_ = nullptr;
    AppState state_;
};

}  // namespace dl
