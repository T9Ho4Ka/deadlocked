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

void appearance_tab(AppState& state) {
    config::Config& config = state.config;
    ThemeConfig& theme = config.theme;
    const Palette palette = theme.palette();
    const Color accent = config.accent_color;
    const float scale = theme.text_scale;

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

        if (ImGui::Button("Reset To Theme")) {
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
            if (ImGui::Button(theme_name(seed).data())) {
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
    if (ImGui::Button("Reset")) {
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
            if (ImGui::Button("Reset")) {
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
        // the radar client is not ported, so there is no connection to report on yet
        ImGui::TextColored(Color(240, 200, 120).vec4(), "Not connected");
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
        if (ImGui::Button("Open")) {
            open_url(link.c_str());
        }
        ImGui::SameLine();
        if (ImGui::Button("Copy link")) {
            ImGui::SetClipboardText(link.c_str());
        }
        ImGui::SameLine();
        if (ImGui::Button("Reset UUID")) {
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
        case Tab::Player: player_tab(state); break;
        case Tab::Hud: hud_tab(state); break;
        case Tab::Unsafe: unsafe_tab(state); break;
        case Tab::Radar: radar_tab(state); break;
        case Tab::Aimbot: placeholder("Aimbot", palette, accent, scale); break;
        case Tab::Grenades: placeholder("Grenades", palette, accent, scale); break;
        case Tab::Application: placeholder("Application", palette, accent, scale); break;
    }
}

}  // namespace dl::ui
