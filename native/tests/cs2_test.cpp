#include <cstdint>

#include "check.hpp"
#include <cmath>

#include "cs2/entity.hpp"
#include "cs2/input.hpp"
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

    // --- bone transforms ---
    // three floats of position, one of padding, then x y z w of the rotation
    const std::array<float, 8> upright{1.0f, 2.0f, 3.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f};
    const cs2::BoneTransform bone = cs2::BoneTransform::from_memory(upright);
    check(bone.position == Vec3(1.0f, 2.0f, 3.0f), "bone position comes from the first three");
    check(bone.visibility == 1.0f, "a fresh bone starts visible");
    const Vec3 translation(bone.matrix[3]);
    check(translation == bone.position, "the matrix carries the position");
    check(bone.matrix[0][0] == 1.0f && bone.matrix[1][1] == 1.0f,
          "an identity rotation leaves the matrix unrotated");

    // a quarter turn about z has to move x onto y
    const float half = std::sqrt(0.5f);
    const std::array<float, 8> turned{0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, half, half};
    const cs2::BoneTransform rotated = cs2::BoneTransform::from_memory(turned);
    const Vec4 moved = rotated.matrix * Vec4(1.0f, 0.0f, 0.0f, 1.0f);
    check(dl::test::close(moved.x, 0.0f, 1e-4f) && dl::test::close(moved.y, 1.0f, 1e-4f),
          "a quarter turn about z sends x to y");

    // a freed node reads back as zeroes or nans, and normalising either poisons the matrix
    const std::array<float, 8> zeroed{5.0f, 6.0f, 7.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    const cs2::BoneTransform degenerate = cs2::BoneTransform::from_memory(zeroed);
    bool finite = true;
    for (int column = 0; column < 4; ++column) {
        for (int row = 0; row < 4; ++row) {
            finite = finite && std::isfinite(degenerate.matrix[column][row]);
        }
    }
    check(finite, "a zero quaternion still yields a finite matrix");
    check(degenerate.position == Vec3(5.0f, 6.0f, 7.0f), "and keeps the position it read");

    const float nan = std::numeric_limits<float>::quiet_NaN();
    const std::array<float, 8> broken{0.0f, 0.0f, 0.0f, 0.0f, nan, nan, nan, nan};
    const cs2::BoneTransform poisoned = cs2::BoneTransform::from_memory(broken);
    bool clean = true;
    for (int column = 0; column < 4; ++column) {
        for (int row = 0; row < 4; ++row) {
            clean = clean && std::isfinite(poisoned.matrix[column][row]);
        }
    }
    check(clean, "a nan quaternion does not leak nans into the matrix");

    // --- bone indices map into the skeleton, gaps and all ---
    std::vector<cs2::BoneTransform> skeleton(cs2::mesh_skeleton_bone_count);
    for (std::size_t i = 0; i < skeleton.size(); ++i) {
        skeleton[i].position = Vec3(static_cast<float>(i), 0.0f, 0.0f);
    }
    const auto positions = cs2::Player::bone_positions(skeleton);
    check(positions.size() == config::bone_count, "every bone gets a position");
    check(positions.at(config::Bones::Head).x == 7.0f, "head reads skeleton slot 7");
    check(positions.at(config::Bones::Hip).x == 1.0f, "hip reads skeleton slot 1");
    check(positions.at(config::Bones::RightFoot).x == 22.0f, "right foot reads skeleton slot 22");

    check(cs2::mesh_skeleton_bone_count == 96 && cs2::bone_stride == 32,
          "skeleton shape matches the game's");

    // --- bit set indexing, which the input layer reads key state through ---
    // 0b0000_0101 -> bits 0 and 2 of the first byte
    const std::array<std::uint8_t, 4> bits{0b0000'0101, 0b1000'0000, 0x00, 0xFF};
    check(cs2::test_bit(bits, 0) && !cs2::test_bit(bits, 1) && cs2::test_bit(bits, 2),
          "bits are read from the least significant end");
    check(cs2::test_bit(bits, 15), "bit 15 is the top bit of the second byte");
    check(!cs2::test_bit(bits, 16) && !cs2::test_bit(bits, 23), "an empty byte reads as unset");
    check(cs2::test_bit(bits, 24) && cs2::test_bit(bits, 31), "a full byte reads as set");
    // an index past the end must answer false rather than read off the end of the buffer
    check(!cs2::test_bit(bits, 32) && !cs2::test_bit(bits, 100000),
          "an index past the end is not pressed, and does not read out of bounds");

    check(cs2::Input::state_bytes * 8 == cs2::Input::max_keys, "the state covers every key");
    // every key code has to fit in the state the game exposes
    bool keys_fit = true;
    for (const auto& entry : config::keycode_entries) {
        if (static_cast<std::size_t>(entry.value) >= cs2::Input::max_keys) {
            keys_fit = false;
        }
    }
    check(keys_fit, "every key code indexes inside that state");

    return dl::test::report();
}
