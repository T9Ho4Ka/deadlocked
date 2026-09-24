#pragma once

#include <array>
#include <string_view>
#include <utility>

#include <chrono>
#include <optional>

#include "config/config.hpp"
#include "cs2/features.hpp"
#include "cs2/game.hpp"
#include "os/mouse.hpp"
#include "cs2/snapshot.hpp"
#include "overlay/esp.hpp"
#include "overlay/trails.hpp"
#include "overlay/window.hpp"

struct GLFWwindow;
struct ImFont;
struct ImGuiContext;

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

/// the aimbot tab edits either the shared settings or one weapon's override
enum class AimbotTab : int { Global, Weapon };

inline constexpr float sidebar_width = 176.0f;

/// Everything the ui draws from. The game side of this, status and frame times included,
/// is still stubbed until the cs2 layer is ported.
struct AppState {
    config::Config config;
    Tab tab = Tab::Aimbot;
    AimbotTab aimbot_tab = AimbotTab::Global;
    config::Weapon aimbot_weapon = config::Weapon::AK47;
    /// set by any control that changes the theme, consumed once per frame
    bool style_dirty = false;
    /// set by any control that changes the config, triggers a save
    bool config_dirty = false;
    /// key of the text settings popup that is open, empty when none is
    std::string text_popup;
    /// one entry per config::Font, all loaded up front so switching needs no atlas rebuild
    std::array<ImFont*, config::font_count> fonts{};

    /// the weapon block the aimbot tab is currently editing
    config::WeaponConfig& weapon_config() {
        return aimbot_tab == AimbotTab::Weapon ? config.aim[aimbot_weapon] : config.aim.global;
    }

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
    /// Keeps the game attached, ticks it, and refreshes the snapshot the overlay draws.
    void update_game();
    void draw_overlay();


    GLFWwindow* window_ = nullptr;
    /// one imgui context per window: the backends bind to a single window each
    ImGuiContext* settings_context_ = nullptr;
    ImGuiContext* overlay_context_ = nullptr;

    overlay::Window overlay_;
    overlay::Trails trails_;
    std::optional<cs2::Game> game_;
    cs2::Features features_;
    os::Mouse mouse_;
    cs2::Snapshot snapshot_;
    std::chrono::steady_clock::time_point last_attach_{};

    AppState state_;
};

}  // namespace dl
