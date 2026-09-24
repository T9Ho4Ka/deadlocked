#pragma once

#include "config/weapon.hpp"

namespace dl::config {

/// Ported from shared/src/weapon_class.rs. The aimbot treats these differently, and the
/// sniper crosshair only shows for one of them.
enum class WeaponClass {
    Unknown,
    Knife,
    Pistol,
    Smg,
    /// negev and m249
    Heavy,
    Shotgun,
    /// every rifle that is not a sniper
    Rifle,
    Sniper,
    Grenade,
    /// the taser
    Utility,
};

/// Generated from the rust match, so the two cannot drift apart.
constexpr WeaponClass weapon_class(Weapon weapon) {
    switch (weapon) {
        case Weapon::None: return WeaponClass::Unknown;
        case Weapon::CZ75: return WeaponClass::Pistol;
        case Weapon::DesertEagle: return WeaponClass::Pistol;
        case Weapon::DualBerettas: return WeaponClass::Pistol;
        case Weapon::FiveSeven: return WeaponClass::Pistol;
        case Weapon::Glock: return WeaponClass::Pistol;
        case Weapon::P2000: return WeaponClass::Pistol;
        case Weapon::P250: return WeaponClass::Pistol;
        case Weapon::Revolver: return WeaponClass::Pistol;
        case Weapon::Tec9: return WeaponClass::Pistol;
        case Weapon::Usp: return WeaponClass::Pistol;
        case Weapon::MAC10: return WeaponClass::Smg;
        case Weapon::MP5: return WeaponClass::Smg;
        case Weapon::MP7: return WeaponClass::Smg;
        case Weapon::MP9: return WeaponClass::Smg;
        case Weapon::P90: return WeaponClass::Smg;
        case Weapon::Bizon: return WeaponClass::Smg;
        case Weapon::UMP45: return WeaponClass::Smg;
        case Weapon::Mag7: return WeaponClass::Shotgun;
        case Weapon::Nova: return WeaponClass::Shotgun;
        case Weapon::SawedOff: return WeaponClass::Shotgun;
        case Weapon::XM1014: return WeaponClass::Shotgun;
        case Weapon::M249: return WeaponClass::Heavy;
        case Weapon::Negev: return WeaponClass::Heavy;
        case Weapon::AK47: return WeaponClass::Rifle;
        case Weapon::Aug: return WeaponClass::Rifle;
        case Weapon::Famas: return WeaponClass::Rifle;
        case Weapon::Galil: return WeaponClass::Rifle;
        case Weapon::M4A1S: return WeaponClass::Rifle;
        case Weapon::M4A4: return WeaponClass::Rifle;
        case Weapon::SG553: return WeaponClass::Rifle;
        case Weapon::Awp: return WeaponClass::Sniper;
        case Weapon::G3SG1: return WeaponClass::Sniper;
        case Weapon::SCAR20: return WeaponClass::Sniper;
        case Weapon::SSG08: return WeaponClass::Sniper;
        case Weapon::KnifeCT: return WeaponClass::Knife;
        case Weapon::KnifeT: return WeaponClass::Knife;
        case Weapon::KnifeBayonet: return WeaponClass::Knife;
        case Weapon::KnifeBowie: return WeaponClass::Knife;
        case Weapon::KnifeButterfly: return WeaponClass::Knife;
        case Weapon::KnifeClassic: return WeaponClass::Knife;
        case Weapon::KnifeFalchion: return WeaponClass::Knife;
        case Weapon::KnifeFlip: return WeaponClass::Knife;
        case Weapon::KnifeGut: return WeaponClass::Knife;
        case Weapon::KnifeHuntsman: return WeaponClass::Knife;
        case Weapon::KnifeKarambit: return WeaponClass::Knife;
        case Weapon::KnifeKukri: return WeaponClass::Knife;
        case Weapon::KnifeM9Bayonet: return WeaponClass::Knife;
        case Weapon::KnifeNavaja: return WeaponClass::Knife;
        case Weapon::KnifeNomad: return WeaponClass::Knife;
        case Weapon::KnifeParacord: return WeaponClass::Knife;
        case Weapon::KnifeShadowDaggers: return WeaponClass::Knife;
        case Weapon::KnifeSkeleton: return WeaponClass::Knife;
        case Weapon::KnifeStiletto: return WeaponClass::Knife;
        case Weapon::KnifeSurvival: return WeaponClass::Knife;
        case Weapon::KnifeTalon: return WeaponClass::Knife;
        case Weapon::KnifeUrsus: return WeaponClass::Knife;
        case Weapon::KnifeGold: return WeaponClass::Knife;
        case Weapon::Flashbang: return WeaponClass::Grenade;
        case Weapon::HE: return WeaponClass::Grenade;
        case Weapon::Smoke: return WeaponClass::Grenade;
        case Weapon::Molotov: return WeaponClass::Grenade;
        case Weapon::Decoy: return WeaponClass::Grenade;
        case Weapon::Incendiary: return WeaponClass::Grenade;
        case Weapon::Taser: return WeaponClass::Utility;
        case Weapon::C4: return WeaponClass::Utility;
        case Weapon::Healthshot: return WeaponClass::Utility;
    }
    return WeaponClass::Unknown;
}

}  // namespace dl::config
