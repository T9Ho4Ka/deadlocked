#include "cs2/entity.hpp"

#include <cmath>
#include <limits>

#include <glm/gtc/matrix_transform.hpp>

#include "cs2/game.hpp"
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
