#include "cs2/physics.hpp"

#include <cstdint>
#include <cstring>
#include <string>
#include <unordered_set>

#include "cs2/game.hpp"
#include "os/process.hpp"

namespace dl::cs2 {
namespace {

/// a count larger than this is a misread, not a real array
constexpr std::size_t max_vector_items = 2'000'000;

/// Layout of the physics structures. These go stale on a game update like every other
/// offset here, so they are named rather than written inline.
namespace layout {
constexpr std::uintptr_t world_inner = 0x30;
constexpr std::uintptr_t inner_bodies = 0x118;
constexpr std::uintptr_t bodies_count = 0x268;
constexpr std::size_t body_stride = 88;
constexpr std::uintptr_t body_root = 0x00;
constexpr std::uintptr_t body_count_a = 0x08;
constexpr std::uintptr_t body_count_b = 0x10;
constexpr std::uintptr_t body_nodes = 0x18;
constexpr std::uintptr_t body_kind = 0x40;
/// the only body kind holding map geometry
constexpr std::uint32_t body_kind_geometry = 2;

/// m_nInteractsAs, whose lowest bit marks real world geometry. without this, trigger and
/// clip volumes count as walls and half the map looks occluded.
constexpr std::uintptr_t shape_interacts_as = 0x50;
constexpr std::uintptr_t shape_hull = 0xB8;
constexpr std::uintptr_t shape_hull_scale = 0xB0;
constexpr std::uintptr_t shape_mesh = 0xC0;

constexpr std::uintptr_t mesh_vertices = 0x30;
constexpr std::uintptr_t mesh_triangles = 0x48;

constexpr std::uintptr_t hull_vertices = 0x70;
constexpr std::uintptr_t hull_edges = 0xC8;
constexpr std::uintptr_t hull_faces = 0xE0;

constexpr std::uintptr_t vtable_rtti = 0x08;
constexpr std::uintptr_t rtti_name = 0x08;
}  // namespace layout

/// the game's own vector type: a count, padding, then the data
struct UtlVector {
    std::int32_t count = 0;
    std::int32_t padding = 0;
    std::uintptr_t data = 0;
};
static_assert(sizeof(UtlVector) == 16, "must match the game's layout exactly");

struct OuterNode {
    std::uint8_t padding_0[12];
    std::int32_t left;
    std::uint8_t padding_1[12];
    std::int32_t right;
    std::uint8_t padding_2[8];
    std::uintptr_t shape;
};
static_assert(sizeof(OuterNode) == 48, "must match the game's layout exactly");

struct HalfEdge {
    std::uint8_t next;
    std::uint8_t twin;
    std::uint8_t origin;
    std::uint8_t face;
};
static_assert(sizeof(HalfEdge) == 4, "must match the game's layout exactly");

struct MeshTriangle {
    std::int32_t index[3];
};
static_assert(sizeof(MeshTriangle) == 12, "must match the game's layout exactly");

bool plausible(const UtlVector& vector) {
    return vector.count >= 0 && static_cast<std::size_t>(vector.count) <= max_vector_items &&
           (vector.count == 0 || vector.data != 0);
}

/// a triangle with no area contributes nothing and would only slow the tree down
bool degenerate(const Vec3& a, const Vec3& b, const Vec3& c) {
    const Vec3 normal = glm::cross(b - a, c - a);
    return glm::dot(normal, normal) <= std::numeric_limits<float>::epsilon();
}

std::string rtti_name(const os::Process& process, std::uintptr_t object) {
    const auto vtable = process.read<std::uintptr_t>(object);
    if (vtable == 0) {
        return {};
    }
    const auto rtti = process.read<std::uintptr_t>(vtable - layout::vtable_rtti);
    if (rtti == 0) {
        return {};
    }
    const auto name = process.read<std::uintptr_t>(rtti + layout::rtti_name);
    if (name == 0) {
        return {};
    }
    return process.read_string(name);
}

void read_mesh(const os::Process& process, std::uintptr_t shape,
               std::vector<geometry::Triangle>& out) {
    const auto mesh = process.read<std::uintptr_t>(shape + layout::shape_mesh);
    if (mesh == 0) {
        return;
    }

    const auto vertex_vector = process.read<UtlVector>(mesh + layout::mesh_vertices);
    const auto index_vector = process.read<UtlVector>(mesh + layout::mesh_triangles);
    if (!plausible(vertex_vector) || !plausible(index_vector)) {
        return;
    }

    const std::vector<Vec3> vertices = process.read_typed_vec<Vec3>(
        vertex_vector.data, sizeof(Vec3), static_cast<std::size_t>(vertex_vector.count));
    const std::vector<MeshTriangle> indices = process.read_typed_vec<MeshTriangle>(
        index_vector.data, sizeof(MeshTriangle), static_cast<std::size_t>(index_vector.count));

    for (const MeshTriangle& triangle : indices) {
        if (triangle.index[0] < 0 || triangle.index[1] < 0 || triangle.index[2] < 0) {
            continue;
        }
        const auto a = static_cast<std::size_t>(triangle.index[0]);
        const auto b = static_cast<std::size_t>(triangle.index[1]);
        const auto c = static_cast<std::size_t>(triangle.index[2]);
        if (a >= vertices.size() || b >= vertices.size() || c >= vertices.size()) {
            continue;
        }
        if (degenerate(vertices[a], vertices[b], vertices[c])) {
            continue;
        }
        out.push_back(geometry::Triangle{vertices[a], vertices[b], vertices[c]});
    }
}

void read_hull(const os::Process& process, std::uintptr_t shape,
               std::vector<geometry::Triangle>& out) {
    const auto hull = process.read<std::uintptr_t>(shape + layout::shape_hull);
    if (hull == 0) {
        return;
    }
    const auto scale = process.read<float>(shape + layout::shape_hull_scale);
    if (!std::isfinite(scale)) {
        return;
    }

    const auto vertex_vector = process.read<UtlVector>(hull + layout::hull_vertices);
    const auto edge_vector = process.read<UtlVector>(hull + layout::hull_edges);
    const auto face_vector = process.read<UtlVector>(hull + layout::hull_faces);
    if (!plausible(vertex_vector) || !plausible(edge_vector) || !plausible(face_vector)) {
        return;
    }

    const std::vector<Vec3> vertices = process.read_typed_vec<Vec3>(
        vertex_vector.data, sizeof(Vec3), static_cast<std::size_t>(vertex_vector.count));
    const std::vector<HalfEdge> edges = process.read_typed_vec<HalfEdge>(
        edge_vector.data, sizeof(HalfEdge), static_cast<std::size_t>(edge_vector.count));
    const std::vector<std::uint8_t> faces = process.read_vec(
        face_vector.data, static_cast<std::size_t>(face_vector.count));
    if (vertices.empty() || edges.empty()) {
        return;
    }

    std::vector<Vec3> polygon;
    std::vector<bool> seen;
    for (const std::uint8_t start : faces) {
        if (start >= edges.size()) {
            continue;
        }

        // walk the half edge ring back to where it started
        polygon.clear();
        seen.assign(edges.size(), false);
        std::size_t current = start;
        bool closed = false;
        while (true) {
            if (current >= edges.size() || seen[current]) {
                break;
            }
            seen[current] = true;
            const std::size_t vertex = edges[current].origin;
            if (vertex >= vertices.size()) {
                polygon.clear();
                break;
            }
            polygon.push_back(vertices[vertex] * scale);
            current = edges[current].next;
            if (current == start) {
                closed = true;
                break;
            }
        }
        if (!closed || polygon.size() < 3) {
            continue;
        }

        // fan the face out from its first vertex
        for (std::size_t i = 1; i + 1 < polygon.size(); ++i) {
            if (degenerate(polygon[0], polygon[i], polygon[i + 1])) {
                continue;
            }
            out.push_back(geometry::Triangle{polygon[0], polygon[i], polygon[i + 1]});
        }
    }
}

}  // namespace

std::vector<geometry::Triangle> read_collision_triangles(const Game& game) {
    std::vector<geometry::Triangle> triangles;
    const os::Process& process = game.process();

    const auto world = process.read<std::uintptr_t>(game.offsets().direct.vphys_world);
    if (world == 0) {
        return triangles;
    }
    const auto inner = process.read<std::uintptr_t>(world + layout::world_inner);
    if (inner == 0) {
        return triangles;
    }
    const auto bodies = process.read<std::uintptr_t>(inner + layout::inner_bodies);
    if (bodies == 0) {
        return triangles;
    }
    const auto body_count = process.read<std::int32_t>(bodies + layout::bodies_count);
    if (body_count <= 0 || static_cast<std::size_t>(body_count) > max_vector_items) {
        return triangles;
    }

    // a shape can be shared by several bodies, and reading it twice would double every
    // triangle it contributes
    std::unordered_set<std::uintptr_t> seen_shapes;

    for (std::size_t i = 0; i < static_cast<std::size_t>(body_count); ++i) {
        const std::uintptr_t body = bodies + i * layout::body_stride;
        if (process.read<std::uint32_t>(body + layout::body_kind) != layout::body_kind_geometry) {
            continue;
        }

        const auto root = process.read<std::int32_t>(body + layout::body_root);
        const auto nodes_pointer = process.read<std::uintptr_t>(body + layout::body_nodes);
        const auto count_a = process.read<std::int32_t>(body + layout::body_count_a);
        const auto count_b = process.read<std::int32_t>(body + layout::body_count_b);
        // the two counts describe the same array, so disagreeing means a bad read
        if (nodes_pointer == 0 || count_a <= 0 || count_a != count_b ||
            static_cast<std::size_t>(count_a) > max_vector_items || root < 0 || root >= count_a) {
            continue;
        }

        const std::vector<OuterNode> nodes = process.read_typed_vec<OuterNode>(
            nodes_pointer, sizeof(OuterNode), static_cast<std::size_t>(count_a));
        if (nodes.size() != static_cast<std::size_t>(count_a)) {
            continue;
        }

        std::vector<std::int32_t> stack{root};
        std::vector<bool> visited(static_cast<std::size_t>(count_a), false);
        while (!stack.empty()) {
            const std::int32_t index = stack.back();
            stack.pop_back();
            if (index < 0 || index >= count_a || visited[static_cast<std::size_t>(index)]) {
                continue;
            }
            visited[static_cast<std::size_t>(index)] = true;

            const OuterNode& node = nodes[static_cast<std::size_t>(index)];
            if (node.left == -1 && node.right == -1) {
                if (node.shape == 0 || !seen_shapes.insert(node.shape).second) {
                    continue;
                }
                if ((process.read<std::uint64_t>(node.shape + layout::shape_interacts_as) & 1) == 0) {
                    continue;
                }
                const std::string name = rtti_name(process, node.shape);
                if (name == "12CRnMeshShape") {
                    read_mesh(process, node.shape, triangles);
                } else if (name == "12CRnHullShape") {
                    read_hull(process, node.shape, triangles);
                }
                continue;
            }
            if (node.left >= 0) {
                stack.push_back(node.left);
            }
            if (node.right >= 0) {
                stack.push_back(node.right);
            }
        }
    }

    return triangles;
}

}  // namespace dl::cs2
