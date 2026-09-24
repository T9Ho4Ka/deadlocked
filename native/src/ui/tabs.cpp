#include "ui/tabs.hpp"

#include <algorithm>
#include <cmath>
#include <string>

#include <imgui.h>

#include "app.hpp"
#include "config/config.hpp"
#include "ui/theme.hpp"
#include "ui/widgets.hpp"

#include <imgui_stdlib.h>

namespace dl::ui {
namespace {

void help(const char* text) {
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", text);
    }
}

/// The profile list: which config file is in use, and making or removing them.
void config_profiles(AppState& state, const Palette& palette, Color accent, float scale) {
    if (!section("Configs", palette, accent, scale)) {
        return;
    }

    if (button("Refresh")) {
        state.available_configs = config::available_configs();
    }

    ImGui::SetNextItemWidth(160.0f);
    ImGui::InputTextWithHint("##new_config", "new profile", &state.new_config_name);
    ImGui::SameLine();
    if (button("+") && !state.new_config_name.empty()) {
        std::string name = state.new_config_name;
        if (!name.ends_with(".toml")) {
            name += ".toml";
        }
        const std::filesystem::path path = config::config_dir() / name;
        // a new profile starts from what is on screen, not from the defaults
        if (config::save_to(state.config, path)) {
            config::select_config(path);
            state.current_config = path;
            state.available_configs = config::available_configs();
            state.new_config_name.clear();
        }
    }

    ImGui::Spacing();
    std::filesystem::path to_open;
    std::filesystem::path to_delete;
    for (const std::filesystem::path& path : state.available_configs) {
        const std::string name = path.filename().string();
        ImGui::PushID(name.c_str());
        if (ImGui::Selectable(name.c_str(), path == state.current_config,
                              ImGuiSelectableFlags_None, ImVec2(140.0f, 0.0f))) {
            to_open = path;
        }
        ImGui::SameLine();
        if (small_button("Delete")) {
            to_delete = path;
        }
        ImGui::PopID();
    }

    // the list is only changed after it has been walked
    if (!to_open.empty()) {
        state.config = config::load_from(to_open);
        config::select_config(to_open);
        state.current_config = to_open;
        state.mark_style_changed();
    }
    if (!to_delete.empty() && config::delete_config(to_delete)) {
        state.available_configs = config::available_configs();
        if (to_delete == state.current_config) {
            state.current_config = config::config_path();
            state.config = config::load_from(state.current_config);
            state.mark_style_changed();
        }
    }
}

void appearance_settings(AppState& state) {
    config::Config& config = state.config;
    ThemeConfig& theme = config.theme;
    const Palette palette = theme.palette();
    const Color accent = config.accent_color;
    const float scale = theme.text_scale;

    if (section("Config", palette, accent, scale)) {
        if (button("Reset")) {
            // back to the defaults, without touching the other profiles
            config = config::Config{};
            state.mark_style_changed();
        }
        ImGui::SameLine();
        if (button("Config Folder")) {
            open_url(("file://" + config::config_dir().string()).c_str());
        }
        ImGui::TextDisabled("%s", state.current_config.filename().c_str());
    }

    if (section("Appearance", palette, accent, scale)) {
        if (ImGui::BeginCombo("Theme", theme_name(theme.theme).data())) {
            for (int i = 0; i < theme_count; ++i) {
                const auto candidate = static_cast<Theme>(i);
                if (ImGui::Selectable(theme_name(candidate).data(), candidate == theme.theme)) {
                    const Theme previous = theme.theme;
                    theme.theme = candidate;
                    if (candidate == Theme::Custom) {
                        // start editing from whatever the user was just looking at
                        theme.seed_custom(previous);
                    } else {
                        // a preset ships its own accent and gradient, adopt them on switch
                        config.accent_color = theme_palette(candidate).accent;
                        theme.sync_gradient();
                    }
                    state.mark_style_changed();
                }
            }
            ImGui::EndCombo();
        }

        const auto current = std::ranges::find_if(
            config::accent_colors, [&](const auto& entry) { return entry.second == accent; });
        const std::string accent_label =
            current == config::accent_colors.end() ? "Custom" : std::string(current->first);
        if (ImGui::BeginCombo("Accent Color", accent_label.c_str())) {
            for (const auto& [name, color] : config::accent_colors) {
                ImGui::PushStyleColor(ImGuiCol_Text, color.vec4());
                if (ImGui::Selectable(std::string(name).c_str(), color == accent)) {
                    config.accent_color = color;
                    state.mark_style_changed();
                }
                ImGui::PopStyleColor();
            }
            ImGui::EndCombo();
        }

        if (ImGui::SliderFloat("Corner Radius", &theme.corner_radius, 0.0f, 16.0f, "%.0f")) {
            state.mark_style_changed();
        }
        if (ImGui::Checkbox("Shadows", &theme.shadows)) {
            state.mark_style_changed();
        }
        help("imgui only has a border shadow, so this is subtler than in the rust client");
    }

    if (section("Gradient", palette, accent, scale)) {
        if (ImGui::Checkbox("Enable Gradient", &theme.gradient)) {
            state.mark_style_changed();
        }

        ImGui::BeginDisabled(!theme.gradient);
        if (color_picker("Start", theme.gradient_start)) {
            state.mark_style_changed();
        }
        ImGui::SameLine();
        if (color_picker("End", theme.gradient_end)) {
            state.mark_style_changed();
        }

        if (ImGui::BeginCombo("Direction",
                              gradient_direction_name(theme.gradient_direction).data())) {
            for (int i = 0; i < gradient_direction_count; ++i) {
                const auto candidate = static_cast<GradientDirection>(i);
                if (ImGui::Selectable(gradient_direction_name(candidate).data(),
                                      candidate == theme.gradient_direction)) {
                    theme.gradient_direction = candidate;
                    state.mark_style_changed();
                }
            }
            ImGui::EndCombo();
        }

        if (ImGui::SliderFloat("Panel Opacity", &theme.panel_opacity, 0.0f, 1.0f, "%.2f")) {
            state.mark_style_changed();
        }
        help("how much of the gradient shows through the panels");

        if (button("Reset To Theme")) {
            theme.sync_gradient();
            state.mark_style_changed();
        }
        ImGui::EndDisabled();
    }

    if (section("Text", palette, accent, scale)) {
        if (ImGui::SliderFloat("Text Size", &theme.text_scale, 0.8f, 1.5f, "%.2f")) {
            state.mark_style_changed();
        }
        help("scales every text size at once, headings included");

        if (ImGui::SliderFloat("Secondary Contrast", &theme.text_contrast, 0.0f, 1.0f, "%.2f")) {
            state.mark_style_changed();
        }
        help("brightens dimmed text, 1.0 makes it as bright as labels");
    }

    if (theme.theme == Theme::Custom && section("Custom Colors", palette, accent, scale)) {
        CustomPalette& custom = theme.custom;
        bool changed = ImGui::Checkbox("Dark Theme", &custom.dark);
        changed |= color_picker("Backdrop", custom.backdrop);
        help("text fields, scroll troughs");
        changed |= color_picker("Base", custom.base);
        help("panel and window background");
        changed |= color_picker("Surface", custom.surface);
        help("idle buttons and controls");
        changed |= color_picker("Overlay", custom.overlay);
        help("hovered controls and borders");
        changed |= color_picker("Text", custom.text);
        help("primary text");
        changed |= color_picker("Subtext", custom.subtext);
        help("hints and idle control text");

        ImGui::Spacing();
        for (const Theme seed :
             {Theme::Midnight, Theme::Mocha, Theme::Nord, Theme::Daylight}) {
            if (button(theme_name(seed).data())) {
                theme.seed_custom(seed);
                changed = true;
            }
            ImGui::SameLine();
        }
        ImGui::NewLine();
        help("copy a preset into the custom palette as a starting point");

        if (changed) {
            state.mark_style_changed();
        }
    }
}


/// Drag box with a reset button next to it, the pattern the sound ranges use.
bool drag_with_reset(const char* label, float& value, float speed, float min, float max,
                     float fallback) {
    bool changed = drag_float(label, value, speed, min, max, "%.0f");
    ImGui::SameLine();
    ImGui::PushID(label);
    if (button("Reset")) {
        value = fallback;
        changed = true;
    }
    ImGui::PopID();
    return changed;
}

void player_tab(AppState& state) {
    config::PlayerConfig& player = state.config.player;
    const Palette palette = state.config.theme.palette();
    const Color accent = state.config.accent_color;
    const float scale = state.config.theme.text_scale;
    bool changed = false;

    if (ImGui::BeginTable("##player_columns", 2)) {
        ImGui::TableNextColumn();
        ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x * 0.55f);

        if (section("Players", palette, accent, scale)) {
            changed |= checkbox("Player", player.enabled);
            changed |= checkbox("Chicken", player.chicken);
            changed |= keybind("ESP Hotkey", player.esp_hotkey);
            changed |= checkbox("Show Friendlies", player.show_friendlies);
            changed |= enum_combo("Box", player.draw_box);
            changed |= enum_combo("Box Mode", player.box_mode);
            changed |= enum_combo("Skeleton", player.draw_skeleton);
            changed |= enum_combo("Model", player.draw_model);
            changed |= enum_combo("Model Style", player.model_mode);
            changed |= checkbox("Head Circle", player.head_circle);
            changed |= enum_combo("Visibility", player.visibility);
        }

        if (section("Snaplines", palette, accent, scale)) {
            changed |= enum_combo("Mode", player.snaplines);
            changed |= enum_combo("Anchor", player.snapline_anchor);
        }
        ImGui::PopItemWidth();

        ImGui::TableNextColumn();
        ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x * 0.55f);

