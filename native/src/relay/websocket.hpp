#pragma once

#include <cstdint>
#include <string>
#include <optional>
#include <vector>

namespace dl::relay {

/// The pieces of RFC 6455 this relay needs: the handshake reply, and text frames in and
/// out. No fragmentation, no extensions, no compression, because a browser opening a
/// radar needs none of them.
namespace websocket {

/// The value the `Sec-WebSocket-Accept` header must carry, which is the client's key with
/// a fixed string appended, hashed and base64'd. Getting it wrong means the browser hangs
/// up without saying why.
[[nodiscard]] std::string accept_key(const std::string& client_key);

/// Wraps a payload as a single unmasked text frame, which is what a server sends.
[[nodiscard]] std::vector<std::uint8_t> text_frame(const std::string& payload);

enum class Opcode : std::uint8_t {
    Continuation = 0x0,
    Text = 0x1,
    Binary = 0x2,
    Close = 0x8,
    Ping = 0x9,
    Pong = 0xA,
};

struct Frame {
    Opcode opcode = Opcode::Text;
    std::string payload;
    /// how many bytes of the buffer the frame took
    std::size_t consumed = 0;
};

/// Reads one frame out of a buffer, unmasking it. Empty when the buffer does not hold a
/// whole frame yet, so the caller reads more and tries again.
[[nodiscard]] std::optional<Frame> parse_frame(const std::vector<std::uint8_t>& buffer);

}  // namespace websocket
}  // namespace dl::relay
