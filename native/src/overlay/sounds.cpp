#include "overlay/sounds.hpp"

#include <algorithm>

#include "config/game.hpp"
#include "cs2/snapshot.hpp"

namespace dl::overlay {

void Sounds::update(const cs2::Snapshot& snapshot) {
    const auto now = std::chrono::steady_clock::now();

    const auto record = [&](const cs2::PlayerData& player) {
        if (player.sound.has_value()) {
            heard_[player.steam_id] = Heard{*player.sound, now};
        }
    };
    for (const cs2::PlayerData& player : snapshot.players) {
        record(player);
    }
    for (const cs2::PlayerData& player : snapshot.friendlies) {
        record(player);
    }

    // a minute of silence and a player is forgotten, which also clears out anyone who has
    // left the server
    std::erase_if(heard_, [&](const auto& entry) {
        return now - entry.second.at > std::chrono::seconds(60);
    });
}

float Sounds::alpha(const cs2::Snapshot& snapshot, const cs2::PlayerData& player,
                    const config::SoundConfig& settings) const {
    if (settings.show_visible && player.visible) {
        // someone in plain sight does not need to make a noise to be drawn
        return 1.0f;
    }

    const auto found = heard_.find(player.steam_id);
    if (found == heard_.end()) {
        return 0.0f;
    }

    // each kind of noise carries a different distance
    const float range = found->second.type == cs2::SoundType::Gunshot ? settings.gunshot_diameter
                        : found->second.type == cs2::SoundType::Weapon ? settings.weapon_diameter
                                                                       : settings.footstep_diameter;
    if (glm::distance(snapshot.local_player.position, player.position) > range) {
        return 0.0f;
    }

    const float age = std::chrono::duration<float>(std::chrono::steady_clock::now() -
                                                   found->second.at)
                          .count();
    // full strength until the fade begins, then down to nothing across its duration
    if (age >= settings.fadeout_start + settings.fadeout_duration) {
        return 0.0f;
    }
    if (settings.fadeout_duration <= 0.0f) {
        return age < settings.fadeout_start ? 1.0f : 0.0f;
    }
    return std::clamp(1.0f - (age - settings.fadeout_start) / settings.fadeout_duration, 0.0f,
                      1.0f);
}

}  // namespace dl::overlay
