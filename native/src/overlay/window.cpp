#include "overlay/window.hpp"

#include <cstdio>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <GLFW/glfw3.h>

namespace dl::overlay {

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
