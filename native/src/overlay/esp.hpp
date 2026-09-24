#pragma once

namespace dl::config {
struct Config;
}
namespace dl::cs2 {
struct Snapshot;
}

namespace dl::overlay {

class Trails;

/// The feature state the hud reports, which lives outside the snapshot.
struct FeatureState {
    bool aimbot_active = false;
    bool triggerbot_active = false;
};

/// Draws one frame of esp into ImGui's background draw list, from a snapshot of the game.
/// Reads nothing from the game itself: by now the entities it came from may be gone.
void draw_esp(const cs2::Snapshot& snapshot, const config::Config& config,
              const FeatureState& features, const Trails& trails);

}  // namespace dl::overlay
