#pragma once

#include <array>
#include <filesystem>
#include <string_view>
#include <utility>

#include "config/aim.hpp"
#include "config/font.hpp"
#include "config/game.hpp"
#include "ui/theme.hpp"

namespace dl::config {

using ui::Color;

/// the accent colors the user can pick, ported from the rust ui::color::Colors
inline constexpr std::array<std::pair<std::string_view, Color>, 7> accent_colors{{
    {"Red", Color(240, 100, 100)},
    {"Orange", Color(240, 140, 90)},
    {"Yellow", Color(240, 200, 120)},
    {"Green", Color(160, 240, 130)},
    {"Teal", Color(80, 200, 200)},
    {"Blue", Color(100, 150, 240)},
    {"Purple", Color(180, 120, 240)},
}};

/// Root config of the native client. Only the parts the ui needs exist so far, the game
/// side of the config lands here as the cs2 layer gets ported.
struct Config {
    Color accent_color = Color(100, 150, 240);
    ui::ThemeConfig theme;
    AimConfig aim;
    PlayerConfig player;
    HudConfig hud;
    UnsafeConfig misc;
    RadarConfig radar;
    Font font = Font::FiraSans;
    unsigned fps = 120;
    /// the rust client keeps this in its application config, it moves there once that is ported
    std::string radar_uuid;
};

/// Random uuid v4, used for the radar session id.
std::string new_uuid();

/// Config file of the native client. Deliberately a different file from the rust client's
/// deadlocked.toml, so a half ported native build cannot corrupt a working config.
std::filesystem::path config_path();

/// Returns a default config when the file is missing or unreadable, never throws.
Config load();
bool save(const Config& config);

}  // namespace dl::config
