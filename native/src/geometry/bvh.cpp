#include "geometry/bvh.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace dl::geometry {
namespace {

/// below this a triangle is edge on to the ray and the intersection is not meaningful
constexpr float ray_epsilon = 1e-6f;

}  // namespace

void Aabb::expand(const Vec3& point) {
    min = glm::min(min, point);
    max = glm::max(max, point);
}

Aabb Aabb::merged(const Aabb& other) const {
    Aabb result;
    result.min = glm::min(min, other.min);
    result.max = glm::max(max, other.max);
    return result;
}

bool Aabb::intersects_ray(const Vec3& origin, const Vec3& inverse_direction,
                          float max_distance) const {
    const Vec3 t1 = (min - origin) * inverse_direction;
    const Vec3 t2 = (max - origin) * inverse_direction;

    const Vec3 low = glm::min(t1, t2);
    const Vec3 high = glm::max(t1, t2);

    const float near = std::max({low.x, low.y, low.z});
    const float far = std::min({high.x, high.y, high.z});

    return near <= far && near <= max_distance && far >= 0.0f;
}

Aabb Triangle::bounds() const {
    Aabb box;
    box.expand(v0);
    box.expand(v1);
    box.expand(v2);
    return box;
}

std::optional<float> Triangle::intersects_ray(const Vec3& origin, const Vec3& direction) const {
    const Vec3 edge1 = v1 - v0;
    const Vec3 edge2 = v2 - v0;
    const Vec3 h = glm::cross(direction, edge2);
    const float a = glm::dot(edge1, h);

    // the ray runs parallel to the triangle's plane
    if (a > -ray_epsilon && a < ray_epsilon) {
        return std::nullopt;
    }

    const float f = 1.0f / a;
    const Vec3 s = origin - v0;
    const float u = f * glm::dot(s, h);
    if (u < 0.0f || u > 1.0f) {
        return std::nullopt;
    }

    const Vec3 q = glm::cross(s, edge1);
    const float v = f * glm::dot(direction, q);
    if (v < 0.0f || u + v > 1.0f) {
        return std::nullopt;
    }

    const float t = f * glm::dot(edge2, q);
    if (t <= ray_epsilon) {
        // the crossing is behind the origin
        return std::nullopt;
    }
    return t;
}

void Bvh::build(std::vector<Triangle> triangles) {
    nodes_.clear();
    order_.clear();
    root_ = npos;
    triangles_ = std::move(triangles);

    if (triangles_.empty()) {
        return;
    }

    order_.resize(triangles_.size());
    std::iota(order_.begin(), order_.end(), 0u);
    // a balanced median split leaves about log2(n / leaf size) levels, so the recursion
    // below stays shallow even for a map with a million triangles
    nodes_.reserve(triangles_.size() / max_leaf_size * 2 + 1);
    root_ = build_recursive(0, order_.size());
}

std::uint32_t Bvh::build_recursive(std::size_t begin, std::size_t end) {
    Node node;
    for (std::size_t i = begin; i < end; ++i) {
        node.bounds = node.bounds.merged(triangles_[order_[i]].bounds());
    }

    if (end - begin <= max_leaf_size) {
        node.first_primitive = static_cast<std::uint32_t>(begin);
        node.primitive_count = static_cast<std::uint32_t>(end - begin);
        nodes_.push_back(node);
        return static_cast<std::uint32_t>(nodes_.size() - 1);
    }

    // split along whichever axis the centroids are most spread out on
    Aabb centroids;
    for (std::size_t i = begin; i < end; ++i) {
        centroids.expand(triangles_[order_[i]].centroid());
    }
    const Vec3 extent = centroids.max - centroids.min;
    const int axis = (extent.x > extent.y && extent.x > extent.z) ? 0 : (extent.y > extent.z ? 1 : 2);

    const std::size_t middle = begin + (end - begin) / 2;
    std::nth_element(order_.begin() + static_cast<std::ptrdiff_t>(begin),
                     order_.begin() + static_cast<std::ptrdiff_t>(middle),
                     order_.begin() + static_cast<std::ptrdiff_t>(end),
                     [this, axis](std::uint32_t a, std::uint32_t b) {
                         return triangles_[a].centroid()[axis] < triangles_[b].centroid()[axis];
                     });

    const std::uint32_t left = build_recursive(begin, middle);
    const std::uint32_t right = build_recursive(middle, end);

    Node branch;
    branch.bounds = nodes_[left].bounds.merged(nodes_[right].bounds);
    branch.left = left;
    branch.right = right;
    nodes_.push_back(branch);
    return static_cast<std::uint32_t>(nodes_.size() - 1);
}

bool Bvh::has_line_of_sight(const Vec3& start, const Vec3& end) const {
    if (root_ == npos) {
        // no geometry loaded, so nothing can be in the way
        return true;
    }

    const Vec3 offset = end - start;
    const float distance = glm::length(offset);
    // the two points are on top of each other, so there is nothing between them. without
    // this the direction below would be a nan and every later comparison would be false
    if (!std::isfinite(distance) || distance <= ray_epsilon) {
        return true;
    }

    const Vec3 direction = offset / distance;
    const Vec3 inverse = 1.0f / direction;

    // an explicit stack rather than recursion: a query runs per bone per player per frame
    std::uint32_t stack[64];
    std::size_t depth = 0;
    stack[depth++] = root_;

    while (depth > 0) {
        const Node& node = nodes_[stack[--depth]];
        if (!node.bounds.intersects_ray(start, inverse, distance)) {
            continue;
        }

        if (node.is_leaf()) {
            for (std::uint32_t i = 0; i < node.primitive_count; ++i) {
                const Triangle& triangle = triangles_[order_[node.first_primitive + i]];
                const std::optional<float> hit = triangle.intersects_ray(start, direction);
                if (hit.has_value() && *hit <= distance) {
                    return false;
                }
            }
            continue;
        }

        // a tree this deep would need more triangles than a map can hold, but bail rather
        // than run off the end of the stack if it ever happens
        if (depth + 2 > std::size(stack)) {
            continue;
        }
        stack[depth++] = node.left;
        stack[depth++] = node.right;
    }

    return true;
}

}  // namespace dl::geometry
