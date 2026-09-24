#include "cs2/game.hpp"

#include <cstdio>
#include <cstring>

#include <array>

#include "constants.hpp"
#include "cs2/physics.hpp"

namespace dl::cs2 {
namespace {

/// The entity list is a fixed grid: 64 buckets of 512 identities each.
constexpr std::size_t bucket_count = 64;
constexpr std::size_t identities_per_bucket = 512;
constexpr std::uintptr_t identity_entity = 0x00;
constexpr std::uintptr_t identity_handle = 0x10;
/// the low bits of a handle are the entity's index, the rest is a reuse counter
constexpr std::uint32_t handle_index_mask = 0x7FFF;

/// where an entity keeps its own identity, and where that identity keeps the class name
constexpr std::uintptr_t entity_identity_pointer = 0x10;
constexpr std::uintptr_t identity_name_pointer = 0x20;
/// the rtti pointer sits just before the vtable
constexpr std::uintptr_t vtable_rtti = 0x8;
constexpr std::uintptr_t rtti_name = 0x8;

/// a convar keeps its value here, past the name and the flags
constexpr std::uintptr_t convar_value = 0x58;
/// inside the game's globals
constexpr std::uintptr_t globals_current_time = 0x30;
constexpr std::uintptr_t globals_map_name = 0x198;
/// an sdl window keeps its position and then its size
constexpr std::uintptr_t sdl_window_position = 0x18;
constexpr std::uintptr_t sdl_window_size = 0x18 + 0x08;

template <typename T>
T read_from(const std::vector<std::uint8_t>& buffer, std::size_t offset) {
    T value{};
    if (offset + sizeof(T) <= buffer.size()) {
        std::memcpy(&value, buffer.data() + offset, sizeof(T));
    }
    return value;
}

}  // namespace

std::optional<Game> Game::attach() {
    std::optional<os::Process> process = os::Process::open(constants::cs2::process_name);
    if (!process.has_value()) {
        return std::nullopt;
    }

    const std::optional<Offsets> offsets = find_offsets(*process);
    if (!offsets.has_value()) {
        return std::nullopt;
    }

    return Game(std::move(*process), *offsets);
}

std::uintptr_t Game::client_entity(std::size_t index) const {
    const std::size_t bucket = index >> 9;
    const std::size_t in_bucket = index & 0x1FF;
    if (bucket >= bucket_count) {
        return 0;
    }

    const auto bucket_pointer =
        process_.read<std::uintptr_t>(offsets_.interface.entity + 0x08 * bucket);
    if (bucket_pointer == 0) {
        return 0;
    }
    return process_.read<std::uintptr_t>(bucket_pointer +
                                         offsets_.entity_identity.size * in_bucket);
}

std::uintptr_t Game::entity_from_handle(std::int32_t handle) const {
    return client_entity(static_cast<std::size_t>(handle) & handle_index_mask);
}

bool Game::entity_has_owner(std::uintptr_t entity) const {
    return process_.read<std::int32_t>(entity + offsets_.controller.owner_entity) != -1;
}

std::string Game::class_name_of(std::uintptr_t entity) const {
    const auto vtable = process_.read<std::uintptr_t>(entity);
    if (vtable == 0) {
        return {};
    }
    const auto rtti = process_.read<std::uintptr_t>(vtable - vtable_rtti);
    if (rtti == 0) {
        return {};
    }
    return process_.read_string(process_.read<std::uintptr_t>(rtti + rtti_name));
}

float Game::sensitivity() const {
    return process_.read<float>(offsets_.convar.sensitivity + convar_value);
}

bool Game::is_ffa() const {
    return process_.read<std::uint8_t>(offsets_.convar.ffa + convar_value) == 1;
}

float Game::current_time() const {
    const auto globals = process_.read<std::uintptr_t>(offsets_.direct.global_vars);
    if (globals == 0) {
        return 0.0f;
    }
    return process_.read<float>(globals + globals_current_time);
}

std::string Game::current_map() const {
    const auto globals = process_.read<std::uintptr_t>(offsets_.direct.global_vars);
    if (globals == 0) {
        return {};
    }
    const auto name = process_.read<std::uintptr_t>(globals + globals_map_name);
    if (name == 0) {
        return {};
    }
    return process_.read_string(name);
}

std::pair<Vec2, Vec2> Game::window_bounds() const {
    // this is sdl's keyboard focus window, which is null whenever the game is not the
    // active window. a one by one size is the sign of that, not of a failed read
    const auto window = process_.read<std::uintptr_t>(offsets_.direct.sdl_window);
    if (window == 0) {
        // one rather than zero, so anything dividing by it survives
        return {Vec2(0.0f), Vec2(1.0f)};
    }
    const auto position = process_.read<std::array<std::int32_t, 2>>(window + sdl_window_position);
    const auto size = process_.read<std::array<std::int32_t, 2>>(window + sdl_window_size);
    return {Vec2(static_cast<float>(position[0]), static_cast<float>(position[1])),
            Vec2(static_cast<float>(size[0]), static_cast<float>(size[1]))};
}

Mat4 Game::view_matrix() const {
    return process_.read<Mat4>(offsets_.direct.view_matrix);
}

void Game::check_map() {
    const std::string map = current_map();
    if (map.empty() || map == bvh_map_) {
        return;
    }

    std::vector<geometry::Triangle> triangles = read_collision_triangles(*this);
    if (triangles.empty()) {
        // the physics world is not up yet, so try again on the next pass
        return;
    }

    const std::size_t count = triangles.size();
    bvh_.build(std::move(triangles));
    bvh_map_ = map;
    std::fprintf(stderr, "loaded %zu collision triangles for %s\n", count, map.c_str());
}

bool Game::has_line_of_sight(const Vec3& from, const Vec3& to) const {
    if (bvh_.empty()) {
        return false;
    }
    return bvh_.has_line_of_sight(from, to);
}

void Game::tick() {
    // reading the input is a single small read, so it happens every pass
    input_.update(process_, offsets_);

    // the entity list is hundreds of reads, so it is rebuilt on a timer instead
    constexpr auto cache_interval = std::chrono::milliseconds(200);
    const auto now = std::chrono::steady_clock::now();
    if (now - last_cache_ >= cache_interval) {
        cache_entities();
        check_map();
        last_cache_ = now;
    }
}

void Game::cache_entities() {
    players_.clear();
    dead_players_.clear();
    entities_.clear();
    planted_c4_.reset();
    local_pawn_index_ = 64;
    local_player_ = Player::local(*this);

    if (!local_player_.has_value()) {
        // not in a match, so there is no entity list worth walking
        return;
    }

    const std::vector<std::uint8_t> pointers =
        process_.read_vec(offsets_.interface.entity, 8 * bucket_count);
    for (std::size_t bucket = 0; bucket < bucket_count; ++bucket) {
        scan_bucket(bucket, read_from<std::uintptr_t>(pointers, bucket * 8), *local_player_);
    }
}

PlayerData Game::player_data(const Player& player, const Player& local) {
    PlayerData data;
    const BaseEntity& pawn = player.pawn();

    data.steam_id = player.steam_id(*this);
    data.money = player.money(*this);
    data.team = pawn.team(*this);
    data.health = pawn.health(*this);
    data.max_health = pawn.max_health(*this);
    data.armor = player.armor(*this);
    data.position = pawn.position(*this);
    data.name = player.name(*this);
    data.model_name = player.model_name(*this);
    data.weapon = player.weapon(*this);
    data.clip_ammo = player.clip_ammo(*this);
    data.reserve_ammo = player.reserve_ammo(*this);
    data.has_defuser = player.has_defuser(*this);
    data.has_helmet = player.has_helmet(*this);
    data.has_bomb = player.has_bomb(*this);
    data.color = player.color(*this);
    data.rotation = player.rotation(*this);

    data.skeleton = player.skeleton(*this, local);
    data.bones = Player::bone_positions(data.skeleton);
    const auto head = data.bones.find(config::Bones::Head);
    if (head != data.bones.end()) {
        data.head = head->second;
    }
    if (bvh_.empty()) {
        data.visible = player.spotted_by_local(*this);
    } else {
        // the bones the esp cares about: if any of them can be seen, so can the player
        const Vec3 eye = local.eye_position(*this);
        data.visible = false;
        for (const config::Bones bone : {config::Bones::Head, config::Bones::LeftFoot,
                                         config::Bones::RightFoot, config::Bones::LeftHand,
                                         config::Bones::RightHand}) {
            const auto found = data.bones.find(bone);
            if (found != data.bones.end() && bvh_.has_line_of_sight(eye, found->second)) {
                data.visible = true;
                break;
            }
        }
    }

    const auto [mins, maxs] = pawn.collision_bounds(*this);
    data.collision_mins = mins;
    data.collision_maxs = maxs;
    data.collision_transform = pawn.collision_transform(*this);

    return data;
}

void Game::build_snapshot(Snapshot& out) {
    out.clear();

    const auto [position, size] = window_bounds();
    out.window_position = position;
    out.window_size = size;

    if (!local_player_.has_value()) {
        return;
    }
    const Player& local = *local_player_;

    const Team local_team = local.pawn().team(*this);
    if (!is_playing(local_team)) {
        // in the menu, or still picking a side
        return;
    }

    out.is_ffa = is_ffa();
    for (const Player& player : players_) {
        PlayerData data = player_data(player, local);
        const bool friendly = !out.is_ffa && data.team == local_team;
        (friendly ? out.friendlies : out.players).push_back(std::move(data));
    }

    out.local_player = player_data(local, local);
    // the local player is always visible to themselves
    out.local_player.visible = true;

    for (const Entity& entity : entities_) {
        std::visit(
            [&](const auto& value) {
                using T = std::decay_t<decltype(value)>;
                const Vec3 at = value.entity.position(*this);
                if constexpr (std::is_same_v<T, DroppedWeapon>) {
                    out.entities.push_back(WeaponInfo{
                        value.weapon, at,
                        process_.read<std::int32_t>(value.entity.address() +
                                                    offsets_.weapon.clip_primary),
                        process_.read<std::int32_t>(value.entity.address() +
                                                    offsets_.weapon.reserve_ammo)});
                } else if constexpr (std::is_same_v<T, Molotov>) {
                    out.entities.push_back(MolotovInfo{at, value.is_incendiary});
                } else if constexpr (std::is_same_v<T, Inferno>) {
                    out.entities.push_back(InfernoInfo{at});
                } else if constexpr (std::is_same_v<T, Chicken>) {
                    out.entities.push_back(ChickenInfo{at});
                } else if constexpr (std::is_same_v<T, Smoke>) {
                    out.entities.push_back(GrenadeInfo{at, "Smoke"});
                } else if constexpr (std::is_same_v<T, Flashbang>) {
                    out.entities.push_back(GrenadeInfo{at, "Flashbang"});
                } else if constexpr (std::is_same_v<T, HeGrenade>) {
                    out.entities.push_back(GrenadeInfo{at, "HE Grenade"});
                } else {
                    out.entities.push_back(GrenadeInfo{at, "Decoy"});
                }
            },
            entity);
    }

    out.weapon = local.weapon(*this);
    out.in_game = true;
    out.map_name = current_map();
    out.view_matrix = view_matrix();
    out.view_angles = local.view_angles(*this);

    if (planted_c4_.has_value()) {
        const PlantedC4& bomb = *planted_c4_;
        out.bomb.planted = bomb.is_ticking(*this);
        out.bomb.position = bomb.entity().position(*this);
        out.bomb.being_defused = bomb.being_defused(*this);
        // both countdowns are absolute game times, so the clock has to be taken off them
        const float now = current_time();
        out.bomb.timer = bomb.blow_time(*this) - now;
        out.bomb.defuse_remaining = bomb.defuse_time(*this) - now;
    }
}

void Game::scan_bucket(std::size_t bucket_index, std::uintptr_t bucket_pointer,
                       const Player& local) {
    // a real pointer never has its top sixteen bits set; anything that does is a misread
    if (bucket_pointer == 0 || (bucket_pointer >> 48) != 0) {
        return;
    }

    const std::size_t identity_size = offsets_.entity_identity.size;
    if (identity_size == 0) {
        return;
    }

    // one read for the whole bucket rather than two per identity
    const std::vector<std::uint8_t> bucket =
        process_.read_vec(bucket_pointer, identities_per_bucket * identity_size);

    for (std::size_t index = 0; index < identities_per_bucket; ++index) {
        const std::size_t base = index * identity_size;

        const auto entity = read_from<std::uintptr_t>(bucket, base + identity_entity);
        if (entity == 0) {
            continue;
        }

        // the identity's handle has to agree with where it sits, otherwise the slot is
        // stale and the entity behind it has already been freed
        const auto handle = read_from<std::uint32_t>(bucket, base + identity_handle);
        const auto expected =
            static_cast<std::uint32_t>(bucket_index * identities_per_bucket + index);
        if ((handle & handle_index_mask) != expected) {
            continue;
        }

        const std::string name = class_name_of(entity);

        if (name == class_name::player_controller) {
            std::optional<Player> player = Player::from_controller(*this, entity);
            if (!player.has_value()) {
                continue;
            }
            if (!player->is_valid(*this)) {
                dead_players_.push_back(*player);
            } else if (*player == local) {
                // the local player's own slot is the bit everyone else's spotted mask uses
                local_pawn_index_ = (static_cast<std::uint64_t>(handle) & handle_index_mask) - 1;
            } else {
                players_.push_back(*player);
            }
            continue;
        }

        if (name == class_name::planted_c4) {
            PlantedC4 bomb(entity);
            if (bomb.is_relevant(*this)) {
                planted_c4_ = bomb;
            }
            continue;
        }

        if (name == class_name::inferno) {
            entities_.push_back(Inferno{BaseEntity(entity)});
            continue;
        }
        if (name == class_name::smoke) {
            entities_.push_back(Smoke{BaseEntity(entity)});
            continue;
        }
        if (name == class_name::molotov) {
            const bool incendiary =
                process_.read<std::uint8_t>(entity + offsets_.molotov.is_incendiary) != 0;
            entities_.push_back(Molotov{BaseEntity(entity), incendiary});
            continue;
        }
        if (name == class_name::flashbang) {
            entities_.push_back(Flashbang{BaseEntity(entity)});
            continue;
        }
        if (name == class_name::he_grenade) {
            entities_.push_back(HeGrenade{BaseEntity(entity)});
            continue;
        }
        if (name == class_name::decoy) {
            entities_.push_back(Decoy{BaseEntity(entity)});
            continue;
        }
        if (name == class_name::chicken) {
            entities_.push_back(Chicken{BaseEntity(entity)});
            continue;
        }

        // anything left might still be a weapon lying on the ground, which the game names
        // through its identity rather than its rtti
        const auto identity = process_.read<std::uintptr_t>(entity + entity_identity_pointer);
        if (identity == 0) {
            continue;
        }
        const auto name_pointer = process_.read<std::uintptr_t>(identity + identity_name_pointer);
        if (name_pointer == 0) {
            continue;
        }
        if (!process_.read_string(name_pointer).starts_with("weapon_")) {
            continue;
        }
        if (entity_has_owner(entity)) {
            // someone is carrying it, so it is not on the ground
            continue;
        }
        entities_.push_back(DroppedWeapon{BaseEntity(entity), weapon_from_entity(*this, entity)});
    }
}

}  // namespace dl::cs2
