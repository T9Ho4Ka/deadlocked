#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <unordered_map>
#include <variant>
#include <vector>

#include "config/weapon.hpp"
#include "cs2/types.hpp"
#include "math.hpp"

namespace dl::cs2 {

class Game;

/// One bone of a player's skeleton, as the model renderer and the esp both want it.
struct BoneTransform {
    Vec3 position{0.0f};
    Mat4 matrix{1.0f};
    /// 1.0 when the bone can be seen from the local player's eyes, 0.0 otherwise
    float visibility = 1.0f;

    /// The game stores a bone as three floats of position, one of padding, then a
    /// quaternion: eight floats in all.
    static BoneTransform from_memory(const std::array<float, 8>& raw);
};

/// How many bones the game keeps per skinned model, whatever the model actually uses.
inline constexpr std::size_t mesh_skeleton_bone_count = 96;

/// bytes between one bone and the next: three floats, padding, then a quaternion
inline constexpr std::size_t bone_stride = 32;

/// A client entity, addressed by where it lives in the game's memory. Nothing is cached:
/// every getter reads the game again, because the values change every frame and the address
/// can stop being valid between two of them.
class BaseEntity {
public:
    BaseEntity() = default;
    explicit BaseEntity(std::uintptr_t address) : address_(address) {}

    [[nodiscard]] std::uintptr_t address() const { return address_; }
    [[nodiscard]] explicit operator bool() const { return address_ != 0; }
    friend bool operator==(const BaseEntity&, const BaseEntity&) = default;

    [[nodiscard]] std::int32_t health(const Game& game) const;
    [[nodiscard]] std::int32_t max_health(const Game& game) const;
    [[nodiscard]] Team team(const Game& game) const;
    [[nodiscard]] LifeState life_state(const Game& game) const;
    [[nodiscard]] std::uintptr_t game_scene_node(const Game& game) const;
    [[nodiscard]] Vec3 position(const Game& game) const;
    [[nodiscard]] Vec3 velocity(const Game& game) const;
    /// mins and maxs of the entity's collision box, both zero when it has none
    [[nodiscard]] std::pair<Vec3, Vec3> collision_bounds(const Game& game) const;
    /// rotation and translation of the collision box, identity when the entity has no node
    [[nodiscard]] Mat4 collision_transform(const Game& game) const;
    [[nodiscard]] bool dormant(const Game& game) const;

private:
    std::uintptr_t address_ = 0;
};

/// A player, which the game splits in two: the controller carries the name and the score,
/// the pawn carries everything that exists in the world.
class Player {
public:
    Player() = default;
    /// a pawn only player, used for entities that have no controller of their own
    static Player from_pawn(std::uintptr_t pawn) { return Player(0, BaseEntity(pawn)); }
    static std::optional<Player> from_controller(const Game& game, std::uintptr_t controller);
    static std::optional<Player> local(const Game& game);

    [[nodiscard]] std::uintptr_t controller() const { return controller_; }
    [[nodiscard]] const BaseEntity& pawn() const { return pawn_; }
    friend bool operator==(const Player&, const Player&) = default;

    [[nodiscard]] std::string name(const Game& game) const;
    [[nodiscard]] std::uint64_t steam_id(const Game& game) const;
    [[nodiscard]] std::int32_t money(const Game& game) const;
    [[nodiscard]] std::int32_t armor(const Game& game) const;
    [[nodiscard]] std::int32_t color(const Game& game) const;
    [[nodiscard]] Vec3 eye_position(const Game& game) const;
    [[nodiscard]] Vec2 view_angles(const Game& game) const;
    /// yaw the model is facing, which is not the same as where the player is looking
    [[nodiscard]] float rotation(const Game& game) const;
    [[nodiscard]] bool is_scoped(const Game& game) const;
    [[nodiscard]] bool is_flashed(const Game& game) const;
    [[nodiscard]] bool deathmatch_immunity(const Game& game) const;
    [[nodiscard]] config::Weapon weapon(const Game& game) const;
    /// Everything in the player's inventory, empty when the game reports a silly count.
    [[nodiscard]] std::vector<config::Weapon> all_weapons(const Game& game) const;
    [[nodiscard]] bool has_bomb(const Game& game) const;
    [[nodiscard]] std::int32_t clip_ammo(const Game& game) const;
    [[nodiscard]] std::int32_t reserve_ammo(const Game& game) const;

