#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <unordered_map>

#include "cs2/sound.hpp"

namespace dl::cs2 {
struct Snapshot;
struct PlayerData;
}

namespace dl::config {
struct SoundConfig;
}

namespace dl::overlay {

/// When each player was last heard, so the esp can fade them out afterwards.
///
/// The snapshot is a fresh copy every frame and carries only what a player is doing right
/// now, so the moment they made a noise has to be remembered outside it. Players are keyed
/// by steam id, which survives them being rebuilt between frames.
class Sounds {
public:
    void update(const cs2::Snapshot& snapshot);

    /// How strongly a player should be drawn: one while they are plainly visible or have
    /// just been heard, fading to zero as the sound ages, and zero when they are silent or
    /// too far away for that kind of sound to carry.
    [[nodiscard]] float alpha(const cs2::Snapshot& snapshot, const cs2::PlayerData& player,
                              const config::SoundConfig& settings) const;

private:
    struct Heard {
        cs2::SoundType type = cs2::SoundType::Footstep;
        std::chrono::steady_clock::time_point at;
    };

    std::unordered_map<std::uint64_t, Heard> heard_;
};

}  // namespace dl::overlay
