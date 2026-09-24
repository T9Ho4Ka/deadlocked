#pragma once

#include <optional>
#include <utility>

#include "math.hpp"

struct GLFWwindow;

namespace dl::overlay {

/// A transparent, undecorated, click through window that sits on top of the game and is
/// moved to cover it.
///
/// It has to be an X11 window: wayland does not let a client place its own window, and an
/// overlay that cannot position itself is useless. The rust client forces X11 for the same
/// reason.
class Window {
public:
    Window() = default;
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    ~Window();

    /// Returns false and explains why when the window or its gl context cannot be made.
    bool create();
    /// Where the game's window is, asked of the display server rather than of the game.
    ///
    /// The game only reports its own window while it has keyboard focus, so following that
    /// alone leaves the overlay a pixel wide whenever the user looks at anything else.
    /// Empty when no such window can be found.
    [[nodiscard]] static std::optional<std::pair<Vec2, Vec2>> find_game_window();

    /// Moves and resizes the overlay to cover the game. Does nothing when the rectangle is
    /// degenerate, which is what the game reports while it is not the active window.
    void follow(const Vec2& position, const Vec2& size);

    void begin_frame();
    /// Presents the frame. The background stays fully transparent, only what was drawn shows.
    void end_frame();

    [[nodiscard]] bool alive() const;
    [[nodiscard]] GLFWwindow* handle() const { return window_; }
    [[nodiscard]] Vec2 size() const { return size_; }

private:
    GLFWwindow* window_ = nullptr;
    Vec2 position_{0.0f};
    Vec2 size_{1.0f};
};

}  // namespace dl::overlay