    [[nodiscard]] std::string model_name(const Game& game) const;
    [[nodiscard]] Vec3 bone_position(const Game& game, std::size_t bone) const;
    /// The whole skeleton in one read, with each bone's visibility already filled in.
    [[nodiscard]] std::vector<BoneTransform> skeleton(const Game& game,
                                                      const Player& local) const;
    /// Positions of the bones the esp draws, taken from a skeleton already read.
    [[nodiscard]] static std::unordered_map<config::Bones, Vec3> bone_positions(
        const std::vector<BoneTransform>& skeleton);

    [[nodiscard]] std::int64_t spotted_mask(const Game& game) const;
    /// Whether the game itself says the local player can see this one.
    [[nodiscard]] bool spotted_by_local(const Game& game) const;
    [[nodiscard]] std::int32_t shots_fired(const Game& game) const;
    [[nodiscard]] float fov_multiplier(const Game& game) const;
    [[nodiscard]] bool is_in_air(const Game& game) const;
    [[nodiscard]] bool has_defuser(const Game& game) const;
    [[nodiscard]] bool has_helmet(const Game& game) const;
    /// The player under this one's crosshair, empty when there is none or it is not alive.
    [[nodiscard]] std::optional<Player> crosshair_entity(const Game& game) const;
    [[nodiscard]] std::optional<std::int32_t> round_kills(const Game& game) const;
    [[nodiscard]] std::optional<float> round_damage(const Game& game) const;

    /// alive, visible to the game, and not protected by spawn immunity
    [[nodiscard]] bool is_valid(const Game& game) const;

private:
    /// address of the active weapon entity, zero when the player holds nothing
    [[nodiscard]] std::uintptr_t weapon_address(const Game& game) const;
    /// address of the bone array, zero when the model has no skeleton
    [[nodiscard]] std::uintptr_t skeleton_address(const Game& game) const;

    Player(std::uintptr_t controller, BaseEntity pawn) : controller_(controller), pawn_(pawn) {}

    std::uintptr_t controller_ = 0;
    BaseEntity pawn_;
};

struct Smoke {
    BaseEntity entity;
};
struct Molotov {
    BaseEntity entity;
    bool is_incendiary = false;
};
struct Inferno {
    BaseEntity entity;
};
struct Chicken {
    BaseEntity entity;
};
struct Flashbang {
    BaseEntity entity;
};
struct HeGrenade {
    BaseEntity entity;
};
struct Decoy {
    BaseEntity entity;
};
struct DroppedWeapon {
    BaseEntity entity;
    config::Weapon weapon = config::Weapon::None;
};

using Entity = std::variant<Smoke, Molotov, Inferno, Chicken, Flashbang, HeGrenade, Decoy,
                            DroppedWeapon>;

/// The bomb once it is planted, which the game keeps as its own entity.
class PlantedC4 {
public:
    PlantedC4() = default;
    explicit PlantedC4(std::uintptr_t address) : entity_(address) {}

    [[nodiscard]] const BaseEntity& entity() const { return entity_; }
    [[nodiscard]] bool is_ticking(const Game& game) const;
    [[nodiscard]] bool is_defused(const Game& game) const;
    [[nodiscard]] bool has_exploded(const Game& game) const;
    [[nodiscard]] bool being_defused(const Game& game) const;
    [[nodiscard]] float blow_time(const Game& game) const;
    [[nodiscard]] float defuse_time(const Game& game) const;
    /// ticking, not yet defused and not yet gone off
    [[nodiscard]] bool is_relevant(const Game& game) const;

private:
    BaseEntity entity_;
};

/// Weapon an entity holds, read through its econ item view.
config::Weapon weapon_from_entity(const Game& game, std::uintptr_t entity);
config::Weapon weapon_from_handle(const Game& game, std::int32_t handle);

}  // namespace dl::cs2
