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
    /// Clears to full transparency and makes this context current, so anything drawn with
    /// raw gl lands on a blank frame. Must come before drawing the models, because the
    /// clear would otherwise wipe them.
    void clear();
    /// Draws the imgui layer over whatever is already there and presents the frame.
    void end_frame();

    /// Shows or hides the overlay. An unmanaged window has to be raised by hand whenever
    /// it is shown, since no window manager will do it.
    void set_visible(bool visible);
    [[nodiscard]] bool visible() const { return visible_; }

    /// Whether the game is the window the user is currently looking at.
    [[nodiscard]] static bool game_is_active();

    [[nodiscard]] bool alive() const;
    [[nodiscard]] GLFWwindow* handle() const { return window_; }
    [[nodiscard]] Vec2 size() const { return size_; }

private:
    GLFWwindow* window_ = nullptr;
    Vec2 position_{0.0f};
    Vec2 size_{1.0f};
    bool visible_ = true;
};

}  // namespace dl::overlay
