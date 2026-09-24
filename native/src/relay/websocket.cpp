#include "relay/websocket.hpp"

#include <openssl/evp.h>
#include <openssl/sha.h>

#include <array>
#include <cstring>

namespace dl::relay::websocket {
namespace {

/// the constant rfc 6455 appends to the client's key before hashing
constexpr const char* magic = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

std::string base64(const std::uint8_t* data, std::size_t size) {
    // four output characters per three input bytes, plus the terminator openssl writes
    std::string out((size + 2) / 3 * 4 + 1, '\0');
    const int written = EVP_EncodeBlock(reinterpret_cast<unsigned char*>(out.data()), data,
                                        static_cast<int>(size));
    out.resize(written < 0 ? 0 : static_cast<std::size_t>(written));
    return out;
}

}  // namespace

std::string accept_key(const std::string& client_key) {
    const std::string combined = client_key + magic;
    std::array<unsigned char, SHA_DIGEST_LENGTH> digest{};
    SHA1(reinterpret_cast<const unsigned char*>(combined.data()), combined.size(), digest.data());
    return base64(digest.data(), digest.size());
}

std::vector<std::uint8_t> text_frame(const std::string& payload) {
    std::vector<std::uint8_t> frame;
    // fin set, opcode text
    frame.push_back(0x80 | static_cast<std::uint8_t>(Opcode::Text));

    // a server never masks, so the length goes out without the mask bit
    const std::size_t size = payload.size();
    if (size < 126) {
        frame.push_back(static_cast<std::uint8_t>(size));
    } else if (size <= 0xFFFF) {
        frame.push_back(126);
        frame.push_back(static_cast<std::uint8_t>(size >> 8));
        frame.push_back(static_cast<std::uint8_t>(size));
    } else {
        frame.push_back(127);
        for (int shift = 56; shift >= 0; shift -= 8) {
            frame.push_back(static_cast<std::uint8_t>(size >> shift));
        }
    }

    frame.insert(frame.end(), payload.begin(), payload.end());
    return frame;
}

std::optional<Frame> parse_frame(const std::vector<std::uint8_t>& buffer) {
    if (buffer.size() < 2) {
        return std::nullopt;
    }

    Frame frame;
    frame.opcode = static_cast<Opcode>(buffer[0] & 0x0F);
    const bool masked = (buffer[1] & 0x80) != 0;
    std::size_t length = buffer[1] & 0x7F;
    std::size_t at = 2;

    if (length == 126) {
        if (buffer.size() < at + 2) {
            return std::nullopt;
        }
        length = (static_cast<std::size_t>(buffer[at]) << 8) | buffer[at + 1];
        at += 2;
    } else if (length == 127) {
        if (buffer.size() < at + 8) {
            return std::nullopt;
        }
        length = 0;
        for (int i = 0; i < 8; ++i) {
            length = (length << 8) | buffer[at + static_cast<std::size_t>(i)];
        }
        at += 8;
    }

    std::array<std::uint8_t, 4> mask{};
    if (masked) {
        if (buffer.size() < at + 4) {
            return std::nullopt;
        }
        std::memcpy(mask.data(), buffer.data() + at, mask.size());
        at += 4;
    }

    if (buffer.size() < at + length) {
        return std::nullopt;
    }

    frame.payload.resize(length);
    for (std::size_t i = 0; i < length; ++i) {
        const std::uint8_t byte = buffer[at + i];
        // every frame a client sends is masked, and unmasking is the same xor both ways
        frame.payload[i] = static_cast<char>(masked ? byte ^ mask[i % 4] : byte);
    }
    frame.consumed = at + length;
    return frame;
}

}  // namespace dl::relay::websocket
