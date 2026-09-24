#include "overlay/window.hpp"

#include <cstdio>
#include <string>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <GLFW/glfw3.h>

#ifdef DL_HAVE_X11
#define GLFW_EXPOSE_NATIVE_X11
#include <GLFW/glfw3native.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#endif

namespace dl::overlay {
namespace {

#ifdef DL_HAVE_X11
/// Takes the overlay out from under the window manager.
///
/// A managed window cannot be put above a fullscreen game: the compositor decides the
/// stacking and a fullscreen window wins. override_redirect tells X to leave the window
/// alone entirely, which is what makes an overlay possible at all. The rust client does
/// the same thing through winit, and glfw has no option for it, so it is set by hand.
void detach_from_window_manager(GLFWwindow* window) {
    ::Display* display = glfwGetX11Display();
    // ::Window is x11's, not this class: inside this namespace the name is taken
    const ::Window handle = glfwGetX11Window(window);
    if (display == nullptr || handle == 0) {
        std::fprintf(stderr, "no x11 handle for the overlay: it will sit under the game\n");
        return;
    }

    // the attribute is only read when a window is mapped, so it has to be hidden first
    XUnmapWindow(display, handle);

    XSetWindowAttributes attributes{};
    attributes.override_redirect = True;
    XChangeWindowAttributes(display, handle, CWOverrideRedirect, &attributes);

    // a tooltip is the closest thing to "decoration that floats over everything", and is
    // what the rust client asks for too
    const ::Atom type = XInternAtom(display, "_NET_WM_WINDOW_TYPE", False);
    const ::Atom tooltip = XInternAtom(display, "_NET_WM_WINDOW_TYPE_TOOLTIP", False);
    XChangeProperty(display, handle, type, XA_ATOM, 32, PropModeReplace,
                    reinterpret_cast<const unsigned char*>(&tooltip), 1);

    XMapRaised(display, handle);
    XFlush(display);
}
#endif

#ifdef DL_HAVE_X11
/// Reads one window's name, preferring the utf-8 property a modern window manager sets.
std::string window_name(::Display* display, ::Window handle) {
    const ::Atom net_name = XInternAtom(display, "_NET_WM_NAME", False);
    const ::Atom utf8 = XInternAtom(display, "UTF8_STRING", False);

    ::Atom type = 0;
    int format = 0;
    unsigned long count = 0;
    unsigned long remaining = 0;
    unsigned char* value = nullptr;
    if (XGetWindowProperty(display, handle, net_name, 0, 1024, False, utf8, &type, &format,
                           &count, &remaining, &value) == Success &&
        value != nullptr) {
        std::string name(reinterpret_cast<const char*>(value), count);
        XFree(value);
        return name;
    }

    char* legacy = nullptr;
    if (XFetchName(display, handle, &legacy) != 0 && legacy != nullptr) {
        std::string name(legacy);
        XFree(legacy);
        return name;
    }
    return {};
}
#endif

}  // namespace

std::optional<std::pair<Vec2, Vec2>> Window::find_game_window() {
#ifndef DL_HAVE_X11
    return std::nullopt;
#else
    if (glfwGetPlatform() != GLFW_PLATFORM_X11) {
        return std::nullopt;
    }
    ::Display* display = glfwGetX11Display();
    if (display == nullptr) {
        return std::nullopt;
    }

    // the window manager's own list, which is where a top level window actually shows up
    const ::Atom client_list = XInternAtom(display, "_NET_CLIENT_LIST", False);
    ::Atom type = 0;
    int format = 0;
    unsigned long count = 0;
    unsigned long remaining = 0;
    unsigned char* value = nullptr;
    if (XGetWindowProperty(display, DefaultRootWindow(display), client_list, 0, 1024, False,
                           XA_WINDOW, &type, &format, &count, &remaining, &value) != Success ||
        value == nullptr) {
        return std::nullopt;
    }

    const auto* windows = reinterpret_cast<const ::Window*>(value);
    std::optional<std::pair<Vec2, Vec2>> found;
    for (unsigned long i = 0; i < count; ++i) {
        if (window_name(display, windows[i]).find("Counter-Strike") == std::string::npos) {
            continue;
        }
        XWindowAttributes attributes{};
        if (XGetWindowAttributes(display, windows[i], &attributes) == 0) {
            continue;
        }
        // a window's own coordinates are relative to its parent, which the compositor
        // reparents, so they have to be translated back to the screen
        int x = 0;
        int y = 0;
        ::Window child = 0;
        XTranslateCoordinates(display, windows[i], DefaultRootWindow(display), 0, 0, &x, &y,
                              &child);
        found = std::pair{Vec2(static_cast<float>(x), static_cast<float>(y)),
                          Vec2(static_cast<float>(attributes.width),
                               static_cast<float>(attributes.height))};
        break;
    }
    XFree(value);
    return found;
#endif
}

Window::~Window() {
    if (window_ != nullptr) {
        glfwDestroyWindow(window_);
    }
}

bool Window::create() {
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // undecorated and transparent, so only what is drawn is visible
    glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
    glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_TRUE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    // on top of the game, and never taking focus or clicks away from it
    glfwWindowHint(GLFW_FLOATING, GLFW_TRUE);
    glfwWindowHint(GLFW_FOCUS_ON_SHOW, GLFW_FALSE);
    glfwWindowHint(GLFW_MOUSE_PASSTHROUGH, GLFW_TRUE);

    window_ = glfwCreateWindow(1, 1, "deadlocked_overlay", nullptr, nullptr);
    if (window_ == nullptr) {
        std::fprintf(stderr, "could not create the overlay window\n");
        return false;
    }

#ifdef DL_HAVE_X11
    if (glfwGetPlatform() == GLFW_PLATFORM_X11) {
        detach_from_window_manager(window_);
    }
#endif

    if (glfwGetWindowAttrib(window_, GLFW_TRANSPARENT_FRAMEBUFFER) == GLFW_FALSE) {
        // without a compositor the window would be opaque black over the whole game
        std::fprintf(stderr,
                     "the overlay window is not transparent: no compositing is available\n");
    }

    glfwMakeContextCurrent(window_);
    // the overlay must not wait for its own vsync, the game sets the pace
    glfwSwapInterval(0);
    return true;
}

void Window::follow(const Vec2& position, const Vec2& size) {
    if (window_ == nullptr || size.x < 2.0f || size.y < 2.0f) {
        // the game reports a one by one window while it is not focused, and following that
        // would shrink the overlay to nothing
        return;
    }
    if (position != position_ || size != size_) {
        glfwSetWindowPos(window_, static_cast<int>(position.x), static_cast<int>(position.y));
        glfwSetWindowSize(window_, static_cast<int>(size.x), static_cast<int>(size.y));
        position_ = position;
        size_ = size;
#ifdef DL_HAVE_X11
        // nothing manages this window, so it has to lift itself back over the game
        if (glfwGetPlatform() == GLFW_PLATFORM_X11) {
            ::Display* display = glfwGetX11Display();
            if (display != nullptr) {
                XRaiseWindow(display, glfwGetX11Window(window_));
                XFlush(display);
            }
        }
#endif
    }
}

void Window::begin_frame() {
    glfwMakeContextCurrent(window_);
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void Window::end_frame() {
    ImGui::Render();
    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(window_, &width, &height);
    glViewport(0, 0, width, height);
    // fully transparent, so the game shows through everywhere nothing was drawn
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    glfwSwapBuffers(window_);
}

bool Window::alive() const {
    return window_ != nullptr && glfwWindowShouldClose(window_) == GLFW_FALSE;
}

}  // namespace dl::overlay
