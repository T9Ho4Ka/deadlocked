#include <random>
#include <vector>

#include "check.hpp"
#include "geometry/bvh.hpp"

using namespace dl;
using dl::test::check;
using dl::test::close;
using geometry::Aabb;
using geometry::Bvh;
using geometry::Triangle;

/// a unit square in the plane x = at, as two triangles
static std::vector<Triangle> wall_at(float at) {
    return {
        Triangle{Vec3(at, -1.0f, -1.0f), Vec3(at, 1.0f, -1.0f), Vec3(at, 1.0f, 1.0f)},
        Triangle{Vec3(at, -1.0f, -1.0f), Vec3(at, 1.0f, 1.0f), Vec3(at, -1.0f, 1.0f)},
    };
}

int main() {
    // --- boxes ---
    Aabb box;
    check(box.min.x > box.max.x, "a fresh box is inverted, so the first point defines it");
    box.expand(Vec3(1.0f, 2.0f, 3.0f));
    check(box.min == Vec3(1.0f, 2.0f, 3.0f) && box.max == Vec3(1.0f, 2.0f, 3.0f),
          "one point makes a box of no size");
    box.expand(Vec3(-1.0f, 5.0f, 0.0f));
    check(box.min == Vec3(-1.0f, 2.0f, 0.0f) && box.max == Vec3(1.0f, 5.0f, 3.0f),
          "a second point grows it on each axis independently");
    check(box.centroid() == Vec3(0.0f, 3.5f, 1.5f), "the centroid is the middle");

    Aabb other;
    other.expand(Vec3(10.0f));
    const Aabb merged = box.merged(other);
    check(merged.max == Vec3(10.0f), "merging takes the outer bounds");

    Aabb unit;
    unit.expand(Vec3(-1.0f));
    unit.expand(Vec3(1.0f));
    const Vec3 along_x(1.0f, 0.0f, 0.0f);
    check(unit.intersects_ray(Vec3(-5.0f, 0.0f, 0.0f), 1.0f / along_x, 100.0f),
          "a ray aimed at a box hits it");
    check(!unit.intersects_ray(Vec3(-5.0f, 9.0f, 0.0f), 1.0f / along_x, 100.0f),
          "a ray aimed past it does not");
    check(!unit.intersects_ray(Vec3(-5.0f, 0.0f, 0.0f), 1.0f / along_x, 1.0f),
          "and neither does one that stops short");

    // --- triangles ---
    const Triangle triangle{Vec3(0.0f, 0.0f, 0.0f), Vec3(1.0f, 0.0f, 0.0f),
                            Vec3(0.0f, 1.0f, 0.0f)};
    const Vec3 down(0.0f, 0.0f, -1.0f);
    const auto hit = triangle.intersects_ray(Vec3(0.25f, 0.25f, 5.0f), down);
    check(hit.has_value() && close(*hit, 5.0f), "a ray through the middle reports the distance");
    check(!triangle.intersects_ray(Vec3(0.9f, 0.9f, 5.0f), down).has_value(),
          "a ray through the corner the triangle does not cover misses");
    check(!triangle.intersects_ray(Vec3(0.25f, 0.25f, -5.0f), down).has_value(),
          "a triangle behind the origin is not a hit");
    // edge on: the ray runs in the triangle's own plane
    check(!triangle.intersects_ray(Vec3(-5.0f, 0.25f, 0.0f), Vec3(1.0f, 0.0f, 0.0f)).has_value(),
          "an edge on ray is not a hit");

    // --- line of sight ---
    Bvh empty;
    check(empty.has_line_of_sight(Vec3(0.0f), Vec3(100.0f)),
          "with no geometry loaded everything is visible");

    Bvh bvh;
    bvh.build(wall_at(0.0f));
    check(bvh.triangle_count() == 2 && !bvh.empty(), "the wall is loaded");
    check(!bvh.has_line_of_sight(Vec3(-5.0f, 0.0f, 0.0f), Vec3(5.0f, 0.0f, 0.0f)),
          "a wall between two points blocks the view");
    check(bvh.has_line_of_sight(Vec3(-5.0f, 0.0f, 0.0f), Vec3(-1.0f, 0.0f, 0.0f)),
          "two points on the same side of it do not");
    check(bvh.has_line_of_sight(Vec3(-5.0f, 9.0f, 0.0f), Vec3(5.0f, 9.0f, 0.0f)),
          "a line that passes the wall by is clear");
    check(bvh.has_line_of_sight(Vec3(0.5f, 0.0f, 0.0f), Vec3(0.5f, 0.0f, 0.0f)),
          "a point can see itself, rather than dividing by a zero distance");

    // --- a tree deep enough to need splitting ---
    std::vector<Triangle> many;
    std::mt19937 rng(1234);
    std::uniform_real_distribution<float> spread(-50.0f, 50.0f);
    for (int i = 0; i < 500; ++i) {
        const Vec3 at(spread(rng), spread(rng), spread(rng));
        many.push_back(Triangle{at, at + Vec3(1.0f, 0.0f, 0.0f), at + Vec3(0.0f, 1.0f, 0.0f)});
    }
    // one wall far away from all the noise, with a clear approach to it
    for (const Triangle& part : wall_at(500.0f)) {
        many.push_back(part);
    }
    Bvh big;
    big.build(std::move(many));
    check(big.triangle_count() == 502, "every triangle survives the build");
    check(big.node_count() > 1, "and the tree actually branched");
    check(!big.has_line_of_sight(Vec3(499.0f, 0.0f, 0.0f), Vec3(501.0f, 0.0f, 0.0f)),
          "the wall still blocks once it is buried in a real tree");
    check(big.has_line_of_sight(Vec3(501.0f, 0.0f, 0.0f), Vec3(505.0f, 0.0f, 0.0f)),
          "and the far side of it is still clear");

    // every triangle must be reachable: aim a ray at each one in turn and expect a block
    bool all_reachable = true;
    for (const Triangle& part : wall_at(500.0f)) {
        const Vec3 centre = part.centroid();
        if (big.has_line_of_sight(centre - Vec3(2.0f, 0.0f, 0.0f),
                                  centre + Vec3(2.0f, 0.0f, 0.0f))) {
            all_reachable = false;
        }
    }
    check(all_reachable, "the reordering during the build loses no triangle");

    return dl::test::report();
}