        if (section("Info", palette, accent, scale)) {
            changed |= checkbox("Health Bar", player.health_bar);
            changed |= checkbox("Armor Bar", player.armor_bar);
            changed |= checkbox("Player Name", player.player_name);
            ImGui::SameLine();
            text_settings_button(config::TextSlot::PlayerName, state.text_popup);

            changed |= checkbox("Weapon Icon", player.weapon_icon);
            ImGui::SameLine();
            text_settings_button(config::TextSlot::WeaponIcon, state.text_popup);

            ImGui::TextUnformatted("Ammo");
            ImGui::SameLine();
            text_settings_button(config::TextSlot::AmmoText, state.text_popup);

            changed |= checkbox("Show Tags", player.tags);
            ImGui::SameLine();
            text_settings_button(config::TextSlot::PlayerTags, state.text_popup);
        }

        if (section("Sound ESP", palette, accent, scale)) {
            config::SoundConfig& sound = player.sound;
            changed |= checkbox("Enabled", sound.enabled,
                                "Show a circle under players when they make sound");
            changed |= drag_float("Fadeout Time (s)", sound.fadeout_duration, 0.01f, 0.0f, 10.0f);
            changed |= checkbox("Show Visible", sound.show_visible);

            if (ImGui::TreeNode("Ranges")) {
                changed |= drag_with_reset("Footstep", sound.footstep_diameter, 10.0f, 200.0f,
                                           6000.0f, config::cs2::sound_esp_footstep_diameter);
                changed |= drag_with_reset("Gunshot", sound.gunshot_diameter, 10.0f, 200.0f,
                                           10000.0f, config::cs2::sound_esp_gunshot_diameter);
                changed |= drag_with_reset("Weapon", sound.weapon_diameter, 10.0f, 200.0f,
                                           6000.0f, config::cs2::sound_esp_weapon_diameter);
                ImGui::TreePop();
            }
        }
        ImGui::PopItemWidth();
        ImGui::EndTable();
    }

    if (section("Colors", palette, accent, scale)) {
        changed |= color_picker("Box (visible)", player.box_visible_color);
        changed |= color_picker("Box (invisible)", player.box_invisible_color);
        changed |= color_picker("Skeleton", player.skeleton_color);
        changed |= color_picker("Model (visible)", player.model_visible_color);
        changed |= color_picker("Model (invisible)", player.model_invisible_color);
        changed |= color_picker("Tracers", player.snapline_color);
    }

    if (changed) {
        state.config_dirty = true;
    }
}

