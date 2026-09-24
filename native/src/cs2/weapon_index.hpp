#pragma once

#include <array>
#include <cstdint>
#include <utility>

#include "config/weapon.hpp"

namespace dl::cs2 {

/// Maps the game's item definition index to a weapon, generated from
/// shared/src/weapon.rs so the two cannot drift apart.
inline constexpr std::array<std::pair<std::uint16_t, config::Weapon>, 64> weapon_indices{{
    {1, config::Weapon::DesertEagle},
    {2, config::Weapon::DualBerettas},
    {3, config::Weapon::FiveSeven},
    {4, config::Weapon::Glock},
    {7, config::Weapon::AK47},
    {8, config::Weapon::Aug},
    {9, config::Weapon::Awp},
    {10, config::Weapon::Famas},
    {11, config::Weapon::G3SG1},
    {13, config::Weapon::Galil},
    {14, config::Weapon::M249},
    {16, config::Weapon::M4A4},
    {17, config::Weapon::MAC10},
    {19, config::Weapon::P90},
    {23, config::Weapon::MP5},
    {24, config::Weapon::UMP45},
    {25, config::Weapon::XM1014},
    {26, config::Weapon::Bizon},
    {27, config::Weapon::Mag7},
    {28, config::Weapon::Negev},
    {29, config::Weapon::SawedOff},
    {30, config::Weapon::Tec9},
    {31, config::Weapon::Taser},
    {32, config::Weapon::P2000},
    {33, config::Weapon::MP7},
    {34, config::Weapon::MP9},
    {35, config::Weapon::Nova},
    {36, config::Weapon::P250},
    {38, config::Weapon::SCAR20},
    {39, config::Weapon::SG553},
    {40, config::Weapon::SSG08},
    {41, config::Weapon::KnifeGold},
    {42, config::Weapon::KnifeCT},
    {43, config::Weapon::Flashbang},
    {44, config::Weapon::HE},
    {45, config::Weapon::Smoke},
    {46, config::Weapon::Molotov},
    {47, config::Weapon::Decoy},
    {48, config::Weapon::Incendiary},
    {49, config::Weapon::C4},
    {57, config::Weapon::Healthshot},
    {59, config::Weapon::KnifeT},
    {60, config::Weapon::M4A1S},
    {61, config::Weapon::Usp},
    {63, config::Weapon::CZ75},
    {64, config::Weapon::Revolver},
    {503, config::Weapon::KnifeClassic},
    {505, config::Weapon::KnifeFlip},
    {506, config::Weapon::KnifeGut},
    {507, config::Weapon::KnifeKarambit},
    {508, config::Weapon::KnifeM9Bayonet},
    {509, config::Weapon::KnifeHuntsman},
    {512, config::Weapon::KnifeFalchion},
    {514, config::Weapon::KnifeBowie},
    {515, config::Weapon::KnifeButterfly},
    {516, config::Weapon::KnifeShadowDaggers},
    {517, config::Weapon::KnifeParacord},
    {518, config::Weapon::KnifeSurvival},
    {519, config::Weapon::KnifeUrsus},
    {520, config::Weapon::KnifeNavaja},
    {521, config::Weapon::KnifeNomad},
    {522, config::Weapon::KnifeStiletto},
    {523, config::Weapon::KnifeTalon},
    {524, config::Weapon::KnifeSkeleton},
}};

constexpr config::Weapon weapon_from_index(std::uint16_t index) {
    for (const auto& [key, weapon] : weapon_indices) {
        if (key == index) {
            return weapon;
        }
    }
    return config::Weapon::None;
}

}  // namespace dl::cs2
