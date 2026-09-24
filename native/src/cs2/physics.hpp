#pragma once

#include <vector>

#include "geometry/bvh.hpp"

namespace dl::cs2 {

class Game;

/// Pulls the map's collision geometry out of the game's physics world as triangles.
///
/// Empty when the world cannot be read, which is the normal state outside a match. A map
/// is tens of thousands of triangles, so this is done once per map and never per frame.
[[nodiscard]] std::vector<geometry::Triangle> read_collision_triangles(const Game& game);

}  // namespace dl::cs2
