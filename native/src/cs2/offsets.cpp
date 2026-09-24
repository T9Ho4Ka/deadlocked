#include "cs2/offsets.hpp"

#include <cstdio>
#include <string>
#include <vector>

#include "constants.hpp"
#include "cs2/schema.hpp"
#include "os/process.hpp"

namespace dl::cs2 {
namespace {

/// Collects every offset that could not be resolved instead of giving up on the first one.
/// After a game update that matters: one run names everything that broke.
class Resolver {
public:
    std::uintptr_t require(std::optional<std::uintptr_t> value, const char* what) {
        if (!value.has_value()) {
            missing_.emplace_back(what);
            return 0;
        }
        return *value;
    }

    /// same, but hands the value back as an optional so the caller can skip its follow up
    std::optional<std::uintptr_t> require_opt(std::optional<std::uintptr_t> value,
                                              const char* what) {
        if (!value.has_value()) {
            missing_.emplace_back(what);
        }
        return value;
    }

    std::uintptr_t field(const ModuleScope* scope, const char* class_name, const char* name,
                         const char* what) {
        return require(scope->get(std::string(class_name), std::string(name)), what);
    }

    std::uintptr_t class_size(const ModuleScope* scope, const char* class_name,
                              const char* what) {
        const Class* entry = scope->get_class(std::string(class_name));
        if (entry == nullptr) {
            missing_.emplace_back(what);
            return 0;
        }
        return static_cast<std::uintptr_t>(entry->size());
    }

    [[nodiscard]] bool complete() const { return missing_.empty(); }

