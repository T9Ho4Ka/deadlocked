#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "cs2/entity.hpp"
#include "cs2/offsets.hpp"
#include "os/process.hpp"

namespace dl::cs2 {

/// A running CS2, with its offsets resolved and a snapshot of what is in the world.
///
/// Nothing here holds onto a pointer across frames: `cache_entities` rebuilds the lists
/// every time, because an entity can be freed between two frames and the address reused.
class Game {
public:
    Game(const Game&) = delete;
    Game& operator=(const Game&) = delete;
    Game(Game&&) = default;
    Game& operator=(Game&&) = default;

    /// Finds the game, opens it and resolves every offset. Empty when CS2 is not running
    /// or when the offsets no longer match it.
    static std::optional<Game> attach();

    [[nodiscard]] const os::Process& process() const { return process_; }
    [[nodiscard]] const Offsets& offsets() const { return offsets_; }
    [[nodiscard]] bool running() const { return process_.valid(); }

    /// Walks the game's entity list and refills the snapshot below.
    void cache_entities();

    [[nodiscard]] const std::optional<Player>& local_player() const { return local_player_; }
    [[nodiscard]] const std::vector<Player>& players() const { return players_; }
    [[nodiscard]] const std::vector<Player>& dead_players() const { return dead_players_; }
    [[nodiscard]] const std::vector<Entity>& entities() const { return entities_; }
    [[nodiscard]] const std::optional<PlantedC4>& planted_c4() const { return planted_c4_; }

    /// Entity at an index in the game's entity list, zero when there is none.
    [[nodiscard]] std::uintptr_t client_entity(std::size_t index) const;
    /// Entity a handle points at, zero when the handle is stale.
    [[nodiscard]] std::uintptr_t entity_from_handle(std::int32_t handle) const;
    /// A dropped weapon has no owner; one being carried does.
    [[nodiscard]] bool entity_has_owner(std::uintptr_t entity) const;
    /// Mangled rtti name of an entity's class, used to tell entities apart.
    [[nodiscard]] std::string class_name_of(std::uintptr_t entity) const;

private:
    Game(os::Process process, Offsets offsets)
        : process_(std::move(process)), offsets_(offsets) {}

    void scan_bucket(std::size_t bucket_index, std::uintptr_t bucket_pointer,
                     const Player& local);

    os::Process process_;
    Offsets offsets_;

    std::optional<Player> local_player_;
    std::vector<Player> players_;
    std::vector<Player> dead_players_;
    std::vector<Entity> entities_;
    std::optional<PlantedC4> planted_c4_;
};

}  // namespace dl::cs2
