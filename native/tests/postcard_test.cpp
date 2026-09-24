#include <cstdio>
#include <string>

#include "check.hpp"
#include "cs2/snapshot.hpp"
#include "net/postcard.hpp"
#include "net/radar.hpp"
#include "net/update.hpp"
#include "net/radar_frame.hpp"

using namespace dl;
using dl::test::check;

static std::string hex(const std::vector<std::uint8_t>& bytes) {
    std::string out;
    char pair[3];
    for (const std::uint8_t byte : bytes) {
        std::snprintf(pair, sizeof(pair), "%02x", byte);
        out += pair;
    }
    return out;
}

template <typename F>
static std::string encode(F&& write) {
    net::Writer writer;
    write(writer);
    return hex(writer.bytes());
}

int main() {
    // every expected string below came from running rust's own postcard serializer on the
    // same value. this is the only thing that can tell a correct encoder from a plausible
    // looking one, since the server just drops a frame it cannot parse.
    check(encode([](auto& w) { w.boolean(true); }) == "01", "bool true");
    check(encode([](auto& w) { w.u8(200); }) == "c8", "u8 is a raw byte");
    check(encode([](auto& w) { w.u32(300); }) == "ac02", "u32 300 is a two byte varint");
    check(encode([](auto& w) { w.u64(UINT64_MAX); }) == "ffffffffffffffffff01",
          "u64 max takes ten bytes");
    check(encode([](auto& w) { w.i32(-1); }) == "01", "i32 -1 zigzags to one byte");
    check(encode([](auto& w) { w.i32(-300); }) == "d704", "i32 -300 zigzags to two");
    check(encode([](auto& w) { w.f32(1.5f); }) == "0000c03f", "f32 is little endian bits");
    check(encode([](auto& w) { w.string("abc"); }) == "03616263", "a string is length then bytes");
    check(encode([](auto& w) { w.length(2); w.u32(1); w.u32(2); }) == "020102",
          "a sequence is length then elements");
    check(encode([](auto& w) { w.variant(3); }) == "03", "an enum is its variant index");
    check(encode([](auto& w) { w.vec3(Vec3(1.0f, 2.0f, 3.0f)); }) ==
              "0000803f0000004000004040",
          "a vec3 is three floats");
    check(encode([](auto& w) { w.i32(7); w.i32(-7); }) == "0e0d", "a tuple is its members");
    check(encode([](auto& w) { w.none(); }) == "00", "none is a zero");
    check(encode([](auto& w) { w.some(); w.u32(5); }) == "0105", "some is a one then the value");

    // and the whole frame, against the same snapshot rust encoded
    cs2::Snapshot snapshot;
    snapshot.in_game = true;
    snapshot.is_ffa = false;
    snapshot.weapon = config::Weapon::AK47;
    snapshot.map_name = "de_dust2";
    snapshot.bomb.planted = true;
    snapshot.bomb.timer = 2.5f;
    snapshot.bomb.being_defused = false;
    snapshot.bomb.position = Vec3(1.0f, 2.0f, 3.0f);
    snapshot.bomb.defuse_remaining = 0.0f;

    cs2::PlayerData player;
    player.steam_id = 76561198000000000ull;
    player.money = 1600;
    player.team = cs2::Team::T;
    player.health = 87;
    player.max_health = 100;
    player.armor = 50;
    player.position = Vec3(10.0f, -20.0f, 30.0f);
    player.name = "Tony";
    player.weapon = config::Weapon::Awp;
    player.clip_ammo = 30;
    player.reserve_ammo = 90;
    player.has_defuser = true;
    player.has_helmet = false;
    player.has_bomb = false;
    player.color = 2;
    player.rotation = 45.0f;
    snapshot.players.push_back(player);

    const std::string expected =
        "010018018098f9929080808801801902ae01c80164000020410000a0c10000f04104546f6e791f3cb4"
        "0101000004000034420000000000000000000000000000000000000000000000000000000000000000"
        "000100002040000000803f0000004000004040000000000864655f6475737432";
    const std::string actual = hex(net::encode_radar_frame(snapshot));
    check(actual == expected, "a whole frame matches rust byte for byte");
    if (actual != expected) {
        std::printf("    expected %s\n    actual   %s\n", expected.c_str(), actual.c_str());
    }

    // --- url normalisation, so a host can be pasted in any of the usual forms ---
    check(net::RadarClient::normalize_url("relay.example.com") == "relay.example.com",
          "a bare host is left alone");
    check(net::RadarClient::normalize_url("https://relay.example.com/") == "relay.example.com",
          "a scheme and a trailing slash come off");
    check(net::RadarClient::normalize_url("  http://relay.example.com  ") == "relay.example.com",
          "and so does surrounding space");
    check(net::RadarClient::normalize_url("relay.example.com///") == "relay.example.com",
          "several trailing slashes come off");
    check(net::RadarClient::normalize_url("").empty(), "an empty host stays empty");

    // --- version comparison, which decides whether the client nags ---
    using net::UpdateCheck;
    check(UpdateCheck::is_newer("1.3.1", "1.3.0"), "a higher patch is newer");
    check(UpdateCheck::is_newer("v1.3.1", "1.3.0"), "a leading v is ignored");
    check(UpdateCheck::is_newer("1.4.0", "1.3.9"), "a higher minor beats a higher patch");
    check(UpdateCheck::is_newer("2.0.0", "1.99.99"), "and a higher major beats both");
    check(!UpdateCheck::is_newer("1.3.0", "1.3.0"), "the same version is not newer");
    check(!UpdateCheck::is_newer("1.2.9", "1.3.0"), "an older one is not newer");
    // a tag that cannot be read sorts as zero, so it reads as older rather than newer
    check(!UpdateCheck::is_newer("not-a-version", "1.3.0"), "an unreadable tag is not newer");
    check(!UpdateCheck::is_newer("1.3", "1.3.0"), "a short tag is not newer than its own zero");

    // --- pulling the fields out of github's answer ---
    const std::string body =
        R"({"url":"x","tag_name":"v1.3.1","name":"1.3.1",)"
        R"("html_url":"https:\/\/github.com\/avitran0\/deadlocked\/releases\/tag\/v1.3.1"})";
    check(UpdateCheck::json_string_field(body, "tag_name") == "v1.3.1", "the tag is read out");
    check(UpdateCheck::json_string_field(body, "html_url") ==
              "https://github.com/avitran0/deadlocked/releases/tag/v1.3.1",
          "and the url, with github's escaped slashes undone");
    check(UpdateCheck::json_string_field(body, "missing").empty(),
          "a field that is not there comes back empty");

    return dl::test::report();
}
