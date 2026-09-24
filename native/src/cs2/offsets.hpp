#pragma once

#include <cstdint>
#include <optional>

namespace dl::os {
class Process;
}

namespace dl::cs2 {

struct LibraryOffsets {
    std::uintptr_t client = 0;
    std::uintptr_t engine = 0;
    std::uintptr_t tier0 = 0;
    std::uintptr_t input = 0;
    std::uintptr_t sdl = 0;
    std::uintptr_t schema = 0;
    std::uintptr_t physics = 0;
};

struct InterfaceOffsets {
    std::uintptr_t resource = 0;
    std::uintptr_t entity = 0;
    std::uintptr_t cvar = 0;
    std::uintptr_t input = 0;
};

struct DirectOffsets {
    std::uintptr_t local_player = 0;
    std::uintptr_t button_state = 0;
    std::uintptr_t view_matrix = 0;
    std::uintptr_t sdl_window = 0;
    std::uintptr_t global_vars = 0;
    std::uintptr_t vphys_world = 0;
    std::uintptr_t build_date = 0;
};

struct ConvarOffsets {
    std::uintptr_t ffa = 0;
    std::uintptr_t sensitivity = 0;
};

struct PlayerControllerOffsets {
    std::uintptr_t steam_id = 0;
    std::uintptr_t money_services = 0;
    std::uintptr_t money = 0;
    std::uintptr_t color = 0;
    std::uintptr_t name = 0;
    std::uintptr_t pawn = 0;
    std::uintptr_t desired_fov = 0;
    std::uintptr_t owner_entity = 0;
    std::uintptr_t rank = 0;
    std::uintptr_t rank_type = 0;
    std::uintptr_t action_tracking_services = 0;
};

struct EntityOffsets {
    std::uintptr_t health = 0;
    std::uintptr_t max_health = 0;
    std::uintptr_t team = 0;
    std::uintptr_t life_state = 0;
    std::uintptr_t game_scene_node = 0;
    std::uintptr_t velocity = 0;
    std::uintptr_t collision = 0;
};

struct CollisionOffsets {
    std::uintptr_t mins = 0;
    std::uintptr_t maxs = 0;
};

struct PawnOffsets {
    std::uintptr_t controller = 0;
    std::uintptr_t armor = 0;
    std::uintptr_t fov_multiplier = 0;
    std::uintptr_t eye_offset = 0;
    std::uintptr_t shots_fired = 0;
    std::uintptr_t view_angles = 0;
    std::uintptr_t eye_angles = 0;
    std::uintptr_t flags = 0;
    std::uintptr_t crosshair_entity = 0;
    std::uintptr_t is_scoped = 0;
    std::uintptr_t deathmatch_immunity = 0;
    std::uintptr_t observer_services = 0;
    std::uintptr_t spotted_state = 0;
    std::uintptr_t flash_alpha = 0;
    std::uintptr_t flash_duration = 0;
    std::uintptr_t camera_services = 0;
    std::uintptr_t item_services = 0;
    std::uintptr_t weapon_services = 0;
    std::uintptr_t aim_punch_services = 0;
    std::uintptr_t bullet_services = 0;
};

struct GameSceneNodeOffsets {
    std::uintptr_t dormant = 0;
    std::uintptr_t origin = 0;
    std::uintptr_t node_to_world = 0;
    std::uintptr_t model_state = 0;
    std::uintptr_t model_name = 0;
};

struct SmokeOffsets {
    std::uintptr_t did_smoke_effect = 0;
    std::uintptr_t smoke_color = 0;
};

struct MolotovOffsets {
    std::uintptr_t is_incendiary = 0;
};

struct InfernoOffsets {
    std::uintptr_t is_burning = 0;
    std::uintptr_t fire_count = 0;
    std::uintptr_t fire_positions = 0;
};

struct SpottedStateOffsets {
    std::uintptr_t spotted = 0;
    std::uintptr_t mask = 0;
};

struct CameraServicesOffsets {
    std::uintptr_t fov = 0;
};

struct ItemServicesOffsets {
    std::uintptr_t has_defuser = 0;
    std::uintptr_t has_helmet = 0;
};

struct WeaponServicesOffsets {
    std::uintptr_t weapons = 0;
    std::uintptr_t active_weapon = 0;
};

struct AimPunchServicesOffsets {
    std::uintptr_t aim_punch_cache = 0;
};

struct ActionTrackingServicesOffsets {
    std::uintptr_t round_kills = 0;
    std::uintptr_t round_damage = 0;
    std::uintptr_t per_round_stats = 0;
};

struct BulletServicesOffsets {
    std::uintptr_t total_hits = 0;
};

struct PerRoundStatsOffsets {
    std::uintptr_t kills = 0;
    std::uintptr_t deaths = 0;
    std::uintptr_t assists = 0;
    std::uintptr_t damage = 0;
    std::uintptr_t size = 0;
};

struct ObserverServicesOffsets {
    std::uintptr_t target = 0;
};

struct EconItemViewOffsets {
    std::uintptr_t item_definition_index = 0;
};

struct WeaponOffsets {
    std::uintptr_t attribute_manager = 0;
    std::uintptr_t item = 0;
    std::uintptr_t item_definition_index = 0;
    std::uintptr_t clip_primary = 0;
    std::uintptr_t reserve_ammo = 0;
};

struct PlantedC4Offsets {
    std::uintptr_t is_ticking = 0;
    std::uintptr_t blow_time = 0;
    std::uintptr_t being_defused = 0;
    std::uintptr_t is_defused = 0;
    std::uintptr_t has_exploded = 0;
    std::uintptr_t defuse_time = 0;
    std::uintptr_t defuse_time_left = 0;
};

struct ModelState {
    std::uintptr_t skeleton_instance = 0;
};

struct NetworkVelocityOffsets {
    std::uintptr_t x = 0;
    std::uintptr_t y = 0;
    std::uintptr_t z = 0;
};

struct EntityIdentityOffsets {
    std::uintptr_t size = 0;
};

struct PhysHullOffsets {
    std::uintptr_t vertices = 0;
    std::uintptr_t edges = 0;
    std::uintptr_t faces = 0;
    std::uintptr_t flags = 0;
};

struct PhysMeshOffsets {
    std::uintptr_t vertices = 0;
    std::uintptr_t triangles = 0;
    std::uintptr_t materials = 0;
    std::uintptr_t flags = 0;
};

/// Every address and field offset the cheat needs, resolved out of a running game.
/// Ported from cheat/src/cs2/offsets.rs, where the same table is a declarative macro.
struct Offsets {
    LibraryOffsets library;
    InterfaceOffsets interface;
    DirectOffsets direct;
    ConvarOffsets convar;
    PlayerControllerOffsets controller;
    EntityOffsets entity;
    CollisionOffsets collision;
    PawnOffsets pawn;
    GameSceneNodeOffsets game_scene_node;
    SmokeOffsets smoke;
    MolotovOffsets molotov;
    InfernoOffsets inferno;
    SpottedStateOffsets spotted_state;
    CameraServicesOffsets camera_services;
    ItemServicesOffsets item_services;
    WeaponServicesOffsets weapon_services;
    AimPunchServicesOffsets aim_punch_services;
    ActionTrackingServicesOffsets action_tracking;
    BulletServicesOffsets bullet_services;
    PerRoundStatsOffsets per_round_stats;
    ObserverServicesOffsets observer_services;
    EconItemViewOffsets econ_item_view;
    WeaponOffsets weapon;
    PlantedC4Offsets planted_c4;
    ModelState model_state;
    NetworkVelocityOffsets network_velocity;
    EntityIdentityOffsets entity_identity;
    PhysHullOffsets hull;
    PhysMeshOffsets mesh;
};

/// Resolves the whole table. Empty when anything could not be found; unlike the rust
/// version, which stops at the first miss, every missing offset is named on stderr, so
/// one run after a game update tells you everything that broke.
std::optional<Offsets> find_offsets(const os::Process& process);

}  // namespace dl::cs2
