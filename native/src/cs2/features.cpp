#include "cs2/features.hpp"

#include <algorithm>
#include <cmath>
#include <random>

#include "config/weapon_class.hpp"
#include "cs2/game.hpp"

namespace dl::cs2 {
namespace {

using config::WeaponClass;

/// these cannot be aimed with at all
bool aimable(WeaponClass weapon_class) {
    return weapon_class != WeaponClass::Unknown && weapon_class != WeaponClass::Knife &&
           weapon_class != WeaponClass::Grenade;
}

/// recoil control only makes sense for weapons that have a spray pattern
bool has_spray(WeaponClass weapon_class) {
    return aimable(weapon_class) && weapon_class != WeaponClass::Pistol &&
           weapon_class != WeaponClass::Shotgun;
}

/// A close target fills more of the screen, so the field of view the aim searches is widened
/// for it. Beyond five hundred units it is left alone.
float distance_scale(float distance) {
    return distance > 500.0f ? 1.0f : 5.0f - (distance / 125.0f);
}

/// The angle from the local player's eyes to a point, with the current recoil taken back out
/// so the aim points where the bullet will go rather than where the barrel is.
Vec2 angle_to(const Game& game, const Player& local, const Vec3& point, const Vec2& aim_punch) {
    const Vec3 forward = glm::normalize(point - local.eye_position(game));
    Vec2 angles = game_math::angles_from_vector(forward) - aim_punch;
    game_math::clamp_angles(angles);
    return angles;
}

/// The game reports no kick between shots, so the last known one is carried forward. A
/// sniper has none worth correcting for.
Vec2 effective_aim_punch(const Game& game, const Player& local, WeaponClass weapon_class,
                         std::int32_t shots_fired, const Vec2& previous, float scale) {
    if (weapon_class == WeaponClass::Sniper) {
        return Vec2(0.0f);
    }
    const Vec2 punch = local.aim_punch(game) * scale;
    if (glm::length(punch) == 0.0f && shots_fired > 1) {
        return previous;
    }
    return punch;
}

/// how hard each axis is allowed to accelerate while correcting recoil
struct AccelerationTuning {
    float multiplier;
    float low;
    float high;
    float fallback;
    float decay;
};

constexpr AccelerationTuning pitch_tuning{3.0f, 4.0f, 20.0f, 10.0f, 0.15f};
constexpr AccelerationTuning yaw_tuning{2.5f, 1.5f, 8.0f, 5.0f, 0.30f};
constexpr Vec2 track_scale{0.65f, 0.55f};
constexpr std::size_t acceleration_history_max = 12;

/// mouse counts per degree at a sensitivity of one
constexpr float counts_per_degree = 45.45f;

}  // namespace

const config::AimbotConfig& Features::aimbot_settings(const config::Config& config,
                                                      config::Weapon weapon) {
    const config::WeaponConfig& per_weapon = config.aim[weapon];
    return per_weapon.aimbot.enable_override ? per_weapon.aimbot : config.aim.global.aimbot;
}

const config::TriggerbotConfig& Features::triggerbot_settings(const config::Config& config,
                                                              config::Weapon weapon) {
    const config::WeaponConfig& per_weapon = config.aim[weapon];
    return per_weapon.triggerbot.enable_override ? per_weapon.triggerbot
                                                 : config.aim.global.triggerbot;
}

const config::RcsConfig& Features::rcs_settings(const config::Config& config,
                                                config::Weapon weapon) {
    const config::WeaponConfig& per_weapon = config.aim[weapon];
    return per_weapon.rcs.enable_override ? per_weapon.rcs : config.aim.global.rcs;
}

bool Features::hotkey(const Game& game, config::KeyMode mode, config::KeyCode key, bool& state) {
    if (mode == config::KeyMode::Hold) {
        return game.input().is_pressed(key);
    }
    if (game.input().just_pressed(key)) {
        state = !state;
    }
    return state;
}

void Features::no_flash(Game& game, const config::Config& config, const Player& local) {
    if (!config.misc.no_flash) {
        return;
    }
    const float wanted = std::clamp(config.misc.max_flash_alpha, 0.0f, 255.0f);
    const std::uintptr_t at = local.pawn().address() + game.offsets().pawn.flash_alpha;
    // only written when it differs, so the game is not fed the same value every frame
    if (game.process().read<float>(at) != wanted) {
        game.process().write(at, wanted);
    }
}

void Features::fov_changer(Game& game, const config::Config& config, const Player& local) {
    if (!config.misc.fov_changer || local.is_scoped(game)) {
        return;
    }
    const auto services = game.process().read<std::uintptr_t>(local.pawn().address() +
                                                              game.offsets().pawn.camera_services);
    if (services == 0) {
        return;
    }
    const std::uintptr_t at = services + game.offsets().camera_services.fov;
    const auto wanted = std::clamp(config.misc.desired_fov, 1u, 179u);
    const auto current = game.process().read<std::uint32_t>(at);
    // a zero means the game has not set it yet, and writing then fights the game
    if (current != 0 && current != wanted) {
        game.process().write(at, wanted);
    }
}

void Features::find_target(Game& game, const config::Config& config, const Player& local) {
    const config::Weapon weapon = local.weapon(game);
    const config::AimbotConfig& settings = aimbot_settings(config, weapon);
    const WeaponClass weapon_class = config::weapon_class(weapon);
    const auto shots_fired = local.shots_fired(game);

    const Vec2 aim_punch = effective_aim_punch(game, local, weapon_class, shots_fired,
                                               target_.previous_aim_punch, 2.0f);

    Target next;
    next.previous_aim_punch = aim_punch;
    if (settings.bones.empty() || game.players().empty()) {
        target_ = next;
        return;
    }

    const Team team = local.pawn().team(game);
    const Vec2 view_angles = local.view_angles(game);
    const Vec3 eye = local.eye_position(game);
    const bool ffa = game.is_ffa();

    float best_fov = 0.0f;
    float best_distance = 0.0f;
    bool found = false;

    for (const Player& player : game.players()) {
        if (!player.is_valid(game)) {
            continue;
        }
        if (!ffa && !settings.target_friendlies && player.pawn().team(game) == team) {
            continue;
        }

        const Vec3 velocity = player.pawn().velocity(game);
        for (const config::Bones bone : settings.bones) {
            const auto index = static_cast<std::size_t>(bone);
            // lead the shot by where the target will be, not where they are
            const Vec3 at = player.bone_position(game, index) +
                            velocity * std::clamp(settings.prediction_time, 0.0f, 0.25f);

            if (settings.visibility_check && !game.has_line_of_sight(eye, at)) {
                continue;
            }

            const float distance = glm::distance(eye, at);
            const Vec2 angle = angle_to(game, local, at, aim_punch);
            const float fov = game_math::angles_to_fov(view_angles, angle);
            const float limit =
                settings.fov * (settings.distance_adjusted_fov ? distance_scale(distance) : 1.0f);
            if (!std::isfinite(fov) || fov > limit) {
                continue;
            }

            const bool better = !found ||
                                (settings.targeting_mode == config::TargetingMode::Fov
                                     ? fov < best_fov
                                     : distance < best_distance);
            if (better) {
                found = true;
                best_fov = fov;
                best_distance = distance;
                next.player = player;
                next.angle = angle;
                next.distance = distance;
                next.bone = index;
            }
        }
    }

    target_ = next;
}

void Features::triggerbot(Game& game, const config::Config& config, const Player& local) {
    const config::Weapon weapon = local.weapon(game);
    const config::TriggerbotConfig& settings = triggerbot_settings(config, weapon);
    if (!settings.enabled) {
        triggerbot_active_ = false;
        return;
    }
    if (!hotkey(game, settings.mode, config.aim.triggerbot_hotkey, triggerbot_active_)) {
        return;
    }
    // a shot is already scheduled
    if (shot_start_.has_value() || shot_end_.has_value()) {
        return;
    }

    if (settings.flash_check && local.is_flashed(game)) {
        return;
    }
    const WeaponClass weapon_class = config::weapon_class(weapon);
    if (settings.scope_check && weapon_class == WeaponClass::Sniper && !local.is_scoped(game)) {
        return;
    }
    if (settings.velocity_check &&
        glm::length(local.pawn().velocity(game)) > settings.velocity_threshold) {
        return;
    }

    const std::optional<Player> under_crosshair = local.crosshair_entity(game);
    if (!under_crosshair.has_value()) {
        return;
    }
    if (!game.is_ffa() &&
        under_crosshair->pawn().team(game) == local.pawn().team(game)) {
        return;
    }

    if (settings.head_only) {
        const Vec3 head = under_crosshair->bone_position(game, static_cast<std::size_t>(config::Bones::Head));
        const Vec2 angle = angle_to(game, local, head, Vec2(0.0f));
        const float fov = game_math::angles_to_fov(local.view_angles(game), angle);
        // the further away a head is, the smaller the angle it covers
        const float distance =
            glm::distance(local.pawn().position(game), under_crosshair->pawn().position(game));
        if (distance <= 0.0f || fov > 3.5f / distance * 100.0f) {
            return;
        }
    }

    // a fixed delay would be a giveaway, so it is drawn from a normal distribution across
    // the configured range
    const auto low = static_cast<float>(settings.delay.start);
    const auto high = static_cast<float>(settings.delay.end);
    static std::mt19937 rng{std::random_device{}()};
    std::normal_distribution<float> distribution((low + high) * 0.5f, (high - low) * 0.5f);
    const auto delay = std::chrono::milliseconds(
        static_cast<std::int64_t>(std::max(distribution(rng), 0.0f)));

    const Clock::time_point now = Clock::now();
    shot_start_ = now + delay;
    shot_end_ = now + delay + std::chrono::milliseconds(settings.shot_duration);
}

void Features::fire_pending_shot(os::Mouse& mouse) {
    const Clock::time_point now = Clock::now();
    if (shot_start_.has_value() && now >= *shot_start_) {
        mouse.press_left();
        shot_start_.reset();
    }
    if (shot_end_.has_value() && now >= *shot_end_) {
        mouse.release_left();
        shot_end_.reset();
    }
}

bool Features::aimbot(Game& game, const config::Config& config, const Player& local,
                      os::Mouse& mouse) {
    const config::Weapon weapon = local.weapon(game);
    const config::AimbotConfig& settings = aimbot_settings(config, weapon);

    if (!settings.enabled || !hotkey(game, settings.mode, config.aim.aimbot_hotkey, aimbot_active_)) {
        aim_inertia_ = Vec2(0.0f);
        return false;
    }
    if (!target_.player.has_value() || !target_.player->is_valid(game)) {
        return false;
    }
    if (!aimable(config::weapon_class(weapon))) {
        return false;
    }
    if (settings.flash_check && local.is_flashed(game)) {
        return false;
    }
    if (local.shots_fired(game) < settings.start_bullet) {
        return false;
    }

    Vec2 aim = local.view_angles(game) - target_.angle;
    if (aim.y < -180.0f) {
        aim.y += 360.0f;
    }
    game_math::clamp_angles(aim);

    // how far the mouse has to travel depends on what the player set their sensitivity to
    const float sensitivity = game.sensitivity() * local.fov_multiplier(game);
    if (!std::isfinite(sensitivity) || sensitivity <= std::numeric_limits<float>::epsilon()) {
        aim_inertia_ = Vec2(0.0f);
        return false;
    }

    const float smoothing = std::clamp(settings.smooth + 1.0f, 1.0f, 20.0f);
    const Vec2 wanted(aim.y / sensitivity * counts_per_degree / smoothing,
                      -aim.x / sensitivity * counts_per_degree / smoothing);

    // the aim eases into the movement instead of jumping to it
    const float alpha = 1.0f - std::clamp(settings.inertia, 0.0f, 1.0f) * 0.5f;
    aim_inertia_ += (wanted - aim_inertia_) * alpha;
    mouse.move(aim_inertia_);

    recoil_previous_ = local.aim_punch(game);
    return true;
}

void Features::recoil_control(Game& game, const config::Config& config, const Player& local,
                              os::Mouse& mouse) {
    const config::Weapon weapon = local.weapon(game);
    const config::RcsConfig& settings = rcs_settings(config, weapon);
    if (!settings.enabled) {
        return;
    }

    const WeaponClass weapon_class = config::weapon_class(weapon);
    if (!has_spray(weapon_class)) {
        return;
    }

    const auto shots_fired = local.shots_fired(game);
    const Vec2 aim_punch =
        effective_aim_punch(game, local, weapon_class, shots_fired, recoil_previous_, 1.0f);

    if (shots_fired < 1) {
        // between sprays there is nothing to correct, and the smoothing starts fresh
        recoil_previous_ = aim_punch;
        recoil_unaccounted_ = Vec2(0.0f);
        recoil_velocity_ = Vec2(0.0f);
        recoil_history_.clear();
        return;
    }

    const float sensitivity = game.sensitivity() * local.fov_multiplier(game);
    if (!std::isfinite(sensitivity) || sensitivity <= std::numeric_limits<float>::epsilon()) {
        return;
    }

    const Vec2 wanted((aim_punch.y - recoil_previous_.y) / sensitivity * 100.0f,
                      -(aim_punch.x - recoil_previous_.x) / sensitivity * 100.0f);
    const Vec2 strength = glm::clamp(Vec2(settings.strength.x, settings.strength.y), Vec2(0.0f),
                                     Vec2(1.0f));
    const Vec2 desired = wanted * strength + recoil_unaccounted_;
    recoil_previous_ = aim_punch;

    const Vec2 track = (desired - recoil_velocity_) * track_scale;
    const Vec2 clamped(
        game_math::soft_clamp_acceleration(
            track.x,
            game_math::max_acceleration_x(recoil_history_, pitch_tuning.multiplier,
                                          pitch_tuning.low, pitch_tuning.high,
                                          pitch_tuning.fallback),
            pitch_tuning.decay),
        game_math::soft_clamp_acceleration(
            track.y,
            game_math::max_acceleration_y(recoil_history_, yaw_tuning.multiplier, yaw_tuning.low,
                                          yaw_tuning.high, yaw_tuning.fallback),
            yaw_tuning.decay));

    recoil_velocity_ += clamped;
    game_math::record_acceleration(recoil_history_, clamped, acceleration_history_max);

    // a mouse only reports whole counts, so the fraction is carried into the next frame
    const Vec2 ready(std::trunc(recoil_velocity_.x), std::trunc(recoil_velocity_.y));
    recoil_unaccounted_ = desired - ready;
    mouse.move(ready);
}

void Features::run(Game& game, const config::Config& config, os::Mouse& mouse) {
    const std::optional<Player>& local = game.local_player();
    if (!local.has_value()) {
        target_ = Target{};
        return;
    }

    // the esp toggle is always a toggle, and its return is the state it just set
    esp_active_ = hotkey(game, config::KeyMode::Toggle, config.player.esp_hotkey, esp_active_);

    for (const Entity& entity : game.entities()) {
        if (const auto* smoke = std::get_if<Smoke>(&entity)) {
            const std::uintptr_t at =
                smoke->entity.address() + game.offsets().smoke.did_smoke_effect;
            if (config.misc.no_smoke && game.process().read<std::uint8_t>(at) == 0) {
                game.process().write<std::uint8_t>(at, 1);
            }
            if (config.misc.change_smoke_color) {
                const std::uintptr_t colour_at =
                    smoke->entity.address() + game.offsets().smoke.smoke_color;
                const ui::Color& wanted = config.misc.smoke_color;
                const std::array<float, 3> target{static_cast<float>(wanted.r),
                                                  static_cast<float>(wanted.g),
                                                  static_cast<float>(wanted.b)};
                if (game.process().read<std::array<float, 3>>(colour_at) != target) {
                    game.process().write(colour_at, target);
                }
            }
        }
    }

    no_flash(game, config, *local);
    fov_changer(game, config, *local);

    if (!mouse.ready()) {
        // everything below moves the mouse, and there is none
        return;
    }

    triggerbot(game, config, *local);
    fire_pending_shot(mouse);

    find_target(game, config, *local);
    if (!aimbot(game, config, *local, mouse)) {
        recoil_control(game, config, *local, mouse);
    }
}

}  // namespace dl::cs2
