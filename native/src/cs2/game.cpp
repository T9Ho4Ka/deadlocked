#include "cs2/game.hpp"

#include <cstdio>
#include <cstring>

#include "constants.hpp"

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

void Game::cache_entities() {
    players_.clear();
    dead_players_.clear();
    entities_.clear();
    planted_c4_.reset();
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
            } else if (!(*player == local)) {
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
