#pragma once

#include <array>
#include <string_view>

#include "config/weapon.hpp"
#include "cs2/types.hpp"

namespace dl::net {

/// How serde names each weapon in json, taken from serde itself rather than guessed: the
/// derive renames the variants to snake case, which turns AK47 into "a_k47" and not
/// "ak47". The web radar looks these up by name, so a wrong one shows no weapon at all.
inline constexpr std::array<std::string_view, config::weapon_count> weapon_json_names{{
    "none",
    "c_z75",
    "desert_eagle",
    "dual_berettas",
    "five_seven",
    "glock",
    "p2000",
    "p250",
    "revolver",
    "tec9",
    "usp",
    "m_a_c10",
    "m_p5",
    "m_p7",
    "m_p9",
    "p90",
    "bizon",
    "u_m_p45",
    "mag7",
    "nova",
    "sawed_off",
    "x_m1014",
    "m249",
    "negev",
    "a_k47",
    "aug",
    "famas",
    "galil",
    "m4_a1_s",
    "m4_a4",
    "s_g553",
    "awp",
    "g3_s_g1",
    "s_c_a_r20",
    "s_s_g08",
    "knife_c_t",
    "knife_t",
    "knife_bayonet",
    "knife_bowie",
    "knife_butterfly",
    "knife_classic",
    "knife_falchion",
    "knife_flip",
    "knife_gut",
    "knife_huntsman",
    "knife_karambit",
    "knife_kukri",
    "knife_m9_bayonet",
    "knife_navaja",
    "knife_nomad",
    "knife_paracord",
    "knife_shadow_daggers",
    "knife_skeleton",
    "knife_stiletto",
    "knife_survival",
    "knife_talon",
    "knife_ursus",
    "knife_gold",
    "flashbang",
    "h_e",
    "smoke",
    "molotov",
    "decoy",
    "incendiary",
    "taser",
    "c4",
    "healthshot",
}};

constexpr std::string_view weapon_json_name(config::Weapon weapon) {
    const auto index = static_cast<std::size_t>(weapon);
    return index < weapon_json_names.size() ? weapon_json_names[index] : "none";
}

/// Team is not renamed, so its variants keep their declared names.
constexpr std::string_view team_json_name(cs2::Team team) {
    switch (team) {
        case cs2::Team::Unassigned: return "Unassigned";
        case cs2::Team::Spectator: return "Spectator";
        case cs2::Team::T: return "T";
        case cs2::Team::CT: return "CT";
    }
    return "Unassigned";
}

}  // namespace dl::net
