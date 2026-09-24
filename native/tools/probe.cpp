/// Attaches to a running CS2 and reports what the ported layers can actually see.
///
/// Everything below cs2/schema can only be checked against a live game, so this is the
/// harness that does it. It reads and never writes.

#include <cmath>
#include <cstdio>
#include <string>
#include <variant>

#include "constants.hpp"
#include "cs2/game.hpp"
#include "cs2/schema.hpp"
#include "game_math.hpp"
#include "os/process.hpp"

using namespace dl;

namespace {

void heading(const char* text) { std::printf("\n== %s ==\n", text); }

const char* yes_no(bool value) { return value ? "yes" : "no"; }

}  // namespace

int main() {
    heading("process");
    std::optional<os::Process> process = os::Process::open(constants::cs2::process_name);
    if (!process.has_value()) {
        std::printf("cs2 is not running, or its memory cannot be opened\n");
        return 1;
    }
    std::printf("pid           %d\n", process->pid());
    std::printf("data range    %#lx .. %#lx\n", process->data_min(), process->data_max());

    heading("modules");
    for (const std::string_view lib : constants::cs2::libs) {
        const std::optional<std::uintptr_t> base = process->module_base_address(lib);
        std::printf("%-22.*s ", static_cast<int>(lib.size()), lib.data());
        if (!base.has_value()) {
            std::printf("not mapped\n");
            continue;
        }
        std::printf("%#-16lx size %#lx\n", *base, process->module_size(*base));
    }

    heading("schema");
    const std::optional<std::uintptr_t> schema_module =
        process->module_base_address(constants::cs2::schema_lib);
    if (!schema_module.has_value()) {
        std::printf("libschemasystem is not mapped\n");
        return 1;
    }
    const std::optional<cs2::Schema> schema = cs2::Schema::create(*process, *schema_module);
    if (!schema.has_value()) {
        std::printf("could not read the schema system, the pattern is probably outdated\n");
        return 1;
    }
    std::printf("scopes        %zu\n", schema->scope_count());
    if (const cs2::ModuleScope* client =
            schema->get_library(std::string(constants::cs2::client_lib))) {
        std::printf("client classes %zu\n", client->class_count());
        for (const char* name : {"C_BaseEntity", "C_CSPlayerPawn", "CCSPlayerController"}) {
            const cs2::Class* entry = client->get_class(name);
            std::printf("  %-22s %s", name, entry != nullptr ? "found" : "MISSING");
            if (entry != nullptr) {
                std::printf(", size %d\n", entry->size());
            } else {
                std::printf("\n");
            }
        }
    }

    heading("offsets");
    const std::optional<cs2::Offsets> offsets = cs2::find_offsets(*process);
    if (!offsets.has_value()) {
        std::printf("some offsets could not be resolved, see above\n");
        return 1;
    }
    std::printf("all 115 resolved\n");
    std::printf("local_player  %#lx\n", offsets->direct.local_player);
    std::printf("entity list   %#lx\n", offsets->interface.entity);
    std::printf("view matrix   %#lx\n", offsets->direct.view_matrix);
    std::printf("pawn.health   %#lx\n", offsets->entity.health);
    std::printf("pawn.armor    %#lx\n", offsets->pawn.armor);
    std::printf("identity size %#lx\n", offsets->entity_identity.size);

    heading("entities");
    std::optional<cs2::Game> game = cs2::Game::attach();
    if (!game.has_value()) {
        std::printf("could not attach\n");
        return 1;
    }
    game->cache_entities();

    const std::optional<cs2::Player>& local = game->local_player();
    if (!local.has_value()) {
        std::printf("no local player, so not in a match yet\n");
        return 0;
    }

    std::printf("local player  %s\n", local->name(*game).c_str());
    std::printf("  team        %d\n", static_cast<int>(local->pawn().team(*game)));
    std::printf("  health      %d / %d\n", local->pawn().health(*game),
                local->pawn().max_health(*game));
    std::printf("  armor       %d\n", local->armor(*game));
    std::printf("  money       %d\n", local->money(*game));
    const Vec3 position = local->pawn().position(*game);
    std::printf("  position    %.1f %.1f %.1f\n", position.x, position.y, position.z);
    const Vec2 angles = local->view_angles(*game);
    std::printf("  view angles %.1f %.1f\n", angles.x, angles.y);
    std::printf("  weapon      %.*s\n",
                static_cast<int>(config::enum_name(local->weapon(*game)).size()),
                config::enum_name(local->weapon(*game)).data());
    std::printf("  scoped      %s, in air %s, defuser %s\n", yes_no(local->is_scoped(*game)),
                yes_no(local->is_in_air(*game)), yes_no(local->has_defuser(*game)));
    std::printf("  model       %s\n", local->model_name(*game).c_str());

    const std::vector<cs2::BoneTransform> skeleton = local->skeleton(*game, *local);
    std::printf("  skeleton    %zu bones", skeleton.size());
    if (!skeleton.empty()) {
        const auto bones = cs2::Player::bone_positions(skeleton);
        const Vec3 head = bones.at(config::Bones::Head);
        std::printf(", head at %.1f %.1f %.1f", head.x, head.y, head.z);
    }
    std::printf("\n");

    std::printf("\nplayers alive %zu, dead %zu\n", game->players().size(),
                game->dead_players().size());
    for (const cs2::Player& player : game->players()) {
        std::printf("  %-20s team %d  hp %3d  %s\n", player.name(*game).c_str(),
                    static_cast<int>(player.pawn().team(*game)), player.pawn().health(*game),
                    player.spotted_by_local(*game) ? "spotted" : "");
    }

    std::size_t smokes = 0;
    std::size_t molotovs = 0;
    std::size_t infernos = 0;
    std::size_t grenades = 0;
    std::size_t weapons = 0;
    std::size_t chickens = 0;
    for (const cs2::Entity& entity : game->entities()) {
        std::visit(
            [&](const auto& value) {
                using T = std::decay_t<decltype(value)>;
                if constexpr (std::is_same_v<T, cs2::Smoke>) {
                    ++smokes;
                } else if constexpr (std::is_same_v<T, cs2::Molotov>) {
                    ++molotovs;
                } else if constexpr (std::is_same_v<T, cs2::Inferno>) {
                    ++infernos;
                } else if constexpr (std::is_same_v<T, cs2::Chicken>) {
                    ++chickens;
                } else if constexpr (std::is_same_v<T, cs2::DroppedWeapon>) {
                    ++weapons;
                } else {
                    ++grenades;
                }
            },
            entity);
    }
    std::printf("entities      %zu total: %zu dropped weapons, %zu smokes, %zu molotovs, "
                "%zu infernos, %zu other grenades, %zu chickens\n",
                game->entities().size(), weapons, smokes, molotovs, infernos, grenades,
                chickens);

    heading("projection");
    // the view matrix is the last bridge between the maths and the game: if it is read
    // wrong, every drawn box lands somewhere else, and nothing else here would notice
    const Mat4 view = game->process().read<Mat4>(game->offsets().direct.view_matrix);
    std::printf("row 0         %8.3f %8.3f %8.3f %8.3f\n", view[0][0], view[0][1], view[0][2],
                view[0][3]);
    std::printf("row 3 (w)     %8.3f %8.3f %8.3f %8.3f\n", view[3][0], view[3][1], view[3][2],
                view[3][3]);

    const Vec2 window(1920.0f, 1080.0f);
    const auto own = game_math::world_to_screen(local->pawn().position(*game), view, window);
    std::printf("own feet      %s\n", own ? "on screen (expected: only if looking down)"
                                          : "off screen or behind the camera");

    std::size_t projected = 0;
    for (const cs2::Player& player : game->players()) {
        const auto screen = game_math::world_to_screen(player.pawn().position(*game), view, window);
        if (screen.has_value()) {
            ++projected;
            if (projected <= 3) {
                std::printf("  %-18s at %7.1f %7.1f\n", player.name(*game).c_str(), screen->x,
                            screen->y);
            }
        }
    }
    std::printf("players on screen %zu of %zu\n", projected, game->players().size());

    // an aim angle worked back from a direction has to agree with where the player looks
    const Vec3 eye = local->eye_position(*game);
    const Vec2 own_angles = local->view_angles(*game);
    const Vec3 forward(std::cos(glm::radians(own_angles.y)) * std::cos(glm::radians(own_angles.x)),
                       std::sin(glm::radians(own_angles.y)) * std::cos(glm::radians(own_angles.x)),
                       -std::sin(glm::radians(own_angles.x)));
    const Vec2 round_trip = game_math::angles_from_vector(forward);
    std::printf("view angles   %.2f %.2f -> direction -> %.2f %.2f (fov delta %.4f)\n",
                own_angles.x, own_angles.y, round_trip.x, round_trip.y,
                game_math::angles_to_fov(own_angles, round_trip));
    std::printf("eye is %.1f above the feet\n", eye.z - local->pawn().position(*game).z);

    heading("snapshot");
    // the whole frame copied out of the game, which is what the overlay will draw from
    cs2::Snapshot snapshot;
    game->build_snapshot(snapshot);
    std::printf("in game       %s\n", yes_no(snapshot.in_game));
    std::printf("map           %s\n", snapshot.map_name.c_str());
    std::printf("free for all  %s\n", yes_no(snapshot.is_ffa));
    std::printf("window        %.0f,%.0f  size %.0fx%.0f%s\n", snapshot.window_position.x,
                snapshot.window_position.y, snapshot.window_size.x, snapshot.window_size.y,
                snapshot.window_size.x <= 1.0f
                    ? "   (sdl reports no keyboard focus, so the game is not the active window)"
                    : "");
    std::printf("enemies %zu, friendlies %zu, entities %zu\n", snapshot.players.size(),
                snapshot.friendlies.size(), snapshot.entities.size());
    std::printf("sensitivity   %.2f\n", game->sensitivity());
    std::printf("game clock    %.1f s\n", game->current_time());
    if (!snapshot.players.empty()) {
        const cs2::PlayerData& first = snapshot.players.front();
        std::printf("first enemy   %s, hp %d, ammo %d/%d, %zu bones, visible %s\n",
                    first.name.c_str(), first.health, first.clip_ammo, first.reserve_ammo,
                    first.bones.size(), yes_no(first.visible));
        // the snapshot is a copy, so its projection must agree with the live read above
        const auto on_screen =
            game_math::world_to_screen(first.position, snapshot.view_matrix, snapshot.window_size);
        std::printf("              projects to %s\n",
                    on_screen ? "the screen" : "off screen");
    }
    std::printf("local ammo    %d/%d, money %d\n", snapshot.local_player.clip_ammo,
                snapshot.local_player.reserve_ammo, snapshot.local_player.money);

    if (game->planted_c4().has_value()) {
        std::printf("bomb          planted, ticking %s, being defused %s\n",
                    yes_no(game->planted_c4()->is_ticking(*game)),
                    yes_no(game->planted_c4()->being_defused(*game)));
    } else {
        std::printf("bomb          not planted\n");
    }

    return 0;
}