void unsafe_tab(AppState& state) {
    config::UnsafeConfig& misc = state.config.misc;
    const Palette palette = state.config.theme.palette();
    const Color accent = state.config.accent_color;
    const float scale = state.config.theme.text_scale;
    bool changed = false;

    if (ImGui::BeginTable("##unsafe_columns", 2)) {
        ImGui::TableNextColumn();
        ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x * 0.55f);
        if (section("No Flash", palette, accent, scale)) {
            changed |= checkbox("No Flash", misc.no_flash);
            changed |= drag_float("Max Flash Alpha", misc.max_flash_alpha, 0.5f, 0.0f, 255.0f,
                                  "%.0f");
        }
        ImGui::PopItemWidth();

        ImGui::TableNextColumn();
        ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x * 0.55f);
        if (section("FOV Changer", palette, accent, scale)) {
            changed |= checkbox("FOV Changer", misc.fov_changer);
            changed |= drag_uint("Desired FOV", misc.desired_fov, 0.1f, 1, 179);
            ImGui::SameLine();
            if (button("Reset")) {
                misc.desired_fov = config::cs2::default_fov;
                changed = true;
            }
        }
        ImGui::PopItemWidth();
        ImGui::EndTable();
    }

    if (section("Smokes", palette, accent, scale)) {
        changed |= checkbox("No Smoke", misc.no_smoke);
        changed |= checkbox("Change Smoke Color", misc.change_smoke_color);
        changed |= color_picker("Smoke Color", misc.smoke_color);
    }

    if (changed) {
        state.config_dirty = true;
    }
}

