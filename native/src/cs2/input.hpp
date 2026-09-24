#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "config/keycode.hpp"

namespace dl::os {
class Process;
}

namespace dl::cs2 {

struct Offsets;

/// Reads one bit out of a little endian bit set: byte index/8, then bit index%8 counting
/// from the least significant. Exposed so the indexing can be tested on its own.
[[nodiscard]] bool test_bit(std::span<const std::uint8_t> bits, std::size_t index);

/// The game's own keyboard and mouse state, read straight out of its input system rather
/// than from the window manager, so a hotkey works whether or not the overlay has focus.
///
/// Two frames are kept so a key can be told from a key that has just gone down, which is
/// what a toggle needs.
class Input {
public:
    /// how many keys the game tracks
    static constexpr std::size_t max_keys = 512;
    static constexpr std::size_t state_bytes = max_keys / 8;

    /// Pulls the current state, moving what was current to previous.
    void update(const os::Process& process, const Offsets& offsets);

    [[nodiscard]] bool is_pressed(config::KeyCode key) const;
    /// True only on the frame the key goes down.
    [[nodiscard]] bool just_pressed(config::KeyCode key) const;

    [[nodiscard]] std::span<const std::uint8_t> state() const { return current_; }

private:
    std::array<std::uint8_t, state_bytes> previous_{};
    std::array<std::uint8_t, state_bytes> current_{};
};

}  // namespace dl::cs2
