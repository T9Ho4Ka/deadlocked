#include <cmath>

#include "check.hpp"
#include "game_math.hpp"

using namespace dl;
using dl::test::check;
using dl::test::close;
namespace gm = dl::game_math;

/// builds a view matrix from the game's four rows, which is how it sits in memory
static Mat4 from_rows(Vec4 r0, Vec4 r1, Vec4 r2, Vec4 r3) {
    Mat4 m;
    m[0] = r0;
    m[1] = r1;
    m[2] = r2;
    m[3] = r3;
    return m;
}

int main() {
    // --- euclidean remainder ---
    // std::fmod would answer -10 here, which folds angles the wrong way
    check(close(gm::euclid_mod(-10.0f, 360.0f), 350.0f), "euclid_mod keeps the divisor's sign");
    check(close(gm::euclid_mod(370.0f, 360.0f), 10.0f), "euclid_mod wraps down");
    check(close(gm::euclid_mod(std::fmod(-10.0f, 360.0f), 360.0f), 350.0f),
          "and differs from fmod, which returns -10");

    check(close(gm::wrap_degrees(190.0f), -170.0f), "190 degrees wraps to -170");
    check(close(gm::wrap_degrees(-190.0f), 170.0f), "-190 degrees wraps to 170");
    check(close(gm::wrap_degrees(0.0f), 0.0f), "zero stays zero");

    // --- angles from a direction ---
    check(close(gm::angles_from_vector(Vec3(1.0f, 0.0f, 0.0f)).y, 0.0f), "forward is yaw 0");
    check(close(gm::angles_from_vector(Vec3(0.0f, 1.0f, 0.0f)).y, 90.0f), "left is yaw 90");
    check(close(gm::angles_from_vector(Vec3(-1.0f, 0.0f, 0.0f)).y, 180.0f), "back is yaw 180");
    check(close(gm::angles_from_vector(Vec3(1.0f, 0.0f, 0.0f)).x, 0.0f), "level is pitch 0");
    // the game's pitch grows downwards, so straight up is -90
    check(close(gm::angles_from_vector(Vec3(0.0f, 0.0f, 1.0f)).x, -90.0f), "up is pitch -90");
    check(close(gm::angles_from_vector(Vec3(0.0f, 0.0f, -1.0f)).x, 90.0f), "down is pitch 90");

    // --- angle between two aims ---
    check(close(gm::angles_to_fov(Vec2(10.0f, 20.0f), Vec2(10.0f, 20.0f)), 0.0f),
          "the same aim is zero degrees away");
    check(close(gm::angles_to_fov(Vec2(0.0f, 0.0f), Vec2(0.0f, 3.0f)), 3.0f),
          "three degrees of yaw is three degrees away");
    // across the seam the raw difference is 358, which without wrapping would read as a
    // target on the far side of the map rather than one two degrees away
    check(close(gm::angles_to_fov(Vec2(0.0f, 179.0f), Vec2(0.0f, -179.0f)), 2.0f),
          "the +-180 seam does not inflate the angle");

    // --- clamping ---
    Vec2 angles(100.0f, 0.0f);
    gm::clamp_angles(angles);
    check(close(angles.x, 89.0f), "pitch is capped at 89");

    angles = Vec2(200.0f, 0.0f);
    gm::clamp_angles(angles);
    check(close(angles.x, -89.0f), "a pitch past 180 unwraps then clamps");

    angles = Vec2(-120.0f, 0.0f);
    gm::clamp_angles(angles);
    check(close(angles.x, -89.0f), "pitch is capped at -89");

    angles = Vec2(0.0f, 370.0f);
    gm::clamp_angles(angles);
    check(close(angles.y, 10.0f), "yaw wraps");

    angles = Vec2(45.0f, -90.0f);
    gm::clamp_angles(angles);
    check(close(angles.x, 45.0f) && close(angles.y, -90.0f), "angles in range are left alone");

    // --- projection ---
    const Vec2 window(1920.0f, 1080.0f);
    const Mat4 identity = from_rows(Vec4(1, 0, 0, 0), Vec4(0, 1, 0, 0), Vec4(0, 0, 1, 0),
                                    Vec4(0, 0, 0, 1));

    auto centre = gm::world_to_screen(Vec3(0.0f), identity, window);
    check(centre.has_value(), "the origin projects");
    check(centre && close(centre->x, 960.5f) && close(centre->y, 540.5f),
          "and lands in the middle of the screen");

    // y is flipped on the way to pixels, so a positive y is above the middle
    auto above = gm::world_to_screen(Vec3(0.0f, 0.5f, 0.0f), identity, window);
    check(above && above->y < 540.0f, "positive y draws above the centre");

    auto right = gm::world_to_screen(Vec3(0.5f, 0.0f, 0.0f), identity, window);
    check(right && right->x > 960.0f, "positive x draws right of the centre");

    check(!gm::world_to_screen(Vec3(1.5f, 0.0f, 0.0f), identity, window).has_value(),
          "a position off the side is rejected");

    // a matrix whose w row is z, so anything with a negative z sits behind the camera
    const Mat4 depth = from_rows(Vec4(1, 0, 0, 0), Vec4(0, 1, 0, 0), Vec4(0, 0, 1, 0),
                                 Vec4(0, 0, 1, 0));
    check(!gm::world_to_screen(Vec3(0.0f, 0.0f, -5.0f), depth, window).has_value(),
          "a position behind the camera is rejected");
    check(gm::world_to_screen(Vec3(0.0f, 0.0f, 5.0f), depth, window).has_value(),
          "and one in front is not");

    // the normalized variant never rejects anything, that is the whole point of it
    const Vec2 behind = gm::world_to_screen_normalized(Vec3(0.0f, 0.0f, -5.0f), depth, window);
    check(std::isfinite(behind.x) && std::isfinite(behind.y),
          "the normalized projection answers even from behind the camera");
    const Vec2 far_off = gm::world_to_screen_normalized(Vec3(9.0f, 0.0f, 1.0f), depth, window);
    check(far_off.x > window.x, "and is allowed to land off screen");

    // --- acceleration smoothing ---
    check(close(gm::soft_clamp_acceleration(3.0f, 5.0f, 0.5f), 3.0f),
          "an acceleration under the cap passes through");
    const float clamped = gm::soft_clamp_acceleration(20.0f, 5.0f, 0.5f);
    check(clamped > 5.0f && clamped < 20.0f, "one over the cap is eased, not cut");
    check(gm::soft_clamp_acceleration(-20.0f, 5.0f, 0.5f) == -clamped, "and keeps its sign");

    gm::AccelerationHistory history;
    gm::record_acceleration(history, Vec2(1.0f, -2.0f), 3);
    check(history.size() == 1 && close(history[0].y, 2.0f), "a sample is stored unsigned");
    gm::record_acceleration(history, Vec2(30.0f, 1.0f), 3);
    check(history.size() == 1, "a flick above the limit is dropped");
    gm::record_acceleration(history, Vec2(2.0f, 0.0f), 3);
    gm::record_acceleration(history, Vec2(3.0f, 0.0f), 3);
    gm::record_acceleration(history, Vec2(4.0f, 0.0f), 3);
    check(history.size() == 3, "the history stops at its limit");
    check(close(history[0].x, 4.0f), "with the newest sample first");

    check(close(gm::max_acceleration_x(gm::AccelerationHistory{}, 1.0f, 0.0f, 10.0f, 7.0f), 7.0f),
          "an empty history falls back");
    gm::AccelerationHistory two;
    gm::record_acceleration(two, Vec2(1.0f, 1.0f), 5);
    gm::record_acceleration(two, Vec2(1.0f, 1.0f), 5);
    check(close(gm::max_acceleration_x(two, 1.0f, 0.0f, 10.0f, 7.0f), 7.0f),
          "so does one with only two samples");
    check(close(gm::max_acceleration_x(history, 1.0f, 0.0f, 1.5f, 0.0f), 1.5f),
          "and the result is clamped to the range");

    return dl::test::report();
}
