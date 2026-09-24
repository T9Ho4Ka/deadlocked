#pragma once

#include <chrono>
#include <cstdint>
#include <unordered_map>
#include <vector>

#include "math.hpp"

namespace dl::cs2 {
struct Snapshot;
}

namespace dl::overlay {

/// Where grenades have been, so their flight can be drawn as a line rather than a dot.
///
/// The snapshot is a fresh copy each frame, so the history has to live outside it. Entries
/// are keyed by the entity's address and dropped once they stop moving, which is also what
/// happens when the grenade is gone and its address gets reused.
class Trails {
public:
    /// one second of stillness and a trail is forgotten
    static constexpr std::chrono::milliseconds max_age{1000};
    /// movement below this is noise rather than flight
    static constexpr float min_step = 1.0f;

    void update(const cs2::Snapshot& snapshot);
    [[nodiscard]] const std::vector<Vec3>* path(std::uintptr_t entity) const;

private:
    struct Trail {
        std::vector<Vec3> path;
        std::chrono::steady_clock::time_point last_moved;
    };

    std::unordered_map<std::uintptr_t, Trail> trails_;
};

}  // namespace dl::overlay
