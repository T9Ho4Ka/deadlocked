#include "cs2/snapshot.hpp"

namespace dl::cs2 {

void Snapshot::clear() {
    in_game = false;
    is_ffa = false;
    weapon = config::Weapon::None;
    players.clear();
    friendlies.clear();
    spectators.clear();
    local_player = PlayerData{};
    entities.clear();
    bomb = BombData{};
    map_name.clear();
}

}  // namespace dl::cs2
