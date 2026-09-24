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

void read_player(Reader& in, cs2::PlayerData& player) {
    player.steam_id = in.u64();
    player.money = in.i32();
    player.team = cs2::team_from(static_cast<std::uint8_t>(in.variant()));
    player.health = in.i32();
    player.max_health = in.i32();
    player.armor = in.i32();
    player.position = in.vec3();
    player.name = in.string();
    const std::size_t weapon = in.variant();
    player.weapon = weapon < config::weapon_count ? static_cast<config::Weapon>(weapon)
                                                  : config::Weapon::None;
    player.clip_ammo = in.i32();
    player.reserve_ammo = in.i32();
    player.has_defuser = in.boolean();
    player.has_helmet = in.boolean();
    player.has_bomb = in.boolean();
    player.color = in.i32();
    player.rotation = in.f32();
}

/// Reads one entity back into whichever native type carries it.
void read_entity(Reader& in, std::vector<cs2::EntityInfo>& out) {
    const auto variant = static_cast<EntityVariant>(in.variant());
    switch (variant) {
        case EntityVariant::Weapon: {
            cs2::WeaponInfo info;
            const std::size_t weapon = in.variant();
            info.weapon = weapon < config::weapon_count
                              ? static_cast<config::Weapon>(weapon)
                              : config::Weapon::None;
            info.position = in.vec3();
            info.clip_ammo = in.i32();
            info.reserve_ammo = in.i32();
            out.push_back(info);
            return;
        }
        case EntityVariant::Inferno: {
            cs2::InfernoInfo info;
            info.entity = in.u64();
            info.position = in.vec3();
            // the hull the rust side sends is read past rather than kept
            const std::size_t hull = in.length();
            for (std::size_t i = 0; i < hull; ++i) {
                in.vec3();
            }
            out.push_back(info);
            return;
        }
        case EntityVariant::Molotov: {
            cs2::MolotovInfo info;
            info.entity = in.u64();
            info.position = in.vec3();
            info.is_incendiary = in.boolean();
            out.push_back(info);
            return;
        }
        case EntityVariant::Chicken: {
            cs2::ChickenInfo info;
            info.position = in.vec3();
            in.boolean();
            const std::size_t bones = in.length();
            for (std::size_t i = 0; i < bones; ++i) {
                in.variant();
                in.vec3();
            }
            out.push_back(info);
            return;
        }
        default: {
            cs2::GrenadeInfo info;
            info.entity = in.u64();
            info.position = in.vec3();
            info.name = in.string();
            out.push_back(info);
            return;
        }
    }
}

}  // namespace

bool decode_radar_frame(std::span<const std::uint8_t> frame, cs2::Snapshot& out) {
    out.clear();
    Reader in(frame);

    out.in_game = in.boolean();
    out.is_ffa = in.boolean();
    const std::size_t weapon = in.variant();
    out.weapon = weapon < config::weapon_count ? static_cast<config::Weapon>(weapon)
                                               : config::Weapon::None;

    const auto read_players = [&](std::vector<cs2::PlayerData>& into) {
        const std::size_t count = in.length();
        into.resize(count);
        for (cs2::PlayerData& player : into) {
            read_player(in, player);
        }
    };
    read_players(out.players);
    read_players(out.friendlies);

    const std::size_t spectators = in.length();
    out.spectators.resize(spectators);
    for (std::string& name : out.spectators) {
        name = in.string();
    }

    read_player(in, out.local_player);

    const std::size_t entities = in.length();
    out.entities.reserve(entities);
    for (std::size_t i = 0; i < entities && in.ok(); ++i) {
        read_entity(in, out.entities);
    }

    out.bomb.planted = in.boolean();
    out.bomb.timer = in.f32();
    out.bomb.being_defused = in.boolean();
    out.bomb.position = in.vec3();
    out.bomb.defuse_remaining = in.f32();

    out.map_name = in.string();
    return in.ok();
}

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
