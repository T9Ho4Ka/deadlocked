#include <cstdint>

#include "check.hpp"
#include "cs2/types.hpp"
#include "cs2/weapon_index.hpp"

using namespace dl;
using dl::test::check;

int main() {
    // --- team ---
    check(cs2::team_from(0) == cs2::Team::Unassigned, "team 0 is unassigned");
    check(cs2::team_from(2) == cs2::Team::T, "team 2 is t");
    check(cs2::team_from(3) == cs2::Team::CT, "team 3 is ct");
    // the game can hand back anything, and the rust TryFrom falls back to the default
    check(cs2::team_from(99) == cs2::Team::Unassigned, "an unknown team falls back");
    check(cs2::is_playing(cs2::Team::T) && cs2::is_playing(cs2::Team::CT), "t and ct play");
    check(!cs2::is_playing(cs2::Team::Spectator) && !cs2::is_playing(cs2::Team::Unassigned),
          "spectators and the unassigned do not");

    // --- life state ---
    check(cs2::life_state_from(0) == cs2::LifeState::Alive, "life state 0 is alive");
    check(cs2::life_state_from(4) == cs2::LifeState::Respawning, "life state 4 is respawning");
    check(cs2::life_state_from(200) == cs2::LifeState::Dead, "an unknown life state is dead");
    check(cs2::is_alive(cs2::LifeState::Alive), "only alive counts as alive");
    check(!cs2::is_alive(cs2::LifeState::Dying) && !cs2::is_alive(cs2::LifeState::Respawnable),
          "dying and respawnable do not count as alive");

    // --- weapon indices ---
    check(cs2::weapon_from_index(1) == config::Weapon::DesertEagle, "index 1 is a deagle");
    check(cs2::weapon_from_index(7) == config::Weapon::AK47, "index 7 is an ak");
    check(cs2::weapon_from_index(9) == config::Weapon::Awp, "index 9 is an awp");
    check(cs2::weapon_from_index(0) == config::Weapon::None, "index 0 is nothing");
    check(cs2::weapon_from_index(60000) == config::Weapon::None, "an unknown index is nothing");

    // no index may map to two weapons, which a careless merge of the table could produce
    bool unique = true;
    for (std::size_t i = 0; i < cs2::weapon_indices.size(); ++i) {
        for (std::size_t j = i + 1; j < cs2::weapon_indices.size(); ++j) {
            if (cs2::weapon_indices[i].first == cs2::weapon_indices[j].first) {
                unique = false;
            }
        }
    }
    check(unique, "every item definition index appears once");

    bool all_resolve = true;
    for (const auto& [index, weapon] : cs2::weapon_indices) {
        if (cs2::weapon_from_index(index) != weapon) {
            all_resolve = false;
        }
        if (weapon == config::Weapon::None) {
            all_resolve = false;
        }
    }
    check(all_resolve, "all 64 indices resolve to a real weapon");

    // --- mangled class names ---
    // these are matched against what the game reports; a typo means an entity type simply
    // never shows up, with nothing to indicate why
    check(cs2::class_name::player_controller == "19CCSPlayerController", "player controller name");
    check(cs2::class_name::planted_c4 == "11C_PlantedC4", "planted c4 name");
    check(cs2::class_name::smoke == "24C_SmokeGrenadeProjectile", "smoke name");
    check(cs2::class_name::molotov == "19C_MolotovProjectile", "molotov name");
    check(cs2::class_name::flashbang == "21C_FlashbangProjectile", "flashbang name");
    check(cs2::class_name::he_grenade == "21C_HEGrenadeProjectile", "he grenade name");
    check(cs2::class_name::decoy == "17C_DecoyProjectile", "decoy name");
    check(cs2::class_name::inferno == "9C_Inferno", "inferno name");
    check(cs2::class_name::chicken == "9C_Chicken", "chicken name");

    // the number prefix is the length of the rest, which is how itanium mangles a name
    const auto prefix_matches = [](std::string_view mangled) {
        std::size_t digits = 0;
        while (digits < mangled.size() && mangled[digits] >= '0' && mangled[digits] <= '9') {
            ++digits;
        }
        if (digits == 0) {
            return false;
        }
        std::size_t declared = 0;
        for (std::size_t i = 0; i < digits; ++i) {
            declared = declared * 10 + static_cast<std::size_t>(mangled[i] - '0');
        }
        return declared == mangled.size() - digits;
    };
    check(prefix_matches(cs2::class_name::player_controller) &&
              prefix_matches(cs2::class_name::planted_c4) &&
              prefix_matches(cs2::class_name::smoke) && prefix_matches(cs2::class_name::molotov) &&
              prefix_matches(cs2::class_name::flashbang) &&
              prefix_matches(cs2::class_name::he_grenade) &&
              prefix_matches(cs2::class_name::decoy) && prefix_matches(cs2::class_name::inferno) &&
              prefix_matches(cs2::class_name::chicken),
          "every mangled name's length prefix matches the name");

    return dl::test::report();
}
