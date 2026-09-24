#pragma once

#include <cstddef>
#include <deque>
#include <optional>

#include "math.hpp"

namespace dl::game_math {

/// How many points a drawn cylinder is approximated with.
inline constexpr std::size_t cylinder_samples = 64;

/// Remainder with the sign of the divisor, the way rust's rem_euclid behaves. C++'s fmod
/// keeps the sign of the dividend instead, which would fold angles the wrong way.
[[nodiscard]] float euclid_mod(float value, float modulus);

/// Wraps an angle in degrees into (-180, 180].
[[nodiscard]] float wrap_degrees(float degrees);

/// Pitch and yaw, in degrees, that point along `forward`.
[[nodiscard]] Vec2 angles_from_vector(const Vec3& forward);

/// Angle between where the player is looking and where they would have to look, in degrees.
[[nodiscard]] float angles_to_fov(const Vec2& view_angles, const Vec2& aim_angles);

/// Clamps a pitch and yaw pair to what the game accepts: pitch within +-89 degrees, yaw
/// wrapped. Modifies in place, like the rust version.
void clamp_angles(Vec2& angles);

/// Projects a world position onto the screen.
///
/// The game hands out a row major matrix, and reading it straight into a column major one
/// means each glm column holds one of the game's rows, which is why the products below look
/// transposed. Empty when the position is behind the camera or off screen.
[[nodiscard]] std::optional<Vec2> world_to_screen(const Vec3& position, const Mat4& view_matrix,
                                                  const Vec2& window_size);

/// Same projection, but it never rejects a position: points behind the camera come back
/// mirrored rather than missing, which is what an off screen arrow wants.
[[nodiscard]] Vec2 world_to_screen_normalized(const Vec3& position, const Mat4& view_matrix,
                                              const Vec2& window_size);

/// Recent mouse accelerations, newest first, used to keep aim movement plausible.
using AccelerationHistory = std::deque<Vec2>;

/// Records an acceleration, dropping the oldest once `max_size` is reached. Values above 25
/// on either axis are ignored: they are flicks, not the steady motion being averaged.
void record_acceleration(AccelerationHistory& history, Vec2 value, std::size_t max_size);

/// Weighted mean of one component, newer samples counting for more. Zero when empty.
[[nodiscard]] float weighted_average_x(const AccelerationHistory& history);
[[nodiscard]] float weighted_average_y(const AccelerationHistory& history);

/// The ceiling to apply to one axis, derived from recent motion. Falls back until there are
/// at least three samples to average.
[[nodiscard]] float max_acceleration_x(const AccelerationHistory& history, float multiplier,
                                       float low, float high, float fallback);
[[nodiscard]] float max_acceleration_y(const AccelerationHistory& history, float multiplier,
                                       float low, float high, float fallback);

/// Lets an acceleration past the ceiling, but with the excess decaying away, so the cap is
/// approached smoothly instead of clipping to a hard edge.
[[nodiscard]] float soft_clamp_acceleration(float acceleration, float max_acceleration,
                                            float decay_rate);

}  // namespace dl::game_math
