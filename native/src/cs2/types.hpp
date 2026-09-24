#pragma once

#include <cstdint>
#include <string_view>

namespace dl::cs2 {

/// Ported from shared/src/team.rs, values are the game's own.
enum class Team : std::uint8_t {
    Unassigned = 0,
    Spectator = 1,
    T = 2,
    CT = 3,
};

constexpr bool is_playing(Team team) { return team == Team::T || team == Team::CT; }

/// Anything the game reports that is not one of the four is treated as unassigned, the same
/// way the rust TryFrom falls back to the default.
constexpr Team team_from(std::uint8_t value) {
    switch (value) {
        case 0: return Team::Unassigned;
        case 1: return Team::Spectator;
        case 2: return Team::T;
        case 3: return Team::CT;
        default: return Team::Unassigned;
    }
}

/// Ported from cheat/src/cs2/class/life_state.rs
enum class LifeState : std::uint8_t {
    Alive = 0,
    Dying = 1,
    Dead = 2,
    Respawnable = 3,
    Respawning = 4,
};

constexpr bool is_alive(LifeState state) { return state == LifeState::Alive; }

constexpr LifeState life_state_from(std::uint8_t value) {
    switch (value) {
        case 0: return LifeState::Alive;
        case 1: return LifeState::Dying;
        case 2: return LifeState::Dead;
        case 3: return LifeState::Respawnable;
        case 4: return LifeState::Respawning;
        // an unknown state counts as dead, which is the rust default
        default: return LifeState::Dead;
    }
}

/// Mangled class names as they appear in the game's rtti, used to tell entities apart.
/// Ported from the `class` module of cheat/src/constants.rs.
namespace class_name {
inline constexpr std::string_view player_controller = "19CCSPlayerController";
inline constexpr std::string_view planted_c4 = "11C_PlantedC4";
inline constexpr std::string_view inferno = "9C_Inferno";
inline constexpr std::string_view smoke = "24C_SmokeGrenadeProjectile";
inline constexpr std::string_view molotov = "19C_MolotovProjectile";
inline constexpr std::string_view flashbang = "21C_FlashbangProjectile";
inline constexpr std::string_view he_grenade = "21C_HEGrenadeProjectile";
inline constexpr std::string_view decoy = "17C_DecoyProjectile";
inline constexpr std::string_view chicken = "9C_Chicken";
}  // namespace class_name

}  // namespace dl::cs2