void radar_tab(AppState& state) {
    config::Config& config = state.config;
    const Palette palette = config.theme.palette();
    const Color accent = config.accent_color;
    const float scale = config.theme.text_scale;
    bool changed = false;

    if (section("Radar", palette, accent, scale)) {
        changed |= checkbox("Enabled", config.radar.enabled);
        ImGui::SameLine();
        switch (state.radar_status) {
            case net::RadarStatus::Connected:
                ImGui::TextColored(Color(120, 240, 120).vec4(), "Connected");
                break;
            case net::RadarStatus::FailedToConnect:
                ImGui::TextColored(Color(240, 120, 120).vec4(), "Failed to connect");
                break;
            case net::RadarStatus::Disconnected:
                ImGui::TextColored(Color(240, 200, 120).vec4(), "Disconnected");
                break;
            case net::RadarStatus::Disabled:
                ImGui::TextColored(Color(240, 200, 120).vec4(), "Disabled");
                break;
        }
    }

    if (section("Connection", palette, accent, scale)) {
        ImGui::TextUnformatted("Server host (without protocol or port):");
        changed |= ImGui::InputText("##radar_url", &config.radar.url);
    }

    if (section("Session", palette, accent, scale)) {
        ImGui::TextUnformatted("Session UUID:");
        ImGui::SameLine();
        ImGui::TextUnformatted(config.radar_uuid.c_str());

        const std::string link = "https://radar.avitrano.com/?url=" + config.radar.url +
                                 "&game=" + config.radar_uuid;
        ImGui::BeginDisabled(state.radar_status != net::RadarStatus::Connected);
        if (button("Open")) {
            open_url(link.c_str());
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (button("Copy link")) {
            ImGui::SetClipboardText(link.c_str());
        }
        ImGui::SameLine();
        if (button("Reset UUID")) {
            config.radar_uuid = config::new_uuid();
            changed = true;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Disconnect and create a new radar session");
        }
    }

    if (changed) {
        state.config_dirty = true;
    }
}


void hud_tab(AppState& state) {
    config::HudConfig& hud = state.config.hud;
    const Palette palette = state.config.theme.palette();
    const Color accent = state.config.accent_color;
    const float scale = state.config.theme.text_scale;
    bool changed = false;

    // checkbox with the gear that opens that text's settings popup, the pattern the rust
    // hud tab uses for every piece of text the overlay draws
    const auto row = [&](const char* label, bool* value, config::TextSlot slot) {
        if (value != nullptr) {
            changed |= checkbox(label, *value);
        } else {
            ImGui::TextUnformatted(label);
        }
        ImGui::SameLine();
        text_settings_button(slot, state.text_popup);
    };

    if (ImGui::BeginTable("##hud_columns", 2)) {
        ImGui::TableNextColumn();
        ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x * 0.55f);

        if (section("HUD", palette, accent, scale)) {
            row("Bomb Timer", &hud.bomb_timer, config::TextSlot::BombTimer);
            changed |= checkbox("FOV Circle", hud.fov_circle);
            row("Dropped Weapons", &hud.dropped_weapons, config::TextSlot::WeaponName);
            row("Keybind List", &hud.keybind_list, config::TextSlot::KeybindList);
            row("Spectator List", &hud.spectator_list, config::TextSlot::SpectatorList);
        }

        if (section("Sniper Crosshair", palette, accent, scale)) {
            config::CrosshairConfig& crosshair = hud.sniper_crosshair;
            changed |= checkbox("Enabled", crosshair.enabled);
            changed |= drag_float("Line Length", crosshair.line_length, 0.2f, 0.1f, 500.0f, "%.1f");
            changed |= drag_float("Line Width", crosshair.line_width, 0.005f, 0.1f, 10.0f, "%.1f");
            changed |= drag_float("Gap", crosshair.gap, 0.2f, 0.0f, 200.0f, "%.1f");
        }
        ImGui::PopItemWidth();

        ImGui::TableNextColumn();
        ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x * 0.55f);

        if (section("Appearance", palette, accent, scale)) {
            changed |= checkbox("Text Outline", hud.text_outline);
            changed |= drag_float("Line Width", hud.line_width, 0.02f, 0.1f, 8.0f, "%.1f");
            changed |= enum_combo("Font", state.config.font);

            ImGui::Separator();
            row("Status Text", nullptr, config::TextSlot::StatusText);
            row("Grenade Name", nullptr, config::TextSlot::GrenadeName);
            row("Grenade Lineup", nullptr, config::TextSlot::GrenadeLineup);
        }

        if (section("Advanced", palette, accent, scale)) {
            changed |= checkbox("Debug Overlay", hud.debug);
            changed |= drag_uint("FPS", state.config.fps, 1.0f, 30, 500);
        }
        ImGui::PopItemWidth();
        ImGui::EndTable();
    }

    if (section("Colors", palette, accent, scale)) {
        changed |= color_picker("Crosshair Color", hud.sniper_crosshair.color);
    }

    if (section("Grenade Trails", palette, accent, scale)) {
        config::TrailConfig& trails = hud.grenade_trails;
        changed |= checkbox("Enable Grenade Trails", trails.enabled);
        changed |= checkbox("Inferno Polygon", trails.inferno_poly);
        changed |= color_picker("Smoke", trails.smoke);
        changed |= color_picker("Molotov", trails.molotov);
        changed |= color_picker("Incendiary", trails.incendiary);
        changed |= color_picker("Flash", trails.flash);
        changed |= color_picker("HE Grenade", trails.he);
        changed |= color_picker("Decoy", trails.decoy);
    }

    if (changed) {
        state.config_dirty = true;
    }
}


