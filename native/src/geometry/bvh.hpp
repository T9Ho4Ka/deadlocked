#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "math.hpp"

namespace dl::geometry {

/// An axis aligned box. Starts inverted, so expanding it with the first point makes it that
/// point rather than a box spanning the world.
struct Aabb {
    Vec3 min{std::numeric_limits<float>::max()};
    Vec3 max{std::numeric_limits<float>::lowest()};

    [[nodiscard]] Vec3 centroid() const { return (min + max) * 0.5f; }
    void expand(const Vec3& point);
    [[nodiscard]] Aabb merged(const Aabb& other) const;

    /// Slab test. `inverse_direction` is the reciprocal of a unit direction, passed in
    /// because a ray is tested against many boxes and the division is the expensive part.
    [[nodiscard]] bool intersects_ray(const Vec3& origin, const Vec3& inverse_direction,
                                      float max_distance) const;
};

struct Triangle {
    Vec3 v0{0.0f};
    Vec3 v1{0.0f};
    Vec3 v2{0.0f};

    [[nodiscard]] Aabb bounds() const;
    [[nodiscard]] Vec3 centroid() const { return (v0 + v1 + v2) * (1.0f / 3.0f); }

    /// Moller-Trumbore. Returns the distance along `direction` where the ray crosses the
    /// triangle, or nothing when it misses or the triangle is edge on.
    [[nodiscard]] std::optional<float> intersects_ray(const Vec3& origin,
                                                      const Vec3& direction) const;
};

/// Bounding volume hierarchy over the map's collision geometry, used to answer whether one
/// point can see another. Built once per map, then only queried.
class Bvh {
public:
    /// a node holding no more than this many triangles is not split further
    static constexpr std::size_t max_leaf_size = 8;

    /// Takes the triangles and builds the tree over them.
    void build(std::vector<Triangle> triangles);

    [[nodiscard]] bool empty() const { return nodes_.empty(); }
    [[nodiscard]] std::size_t triangle_count() const { return triangles_.size(); }
    [[nodiscard]] std::size_t node_count() const { return nodes_.size(); }

    /// Whether nothing in the geometry stands between the two points. An empty tree means
    /// nothing can block, so everything is visible.
    [[nodiscard]] bool has_line_of_sight(const Vec3& start, const Vec3& end) const;

private:
    struct Node {
        Aabb bounds;
        /// for a branch, the index of its children; for a leaf, both are npos
        std::uint32_t left = npos;
        std::uint32_t right = npos;
        /// for a leaf, where its triangles sit in the index array
        std::uint32_t first_primitive = 0;
        std::uint32_t primitive_count = 0;

        [[nodiscard]] bool is_leaf() const { return left == npos; }
    };

    static constexpr std::uint32_t npos = 0xFFFF'FFFF;

    std::uint32_t build_recursive(std::size_t begin, std::size_t end);

    std::vector<Node> nodes_;
    std::vector<Triangle> triangles_;
    /// triangle indices, reordered as the tree is built so each leaf owns a contiguous run
    std::vector<std::uint32_t> order_;
    std::uint32_t root_ = npos;
};

}  // namespace dl::geometry
