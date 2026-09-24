#include "net/json.hpp"

#include <array>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <string_view>
#include <variant>

#include "cs2/snapshot.hpp"
#include "net/json_names.hpp"

namespace dl::net {
namespace {

void append_vec3(std::string& out, const Vec3& value) {
    out += '[';
    out += json_number(value.x);
    out += ',';
    out += json_number(value.y);
    out += ',';
    out += json_number(value.z);
    out += ']';
}

void append_field(std::string& out, std::string_view name) {
    if (out.back() != '{' && out.back() != '[') {
        out += ',';
    }
    out += '"';
    out += name;
    out += "\":";
}

void append_player(std::string& out, const cs2::PlayerData& player) {
    out += '{';
    append_field(out, "steam_id");
    out += std::to_string(player.steam_id);
    append_field(out, "money");
    out += std::to_string(player.money);
    append_field(out, "team");
    out += '"';
    out += team_json_name(player.team);
    out += '"';
    append_field(out, "health");
    out += std::to_string(player.health);
    append_field(out, "max_health");
    out += std::to_string(player.max_health);
    append_field(out, "armor");
    out += std::to_string(player.armor);
    append_field(out, "position");
    append_vec3(out, player.position);
    append_field(out, "name");
    out += '"';
    out += json_escape(player.name);
    out += '"';
    append_field(out, "weapon");
    out += '"';
    out += weapon_json_name(player.weapon);
    out += '"';
    append_field(out, "ammo");
    out += '[';
    out += std::to_string(player.clip_ammo);
    out += ',';
    out += std::to_string(player.reserve_ammo);
    out += ']';
    append_field(out, "has_defuser");
    out += player.has_defuser ? "true" : "false";
    append_field(out, "has_helmet");
    out += player.has_helmet ? "true" : "false";
    append_field(out, "has_bomb");
    out += player.has_bomb ? "true" : "false";
    append_field(out, "color");
    out += std::to_string(player.color);
    append_field(out, "rotation");
    out += json_number(player.rotation);
    out += '}';
}

/// The rust EntityInfo is an externally tagged enum, which serde renders as an object with
/// one field named after the variant.
void append_entity(std::string& out, const cs2::EntityInfo& entity) {
    std::visit(
        [&](const auto& info) {
            using T = std::decay_t<decltype(info)>;
            out += '{';
            if constexpr (std::is_same_v<T, cs2::WeaponInfo>) {
                out += "\"Weapon\":{";
                out += "\"weapon\":\"";
                out += weapon_json_name(info.weapon);
                out += "\",\"position\":";
                append_vec3(out, info.position);
                out += ",\"ammo\":[";
                out += std::to_string(info.clip_ammo);
                out += ',';
                out += std::to_string(info.reserve_ammo);
                out += "]}";
            } else if constexpr (std::is_same_v<T, cs2::InfernoInfo>) {
                out += "\"Inferno\":{\"entity\":";
                out += std::to_string(info.entity);
                out += ",\"position\":";
                append_vec3(out, info.position);
                out += ",\"hull\":[]}";
            } else if constexpr (std::is_same_v<T, cs2::MolotovInfo>) {
                out += "\"Molotov\":{\"entity\":";
                out += std::to_string(info.entity);
                out += ",\"position\":";
                append_vec3(out, info.position);
                out += ",\"is_incendiary\":";
                out += info.is_incendiary ? "true" : "false";
                out += '}';
            } else if constexpr (std::is_same_v<T, cs2::ChickenInfo>) {
                out += "\"Chicken\":{\"position\":";
                append_vec3(out, info.position);
                out += ",\"visible\":false,\"bones\":{}}";
            } else {
                // the four plain grenades share one native type, so the name picks the
                // variant back out
                const std::string_view variant = info.name == "Smoke"       ? "Smoke"
                                                 : info.name == "Flashbang" ? "Flashbang"
                                                 : info.name == "Decoy"     ? "Decoy"
                                                                            : "HeGrenade";
                out += '"';
                out += variant;
                out += "\":{\"entity\":";
                out += std::to_string(info.entity);
                out += ",\"position\":";
                append_vec3(out, info.position);
                out += ",\"name\":\"";
                out += json_escape(info.name);
                out += "\"}";
            }
            out += '}';
        },
        entity);
}

}  // namespace

std::string json_number(float value) {
    if (!std::isfinite(value)) {
        // json has no way to say infinity, and serde writes null
        return "null";
    }
    std::array<char, 32> buffer{};
    const auto result =
        std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
    std::string text(buffer.data(), result.ptr);
    // serde always writes a fractional part, so 45 goes out as 45.0
    if (text.find('.') == std::string::npos && text.find('e') == std::string::npos) {
        text += ".0";
    }
    return text;
}

std::string json_escape(std::string_view text) {
    std::string out;
    out.reserve(text.size());
    for (const char c : text) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    std::array<char, 8> escape{};
                    std::snprintf(escape.data(), escape.size(), "\\u%04x",
                                  static_cast<unsigned>(static_cast<unsigned char>(c)));
                    out += escape.data();
                } else {
                    out += c;
                }
        }
    }
    return out;
}

std::string snapshot_to_json(const cs2::Snapshot& snapshot) {
    std::string out;
    out.reserve(4096);
    out += '{';

    append_field(out, "in_game");
    out += snapshot.in_game ? "true" : "false";
    append_field(out, "is_ffa");
    out += snapshot.is_ffa ? "true" : "false";
    append_field(out, "weapon");
    out += '"';
    out += weapon_json_name(snapshot.weapon);
    out += '"';

    const auto append_players = [&](std::string_view name,
                                    const std::vector<cs2::PlayerData>& players) {
        append_field(out, name);
        out += '[';
        for (const cs2::PlayerData& player : players) {
            if (out.back() != '[') {
                out += ',';
            }
            append_player(out, player);
        }
        out += ']';
    };
    append_players("players", snapshot.players);
    append_players("friendlies", snapshot.friendlies);

    append_field(out, "spectators");
    out += '[';
    for (const std::string& name : snapshot.spectators) {
        if (out.back() != '[') {
            out += ',';
        }
        out += '"';
        out += json_escape(name);
        out += '"';
    }
    out += ']';

    append_field(out, "local_player");
    append_player(out, snapshot.local_player);

    append_field(out, "entities");
    out += '[';
    for (const cs2::EntityInfo& entity : snapshot.entities) {
        if (out.back() != '[') {
            out += ',';
        }
        append_entity(out, entity);
    }
    out += ']';

    append_field(out, "bomb");
    out += "{\"planted\":";
    out += snapshot.bomb.planted ? "true" : "false";
    out += ",\"timer\":";
    out += json_number(snapshot.bomb.timer);
    out += ",\"being_defused\":";
    out += snapshot.bomb.being_defused ? "true" : "false";
    out += ",\"position\":";
    append_vec3(out, snapshot.bomb.position);
    out += ",\"defuse_remain_time\":";
    out += json_number(snapshot.bomb.defuse_remaining);
    out += '}';

    append_field(out, "map_name");
    out += '"';
    out += json_escape(snapshot.map_name);
    out += '"';

    out += '}';
    return out;
}

}  // namespace dl::net
