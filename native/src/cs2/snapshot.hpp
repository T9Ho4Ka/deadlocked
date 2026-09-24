#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#include "config/weapon.hpp"
#include "cs2/entity.hpp"
#include "cs2/types.hpp"
#include "math.hpp"

namespace dl::cs2 {

/// What the overlay draws from: one frame's worth of the game, copied out of it so the
/// renderer never touches the game's memory itself. Ported from shared/src/data.rs.
///
/// Everything here is a value. Nothing points back into the game, because by the time the
/// overlay draws, the entity it came from may already be gone.

enum class SoundType { Footstep, Gunshot, Weapon };

struct PlayerData {
    std::uint64_t steam_id = 0;
    std::int32_t money = 0;
    Team team = Team::Unassigned;
    std::int32_t health = 0;
    std::int32_t max_health = 0;
    std::int32_t armor = 0;
    Vec3 position{0.0f};
    Vec3 head{0.0f};
    std::string name;
    std::string model_name;
    config::Weapon weapon = config::Weapon::None;
    /// what is in the magazine, and what is left in reserve
    std::int32_t clip_ammo = 0;
    std::int32_t reserve_ammo = 0;
    std::unordered_map<config::Bones, Vec3> bones;
    std::vector<BoneTransform> skeleton;
    bool has_defuser = false;
    bool has_helmet = false;
    bool has_bomb = false;
    bool visible = false;
    std::int32_t color = 0;
    /// yaw the model faces, for drawing which way a player is turned
    float rotation = 0.0f;
    std::optional<SoundType> sound;
    Vec3 collision_mins{0.0f};
    Vec3 collision_maxs{0.0f};
    Mat4 collision_transform{1.0f};
};

struct WeaponInfo {
    config::Weapon weapon = config::Weapon::None;
    Vec3 position{0.0f};
    std::int32_t clip_ammo = 0;
    std::int32_t reserve_ammo = 0;
};

struct GrenadeInfo {
    Vec3 position{0.0f};
    std::string name;
};

struct MolotovInfo {
    Vec3 position{0.0f};
    bool is_incendiary = false;
};

struct InfernoInfo {
    Vec3 position{0.0f};
};

struct ChickenInfo {
    Vec3 position{0.0f};
};

using EntityInfo = std::variant<WeaponInfo, GrenadeInfo, MolotovInfo, InfernoInfo, ChickenInfo>;

struct BombData {
    bool planted = false;
    /// seconds until it goes off
    float timer = 0.0f;
    bool being_defused = false;
    Vec3 position{0.0f};
    float defuse_remaining = 0.0f;
};

struct Snapshot {
    bool in_game = false;
    bool is_ffa = false;
    config::Weapon weapon = config::Weapon::None;
    /// everyone the local player may shoot
    std::vector<PlayerData> players;
    /// everyone on the local player's own side, empty in free for all
    std::vector<PlayerData> friendlies;
    std::vector<std::string> spectators;
    PlayerData local_player;
    std::vector<EntityInfo> entities;
    BombData bomb;
    std::string map_name;
    Mat4 view_matrix{1.0f};
    Vec2 view_angles{0.0f};
    /// where the game's window sits, so the overlay can line up with it
    Vec2 window_position{0.0f};
    Vec2 window_size{1.0f};

    void clear();
};

}  // namespace dl::cs2
