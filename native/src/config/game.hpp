#pragma once

#include <array>
#include <string>

#include "config/enums.hpp"
#include "config/keycode.hpp"
#include "config/text.hpp"
#include "ui/theme.hpp"

namespace dl::config {

using ui::Color;

/// defaults ported from cheat/src/constants.rs
namespace cs2 {
inline constexpr unsigned default_fov = 90;
inline constexpr float sound_esp_footstep_diameter = 2000.0f;
inline constexpr float sound_esp_gunshot_diameter = 3000.0f;
inline constexpr float sound_esp_weapon_diameter = 1000.0f;
}  // namespace cs2

enum class DrawMode { None, Health, Color, PlayerColor };

inline constexpr std::array<EnumEntry<DrawMode>, 4> draw_mode_entries{{
    {DrawMode::None, "None", "none"},
    {DrawMode::Health, "Health", "health"},
    {DrawMode::Color, "Color", "color"},
    {DrawMode::PlayerColor, "Player Color", "player_color"},
}};
constexpr const auto& enum_entries(DrawMode) { return draw_mode_entries; }

enum class SnaplineMode { None, Health, Distance, Color, PlayerColor };

inline constexpr std::array<EnumEntry<SnaplineMode>, 5> snapline_mode_entries{{
    {SnaplineMode::None, "None", "none"},
    {SnaplineMode::Health, "Health", "health"},
    {SnaplineMode::Distance, "Distance", "distance"},
    {SnaplineMode::Color, "Color", "color"},
    {SnaplineMode::PlayerColor, "Player Color", "player_color"},
}};
constexpr const auto& enum_entries(SnaplineMode) { return snapline_mode_entries; }

enum class SnaplineAnchor { Center, Bottom };

inline constexpr std::array<EnumEntry<SnaplineAnchor>, 2> snapline_anchor_entries{{
    {SnaplineAnchor::Center, "Center", "center"},
    {SnaplineAnchor::Bottom, "Bottom", "bottom"},
}};
constexpr const auto& enum_entries(SnaplineAnchor) { return snapline_anchor_entries; }

enum class BoxMode { Gap, Full };

inline constexpr std::array<EnumEntry<BoxMode>, 2> box_mode_entries{{
    {BoxMode::Gap, "Gap", "gap"},
    {BoxMode::Full, "Full", "full"},
}};
constexpr const auto& enum_entries(BoxMode) { return box_mode_entries; }

enum class ModelRenderMode { Filled, Wireframe };

inline constexpr std::array<EnumEntry<ModelRenderMode>, 2> model_render_mode_entries{{
    {ModelRenderMode::Filled, "Filled", "filled"},
    {ModelRenderMode::Wireframe, "Wireframe", "wireframe"},
}};
constexpr const auto& enum_entries(ModelRenderMode) { return model_render_mode_entries; }

enum class VisibilityMode { All, InvisibleOnly, VisibleOnly };

inline constexpr std::array<EnumEntry<VisibilityMode>, 3> visibility_mode_entries{{
    {VisibilityMode::All, "All", "all"},
    {VisibilityMode::InvisibleOnly, "Invisible Only", "invisible_only"},
    {VisibilityMode::VisibleOnly, "Visible Only", "visible_only"},
}};
constexpr const auto& enum_entries(VisibilityMode) { return visibility_mode_entries; }

struct SoundConfig {
    bool enabled = false;
    float footstep_diameter = cs2::sound_esp_footstep_diameter;
    float gunshot_diameter = cs2::sound_esp_gunshot_diameter;
    float weapon_diameter = cs2::sound_esp_weapon_diameter;
    float fadeout_start = 1.0f;
    float fadeout_duration = 1.0f;
    bool show_visible = true;
};

struct PlayerConfig {
    bool enabled = true;
    bool chicken = true;
    KeyCode esp_hotkey = KeyCode::X;
    bool show_friendlies = false;
    DrawMode draw_box = DrawMode::Color;
    BoxMode box_mode = BoxMode::Gap;
    Color box_visible_color = Color(255, 255, 255);
    Color box_invisible_color = Color(255, 0, 0);
    SnaplineMode snaplines = SnaplineMode::None;
    Color snapline_color = Color(128, 0, 128);
    SnaplineAnchor snapline_anchor = SnaplineAnchor::Center;
    DrawMode draw_skeleton = DrawMode::Health;
    Color skeleton_color = Color(255, 255, 255);
    DrawMode draw_model = DrawMode::Health;
    ModelRenderMode model_mode = ModelRenderMode::Filled;
    Color model_visible_color = Color(255, 255, 255, 127);
    Color model_invisible_color = Color(255, 0, 0, 127);
    bool head_circle = true;
    bool health_bar = true;
    bool armor_bar = true;
    bool player_name = true;
    bool weapon_icon = true;
    bool tags = true;
    VisibilityMode visibility = VisibilityMode::All;
    SoundConfig sound;
};

/// `misc` in the rust config, the tab is called Unsafe
struct UnsafeConfig {
    bool no_flash = false;
    float max_flash_alpha = 127.0f;
    bool fov_changer = false;
    unsigned desired_fov = cs2::default_fov;
    bool no_smoke = false;
    bool change_smoke_color = false;
    Color smoke_color = Color(255, 0, 0);
};

struct CrosshairConfig {
    bool enabled = true;
    Color color = Color(255, 255, 255);
    float line_length = 50.0f;
    float line_width = 2.0f;
    float gap = 20.0f;
};

struct TrailConfig {
    bool enabled = true;
    bool inferno_poly = true;
    Color smoke = Color(220, 220, 220);
    Color molotov = Color(255, 0, 0);
    Color incendiary = Color(255, 165, 0);
    Color flash = Color(255, 255, 255);
    Color he = Color(96, 96, 96);
    Color decoy = Color(128, 0, 128);
};

struct HudConfig {
    bool bomb_timer = true;
    bool fov_circle = false;
    CrosshairConfig sniper_crosshair;
    bool dropped_weapons = true;
    bool keybind_list = false;
    bool spectator_list = false;
    TrailConfig grenade_trails;
    bool text_outline = true;
    float line_width = 2.0f;
    bool debug = false;
    OverlayTextConfig overlay_text;
};

struct RadarConfig {
    bool enabled = true;
    std::string url = "relay.avitrano.com";
};

}  // namespace dl::config