void aimbot_tab(AppState& state) {
    const Palette palette = state.config.theme.palette();
    const Color accent = state.config.accent_color;
    const float scale = state.config.theme.text_scale;
    const bool per_weapon = state.aimbot_tab == AimbotTab::Weapon;
    bool changed = false;

    const auto sub_tab = [&](const char* label, AimbotTab value) {
        const bool selected = state.aimbot_tab == value;
        if (selected) {
            ImGui::PushStyleColor(ImGuiCol_Button, accent.vec4());
        }
        if (button(label)) {
            state.aimbot_tab = value;
        }
        if (selected) {
            ImGui::PopStyleColor();
        }
    };

    sub_tab("Global", AimbotTab::Global);
    ImGui::SameLine();
    sub_tab("Weapon", AimbotTab::Weapon);
    if (per_weapon) {
        ImGui::SameLine();
        ImGui::SetNextItemWidth(220.0f);
        enum_combo("Weapon", state.aimbot_weapon);
    }
    ImGui::Separator();

    config::WeaponConfig& weapon = state.weapon_config();

    if (ImGui::BeginTable("##aimbot_columns", 2)) {
        ImGui::TableNextColumn();
        ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x * 0.5f);

        config::AimbotConfig& aimbot = weapon.aimbot;
        if (section("Aimbot", palette, accent, scale)) {
            changed |= keybind("Hotkey", state.config.aim.aimbot_hotkey);
            if (per_weapon) {
                changed |= checkbox("Enable Override", aimbot.enable_override,
                                    "Enable aimbot settings override for a specific weapon");
            }
            changed |= checkbox("Enable Aimbot", aimbot.enabled);
            changed |= enum_combo("Mode", aimbot.mode);
        }

        if (section("Targeting", palette, accent, scale)) {
            changed |= checkbox("Target Friendlies", aimbot.target_friendlies);
            changed |= checkbox("Distance-Adjusted FOV", aimbot.distance_adjusted_fov,
                                "Adjusts FOV based on target distance");
            changed |= drag_float("FOV", aimbot.fov, 0.02f, 0.1f, 360.0f, "%.1f°");
            changed |= drag_float("Smooth", aimbot.smooth, 0.02f, 0.0f, 20.0f, "%.1f");
            changed |= drag_float("Inertia", aimbot.inertia, 0.005f, 0.0f, 1.0f, "%.2f");
            changed |= drag_float("Prediction", aimbot.prediction_time, 0.002f, 0.0f, 0.25f,
                                  "%.2f s");
            changed |= ImGui::DragInt("Start Bullet", &aimbot.start_bullet, 0.05f, 0, 10, "%d",
                                      ImGuiSliderFlags_AlwaysClamp);
            changed |= enum_combo("Targeting Mode", aimbot.targeting_mode);
        }

        if (section("Checks", palette, accent, scale)) {
            changed |= checkbox("Visibility Check", aimbot.visibility_check);
            changed |= checkbox("Flash Check", aimbot.flash_check);
        }

        if (section("Bones", palette, accent, scale)) {
            for (const auto& entry : config::bone_entries) {
                const auto found = std::ranges::find(aimbot.bones, entry.value);
                const bool selected = found != aimbot.bones.end();
                const std::string label(entry.name);
                if (ImGui::Selectable(label.c_str(), selected)) {
                    if (selected) {
                        aimbot.bones.erase(found);
                    } else {
                        aimbot.bones.push_back(entry.value);
                    }
                    changed = true;
                }
            }
        }
        ImGui::PopItemWidth();

        ImGui::TableNextColumn();
        ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x * 0.5f);

        config::TriggerbotConfig& trigger = weapon.triggerbot;
        if (section("Triggerbot", palette, accent, scale)) {
            if (per_weapon) {
                changed |= checkbox("Enable Override##trigger", trigger.enable_override);
            }
            changed |= checkbox("Enable Triggerbot", trigger.enabled);
            changed |= keybind("Hotkey##trigger", state.config.aim.triggerbot_hotkey);

            int delay[2]{static_cast<int>(trigger.delay.start),
                         static_cast<int>(trigger.delay.end)};
            if (ImGui::DragInt2("Delay (ms)", delay, 1.0f, 0, 999, "%d",
                                ImGuiSliderFlags_AlwaysClamp)) {
                // keep the range ordered whichever handle the user dragged past the other
                trigger.delay.start = static_cast<std::uint64_t>(std::min(delay[0], delay[1]));
                trigger.delay.end = static_cast<std::uint64_t>(std::max(delay[0], delay[1]));
                changed = true;
            }

            changed |= enum_combo("Mode##trigger", trigger.mode);
            changed |= checkbox("Head Only", trigger.head_only);

            int duration = static_cast<int>(trigger.shot_duration);
            if (ImGui::DragInt("Hold Duration (ms)", &duration, 10.0f, 0, 2000, "%d",
                               ImGuiSliderFlags_AlwaysClamp)) {
                trigger.shot_duration = static_cast<std::uint64_t>(duration);
                changed = true;
            }
        }

        if (section("Checks##trigger", palette, accent, scale)) {
            changed |= checkbox("Flash Check##trigger", trigger.flash_check);
            changed |= checkbox("Scope Check", trigger.scope_check);
            changed |= checkbox("Velocity Check", trigger.velocity_check,
                                "Only shoot if the player moves slower than the threshold");
            changed |= drag_float("Velocity Threshold", trigger.velocity_threshold, 1.0f, 0.0f,
                                  5000.0f, "%.0f");
        }

        if (section("RCS", palette, accent, scale)) {
            if (per_weapon) {
                changed |= checkbox("Enable Override##rcs", weapon.rcs.enable_override);
            }
            changed |= checkbox("Enable RCS", weapon.rcs.enabled);
            changed |= drag_float("Strength X", weapon.rcs.strength.x, 0.01f, 0.0f, 1.0f);
            changed |= drag_float("Strength Y", weapon.rcs.strength.y, 0.01f, 0.0f, 1.0f);
        }
        ImGui::PopItemWidth();
        ImGui::EndTable();
    }

    if (changed) {
        state.config_dirty = true;
    }
}

