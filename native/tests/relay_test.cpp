#include <cstdint>
#include <string>
#include <vector>

#include "check.hpp"
#include "relay/state.hpp"
#include "relay/websocket.hpp"

using namespace dl;
using dl::test::check;
namespace ws = dl::relay::websocket;

/// masks a payload the way a browser does, so the parser can be given real client input
static std::vector<std::uint8_t> client_frame(const std::string& payload, ws::Opcode opcode) {
    const std::array<std::uint8_t, 4> mask{0x37, 0xFA, 0x21, 0x3D};
    std::vector<std::uint8_t> frame;
    frame.push_back(0x80 | static_cast<std::uint8_t>(opcode));
    frame.push_back(0x80 | static_cast<std::uint8_t>(payload.size()));
    frame.insert(frame.end(), mask.begin(), mask.end());
    for (std::size_t i = 0; i < payload.size(); ++i) {
        frame.push_back(static_cast<std::uint8_t>(payload[i]) ^ mask[i % 4]);
    }
    return frame;
}

int main() {
    // --- the handshake ---
    // the example from rfc 6455 section 1.3, so this is checked against the standard
    // rather than against itself
    check(ws::accept_key("dGhlIHNhbXBsZSBub25jZQ==") == "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=",
          "the accept key matches the rfc's own example");
    check(ws::accept_key("x3JJHMbDL1EzLkh9GBhXDw==") == "HSmrc0sMlYUkAGmm5OPpG2HaGWk=",
          "and a second known key");
    check(ws::accept_key("a") != ws::accept_key("b"), "different keys give different answers");

    // --- frames out ---
    const auto small = ws::text_frame("hi");
    check(small.size() == 4, "a short payload needs two header bytes");
    check(small[0] == 0x81, "fin is set and the opcode is text");
    check(small[1] == 2, "the length is written inline and unmasked");
    check(small[2] == 'h' && small[3] == 'i', "the payload follows");

    const auto medium = ws::text_frame(std::string(300, 'x'));
    check(medium[1] == 126, "a payload over 125 bytes switches to a two byte length");
    check(medium[2] == 1 && medium[3] == 44, "written big endian: 300 is 0x012c");
    check(medium.size() == 4 + 300, "and the payload follows that");

    const auto large = ws::text_frame(std::string(70000, 'x'));
    check(large[1] == 127, "a payload over 65535 switches to eight bytes");
    check(large.size() == 10 + 70000, "with the payload after it");

    // a server must never mask, which is the bit a browser rejects
    check((small[1] & 0x80) == 0 && (medium[1] & 0x80) == 0 && (large[1] & 0x80) == 0,
          "nothing a server sends is masked");

    // --- frames in ---
    const std::string uuid = "f33a18c7-56cc-400d-88b3-41633b96e802";
    const auto incoming = client_frame(uuid, ws::Opcode::Text);
    const auto parsed = ws::parse_frame(incoming);
    check(parsed.has_value(), "a masked client frame parses");
    check(parsed && parsed->payload == uuid, "and comes out unmasked");
    check(parsed && parsed->opcode == ws::Opcode::Text, "with its opcode");
    check(parsed && parsed->consumed == incoming.size(), "having consumed the whole frame");

    const auto closing = ws::parse_frame(client_frame("", ws::Opcode::Close));
    check(closing && closing->opcode == ws::Opcode::Close, "a close frame is recognised");

    // half a frame must be asked for again rather than read past
    std::vector<std::uint8_t> partial(incoming.begin(), incoming.begin() + 6);
    check(!ws::parse_frame(partial).has_value(), "an incomplete frame is not parsed");
    check(!ws::parse_frame({}).has_value(), "and neither is an empty buffer");
    check(!ws::parse_frame({0x81}).has_value(), "nor a lone header byte");

    // --- session bookkeeping ---
    relay::State state;
    check(state.session_count() == 0, "a fresh relay holds no sessions");

    cs2::Snapshot snapshot;
    snapshot.in_game = true;
    snapshot.map_name = "de_dust2";
    state.publish("abc", snapshot);
    check(state.session_count() == 1, "publishing opens a session");
    const auto stored = state.snapshot("abc");
    check(stored.has_value() && stored->map_name == "de_dust2", "and the frame is kept");
    check(!state.snapshot("nope").has_value(), "an unknown session has nothing");

    check(state.drop_stale() == 0, "a fresh session is not stale");
    check(state.session_count() == 1, "so it stays");

    state.close_session("abc");
    check(state.session_count() == 0, "closing removes it");

    check(state.tcp_connections() == 0 && state.websocket_connections() == 0,
          "and no connections are counted");
    state.tcp_opened();
    state.websocket_opened();
    state.websocket_opened();
    check(state.tcp_connections() == 1 && state.websocket_connections() == 2,
          "connections are counted as they open");
    state.websocket_closed();
    check(state.websocket_connections() == 1, "and uncounted as they close");

    return dl::test::report();
}
