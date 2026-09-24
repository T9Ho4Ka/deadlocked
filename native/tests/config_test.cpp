#include <cstdlib>

#include <cstddef>
#include <filesystem>

#include "check.hpp"
#include "config/config.hpp"

using namespace dl;
using dl::test::check;
using dl::test::close;

int main() {
    // this test writes a config, so it is pointed at a scratch directory first. without
    // this it would overwrite the real one in the user's home.
    const std::filesystem::path sandbox =
        std::filesystem::temp_directory_path() / "deadlocked-config-test";
    std::error_code error;
    std::filesystem::remove_all(sandbox, error);
    std::filesystem::create_directories(sandbox, error);
    ::setenv("XDG_CONFIG_HOME", sandbox.c_str(), 1);
    check(config::config_path().string().starts_with(sandbox.string()),
          "test writes into a sandbox, not the real config");

    config::Config original;
    original.accent_color = ui::Color(250, 189, 47);
    original.theme.theme = ui::Theme::Gruvbox;
    original.theme.gradient_direction = ui::GradientDirection::DiagonalUp;
    original.theme.text_scale = 1.23f;
    original.theme.sync_gradient();
    original.theme.seed_custom(ui::Theme::Nord);

    original.player.esp_hotkey = config::KeyCode::Mouse5;
    original.player.draw_box = config::DrawMode::PlayerColor;
    original.player.visibility = config::VisibilityMode::VisibleOnly;
    original.player.box_visible_color = ui::Color(1, 2, 3, 4);
    original.player.sound.gunshot_diameter = 4321.0f;

    original.misc.desired_fov = 120;
    original.misc.smoke_color = ui::Color(9, 8, 7, 6);
    original.radar.url = "example.invalid";
    original.radar_uuid = config::new_uuid();

    original.font = config::Font::JetBrainsMono;
    original.fps = 240;
    original.hud.sniper_crosshair.gap = 42.0f;
    original.hud.grenade_trails.he = ui::Color(1, 2, 3, 4);
    for (std::size_t i = 0; i < config::text_slot_count; ++i) {
        config::TextCategory& category = original.hud.overlay_text.categories[i];
        category.font_size = 10.0f + static_cast<float>(i);
        category.color = ui::Color(static_cast<unsigned char>(i), 7, 7, 255);
        category.position = static_cast<config::TextPosition>(i % 9);
        category.align = static_cast<config::TextAlign>((i + 3) % 9);
        category.use_player_color = (i % 2) == 0;
    }

    original.aim.aimbot_hotkey = config::KeyCode::Mouse4;
    original.aim.global.aimbot.fov = 7.25f;
    original.aim.global.aimbot.targeting_mode = config::TargetingMode::Distance;
    original.aim.global.aimbot.bones = {config::Bones::Head, config::Bones::RightFoot};
    original.aim.global.rcs.strength = {0.31f, 0.77f};
    original.aim.global.triggerbot.delay = {13, 456};
    original.aim[config::Weapon::AK47].aimbot.smooth = 12.5f;
    original.aim[config::Weapon::Awp].triggerbot.head_only = true;

    check(config::save(original), "save");
    const config::Config back = config::load();

    check(back.accent_color == original.accent_color, "accent color");
    check(back.theme.theme == ui::Theme::Gruvbox, "theme enum");
    check(back.theme.gradient_direction == ui::GradientDirection::DiagonalUp, "gradient direction");
    check(close(back.theme.text_scale, 1.23f), "text scale");
    check(back.theme.custom.text == original.theme.custom.text, "custom palette");

    check(back.player.esp_hotkey == config::KeyCode::Mouse5, "esp hotkey");
    check(back.player.draw_box == config::DrawMode::PlayerColor, "draw box enum");
    check(back.player.visibility == config::VisibilityMode::VisibleOnly, "visibility enum");
    check(back.player.box_visible_color == ui::Color(1, 2, 3, 4), "rgba with alpha");
    check(close(back.player.sound.gunshot_diameter, 4321.0f), "sound range");

    check(back.misc.desired_fov == 120u, "desired fov");
    check(back.misc.smoke_color == ui::Color(9, 8, 7, 6), "smoke color");
    check(back.radar.url == "example.invalid", "radar url");
    check(back.radar_uuid == original.radar_uuid, "radar uuid persists");
    check(original.radar_uuid.size() == 36 && original.radar_uuid[14] == '4', "uuid v4 shape");

    check(back.font == config::Font::JetBrainsMono, "font enum");
    check(back.fps == 240u, "fps");
    check(close(back.hud.sniper_crosshair.gap, 42.0f), "crosshair gap");
    check(back.hud.grenade_trails.he == ui::Color(1, 2, 3, 4), "trail color");

    bool slots_ok = true;
    for (std::size_t i = 0; i < config::text_slot_count; ++i) {
        const config::TextCategory& a = back.hud.overlay_text.categories[i];
        const config::TextCategory& b = original.hud.overlay_text.categories[i];
        slots_ok = slots_ok && close(a.font_size, b.font_size) && a.color == b.color &&
                   a.position == b.position && a.align == b.align &&
                   a.use_player_color == b.use_player_color;
    }
    check(slots_ok, "all 11 text slots keep their own values");

    check(back.aim.aimbot_hotkey == config::KeyCode::Mouse4, "aimbot hotkey");
    check(close(back.aim.global.aimbot.fov, 7.25f), "global fov");
    check(back.aim.global.aimbot.targeting_mode == config::TargetingMode::Distance, "targeting mode");
    check(back.aim.global.aimbot.bones == original.aim.global.aimbot.bones, "bone list");
    check(back.aim.global.rcs.strength == original.aim.global.rcs.strength, "rcs strength");
    check(back.aim.global.triggerbot.delay == config::Range{13, 456}, "triggerbot delay range");
    check(close(back.aim[config::Weapon::AK47].aimbot.smooth, 12.5f), "per weapon override");
    check(back.aim[config::Weapon::Awp].triggerbot.head_only, "second per weapon override");

    const config::WeaponConfig default_weapon;
    bool others_default = true;
    for (std::size_t i = 0; i < config::weapon_count; ++i) {
        const auto weapon = static_cast<config::Weapon>(i);
        if (weapon == config::Weapon::AK47 || weapon == config::Weapon::Awp) {
            continue;
        }
        others_default = others_default && back.aim.weapons[i] == default_weapon;
    }
    check(others_default, "the other 65 weapons stay at defaults");

    // defaults must still match the ones the rust client ships
    const config::Config defaults;
    check(close(defaults.hud.overlay_text[config::TextSlot::BombTimer].font_size, 24.0f),
          "default bomb timer size");
    check(defaults.hud.overlay_text[config::TextSlot::PlayerName].position ==
              config::TextPosition::TopRight,
          "default player name position");
    check(defaults.aim.global.aimbot.bones.size() == 7, "default bone list has 7 entries");
    check(defaults.aim.aimbot_hotkey == config::KeyCode::Mouse5, "default aimbot hotkey");
    check(close(defaults.aim.global.aimbot.fov, 2.5f), "default fov");
    check(defaults.fps == 120u && defaults.font == config::Font::FiraSans, "root defaults");
    check(config::weapon_count == 67 && config::bone_count == 19, "67 weapons, 19 bones");

    return dl::test::report();
}