/// The Config tab: settings on the left, the profiles on disk on the right, the same way
/// the rust client lays it out.
void appearance_tab(AppState& state) {
    const Palette palette = state.config.theme.palette();
    const Color accent = state.config.accent_color;
    const float scale = state.config.theme.text_scale;

    if (ImGui::BeginTable("##config_columns", 2)) {
        ImGui::TableNextColumn();
        ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x * 0.55f);
        appearance_settings(state);
        ImGui::PopItemWidth();

        ImGui::TableNextColumn();
        config_profiles(state, palette, accent, scale);
        ImGui::EndTable();
    }
}

/// Tabs whose settings live in the cs2 layer, which is not ported yet.
void placeholder(const char* name, const Palette& palette, Color accent, float scale) {
    ImGui::PushFont(nullptr, heading_size * scale);
    ImGui::TextColored(heading_color(palette, accent).vec4(), "%s", name);
    ImGui::PopFont();
    ImGui::Spacing();
    ImGui::TextDisabled("Not ported yet.");
    ImGui::TextDisabled("This tab arrives with the cs2 layer, the settings it");
    ImGui::TextDisabled("edits do not exist in the native config so far.");
}

}  // namespace

void draw_tab(AppState& state) {
    const Palette palette = state.config.theme.palette();
    const Color accent = state.config.accent_color;
    const float scale = state.config.theme.text_scale;

    switch (state.tab) {
        case Tab::Config: appearance_tab(state); break;
        case Tab::Aimbot: aimbot_tab(state); break;
        case Tab::Player: player_tab(state); break;
        case Tab::Hud: hud_tab(state); break;
        case Tab::Unsafe: unsafe_tab(state); break;
        case Tab::Radar: radar_tab(state); break;
        case Tab::Grenades: placeholder("Grenades", palette, accent, scale); break;
        case Tab::Application: placeholder("Application", palette, accent, scale); break;
    }
}

}  // namespace dl::ui
