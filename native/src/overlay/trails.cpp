#include "overlay/trails.hpp"

#include <variant>

#include "cs2/snapshot.hpp"

namespace dl::overlay {

void Trails::update(const cs2::Snapshot& snapshot) {
    const auto now = std::chrono::steady_clock::now();

    for (const cs2::EntityInfo& info : snapshot.entities) {
        std::visit(
            [&](const auto& value) {
                using T = std::decay_t<decltype(value)>;
                // only the things that fly leave a trail
                if constexpr (std::is_same_v<T, cs2::WeaponInfo> ||
                              std::is_same_v<T, cs2::ChickenInfo>) {
                    return;
                } else {
                    if (value.entity == 0) {
                        return;
                    }
                    Trail& trail = trails_[value.entity];
                    if (trail.path.empty()) {
                        trail.path.push_back(value.position);
                        trail.last_moved = now;
                        return;
                    }
                    if (glm::distance(trail.path.back(), value.position) < min_step) {
                        return;
                    }
                    trail.path.push_back(value.position);
                    trail.last_moved = now;
                }
            },
            info);
    }

    std::erase_if(trails_, [&](const auto& entry) {
        return now - entry.second.last_moved >= max_age;
    });
}

const std::vector<Vec3>* Trails::path(std::uintptr_t entity) const {
    const auto found = trails_.find(entity);
    return found == trails_.end() ? nullptr : &found->second.path;
}

}  // namespace dl::overlay
