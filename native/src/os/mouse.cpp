#include "os/mouse.hpp"

#include <fcntl.h>
#include <linux/uinput.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <ctime>

namespace dl::os {
namespace {

constexpr const char* uinput_path = "/dev/uinput";

std::string errno_message(const char* what) {
    return std::string(what) + ": " + std::strerror(errno);
}

}  // namespace

bool uinput_available(std::string& reason) {
    const int fd = ::open(uinput_path, O_WRONLY | O_CLOEXEC);
    if (fd < 0) {
        reason = errno == ENOENT
                     ? "/dev/uinput does not exist, the uinput kernel module is not loaded"
                     : errno_message("cannot write to /dev/uinput");
        return false;
    }
    ::close(fd);
    reason.clear();
    return true;
}

Mouse::~Mouse() {
    if (fd_ >= 0) {
        ::ioctl(fd_, UI_DEV_DESTROY);
        ::close(fd_);
    }
}

std::string Mouse::open() {
    if (fd_ >= 0) {
        return "the mouse is already open";
    }

    fd_ = ::open(uinput_path, O_WRONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd_ < 0) {
        return errno_message("could not open /dev/uinput");
    }

    // relative motion and one button is all a mouse needs to be
    const int bits[][2]{
        {UI_SET_EVBIT, EV_SYN},  {UI_SET_EVBIT, EV_KEY},   {UI_SET_EVBIT, EV_REL},
        {UI_SET_RELBIT, REL_X},  {UI_SET_RELBIT, REL_Y},   {UI_SET_KEYBIT, BTN_LEFT},
    };
    for (const auto& [request, value] : bits) {
        if (::ioctl(fd_, request, value) < 0) {
            const std::string reason = errno_message("could not configure the uinput device");
            ::close(fd_);
            fd_ = -1;
            return reason;
        }
    }

    uinput_setup setup{};
    setup.id.bustype = BUS_VIRTUAL;
    setup.id.vendor = 0x0001;
    setup.id.product = 0x0001;
    setup.id.version = 1;
    std::snprintf(setup.name, sizeof(setup.name), "deadlocked virtual mouse");

    if (::ioctl(fd_, UI_DEV_SETUP, &setup) < 0 || ::ioctl(fd_, UI_DEV_CREATE) < 0) {
        const std::string reason = errno_message("could not create the uinput device");
        ::close(fd_);
        fd_ = -1;
        return reason;
    }
    return {};
}

void Mouse::emit(std::uint16_t type, std::uint16_t code, std::int32_t value) {
    if (fd_ < 0) {
        return;
    }
    input_event event{};
    timespec now{};
    ::clock_gettime(CLOCK_MONOTONIC, &now);
    event.input_event_sec = now.tv_sec;
    event.input_event_usec = now.tv_nsec / 1000;
    event.type = type;
    event.code = code;
    event.value = value;
    // a short write cannot happen for a single event, and a failed one is not worth
    // tearing the device down over
    [[maybe_unused]] const ssize_t written = ::write(fd_, &event, sizeof(event));
}

void Mouse::sync() { emit(EV_SYN, SYN_REPORT, 0); }

void Mouse::move(const Vec2& delta) {
    const auto x = static_cast<std::int32_t>(delta.x);
    const auto y = static_cast<std::int32_t>(delta.y);
    if (x == 0 && y == 0) {
        return;
    }
    // each axis is reported and synced on its own, the same way the rust client does
    if (x != 0) {
        emit(EV_REL, REL_X, x);
        sync();
    }
    if (y != 0) {
        emit(EV_REL, REL_Y, y);
        sync();
    }
}

void Mouse::press_left() {
    emit(EV_KEY, BTN_LEFT, 1);
    sync();
}

void Mouse::release_left() {
    emit(EV_KEY, BTN_LEFT, 0);
    sync();
}

}  // namespace dl::os
