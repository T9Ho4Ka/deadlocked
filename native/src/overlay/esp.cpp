#include "overlay/esp.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <cstdio>
#include <optional>
#include <string>
#include <variant>

#include <imgui.h>

#include "config/config.hpp"
#include "config/weapon_class.hpp"
#include "cs2/snapshot.hpp"
#include "game_math.hpp"
#include "overlay/sounds.hpp"
#include "overlay/trails.hpp"

namespace dl::overlay {
namespace {

using config::BoxMode;
using config::DrawMode;
using config::SnaplineAnchor;
using config::SnaplineMode;
using config::VisibilityMode;
using cs2::PlayerData;
using cs2::Snapshot;

ImU32 to_u32(const ui::Color& color) { return color.u32(); }

/// green at full health through yellow to red at none, the same ramp the rust client uses
ImU32 health_color(std::int32_t health, std::int32_t max_health, std::uint8_t alpha) {
    const std::int32_t cap = std::max(max_health, 1);
    const float percent = static_cast<float>(std::clamp(health, 0, cap)) / static_cast<float>(cap);

    std::uint8_t red = 0;
    std::uint8_t green = 0;
    if (percent <= 0.5f) {
        red = 255;
        green = static_cast<std::uint8_t>(255.0f * (percent * 2.0f));
    } else {
        red = static_cast<std::uint8_t>(255.0f * (1.0f - (percent - 0.5f) * 2.0f));
        green = 255;
    }
    return IM_COL32(red, green, 0, alpha);
}

/// The colour the game assigns a player in a competitive lobby. These are the rust
/// client's own accent colours, not the game's, so the two look the same.
ImU32 player_color(std::int32_t index) {
    constexpr std::array<ImU32, 5> colors{
        IM_COL32(100, 150, 240, 255),  // blue
        IM_COL32(160, 240, 130, 255),  // green
        IM_COL32(240, 200, 120, 255),  // yellow
        IM_COL32(240, 140, 90, 255),   // orange
        IM_COL32(180, 120, 240, 255),  // purple
    };
    if (index < 0 || static_cast<std::size_t>(index) >= colors.size()) {
        // subtext, for a player the lobby gave no colour
        return IM_COL32(140, 140, 140, 255);
    }
    return colors[static_cast<std::size_t>(index)];
}

/// Reapplies an alpha to a colour, for fading a player out as the sound that revealed them
/// ages away.
ImU32 with_alpha(ImU32 color, float alpha) {
    const auto scaled = static_cast<std::uint32_t>(std::clamp(alpha, 0.0f, 1.0f) * 255.0f);
    return (color & 0x00FF'FFFFu) | (scaled << IM_COL32_A_SHIFT);
}

ImU32 mode_color(DrawMode mode, const PlayerData& player, const ui::Color& fixed) {
    switch (mode) {
        case DrawMode::Health: return health_color(player.health, player.max_health, fixed.a);
        case DrawMode::PlayerColor: return player_color(player.color);
        case DrawMode::Color:
        case DrawMode::None: break;
    }
    return to_u32(fixed);
}

ImVec2 to_im(const Vec2& value) { return ImVec2(value.x, value.y); }

/// The player's box in screen space, as its four corners.
///
/// A cylinder is sampled around the collision bounds and every point projected, then the
/// leftmost and rightmost samples become the sides. Projecting the eight corners of the box
/// instead would make the box swing as the player turns.
std::optional<std::array<Vec2, 4>> projected_bounds(const PlayerData& player,
                                                    const Snapshot& snapshot) {
    const Vec3 min = player.collision_mins;
    const Vec3 max = player.collision_maxs;
    const bool finite = std::isfinite(min.x) && std::isfinite(min.y) && std::isfinite(min.z) &&
                        std::isfinite(max.x) && std::isfinite(max.y) && std::isfinite(max.z);
    if (!finite || min.x > max.x || min.y > max.y || min.z > max.z || min == max) {
        return std::nullopt;
    }

    constexpr std::size_t samples = game_math::cylinder_samples;
    const Vec3 centre = (min + max) * 0.5f;
    const float radius = 0.5f * std::max(std::abs(max.x - min.x), std::abs(max.y - min.y));

    std::array<Vec2, samples * 2> points{};
    for (std::size_t layer = 0; layer < 2; ++layer) {
        const float z = layer == 0 ? min.z : max.z;
        for (std::size_t i = 0; i < samples; ++i) {
            const float angle =
                2.0f * std::numbers::pi_v<float> * static_cast<float>(i) / static_cast<float>(samples);
            const Vec3 local(centre.x + radius * std::cos(angle),
                             centre.y + radius * std::sin(angle), z);
            const Vec3 world = Vec3(player.collision_transform * Vec4(local, 1.0f));
            const auto screen =
                game_math::world_to_screen(world, snapshot.view_matrix, snapshot.window_size);
            if (!screen.has_value()) {
                // the rust client gives up on the whole box as soon as one sample falls off
                // screen, so a player at the edge simply has no box. kept as is.
                return std::nullopt;
            }
            points[layer * samples + i] = *screen;
        }
    }

    std::size_t left = 0;
    std::size_t right = 0;
    for (std::size_t i = 1; i < samples; ++i) {
        if (std::min(points[i].x, points[i + samples].x) <
            std::min(points[left].x, points[left + samples].x)) {
            left = i;
        }
        if (std::max(points[i].x, points[i + samples].x) >
            std::max(points[right].x, points[right + samples].x)) {
            right = i;
        }
    }

    // top pair comes from the upper layer, bottom pair from the lower one
    return std::array<Vec2, 4>{points[left + samples], points[right + samples], points[left],
                               points[right]};
}

void draw_gap_box(ImDrawList* list, const std::array<Vec2, 4>& corners, ImU32 color,
                  float thickness) {
    const auto& [top_left, top_right, bottom_left, bottom_right] = corners;
    // only the corners, a quarter of each edge long
    const auto segment = [&](const Vec2& from, const Vec2& to) {
        const Vec2 quarter = (to - from) * 0.25f;
        list->AddLine(to_im(from), to_im(from + quarter), color, thickness);
        list->AddLine(to_im(to - quarter), to_im(to), color, thickness);
    };
    segment(top_left, top_right);
    segment(bottom_left, bottom_right);
    segment(top_left, bottom_left);
    segment(top_right, bottom_right);
}

void draw_full_box(ImDrawList* list, const std::array<Vec2, 4>& corners, ImU32 color,
                   float thickness) {
    const auto& [top_left, top_right, bottom_left, bottom_right] = corners;
    list->AddLine(to_im(top_left), to_im(top_right), color, thickness);
    list->AddLine(to_im(top_right), to_im(bottom_right), color, thickness);
    list->AddLine(to_im(bottom_right), to_im(bottom_left), color, thickness);
    list->AddLine(to_im(bottom_left), to_im(top_left), color, thickness);
}

void draw_bars(ImDrawList* list, const std::array<Vec2, 4>& corners, const PlayerData& player,
               const config::PlayerConfig& settings) {
    const Vec2 top_left = corners[0];
    const Vec2 bottom_left = corners[2];
    const float height = bottom_left.y - top_left.y;
    if (height <= 0.0f) {
        return;
    }

    float x = top_left.x - 6.0f;
    const auto bar = [&](float fraction, ImU32 color) {
        const float filled = height * std::clamp(fraction, 0.0f, 1.0f);
        list->AddRectFilled(ImVec2(x - 2.0f, top_left.y - 1.0f), ImVec2(x + 2.0f, bottom_left.y + 1.0f),
                            IM_COL32(0, 0, 0, 180));
        list->AddRectFilled(ImVec2(x - 1.0f, bottom_left.y - filled), ImVec2(x + 1.0f, bottom_left.y),
                            color);
        x -= 6.0f;
    };

    if (settings.health_bar) {
        const float fraction = static_cast<float>(player.health) /
                               static_cast<float>(std::max(player.max_health, 1));
        bar(fraction, health_color(player.health, player.max_health, 255));
    }
    if (settings.armor_bar && player.armor > 0) {
        bar(static_cast<float>(player.armor) / 100.0f, IM_COL32(80, 160, 255, 255));
    }
}

void draw_skeleton(ImDrawList* list, const PlayerData& player, const Snapshot& snapshot,
                   const config::Config& config, float scale, float alpha) {
    const config::PlayerConfig& settings = config.player;
    if (settings.draw_skeleton == DrawMode::None) {
        return;
    }
    const ImU32 color =
        with_alpha(mode_color(settings.draw_skeleton, player, settings.skeleton_color), alpha);
    const float thickness = config.hud.line_width * scale;

    for (const auto& [from, to] : config::bone_connections) {
        const auto a = player.bones.find(from);
        const auto b = player.bones.find(to);
        if (a == player.bones.end() || b == player.bones.end()) {
            continue;
        }
        const auto start =
            game_math::world_to_screen(a->second, snapshot.view_matrix, snapshot.window_size);
        const auto end =
            game_math::world_to_screen(b->second, snapshot.view_matrix, snapshot.window_size);
        if (!start.has_value() || !end.has_value()) {
            continue;
        }
        list->AddLine(to_im(*start), to_im(*end), color, thickness);
    }

    if (!settings.head_circle) {
        return;
    }
    const auto neck = player.bones.find(config::Bones::Neck);
    const auto spine = player.bones.find(config::Bones::Spine3);
    const auto head = player.bones.find(config::Bones::Head);
    if (neck == player.bones.end() || spine == player.bones.end() ||
        head == player.bones.end()) {
        return;
    }
    const auto neck_screen =
        game_math::world_to_screen(neck->second, snapshot.view_matrix, snapshot.window_size);
    const auto spine_screen =
        game_math::world_to_screen(spine->second, snapshot.view_matrix, snapshot.window_size);
    const auto head_screen =
        game_math::world_to_screen(head->second, snapshot.view_matrix, snapshot.window_size);
    if (!neck_screen || !spine_screen || !head_screen) {
        return;
    }
    // the neck to spine distance on screen scales with how far away the player is, so the
    // circle shrinks with them without needing the distance itself
    const float radius = std::abs(spine_screen->y - neck_screen->y) / 2.4f;
    list->AddCircle(to_im(*head_screen), radius, color, 0, config.hud.line_width * scale);
}

void draw_snapline(ImDrawList* list, const PlayerData& player, const Snapshot& snapshot,
                   const config::PlayerConfig& settings, float alpha) {
    if (settings.snaplines == SnaplineMode::None) {
        return;
    }
    const auto target =
        game_math::world_to_screen(player.position, snapshot.view_matrix, snapshot.window_size);
    if (!target.has_value()) {
        return;
    }

    const Vec2 from = settings.snapline_anchor == SnaplineAnchor::Bottom
                          ? Vec2(snapshot.window_size.x * 0.5f, snapshot.window_size.y)
                          : snapshot.window_size * 0.5f;

    ImU32 color = to_u32(settings.snapline_color);
    if (settings.snaplines == SnaplineMode::Health) {
        color = health_color(player.health, player.max_health, settings.snapline_color.a);
    } else if (settings.snaplines == SnaplineMode::PlayerColor) {
        color = player_color(player.color);
    }
    list->AddLine(to_im(from), to_im(*target), with_alpha(color, alpha), 1.0f);
}

bool passes_visibility(const PlayerData& player, VisibilityMode mode) {
    switch (mode) {
        case VisibilityMode::VisibleOnly: return player.visible;
        case VisibilityMode::InvisibleOnly: return !player.visible;
        case VisibilityMode::All: break;
    }
    return true;
}

void draw_player(ImDrawList* list, const PlayerData& player, const Snapshot& snapshot,
                 const config::Config& config, float alpha) {
    const config::PlayerConfig& settings = config.player;
    if (!passes_visibility(player, settings.visibility)) {
        return;
    }

    // everything thins out with distance, so a far away player is not a blob of lines
    const float distance = std::max(glm::distance(snapshot.local_player.position, player.position), 1.0f);
    const float scale = std::clamp(500.0f / distance, 0.25f, 1.0f);

    draw_snapline(list, player, snapshot, settings, alpha);
    draw_skeleton(list, player, snapshot, config, scale, alpha);

    const auto corners = projected_bounds(player, snapshot);
    if (!corners.has_value()) {
        return;
    }

    if (settings.draw_box != DrawMode::None) {
        const ui::Color& fixed =
            player.visible ? settings.box_visible_color : settings.box_invisible_color;
        const ImU32 color = with_alpha(mode_color(settings.draw_box, player, fixed), alpha);
        const float thickness = config.hud.line_width * scale;
        if (settings.box_mode == BoxMode::Gap) {
            draw_gap_box(list, *corners, color, thickness);
        } else {
            draw_full_box(list, *corners, color, thickness);
        }
    }

    draw_bars(list, *corners, player, settings);

    if (settings.player_name && !player.name.empty()) {
        const Vec2 above((*corners)[0].x, (*corners)[0].y - 14.0f);
        list->AddText(to_im(above), with_alpha(IM_COL32(255, 255, 255, 255), alpha),
                      player.name.c_str());
    }
}

/// Draws a string with one overlay text category's size, colour and anchor.
void draw_text(ImDrawList* list, const config::TextCategory& style, const Vec2& at,
               const std::string& text, float offset_y = 0.0f) {
    if (text.empty()) {
        return;
    }
    const ImVec2 measured = ImGui::GetFont()->CalcTextSizeA(style.font_size, FLT_MAX, 0.0f,
                                                            text.c_str());
    // horizontal alignment comes from the left, centre or right third of the align enum
    const auto align = static_cast<int>(style.align);
    const float horizontal = static_cast<float>(align / 3) * 0.5f;   // 0, 0.5, 1
    const float vertical = static_cast<float>(align % 3) * 0.5f;

    const ImVec2 position(at.x - measured.x * horizontal,
                          at.y - measured.y * vertical + offset_y);
    list->AddText(ImGui::GetFont(), style.font_size, position, to_u32(style.color),
                  text.c_str());
}

/// The path a grenade has flown, projected and drawn as a line.
void draw_trail(ImDrawList* list, const Trails& trails, std::uintptr_t entity,
                const Snapshot& snapshot, const ui::Color& color, float thickness) {
    const std::vector<Vec3>* path = trails.path(entity);
    if (path == nullptr || path->size() < 2) {
        return;
    }
    for (std::size_t i = 1; i < path->size(); ++i) {
        const auto from = game_math::world_to_screen((*path)[i - 1], snapshot.view_matrix,
                                                      snapshot.window_size);
        const auto to = game_math::world_to_screen((*path)[i], snapshot.view_matrix,
                                                    snapshot.window_size);
        if (!from.has_value() || !to.has_value()) {
            continue;
        }
        list->AddLine(to_im(*from), to_im(*to), to_u32(color), thickness);
    }
}

void draw_entities(ImDrawList* list, const Snapshot& snapshot, const config::Config& config,
                   const Trails& trails) {
    const config::HudConfig& hud = config.hud;

    for (const cs2::EntityInfo& entity : snapshot.entities) {
        std::visit(
            [&](const auto& info) {
                using T = std::decay_t<decltype(info)>;
                const auto screen = game_math::world_to_screen(info.position, snapshot.view_matrix,
                                                               snapshot.window_size);
                if (!screen.has_value()) {
                    return;
                }

                if constexpr (std::is_same_v<T, cs2::WeaponInfo>) {
                    if (!hud.dropped_weapons) {
                        return;
                    }
                    const config::TextCategory& style =
                        hud.overlay_text[config::TextSlot::WeaponName];
                    draw_text(list, style, *screen, std::string(config::enum_name(info.weapon)));
                    if (info.clip_ammo >= 0) {
                        draw_text(list, style, *screen,
                                  std::to_string(info.clip_ammo) + "/" +
                                      std::to_string(info.reserve_ammo),
                                  style.font_size);
                    }
                } else if constexpr (std::is_same_v<T, cs2::GrenadeInfo>) {
                    if (!hud.grenade_trails.enabled) {
                        return;
                    }
                    draw_text(list, hud.overlay_text[config::TextSlot::GrenadeName], *screen,
                              info.name);
                    // each kind of grenade gets its own trail colour
                    const ui::Color& trail = info.name == "Smoke"      ? hud.grenade_trails.smoke
                                             : info.name == "Flashbang" ? hud.grenade_trails.flash
                                             : info.name == "Decoy"     ? hud.grenade_trails.decoy
                                                                        : hud.grenade_trails.he;
                    draw_trail(list, trails, info.entity, snapshot, trail, hud.line_width);
                } else if constexpr (std::is_same_v<T, cs2::MolotovInfo>) {
                    if (!hud.grenade_trails.enabled) {
                        return;
                    }
                    const ui::Color& color =
                        info.is_incendiary ? hud.grenade_trails.incendiary : hud.grenade_trails.molotov;
                    list->AddCircle(to_im(*screen), 8.0f, to_u32(color), 0, hud.line_width);
                    draw_text(list, hud.overlay_text[config::TextSlot::GrenadeName], *screen,
                              info.is_incendiary ? "Incendiary" : "Molotov");
                    draw_trail(list, trails, info.entity, snapshot, color, hud.line_width);
                } else if constexpr (std::is_same_v<T, cs2::InfernoInfo>) {
                    if (!hud.grenade_trails.enabled) {
                        return;
                    }
                    list->AddCircle(to_im(*screen), 12.0f, to_u32(hud.grenade_trails.molotov), 0,
                                    hud.line_width);
                } else {
                    // a chicken, which is here for the same reason it is in the game
                    list->AddCircle(to_im(*screen), 5.0f, IM_COL32(255, 200, 80, 255), 0,
                                    hud.line_width);
                }
            },
            entity);
    }
}

void draw_bomb(ImDrawList* list, const Snapshot& snapshot, const config::Config& config) {
    if (!config.hud.bomb_timer || !snapshot.bomb.planted) {
        return;
    }

    const config::TextCategory& style = config.hud.overlay_text[config::TextSlot::BombTimer];
    const auto screen = game_math::world_to_screen(snapshot.bomb.position, snapshot.view_matrix,
                                                   snapshot.window_size);
    if (screen.has_value()) {
        char text[32];
        std::snprintf(text, sizeof(text), "%.3f", snapshot.bomb.timer);
        draw_text(list, style, *screen, text);
        if (snapshot.bomb.being_defused) {
            char defusing[48];
            std::snprintf(defusing, sizeof(defusing), "defusing %.3f",
                          snapshot.bomb.defuse_remaining);
            draw_text(list, style, *screen, defusing, style.font_size);
        }
    }

    // a bar across the bottom of the screen, draining as the fuse burns down
    const float fraction = std::clamp(snapshot.bomb.timer / 40.0f, 0.0f, 1.0f);
    const ImU32 color = health_color(static_cast<std::int32_t>(fraction * 100.0f), 100, 255);
    list->AddLine(ImVec2(0.0f, snapshot.window_size.y),
                  ImVec2(snapshot.window_size.x * fraction, snapshot.window_size.y), color,
                  config.hud.line_width * 3.0f);
}

void draw_crosshair(ImDrawList* list, const Snapshot& snapshot, const config::Config& config) {
    const config::CrosshairConfig& crosshair = config.hud.sniper_crosshair;
    // only while scoped weapons are held, which is when the game hides its own crosshair
    if (!crosshair.enabled || config::weapon_class(snapshot.weapon) != config::WeaponClass::Sniper) {
        return;
    }

    const Vec2 centre = snapshot.window_size * 0.5f;
    const float gap = crosshair.gap * 0.5f;
    const ImU32 color = to_u32(crosshair.color);
    const float width = crosshair.line_width;
    const float length = crosshair.line_length;

    list->AddLine(ImVec2(centre.x + gap, centre.y), ImVec2(centre.x + gap + length, centre.y),
                  color, width);
    list->AddLine(ImVec2(centre.x - gap, centre.y), ImVec2(centre.x - gap - length, centre.y),
                  color, width);
    list->AddLine(ImVec2(centre.x, centre.y + gap), ImVec2(centre.x, centre.y + gap + length),
                  color, width);
    list->AddLine(ImVec2(centre.x, centre.y - gap), ImVec2(centre.x, centre.y - gap - length),
                  color, width);
}

/// The radius on screen that a given angle covers, given the field of view the player has.
float fov_radius(float target_fov, float current_fov, float screen_width) {
    const float current = std::tan(glm::radians(current_fov) * 0.5f);
    if (current == 0.0f) {
        return 0.0f;
    }
    return std::tan(glm::radians(target_fov) * 0.5f) / current * (screen_width * 0.5f);
}

void draw_fov_circle(ImDrawList* list, const Snapshot& snapshot, const config::Config& config,
                     const FeatureState& features) {
    if (!config.hud.fov_circle) {
        return;
    }
    const config::WeaponConfig& per_weapon = config.aim[snapshot.weapon];
    const config::AimbotConfig& settings =
        per_weapon.aimbot.enable_override ? per_weapon.aimbot : config.aim.global.aimbot;
    if (!settings.enabled ||
        (settings.mode == config::KeyMode::Toggle && !features.aimbot_active)) {
        return;
    }

    const float current_fov = config.misc.fov_changer
                                  ? static_cast<float>(config.misc.desired_fov)
                                  : static_cast<float>(config::cs2::default_fov);
    const ImVec2 centre(snapshot.window_size.x * 0.5f, snapshot.window_size.y * 0.5f);

    const auto circle = [&](float target, ImU32 color) {
        list->AddCircle(centre, fov_radius(target, current_fov, snapshot.window_size.x), color, 0,
                        config.hud.line_width);
    };

    if (!settings.distance_adjusted_fov) {
        circle(settings.fov, IM_COL32(255, 255, 255, 255));
        return;
    }
    // one ring per distance band, since the aim widens its search for closer targets
    for (const auto& [distance, color] :
         {std::pair{125.0f, IM_COL32(0, 255, 0, 255)}, std::pair{250.0f, IM_COL32(255, 255, 0, 255)},
          std::pair{500.0f, IM_COL32(255, 0, 0, 255)}}) {
        circle(settings.fov * std::max(5.0f - (distance / 125.0f), 1.0f), color);
    }
}

void draw_keybinds(ImDrawList* list, const Snapshot& snapshot, const config::Config& config,
                   const FeatureState& features) {
    if (!config.hud.keybind_list) {
        return;
    }
    const config::TextCategory& style = config.hud.overlay_text[config::TextSlot::KeybindList];
    Vec2 at(12.0f, snapshot.window_size.y * 0.5f);

    // green while the feature is actually on, so the list doubles as a status light
    const ui::Color active(120, 240, 120);
    config::TextCategory shown = style;

    shown.color = features.aimbot_active ? active : style.color;
    draw_text(list, shown, at,
              "Aimbot: " + std::string(config::enum_name(config.aim.aimbot_hotkey)));
    at.y += style.font_size;

    shown.color = features.triggerbot_active ? active : style.color;
    draw_text(list, shown, at,
              "Triggerbot: " + std::string(config::enum_name(config.aim.triggerbot_hotkey)));
}

void draw_spectators(ImDrawList* list, const Snapshot& snapshot, const config::Config& config) {
    if (!config.hud.spectator_list || snapshot.spectators.empty()) {
        return;
    }
    const config::TextCategory& style = config.hud.overlay_text[config::TextSlot::SpectatorList];
    Vec2 at(12.0f, snapshot.window_size.y * 0.5f);
    for (const std::string& name : snapshot.spectators) {
        draw_text(list, style, at, name);
        at.y += style.font_size + 2.0f;
    }
}

}  // namespace

void draw_esp(const Snapshot& snapshot, const config::Config& config,
              const FeatureState& features, const Trails& trails, const Sounds& sounds) {
    if (!snapshot.in_game) {
        return;
    }

    ImDrawList* list = ImGui::GetBackgroundDrawList();

    if (config.player.enabled) {
        // with sound esp on, a player is only drawn while they can be seen or have just
        // been heard, fading out as that ages
        const auto strength = [&](const PlayerData& player) {
            return config.player.sound.enabled
                       ? sounds.alpha(snapshot, player, config.player.sound)
                       : 1.0f;
        };

        for (const PlayerData& player : snapshot.players) {
            const float alpha = strength(player);
            if (alpha > 0.0f) {
                draw_player(list, player, snapshot, config, alpha);
            }
        }
        if (config.player.show_friendlies) {
            for (const PlayerData& player : snapshot.friendlies) {
                const float alpha = strength(player);
                if (alpha > 0.0f) {
                    draw_player(list, player, snapshot, config, alpha);
                }
            }
        }
    }

    draw_entities(list, snapshot, config, trails);
    draw_bomb(list, snapshot, config);
    draw_crosshair(list, snapshot, config);
    draw_fov_circle(list, snapshot, config, features);
    draw_keybinds(list, snapshot, config, features);
    draw_spectators(list, snapshot, config);
}

}  // namespace dl::overlay