    void report() const {
        std::fprintf(stderr, "%zu offsets could not be resolved:\n", missing_.size());
        for (const std::string& entry : missing_) {
            std::fprintf(stderr, "  %s\n", entry.c_str());
        }
    }

private:
    std::vector<std::string> missing_;
};

}  // namespace

std::optional<Offsets> find_offsets(const os::Process& process) {
    Offsets offsets;
    Resolver resolver;

    // --- library ---
    offsets.library.client = resolver.require(process.module_base_address(constants::cs2::client_lib), "library.client");
    offsets.library.engine = resolver.require(process.module_base_address(constants::cs2::engine_lib), "library.engine");
    offsets.library.tier0 = resolver.require(process.module_base_address(constants::cs2::tier0_lib), "library.tier0");
    offsets.library.input = resolver.require(process.module_base_address(constants::cs2::input_lib), "library.input");
    offsets.library.sdl = resolver.require(process.module_base_address(constants::cs2::sdl_lib), "library.sdl");
    offsets.library.schema = resolver.require(process.module_base_address(constants::cs2::schema_lib), "library.schema");
    offsets.library.physics = resolver.require(process.module_base_address(constants::cs2::physics_lib), "library.physics");

    // --- interface ---
    offsets.interface.resource = resolver.require(process.get_interface_offset(offsets.library.engine, "GameResourceServiceClientV0"), "interface.resource");
    offsets.interface.entity = process.read<std::uintptr_t>(offsets.interface.resource + 0x50) + 0x10;
    offsets.interface.cvar = resolver.require(process.get_interface_offset(offsets.library.tier0, "VEngineCvar0"), "interface.cvar");
    offsets.interface.input = resolver.require(process.get_interface_offset(offsets.library.input, "InputSystemVersion0"), "interface.input");

    // --- direct ---
    if (const auto found = resolver.require_opt(process.scan("48 83 3D ? ? ? ? 00 0F 95 C0 C3", offsets.library.client), "direct.local_player")) {
        offsets.direct.local_player = process.get_relative_address(*found, 0x03, 0x08);
    }
    offsets.direct.button_state = process.read<std::uint32_t>(
        process.get_interface_function(offsets.interface.input, 19) + 0x14);
    if (const auto found = resolver.require_opt(process.scan("C6 83 ? ? 00 00 01 4C 8D 05", offsets.library.client), "direct.view_matrix")) {
        offsets.direct.view_matrix = process.get_relative_address(*found + 0x0A, 0x00, 0x04);
    }
    if (const auto found = resolver.require_opt(
            process.get_module_export(offsets.library.sdl, "SDL_GetKeyboardFocus"), "direct.sdl_window")) {
        const auto window = process.read<std::uintptr_t>(
            process.get_relative_address(*found, 0x02, 0x06));
        offsets.direct.sdl_window = process.get_relative_address(window, 0x03, 0x07);
    }
    if (const auto found = resolver.require_opt(process.scan("48 8D 05 ? ? ? ? 45 31 E4 48 8B 00 44 8B 40 10", offsets.library.client), "direct.global_vars")) {
        offsets.direct.global_vars = process.get_relative_address(*found, 0x03, 0x07);
    }
    if (const auto found = resolver.require_opt(process.scan("4c 8d 35 ? ? ? ? 49 8b 3e e8 ? ? ? ? 48 89 c2", offsets.library.client), "direct.vphys_world")) {
        offsets.direct.vphys_world = process.read<std::uintptr_t>(process.get_relative_address(*found, 3, 7));
    }
    if (const auto found = resolver.require_opt(process.scan("4c 89 e6 e8 ? ? ? ? 48 8d 35 ? ? ? ? 48 8d 3d", offsets.library.engine), "direct.build_date")) {
        offsets.direct.build_date = process.get_relative_address(*found, 11, 15);
    }

    // --- convar ---
    offsets.convar.ffa = resolver.require(process.get_convar(offsets.interface.cvar, "mp_teammates_are_enemies"), "convar.ffa");
    offsets.convar.sensitivity = resolver.require(process.get_convar(offsets.interface.cvar, "sensitivity"), "convar.sensitivity");

    // the schema itself has to be read before any schema backed offset below
    const std::optional<Schema> schema = Schema::create(process, offsets.library.schema);
    if (!schema.has_value()) {
        std::fprintf(stderr, "could not read the schema system\n");
        return std::nullopt;
    }
    const ModuleScope* client = schema->get_library(std::string(constants::cs2::client_lib));
    const ModuleScope* physics = schema->get_library(std::string(constants::cs2::physics_lib));
    if (client == nullptr || physics == nullptr) {
        std::fprintf(stderr, "the schema has no client or physics scope\n");
        return std::nullopt;
    }

    // --- controller ---
    offsets.controller.steam_id = resolver.field(client, "CBasePlayerController", "m_steamID", "controller.steam_id");
    offsets.controller.money_services = resolver.field(client, "CCSPlayerController", "m_pInGameMoneyServices", "controller.money_services");
    offsets.controller.money = resolver.field(client, "CCSPlayerController_InGameMoneyServices", "m_iAccount", "controller.money");
    offsets.controller.color = resolver.field(client, "CCSPlayerController", "m_iCompTeammateColor", "controller.color");
    offsets.controller.name = resolver.field(client, "CBasePlayerController", "m_iszPlayerName", "controller.name");
    offsets.controller.pawn = resolver.field(client, "CBasePlayerController", "m_hPawn", "controller.pawn");
    offsets.controller.desired_fov = resolver.field(client, "CBasePlayerController", "m_iDesiredFOV", "controller.desired_fov");
    offsets.controller.owner_entity = resolver.field(client, "C_BaseEntity", "m_hOwnerEntity", "controller.owner_entity");
    offsets.controller.rank = resolver.field(client, "CCSPlayerController", "m_iCompetitiveRanking", "controller.rank");
    offsets.controller.rank_type = resolver.field(client, "CCSPlayerController", "m_iCompetitiveRankType", "controller.rank_type");
    offsets.controller.action_tracking_services = resolver.field(client, "CCSPlayerController", "m_pActionTrackingServices", "controller.action_tracking_services");

    // --- entity ---
    offsets.entity.health = resolver.field(client, "C_BaseEntity", "m_iHealth", "entity.health");
    offsets.entity.max_health = resolver.field(client, "C_BaseEntity", "m_iMaxHealth", "entity.max_health");
    offsets.entity.team = resolver.field(client, "C_BaseEntity", "m_iTeamNum", "entity.team");
    offsets.entity.life_state = resolver.field(client, "C_BaseEntity", "m_lifeState", "entity.life_state");
    offsets.entity.game_scene_node = resolver.field(client, "C_BaseEntity", "m_pGameSceneNode", "entity.game_scene_node");
    offsets.entity.velocity = resolver.field(client, "C_BaseEntity", "m_vecVelocity", "entity.velocity");
    offsets.entity.collision = resolver.field(client, "C_BaseEntity", "m_pCollision", "entity.collision");

    // --- collision ---
    offsets.collision.mins = resolver.field(client, "CCollisionProperty", "m_vecMins", "collision.mins");
    offsets.collision.maxs = resolver.field(client, "CCollisionProperty", "m_vecMaxs", "collision.maxs");

    // --- pawn ---
    offsets.pawn.controller = resolver.field(client, "C_BasePlayerPawn", "m_hController", "pawn.controller");
    offsets.pawn.armor = resolver.field(client, "C_CSPlayerPawn", "m_ArmorValue", "pawn.armor");
    offsets.pawn.fov_multiplier = resolver.field(client, "C_BasePlayerPawn", "m_flFOVSensitivityAdjust", "pawn.fov_multiplier");
    offsets.pawn.eye_offset = resolver.field(client, "C_BaseModelEntity", "m_vecViewOffset", "pawn.eye_offset");
    offsets.pawn.shots_fired = resolver.field(client, "C_CSPlayerPawn", "m_iShotsFired", "pawn.shots_fired");
    offsets.pawn.view_angles = resolver.field(client, "C_BasePlayerPawn", "v_angle", "pawn.view_angles");
    offsets.pawn.eye_angles = resolver.field(client, "C_CSPlayerPawn", "m_angEyeAngles", "pawn.eye_angles");
    offsets.pawn.flags = resolver.field(client, "C_BaseEntity", "m_fFlags", "pawn.flags");
    offsets.pawn.crosshair_entity = resolver.field(client, "C_CSPlayerPawn", "m_iIDEntIndex", "pawn.crosshair_entity");
    offsets.pawn.is_scoped = resolver.field(client, "C_CSPlayerPawn", "m_bIsScoped", "pawn.is_scoped");
    offsets.pawn.deathmatch_immunity = resolver.field(client, "C_CSPlayerPawn", "m_bGunGameImmunity", "pawn.deathmatch_immunity");
    offsets.pawn.observer_services = resolver.field(client, "C_BasePlayerPawn", "m_pObserverServices", "pawn.observer_services");
    offsets.pawn.spotted_state = resolver.field(client, "C_CSPlayerPawn", "m_entitySpottedState", "pawn.spotted_state");
    offsets.pawn.flash_alpha = resolver.field(client, "C_CSPlayerPawnBase", "m_flFlashMaxAlpha", "pawn.flash_alpha");
    offsets.pawn.flash_duration = resolver.field(client, "C_CSPlayerPawnBase", "m_flFlashDuration", "pawn.flash_duration");
    offsets.pawn.camera_services = resolver.field(client, "C_BasePlayerPawn", "m_pCameraServices", "pawn.camera_services");
    offsets.pawn.item_services = resolver.field(client, "C_BasePlayerPawn", "m_pItemServices", "pawn.item_services");
    offsets.pawn.weapon_services = resolver.field(client, "C_BasePlayerPawn", "m_pWeaponServices", "pawn.weapon_services");
    offsets.pawn.aim_punch_services = resolver.field(client, "C_CSPlayerPawn", "m_pAimPunchServices", "pawn.aim_punch_services");
    offsets.pawn.bullet_services = resolver.field(client, "C_CSPlayerPawn", "m_pBulletServices", "pawn.bullet_services");

    // --- game_scene_node ---
    offsets.game_scene_node.dormant = resolver.field(client, "CGameSceneNode", "m_bDormant", "game_scene_node.dormant");
    offsets.game_scene_node.origin = resolver.field(client, "CGameSceneNode", "m_vecAbsOrigin", "game_scene_node.origin");
    offsets.game_scene_node.node_to_world = resolver.field(client, "CGameSceneNode", "m_nodeToWorld", "game_scene_node.node_to_world");
    offsets.game_scene_node.model_state = resolver.field(client, "CSkeletonInstance", "m_modelState", "game_scene_node.model_state");
    offsets.game_scene_node.model_name = resolver.field(client, "CModelState", "m_ModelName", "game_scene_node.model_name");

    // --- smoke ---
    offsets.smoke.did_smoke_effect = resolver.field(client, "C_SmokeGrenadeProjectile", "m_bDidSmokeEffect", "smoke.did_smoke_effect");
    offsets.smoke.smoke_color = resolver.field(client, "C_SmokeGrenadeProjectile", "m_vSmokeColor", "smoke.smoke_color");

    // --- molotov ---
    offsets.molotov.is_incendiary = resolver.field(client, "C_MolotovProjectile", "m_bIsIncGrenade", "molotov.is_incendiary");

    // --- inferno ---
    offsets.inferno.is_burning = resolver.field(client, "C_Inferno", "m_bFireIsBurning", "inferno.is_burning");
    offsets.inferno.fire_count = resolver.field(client, "C_Inferno", "m_fireCount", "inferno.fire_count");
    offsets.inferno.fire_positions = resolver.field(client, "C_Inferno", "m_firePositions", "inferno.fire_positions");

    // --- spotted_state ---
    offsets.spotted_state.spotted = resolver.field(client, "EntitySpottedState_t", "m_bSpotted", "spotted_state.spotted");
    offsets.spotted_state.mask = resolver.field(client, "EntitySpottedState_t", "m_bSpottedByMask", "spotted_state.mask");

    // --- camera_services ---
    offsets.camera_services.fov = resolver.field(client, "CCSPlayerBase_CameraServices", "m_iFOV", "camera_services.fov");

    // --- item_services ---
    offsets.item_services.has_defuser = resolver.field(client, "CCSPlayer_ItemServices", "m_bHasDefuser", "item_services.has_defuser");
    offsets.item_services.has_helmet = resolver.field(client, "CCSPlayer_ItemServices", "m_bHasHelmet", "item_services.has_helmet");

    // --- weapon_services ---
    offsets.weapon_services.weapons = resolver.field(client, "CPlayer_WeaponServices", "m_hMyWeapons", "weapon_services.weapons");
    offsets.weapon_services.active_weapon = resolver.field(client, "CPlayer_WeaponServices", "m_hActiveWeapon", "weapon_services.active_weapon");

    // --- aim_punch_services ---
    offsets.aim_punch_services.aim_punch_cache = resolver.field(client, "CCSPlayer_AimPunchServices", "m_unpredictableBaseTick", "aim_punch_services.aim_punch_cache") - 0x18;

    // --- action_tracking ---
    offsets.action_tracking.round_kills = resolver.field(client, "CCSPlayerController_ActionTrackingServices", "m_iNumRoundKills", "action_tracking.round_kills");
    offsets.action_tracking.round_damage = resolver.field(client, "CCSPlayerController_ActionTrackingServices", "m_flTotalRoundDamageDealt", "action_tracking.round_damage");
    offsets.action_tracking.per_round_stats = resolver.field(client, "CCSPlayerController_ActionTrackingServices", "m_perRoundStats", "action_tracking.per_round_stats");

    // --- bullet_services ---
    offsets.bullet_services.total_hits = resolver.field(client, "CCSPlayer_BulletServices", "m_totalHitsOnServer", "bullet_services.total_hits");

    // --- per_round_stats ---
    offsets.per_round_stats.kills = resolver.field(client, "CSPerRoundStats_t", "m_iKills", "per_round_stats.kills");
    offsets.per_round_stats.deaths = resolver.field(client, "CSPerRoundStats_t", "m_iDeaths", "per_round_stats.deaths");
    offsets.per_round_stats.assists = resolver.field(client, "CSPerRoundStats_t", "m_iAssists", "per_round_stats.assists");
    offsets.per_round_stats.damage = resolver.field(client, "CSPerRoundStats_t", "m_iDamage", "per_round_stats.damage");
    offsets.per_round_stats.size = resolver.class_size(client, "CSPerRoundStats_t", "per_round_stats.size");

    // --- observer_services ---
    offsets.observer_services.target = resolver.field(client, "CPlayer_ObserverServices", "m_hObserverTarget", "observer_services.target");

    // --- econ_item_view ---
    offsets.econ_item_view.item_definition_index = resolver.field(client, "C_EconItemView", "m_iItemDefinitionIndex", "econ_item_view.item_definition_index");

    // --- weapon ---
    offsets.weapon.attribute_manager = resolver.field(client, "C_EconEntity", "m_AttributeManager", "weapon.attribute_manager");
    offsets.weapon.item = resolver.field(client, "C_AttributeContainer", "m_Item", "weapon.item");
    offsets.weapon.item_definition_index = resolver.field(client, "C_EconItemView", "m_iItemDefinitionIndex", "weapon.item_definition_index");
    offsets.weapon.clip_primary = resolver.field(client, "C_BasePlayerWeapon", "m_iClip1", "weapon.clip_primary");
    offsets.weapon.reserve_ammo = resolver.field(client, "C_BasePlayerWeapon", "m_pReserveAmmo", "weapon.reserve_ammo");

    // --- planted_c4 ---
    offsets.planted_c4.is_ticking = resolver.field(client, "C_PlantedC4", "m_bBombTicking", "planted_c4.is_ticking");
    offsets.planted_c4.blow_time = resolver.field(client, "C_PlantedC4", "m_flC4Blow", "planted_c4.blow_time");
    offsets.planted_c4.being_defused = resolver.field(client, "C_PlantedC4", "m_bBeingDefused", "planted_c4.being_defused");
    offsets.planted_c4.is_defused = resolver.field(client, "C_PlantedC4", "m_bBombDefused", "planted_c4.is_defused");
    offsets.planted_c4.has_exploded = resolver.field(client, "C_PlantedC4", "m_bHasExploded", "planted_c4.has_exploded");
    offsets.planted_c4.defuse_time = resolver.field(client, "C_PlantedC4", "m_flDefuseCountDown", "planted_c4.defuse_time");
    offsets.planted_c4.defuse_time_left = resolver.field(client, "C_PlantedC4", "m_flDefuseCountDown", "planted_c4.defuse_time_left");

    // --- model_state ---
    offsets.model_state.skeleton_instance = resolver.field(client, "CBodyComponentSkeletonInstance", "m_skeletonInstance", "model_state.skeleton_instance");

    // --- network_velocity ---
    offsets.network_velocity.x = 0x10;
    offsets.network_velocity.y = 0x18;
    offsets.network_velocity.z = 0x20;

    // --- entity_identity ---
    offsets.entity_identity.size = resolver.class_size(client, "CEntityIdentity", "entity_identity.size");

    // --- hull ---
    offsets.hull.vertices = resolver.field(physics, "RnHull_t", "m_VertexPositions", "hull.vertices");
    offsets.hull.edges = resolver.field(physics, "RnHull_t", "m_Edges", "hull.edges");
    offsets.hull.faces = resolver.field(physics, "RnHull_t", "m_Faces", "hull.faces");
    offsets.hull.flags = resolver.field(physics, "RnHull_t", "m_nFlags", "hull.flags");

    // --- mesh ---
    offsets.mesh.vertices = resolver.field(physics, "RnMesh_t", "m_Vertices", "mesh.vertices");
    offsets.mesh.triangles = resolver.field(physics, "RnMesh_t", "m_Triangles", "mesh.triangles");
    offsets.mesh.materials = resolver.field(physics, "RnMesh_t", "m_Materials", "mesh.materials");
    offsets.mesh.flags = resolver.field(physics, "RnMesh_t", "m_nFlags", "mesh.flags");

    if (!resolver.complete()) {
        resolver.report();
        return std::nullopt;
    }
    return offsets;
}

}  // namespace dl::cs2
