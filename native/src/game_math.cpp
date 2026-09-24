#include "game_math.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace dl::game_math {
namespace {

constexpr float radians_to_degrees = 180.0f / std::numbers::pi_v<float>;

/// accelerations above this on either axis are flicks rather than steady motion
constexpr float acceleration_sample_limit = 25.0f;
/// each older sample counts for a little less than the one in front of it
constexpr float weight_step = 0.15f;

float weighted_average(const AccelerationHistory& history, bool use_x) {
    if (history.empty()) {
        return 0.0f;
    }
    float sum = 0.0f;
    float weight_sum = 0.0f;
    for (std::size_t i = 0; i < history.size(); ++i) {
        const float weight = 1.0f + static_cast<float>(i) * weight_step;
        sum += (use_x ? history[i].x : history[i].y) * weight;
        weight_sum += weight;
    }
    return sum / weight_sum;
}

float max_acceleration(const AccelerationHistory& history, bool use_x, float multiplier,
                       float low, float high, float fallback) {
    if (history.size() < 3) {
        return fallback;
    }
    return std::clamp(weighted_average(history, use_x) * multiplier, low, high);
}

}  // namespace

float euclid_mod(float value, float modulus) {
    const float remainder = std::fmod(value, modulus);
    return remainder < 0.0f ? remainder + std::abs(modulus) : remainder;
}

float wrap_degrees(float degrees) { return euclid_mod(degrees + 180.0f, 360.0f) - 180.0f; }

Vec2 angles_from_vector(const Vec3& forward) {
    const float yaw = std::atan2(forward.y, forward.x) * radians_to_degrees;
    const float horizontal = glm::length(Vec2(forward.x, forward.y));
    const float pitch = std::atan2(-forward.z, horizontal) * radians_to_degrees;
    return Vec2(pitch, yaw);
}

float angles_to_fov(const Vec2& view_angles, const Vec2& aim_angles) {
    const Vec2 delta = view_angles - aim_angles;
    // both components have to be wrapped first, or looking across the +-180 seam reads as
    // an enormous angle instead of a small one
    const float pitch = wrap_degrees(delta.x);
    const float yaw = wrap_degrees(delta.y);
    return glm::length(Vec2(std::abs(pitch), std::abs(yaw)));
}

void clamp_angles(Vec2& angles) {
    if (angles.x > 89.0f && angles.x <= 180.0f) {
        angles.x = 89.0f;
    }
    if (angles.x > 180.0f) {
        angles.x -= 360.0f;
    }
    if (angles.x < -89.0f) {
        angles.x = -89.0f;
    }
    angles.y = wrap_degrees(angles.y);
}

namespace {

/// shared by both projections: the clip space x, y and w of a world position
struct Projected {
    Vec2 xy{0.0f};
    float w = 0.0f;
};

Projected project(const Vec3& position, const Mat4& view_matrix) {
    Projected out;
    out.xy = Vec2(view_matrix[0][0] * position.x + view_matrix[0][1] * position.y +
                      view_matrix[0][2] * position.z + view_matrix[0][3],
                  view_matrix[1][0] * position.x + view_matrix[1][1] * position.y +
                      view_matrix[1][2] * position.z + view_matrix[1][3]);
    out.w = view_matrix[3][0] * position.x + view_matrix[3][1] * position.y +
            view_matrix[3][2] * position.z + view_matrix[3][3];
    return out;
}

/// clip space to pixels, with y flipped because the screen counts downwards
Vec2 to_pixels(Vec2 clip, const Vec2& window_size) {
    const Vec2 half = window_size * 0.5f;
    return Vec2(half.x + 0.5f * clip.x * window_size.x + 0.5f,
                half.y - 0.5f * clip.y * window_size.y + 0.5f);
}

}  // namespace

std::optional<Vec2> world_to_screen(const Vec3& position, const Mat4& view_matrix,
                                    const Vec2& window_size) {
    Projected projected = project(position, view_matrix);
    // a w at or below zero means the point is behind the camera
    if (projected.w < 0.0001f) {
        return std::nullopt;
    }
    projected.xy /= projected.w;

    const Vec2 screen = to_pixels(projected.xy, window_size);
    if (screen.x < 0.0f || screen.x > window_size.x || screen.y < 0.0f ||
        screen.y > window_size.y) {
        return std::nullopt;
    }
    return screen;
}

Vec2 world_to_screen_normalized(const Vec3& position, const Mat4& view_matrix,
                                const Vec2& window_size) {
    Projected projected = project(position, view_matrix);
    if (projected.w > 0.0f) {
        projected.xy /= projected.w;
    }
    return to_pixels(projected.xy, window_size);
}

void record_acceleration(AccelerationHistory& history, Vec2 value, std::size_t max_size) {
    if (std::abs(value.x) >= acceleration_sample_limit ||
        std::abs(value.y) >= acceleration_sample_limit) {
        return;
    }
    history.push_front(Vec2(std::abs(value.x), std::abs(value.y)));
    while (history.size() > max_size) {
        history.pop_back();
    }
}

float weighted_average_x(const AccelerationHistory& history) {
    return weighted_average(history, true);
}

float weighted_average_y(const AccelerationHistory& history) {
    return weighted_average(history, false);
}

float max_acceleration_x(const AccelerationHistory& history, float multiplier, float low,
                         float high, float fallback) {
    return max_acceleration(history, true, multiplier, low, high, fallback);
}

float max_acceleration_y(const AccelerationHistory& history, float multiplier, float low,
                         float high, float fallback) {
    return max_acceleration(history, false, multiplier, low, high, fallback);
}

float soft_clamp_acceleration(float acceleration, float max_acceleration, float decay_rate) {
    const float magnitude = std::abs(acceleration);
    if (magnitude <= max_acceleration) {
        return acceleration;
    }
    const float excess = magnitude - max_acceleration;
    const float sign = acceleration < 0.0f ? -1.0f : 1.0f;
    return sign * (max_acceleration + excess * std::exp(-excess * decay_rate));
}

}  // namespace dl::game_math
