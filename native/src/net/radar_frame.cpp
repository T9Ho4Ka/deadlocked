#include "net/radar_frame.hpp"

#include <variant>

#include "cs2/snapshot.hpp"
#include "net/postcard.hpp"

namespace dl::net {
namespace {

/// The variants of the rust EntityInfo enum, in the order they are declared, which is what
/// their index on the wire is.
enum class EntityVariant : std::size_t {
    Weapon = 0,
    Inferno = 1,
    Molotov = 2,
    Smoke = 3,
    Flashbang = 4,
    HeGrenade = 5,
    Decoy = 6,
    Chicken = 7,
};

/// The native side folds the four plain grenades into one type with a name, so the name has
/// to pick the variant back out on the way to the wire.
EntityVariant grenade_variant(const std::string& name) {
    if (name == "Smoke") {
        return EntityVariant::Smoke;
    }
    if (name == "Flashbang") {
        return EntityVariant::Flashbang;
    }
    if (name == "Decoy") {
        return EntityVariant::Decoy;
    }
    return EntityVariant::HeGrenade;
}

/// Fields in the order shared/src/data.rs declares them, skipping the ones marked
/// serde(skip): head, model_name, bones, skeleton, visible, sound and the collision box.
void write_player(Writer& out, const cs2::PlayerData& player) {
    out.u64(player.steam_id);
    out.i32(player.money);
    out.variant(static_cast<std::size_t>(player.team));
    out.i32(player.health);
    out.i32(player.max_health);
    out.i32(player.armor);
    out.vec3(player.position);
    out.string(player.name);
    out.variant(static_cast<std::size_t>(player.weapon));
    // ammo is a tuple, which is just its members back to back
    out.i32(player.clip_ammo);
    out.i32(player.reserve_ammo);
    out.boolean(player.has_defuser);
    out.boolean(player.has_helmet);
    out.boolean(player.has_bomb);
    out.i32(player.color);
    out.f32(player.rotation);
}

void write_entity(Writer& out, const cs2::EntityInfo& entity) {
    std::visit(
        [&](const auto& info) {
            using T = std::decay_t<decltype(info)>;
            if constexpr (std::is_same_v<T, cs2::WeaponInfo>) {
                out.variant(static_cast<std::size_t>(EntityVariant::Weapon));
                out.variant(static_cast<std::size_t>(info.weapon));
                out.vec3(info.position);
                out.i32(info.clip_ammo);
                out.i32(info.reserve_ammo);
            } else if constexpr (std::is_same_v<T, cs2::InfernoInfo>) {
                out.variant(static_cast<std::size_t>(EntityVariant::Inferno));
                out.u64(info.entity);
                out.vec3(info.position);
                // the hull the rust side sends is not computed here yet
                out.length(0);
            } else if constexpr (std::is_same_v<T, cs2::MolotovInfo>) {
                out.variant(static_cast<std::size_t>(EntityVariant::Molotov));
                out.u64(info.entity);
                out.vec3(info.position);
                out.boolean(info.is_incendiary);
            } else if constexpr (std::is_same_v<T, cs2::ChickenInfo>) {
                out.variant(static_cast<std::size_t>(EntityVariant::Chicken));
                out.vec3(info.position);
                out.boolean(false);
                // no bones are tracked for chickens here
                out.length(0);
            } else {
                out.variant(static_cast<std::size_t>(grenade_variant(info.name)));
                out.u64(info.entity);
                out.vec3(info.position);
                out.string(info.name);
            }
        },
        entity);
}

}  // namespace

std::vector<std::uint8_t> encode_radar_frame(const cs2::Snapshot& snapshot) {
    Writer out;

    out.boolean(snapshot.in_game);
    out.boolean(snapshot.is_ffa);
    out.variant(static_cast<std::size_t>(snapshot.weapon));

    out.length(snapshot.players.size());
    for (const cs2::PlayerData& player : snapshot.players) {
        write_player(out, player);
    }
    out.length(snapshot.friendlies.size());
    for (const cs2::PlayerData& player : snapshot.friendlies) {
        write_player(out, player);
    }
    out.length(snapshot.spectators.size());
    for (const std::string& name : snapshot.spectators) {
        out.string(name);
    }

    write_player(out, snapshot.local_player);

    out.length(snapshot.entities.size());
    for (const cs2::EntityInfo& entity : snapshot.entities) {
        write_entity(out, entity);
    }

    out.boolean(snapshot.bomb.planted);
    out.f32(snapshot.bomb.timer);
    out.boolean(snapshot.bomb.being_defused);
    out.vec3(snapshot.bomb.position);
    out.f32(snapshot.bomb.defuse_remaining);

    out.string(snapshot.map_name);
    return out.bytes();
}

}  // namespace dl::net
