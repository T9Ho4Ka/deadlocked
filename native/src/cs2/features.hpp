#pragma once

#include <chrono>
#include <cstddef>
#include <optional>

#include "config/config.hpp"
#include "cs2/entity.hpp"
#include "game_math.hpp"
#include "os/mouse.hpp"

namespace dl::cs2 {

class Game;

/// Which player, and which of their bones, the aim is currently working towards.
struct Target {
    std::optional<Player> player;
    /// where the aim would have to point, already corrected for recoil
    Vec2 angle{0.0f};
    float distance = 0.0f;
    std::size_t bone = 0;
    /// the previous frame's recoil, kept because the game reports zero between shots
    Vec2 previous_aim_punch{0.0f};
};

/// Everything that acts on the game rather than just reading it.
///
/// This is the only part of the port that writes: to the game's memory for the flash and
/// field of view settings, and to a virtual mouse for anything that moves the aim.
class Features {
public:
    /// Runs one pass. Does nothing that needs a mouse when there is no mouse.
    void run(Game& game, const config::Config& config, os::Mouse& mouse);

    [[nodiscard]] bool esp_enabled(const config::Config& config) const {
        return config.player.enabled && esp_active_;
    }
    [[nodiscard]] bool aimbot_active() const { return aimbot_active_; }
    [[nodiscard]] bool triggerbot_active() const { return triggerbot_active_; }
    [[nodiscard]] const Target& target() const { return target_; }

private:
    using Clock = std::chrono::steady_clock;

    /// Picks the weapon's own settings when it overrides the global ones.
    [[nodiscard]] static const config::AimbotConfig& aimbot_settings(
        const config::Config& config, config::Weapon weapon);
    [[nodiscard]] static const config::TriggerbotConfig& triggerbot_settings(
        const config::Config& config, config::Weapon weapon);
    [[nodiscard]] static const config::RcsConfig& rcs_settings(const config::Config& config,
                                                               config::Weapon weapon);

    /// Hold reports whether the key is down; toggle flips `state` on each press.
    [[nodiscard]] static bool hotkey(const Game& game, config::KeyMode mode,
                                     config::KeyCode key, bool& state);

    void no_flash(Game& game, const config::Config& config, const Player& local);
    void fov_changer(Game& game, const config::Config& config, const Player& local);
    void find_target(Game& game, const config::Config& config, const Player& local);
    void triggerbot(Game& game, const config::Config& config, const Player& local);
    void fire_pending_shot(os::Mouse& mouse);
    [[nodiscard]] bool aimbot(Game& game, const config::Config& config, const Player& local,
                              os::Mouse& mouse);
    void recoil_control(Game& game, const config::Config& config, const Player& local,
                        os::Mouse& mouse);

    bool esp_active_ = true;
    bool aimbot_active_ = false;
    bool triggerbot_active_ = false;

    Target target_;

    /// the aim eases towards its goal rather than snapping, so this carries between frames
    Vec2 aim_inertia_{0.0f};

    /// a shot is scheduled rather than fired at once, so the delay can be random
    std::optional<Clock::time_point> shot_start_;
    std::optional<Clock::time_point> shot_end_;

    Vec2 recoil_previous_{0.0f};
    /// motion asked for but not yet sent, because the mouse only moves whole counts
    Vec2 recoil_unaccounted_{0.0f};
    Vec2 recoil_velocity_{0.0f};
    game_math::AccelerationHistory recoil_history_;
};

}  // namespace dl::cs2
