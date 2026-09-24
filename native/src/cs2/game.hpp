#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <vector>

#include "cs2/entity.hpp"
#include "cs2/input.hpp"
#include "geometry/bvh.hpp"
#include "cs2/offsets.hpp"
#include "cs2/snapshot.hpp"
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

    /// One pass of the loop: refreshes the input, and rebuilds the entity snapshot when
    /// enough time has gone by. Entities are rebuilt far less often than input is read,
    /// because walking the entity list is expensive and the world changes slowly.
    void tick();

    /// Walks the game's entity list and refills the snapshot below.
    void cache_entities();

    [[nodiscard]] const Input& input() const { return input_; }

    /// Rebuilds the collision geometry when the map has changed. Reading a map is tens of
    /// thousands of triangles, so it happens once per map, never per frame.
    void check_map();
    [[nodiscard]] const geometry::Bvh& geometry() const { return bvh_; }

    /// Whether `from` can see `to` through the map. Falls back to the game's own spotted
    /// flag while no geometry is loaded, which is a weaker answer: the game only reports
    /// what it has decided to render, not what is geometrically in view.
    [[nodiscard]] bool has_line_of_sight(const Vec3& from, const Vec3& to) const;

    [[nodiscard]] const std::optional<Player>& local_player() const { return local_player_; }
    [[nodiscard]] const std::vector<Player>& players() const { return players_; }
    [[nodiscard]] const std::vector<Player>& dead_players() const { return dead_players_; }
    [[nodiscard]] const std::vector<Entity>& entities() const { return entities_; }
    [[nodiscard]] const std::optional<PlantedC4>& planted_c4() const { return planted_c4_; }
    /// Bit the local player occupies in every other player's spotted mask.
    [[nodiscard]] std::uint64_t local_pawn_index() const { return local_pawn_index_; }

    /// Entity at an index in the game's entity list, zero when there is none.
    [[nodiscard]] std::uintptr_t client_entity(std::size_t index) const;
    /// Entity a handle points at, zero when the handle is stale.
    [[nodiscard]] std::uintptr_t entity_from_handle(std::int32_t handle) const;
    /// A dropped weapon has no owner; one being carried does.
    [[nodiscard]] bool entity_has_owner(std::uintptr_t entity) const;
    /// Copies this frame's worth of the game into `out`, which is what the overlay draws
    /// from. Everything copied is a value: by the time it is drawn, the entity behind it
    /// may be gone.
    void build_snapshot(Snapshot& out);

    // --- convars and globals ---
    /// Mouse sensitivity the player has set, needed to convert aim movement to mouse counts.
    [[nodiscard]] float sensitivity() const;
    /// Free for all, where teammates are enemies too.
    [[nodiscard]] bool is_ffa() const;
    /// The game's own clock, in seconds.
    [[nodiscard]] float current_time() const;
    [[nodiscard]] std::string current_map() const;
    /// Where the game's window sits and how big it is, both zero sized when there is none.
    [[nodiscard]] std::pair<Vec2, Vec2> window_bounds() const;
    [[nodiscard]] Mat4 view_matrix() const;

    /// Mangled rtti name of an entity's class, used to tell entities apart.
    [[nodiscard]] std::string class_name_of(std::uintptr_t entity) const;

private:
    Game(os::Process process, Offsets offsets)
        : process_(std::move(process)), offsets_(offsets) {}

    void scan_bucket(std::size_t bucket_index, std::uintptr_t bucket_pointer,
                     const Player& local);
    /// Copies one player out of the game, skeleton and all.
    [[nodiscard]] PlayerData player_data(const Player& player, const Player& local);

    os::Process process_;
    Offsets offsets_;
    Input input_;
    std::chrono::steady_clock::time_point last_cache_{};
    geometry::Bvh bvh_;
    std::string bvh_map_;

    std::optional<Player> local_player_;
    std::vector<Player> players_;
    std::vector<Player> dead_players_;
    std::vector<Entity> entities_;
    std::optional<PlantedC4> planted_c4_;
    std::uint64_t local_pawn_index_ = 64;
};

}  // namespace dl::cs2
