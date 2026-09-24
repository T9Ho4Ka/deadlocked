#include "cs2/entity.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

#include <glm/gtc/matrix_transform.hpp>

#include "cs2/game.hpp"
#include "config/weapon_class.hpp"
#include "cs2/weapon_index.hpp"

namespace dl::cs2 {

std::int32_t BaseEntity::health(const Game& game) const {
    return game.process().read<std::int32_t>(address_ + game.offsets().entity.health);
}

std::int32_t BaseEntity::max_health(const Game& game) const {
    return game.process().read<std::int32_t>(address_ + game.offsets().entity.max_health);
}

Team BaseEntity::team(const Game& game) const {
    return team_from(game.process().read<std::uint8_t>(address_ + game.offsets().entity.team));
}

LifeState BaseEntity::life_state(const Game& game) const {
    return life_state_from(
        game.process().read<std::uint8_t>(address_ + game.offsets().entity.life_state));
}

std::uintptr_t BaseEntity::game_scene_node(const Game& game) const {
    return game.process().read<std::uintptr_t>(address_ + game.offsets().entity.game_scene_node);
}

Vec3 BaseEntity::position(const Game& game) const {
    const std::uintptr_t node = game_scene_node(game);
    if (node == 0) {
        return Vec3(0.0f);
    }
    return game.process().read<Vec3>(node + game.offsets().game_scene_node.origin);
}

Vec3 BaseEntity::velocity(const Game& game) const {
    // the three components are not adjacent, the game interleaves them with change flags
    const auto& offsets = game.offsets().network_velocity;
    const std::uintptr_t base = address_ + game.offsets().entity.velocity;
    return Vec3(game.process().read<float>(base + offsets.x),
                game.process().read<float>(base + offsets.y),
                game.process().read<float>(base + offsets.z));
}

std::pair<Vec3, Vec3> BaseEntity::collision_bounds(const Game& game) const {
    const auto collision =
        game.process().read<std::uintptr_t>(address_ + game.offsets().entity.collision);
    if (collision == 0) {
        return {Vec3(0.0f), Vec3(0.0f)};
    }
    return {game.process().read<Vec3>(collision + game.offsets().collision.mins),
            game.process().read<Vec3>(collision + game.offsets().collision.maxs)};
}

Mat4 BaseEntity::collision_transform(const Game& game) const {
    const std::uintptr_t node = game_scene_node(game);
    if (node == 0) {
        return Mat4(1.0f);
    }

    const Vec3 origin = position(game);
    const auto values = game.process().read<std::array<float, 4>>(
        node + game.offsets().game_scene_node.node_to_world + 0x10);
    Quat rotation(values[3], values[0], values[1], values[2]);  // glm stores w first

    // a freed or half written node reads back as garbage, and normalising that produces
    // nans that poison every later transform
    const float length = glm::dot(rotation, rotation);
    if (!std::isfinite(length) || length <= std::numeric_limits<float>::epsilon()) {
        rotation = Quat(1.0f, 0.0f, 0.0f, 0.0f);
    } else {
        rotation = glm::normalize(rotation);
    }

    return glm::translate(Mat4(1.0f), origin) * glm::mat4_cast(rotation);
}

bool BaseEntity::dormant(const Game& game) const {
    const std::uintptr_t node = game_scene_node(game);
    if (node == 0) {
        return true;
    }
    return game.process().read<std::uint8_t>(node + game.offsets().game_scene_node.dormant) != 0;
}

BoneTransform BoneTransform::from_memory(const std::array<float, 8>& raw) {
    BoneTransform bone;
    bone.position = Vec3(raw[0], raw[1], raw[2]);

    // raw[3] is padding the game leaves between the position and the rotation
    Quat rotation(raw[7], raw[4], raw[5], raw[6]);  // glm stores w first
    const float length = glm::dot(rotation, rotation);
    if (!std::isfinite(length) || length <= std::numeric_limits<float>::epsilon()) {
        rotation = Quat(1.0f, 0.0f, 0.0f, 0.0f);
    } else {
        rotation = glm::normalize(rotation);
    }

    bone.matrix = glm::translate(Mat4(1.0f), bone.position) * glm::mat4_cast(rotation);
    return bone;
}

std::optional<Player> Player::from_controller(const Game& game, std::uintptr_t controller) {
    const auto pawn_handle =
        game.process().read<std::int32_t>(controller + game.offsets().controller.pawn);
    if (pawn_handle == -1) {
        return std::nullopt;
    }
    const std::uintptr_t pawn = game.entity_from_handle(pawn_handle);
    if (pawn == 0) {
        return std::nullopt;
    }
    return Player(controller, BaseEntity(pawn));
}

std::optional<Player> Player::local(const Game& game) {
    const auto controller =
        game.process().read<std::uintptr_t>(game.offsets().direct.local_player);
    if (controller == 0) {
        return std::nullopt;
    }
    return from_controller(game, controller);
}

std::string Player::name(const Game& game) const {
    return game.process().read_string(controller_ + game.offsets().controller.name);
}

std::uint64_t Player::steam_id(const Game& game) const {
    return game.process().read<std::uint64_t>(controller_ + game.offsets().controller.steam_id);
}

std::int32_t Player::money(const Game& game) const {
    const auto services =
        game.process().read<std::uintptr_t>(controller_ + game.offsets().controller.money_services);
    if (services == 0) {
        return 0;
    }
    return game.process().read<std::int32_t>(services + game.offsets().controller.money);
}

std::int32_t Player::armor(const Game& game) const {
    return game.process().read<std::int32_t>(pawn_.address() + game.offsets().pawn.armor);
}

std::int32_t Player::color(const Game& game) const {
    return game.process().read<std::int32_t>(controller_ + game.offsets().controller.color);
}

Vec3 Player::eye_position(const Game& game) const {
    const Vec3 offset =
        game.process().read<Vec3>(pawn_.address() + game.offsets().pawn.eye_offset);
    return pawn_.position(game) + offset;
}

Vec2 Player::view_angles(const Game& game) const {
    return game.process().read<Vec2>(pawn_.address() + game.offsets().pawn.view_angles);
}

float Player::rotation(const Game& game) const {
    // yaw sits second in the eye angles triple
    return game.process().read<float>(pawn_.address() + game.offsets().pawn.eye_angles + 0x04);
}

bool Player::is_scoped(const Game& game) const {
    return game.process().read<std::uint8_t>(pawn_.address() + game.offsets().pawn.is_scoped) != 0;
}

bool Player::is_flashed(const Game& game) const {
    return game.process().read<float>(pawn_.address() + game.offsets().pawn.flash_duration) > 0.2f;
}

bool Player::deathmatch_immunity(const Game& game) const {
    return game.process().read<std::uint8_t>(pawn_.address() +
                                             game.offsets().pawn.deathmatch_immunity) != 0;
}

config::Weapon Player::weapon(const Game& game) const {
    const auto services =
        game.process().read<std::uintptr_t>(pawn_.address() + game.offsets().pawn.weapon_services);
    if (services == 0) {
        return config::Weapon::None;
    }
    const auto handle = game.process().read<std::int32_t>(
        services + game.offsets().weapon_services.active_weapon);
    return weapon_from_handle(game, handle);
}

bool Player::is_valid(const Game& game) const {
    return !pawn_.dormant(game) && pawn_.health(game) > 0 && is_alive(pawn_.life_state(game)) &&
           !deathmatch_immunity(game);
}

std::vector<config::Weapon> Player::all_weapons(const Game& game) const {
    std::vector<config::Weapon> weapons;

    const auto services =
        game.process().read<std::uintptr_t>(pawn_.address() + game.offsets().pawn.weapon_services);
    if (services == 0) {
        return weapons;
    }

    const auto length = game.process().read<std::int32_t>(
        services + game.offsets().weapon_services.weapons);
    // nobody carries more than a handful; a larger count means the read went wrong
    if (length <= 0 || length > 10) {
        return weapons;
    }

    const auto list = game.process().read<std::uintptr_t>(
        services + game.offsets().weapon_services.weapons + 0x08);
    if (list == 0) {
        return weapons;
    }

    for (std::int32_t i = 0; i < length; ++i) {
        const auto handle = game.process().read<std::int32_t>(list + 0x04 * i);
        const config::Weapon weapon = weapon_from_handle(game, handle);
        if (weapon != config::Weapon::None) {
            weapons.push_back(weapon);
        }
    }
    return weapons;
}

bool Player::has_bomb(const Game& game) const {
    const std::vector<config::Weapon> weapons = all_weapons(game);
    return std::ranges::find(weapons, config::Weapon::C4) != weapons.end();
}

std::int32_t Player::clip_ammo(const Game& game) const {
    const std::uintptr_t weapon = weapon_address(game);
    if (weapon == 0) {
        return 0;
    }
    return game.process().read<std::int32_t>(weapon + game.offsets().weapon.clip_primary);
}

std::int32_t Player::reserve_ammo(const Game& game) const {
    const std::uintptr_t weapon = weapon_address(game);
    if (weapon == 0) {
        return 0;
    }
    return game.process().read<std::int32_t>(weapon + game.offsets().weapon.reserve_ammo);
}

std::uintptr_t Player::weapon_address(const Game& game) const {
    const auto services =
        game.process().read<std::uintptr_t>(pawn_.address() + game.offsets().pawn.weapon_services);
    if (services == 0) {
        return 0;
    }
    const auto handle = game.process().read<std::int32_t>(
        services + game.offsets().weapon_services.active_weapon);
    if (handle == 0) {
        return 0;
    }
    return game.client_entity(static_cast<std::size_t>(handle) & 0xFFF);
}

std::string Player::model_name(const Game& game) const {
    const std::uintptr_t node = pawn_.game_scene_node(game);
    if (node == 0) {
        return {};
    }
    const std::uintptr_t model_state = node + game.offsets().game_scene_node.model_state;
    const auto name =
        game.process().read<std::uintptr_t>(model_state + game.offsets().game_scene_node.model_name);
    if (name == 0) {
        return {};
    }
    return game.process().read_string(name);
}

std::uintptr_t Player::skeleton_address(const Game& game) const {
    const std::uintptr_t node = pawn_.game_scene_node(game);
    if (node == 0) {
        return 0;
    }
    return game.process().read<std::uintptr_t>(node + game.offsets().game_scene_node.model_state +
                                               game.offsets().model_state.skeleton_instance);
}

Vec3 Player::bone_position(const Game& game, std::size_t bone) const {
    const std::uintptr_t bones = skeleton_address(game);
    if (bones == 0) {
        return Vec3(0.0f);
    }
    return game.process().read<Vec3>(bones + bone * bone_stride);
}

std::vector<BoneTransform> Player::skeleton(const Game& game, const Player& local) const {
    std::vector<BoneTransform> result;
    const std::uintptr_t bones = skeleton_address(game);
    if (bones == 0) {
        return result;
    }

    // the bones sit next to each other, so the whole skeleton is one read. the rust client
    // reads them one at a time, which is ninety six syscalls per player per frame.
    const std::vector<std::uint8_t> raw =
        game.process().read_vec(bones, mesh_skeleton_bone_count * bone_stride);
    if (raw.size() < mesh_skeleton_bone_count * bone_stride) {
        return result;
    }

    result.reserve(mesh_skeleton_bone_count);
    for (std::size_t i = 0; i < mesh_skeleton_bone_count; ++i) {
        std::array<float, 8> values{};
        std::memcpy(values.data(), raw.data() + i * bone_stride, sizeof(values));
        result.push_back(BoneTransform::from_memory(values));
    }

    if (game.geometry().empty()) {
        // no collision geometry yet, so the game's own spotted flag is all there is, and it
        // is the same answer for the whole body
        const float spotted = spotted_by_local(game) ? 1.0f : 0.0f;
        for (BoneTransform& bone : result) {
            bone.visibility = spotted;
        }
        return result;
    }

    const Vec3 eye = local.eye_position(game);
    for (BoneTransform& bone : result) {
        bone.visibility = game.has_line_of_sight(eye, bone.position) ? 1.0f : 0.0f;
    }
    return result;
}

std::unordered_map<config::Bones, Vec3> Player::bone_positions(
    const std::vector<BoneTransform>& skeleton) {
    std::unordered_map<config::Bones, Vec3> positions;
    for (const auto& entry : config::bone_entries) {
        const auto index = static_cast<std::size_t>(entry.value);
        if (index < skeleton.size()) {
            positions.emplace(entry.value, skeleton[index].position);
        }
    }
    return positions;
}

std::int64_t Player::spotted_mask(const Game& game) const {
    return game.process().read<std::int64_t>(pawn_.address() + game.offsets().pawn.spotted_state +
                                             game.offsets().spotted_state.mask);
}

bool Player::spotted_by_local(const Game& game) const {
    const std::uint64_t index = game.local_pawn_index();
    if (index >= 64) {
        return false;
    }
    return (spotted_mask(game) & (std::int64_t{1} << index)) != 0;
}

std::int32_t Player::shots_fired(const Game& game) const {
    return game.process().read<std::int32_t>(pawn_.address() + game.offsets().pawn.shots_fired);
}

float Player::fov_multiplier(const Game& game) const {
    return game.process().read<float>(pawn_.address() + game.offsets().pawn.fov_multiplier);
}

bool Player::is_in_air(const Game& game) const {
    const auto flags = game.process().read<std::int32_t>(pawn_.address() + game.offsets().pawn.flags);
    // FL_ONGROUND is the lowest bit, so its absence means the player is off the ground
    return (flags & 1) == 0;
}

bool Player::has_defuser(const Game& game) const {
    const auto services =
        game.process().read<std::uintptr_t>(pawn_.address() + game.offsets().pawn.item_services);
    if (services == 0) {
        return false;
    }
    return game.process().read<std::uint8_t>(services + game.offsets().item_services.has_defuser) != 0;
}

bool Player::has_helmet(const Game& game) const {
    const auto services =
        game.process().read<std::uintptr_t>(pawn_.address() + game.offsets().pawn.item_services);
    if (services == 0) {
        return false;
    }
    return game.process().read<std::uint8_t>(services + game.offsets().item_services.has_helmet) != 0;
}

std::optional<Player> Player::crosshair_entity(const Game& game) const {
    const auto index =
        game.process().read<std::int32_t>(pawn_.address() + game.offsets().pawn.crosshair_entity);
    if (index == -1) {
        return std::nullopt;
    }
    const std::uintptr_t entity = game.client_entity(static_cast<std::size_t>(index));
    if (entity == 0) {
        return std::nullopt;
    }
    const Player player = Player::from_pawn(entity);
    if (!player.is_valid(game)) {
        return std::nullopt;
    }
    return player;
}

std::optional<std::int32_t> Player::round_kills(const Game& game) const {
    const auto services = game.process().read<std::uintptr_t>(
        controller_ + game.offsets().controller.action_tracking_services);
    if (services == 0) {
        return std::nullopt;
    }
    return game.process().read<std::int32_t>(services + game.offsets().action_tracking.round_kills);
}

std::optional<float> Player::round_damage(const Game& game) const {
    const auto services = game.process().read<std::uintptr_t>(
        controller_ + game.offsets().controller.action_tracking_services);
    if (services == 0) {
        return std::nullopt;
    }
    return game.process().read<float>(services + game.offsets().action_tracking.round_damage);
}

Vec2 Player::aim_punch(const Game& game) const {
    const auto services =
        game.process().read<std::uintptr_t>(pawn_.address() + game.offsets().pawn.aim_punch_services);
    if (services == 0) {
        return Vec2(0.0f);
    }

    // the cache is a vector of angle triples; the last entry is the current kick
    const std::uintptr_t cache = services + game.offsets().aim_punch_services.aim_punch_cache;
    const auto length = game.process().read<std::size_t>(cache);
    if (length < 1 || length > 1024) {
        return Vec2(0.0f);
    }
    const auto data = game.process().read<std::uintptr_t>(cache + 0x08);
    if (data == 0) {
        return Vec2(0.0f);
    }
    return game.process().read<Vec2>(data + (length - 1) * 12);
}

std::optional<SoundType> Player::making_sound(const Game& game) const {
    if (shots_fired(game) > 0) {
        return SoundType::Gunshot;
    }

    const Vec3 velocity = pawn_.velocity(game);
    const float speed = glm::length(Vec2(velocity.x, velocity.y));

    // a player who is still, or creeping, makes no sound the game would play
    constexpr float standing_speed = 10.0f;
    constexpr float walking_speed = 100.0f;
    constexpr float audible_speed = 150.0f;
    if (speed > walking_speed || speed < standing_speed) {
        return std::nullopt;
    }

    if (is_scoped(game) &&
        config::weapon_class(weapon(game)) == config::WeaponClass::Sniper) {
        return SoundType::Weapon;
    }

    const bool jumping = velocity.z > 100.0f && is_in_air(game);
    const bool landing = velocity.z < -200.0f;
    if (speed > audible_speed || jumping || landing) {
        return SoundType::Footstep;
    }
    return std::nullopt;
}

bool PlantedC4::is_ticking(const Game& game) const {
    return game.process().read<std::uint8_t>(entity_.address() +
                                             game.offsets().planted_c4.is_ticking) != 0;
}

bool PlantedC4::is_defused(const Game& game) const {
    return game.process().read<std::uint8_t>(entity_.address() +
                                             game.offsets().planted_c4.is_defused) != 0;
}

bool PlantedC4::has_exploded(const Game& game) const {
    return game.process().read<std::uint8_t>(entity_.address() +
                                             game.offsets().planted_c4.has_exploded) != 0;
}

bool PlantedC4::being_defused(const Game& game) const {
    return game.process().read<std::uint8_t>(entity_.address() +
                                             game.offsets().planted_c4.being_defused) != 0;
}

float PlantedC4::blow_time(const Game& game) const {
    return game.process().read<float>(entity_.address() + game.offsets().planted_c4.blow_time);
}

float PlantedC4::defuse_time(const Game& game) const {
    return game.process().read<float>(entity_.address() + game.offsets().planted_c4.defuse_time);
}

bool PlantedC4::is_relevant(const Game& game) const {
    return !has_exploded(game) && !is_defused(game);
}

config::Weapon weapon_from_entity(const Game& game, std::uintptr_t entity) {
    const auto& offsets = game.offsets();
    const auto index = game.process().read<std::uint16_t>(
        entity + offsets.weapon.attribute_manager + offsets.weapon.item +
        offsets.econ_item_view.item_definition_index);
    return weapon_from_index(index);
}

config::Weapon weapon_from_handle(const Game& game, std::int32_t handle) {
    if (handle == 0) {
        return config::Weapon::None;
    }
    // a weapon handle masks off fewer bits than an entity handle does
    const std::uintptr_t entity =
        game.client_entity(static_cast<std::size_t>(handle) & 0xFFF);
    if (entity == 0) {
        return config::Weapon::None;
    }
    return weapon_from_entity(game, entity);
}

}  // namespace dl::cs2
