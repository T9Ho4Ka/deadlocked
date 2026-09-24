#pragma once

#include <string>

#include "math.hpp"

namespace dl::os {

/// A virtual mouse, created through the kernel's uinput device.
///
/// This is how aim assistance moves the pointer: the game reads its input from the kernel,
/// so a synthetic device is indistinguishable from a real one as far as the game's own
/// input handling is concerned.
///
/// The device identifies itself honestly. The rust client instead borrows the usb vendor
/// and product id of a texas instruments calculator, which is there to stop the device
/// being recognised for what it is rather than to make it work.
class Mouse {
public:
    Mouse() = default;
    Mouse(const Mouse&) = delete;
    Mouse& operator=(const Mouse&) = delete;
    ~Mouse();

    /// Creates the device. Returns an empty string on success, the reason otherwise.
    [[nodiscard]] std::string open();
    [[nodiscard]] bool ready() const { return fd_ >= 0; }

    /// Moves by a relative amount, which is what a mouse reports and what the game expects.
    void move(const Vec2& delta);
    void press_left();
    void release_left();

private:
    void emit(std::uint16_t type, std::uint16_t code, std::int32_t value);
    void sync();

    int fd_ = -1;
};

/// Whether /dev/uinput exists and can be written to. Without it there is no mouse control.
[[nodiscard]] bool uinput_available(std::string& reason);

}  // namespace dl::os
