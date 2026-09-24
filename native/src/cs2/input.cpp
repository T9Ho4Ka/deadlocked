#include "cs2/input.hpp"

#include <algorithm>

#include "cs2/offsets.hpp"
#include "os/process.hpp"

namespace dl::cs2 {

bool test_bit(std::span<const std::uint8_t> bits, std::size_t index) {
    const std::size_t byte = index / 8;
    if (byte >= bits.size()) {
        return false;
    }
    return ((bits[byte] >> (index % 8)) & 1) == 1;
}

void Input::update(const os::Process& process, const Offsets& offsets) {
    previous_ = current_;

    // the rust client reads this through /proc/pid/mem rather than process_vm_readv, and
    // so does this, to keep the two reading the same way
    const std::vector<std::uint8_t> state = process.read_bytes(
        offsets.interface.input + offsets.direct.button_state, state_bytes);
    if (state.size() < state_bytes) {
        current_.fill(0);
        return;
    }
    std::copy_n(state.begin(), state_bytes, current_.begin());
}

bool Input::is_pressed(config::KeyCode key) const {
    if (key == config::KeyCode::None) {
        return false;
    }
    return test_bit(current_, static_cast<std::size_t>(key));
}

bool Input::just_pressed(config::KeyCode key) const {
    if (key == config::KeyCode::None) {
        return false;
    }
    const auto index = static_cast<std::size_t>(key);
    return !test_bit(previous_, index) && test_bit(current_, index);
}

}  // namespace dl::cs2
