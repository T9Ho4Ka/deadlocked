#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "config/enums.hpp"
#include "config/keycode.hpp"
#include "config/weapon.hpp"

namespace dl::config {

enum class KeyMode { Hold, Toggle };

inline constexpr std::array<EnumEntry<KeyMode>, 2> key_mode_entries{{
    {KeyMode::Hold, "Hold", "hold"},
    {KeyMode::Toggle, "Toggle", "toggle"},
}};
constexpr const auto& enum_entries(KeyMode) { return key_mode_entries; }

enum class TargetingMode { Fov, Distance };

inline constexpr std::array<EnumEntry<TargetingMode>, 2> targeting_mode_entries{{
    {TargetingMode::Fov, "FOV", "fov"},
    {TargetingMode::Distance, "Distance", "distance"},
}};
constexpr const auto& enum_entries(TargetingMode) { return targeting_mode_entries; }

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;
    friend bool operator==(const Vec2&, const Vec2&) = default;
};

/// inclusive millisecond range, the rust side is a RangeInclusive<u64>
struct Range {
    std::uint64_t start = 0;
    std::uint64_t end = 0;
    friend bool operator==(const Range&, const Range&) = default;
};

struct AimbotConfig {
    bool enable_override = false;
    bool enabled = true;
    KeyMode mode = KeyMode::Hold;
    bool target_friendlies = false;
    bool distance_adjusted_fov = true;
    int start_bullet = 0;
    bool visibility_check = true;
    bool flash_check = true;
    float fov = 2.5f;
    float smooth = 5.0f;
    float inertia = 1.0f;
    /// seconds of target motion to lead when aiming
    float prediction_time = 0.05f;
    std::vector<Bones> bones{Bones::Head,   Bones::Neck,   Bones::Spine4, Bones::Spine3,
                             Bones::Spine2, Bones::Spine1, Bones::Hip};
    TargetingMode targeting_mode = TargetingMode::Fov;

    friend bool operator==(const AimbotConfig&, const AimbotConfig&) = default;
};

struct RcsConfig {
    bool enable_override = false;
    bool enabled = false;
    Vec2 strength{0.5f, 0.5f};

    friend bool operator==(const RcsConfig&, const RcsConfig&) = default;
};

struct TriggerbotConfig {
    bool enable_override = false;
    bool enabled = false;
    Range delay{100, 200};
    std::uint64_t shot_duration = 200;
    KeyMode mode = KeyMode::Hold;
    bool flash_check = true;
    bool scope_check = true;
    bool velocity_check = true;
    float velocity_threshold = 100.0f;
    bool head_only = false;

    friend bool operator==(const TriggerbotConfig&, const TriggerbotConfig&) = default;
};

struct WeaponConfig {
    AimbotConfig aimbot;
    RcsConfig rcs;
    TriggerbotConfig triggerbot;

    friend bool operator==(const WeaponConfig&, const WeaponConfig&) = default;
};

struct AimConfig {
    KeyCode aimbot_hotkey = KeyCode::Mouse5;
    KeyCode triggerbot_hotkey = KeyCode::Mouse4;
    /// settings used unless a weapon overrides them
    WeaponConfig global;
    /// one entry per weapon, indexed by the enum. the rust side keeps a hash map that is
    /// always filled for every weapon, an array says the same thing without the lookups.
    std::array<WeaponConfig, weapon_count> weapons;

    AimConfig() {
        // the global block is what the rust default calls WeaponConfig::enabled(true)
        global.aimbot.enable_override = true;
    }

    WeaponConfig& operator[](Weapon weapon) {
        return weapons[static_cast<std::size_t>(weapon)];
    }
    const WeaponConfig& operator[](Weapon weapon) const {
        return weapons[static_cast<std::size_t>(weapon)];
    }
};

}  // namespace dl::config
