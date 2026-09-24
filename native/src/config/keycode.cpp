#include "config/keycode.hpp"

#include <imgui.h>

namespace dl::config {
namespace {

struct KeyMapping {
    ImGuiKey key;
    KeyCode code;
};

constexpr std::array<KeyMapping, 48> mappings{{
    {ImGuiKey_0, KeyCode::Num0},
    {ImGuiKey_1, KeyCode::Num1},
    {ImGuiKey_2, KeyCode::Num2},
    {ImGuiKey_3, KeyCode::Num3},
    {ImGuiKey_4, KeyCode::Num4},
    {ImGuiKey_5, KeyCode::Num5},
    {ImGuiKey_6, KeyCode::Num6},
    {ImGuiKey_7, KeyCode::Num7},
    {ImGuiKey_8, KeyCode::Num8},
    {ImGuiKey_9, KeyCode::Num9},
    {ImGuiKey_A, KeyCode::A},
    {ImGuiKey_B, KeyCode::B},
    {ImGuiKey_C, KeyCode::C},
    {ImGuiKey_D, KeyCode::D},
    {ImGuiKey_E, KeyCode::E},
    {ImGuiKey_F, KeyCode::F},
    {ImGuiKey_G, KeyCode::G},
    {ImGuiKey_H, KeyCode::H},
    {ImGuiKey_I, KeyCode::I},
    {ImGuiKey_J, KeyCode::J},
    {ImGuiKey_K, KeyCode::K},
    {ImGuiKey_L, KeyCode::L},
    {ImGuiKey_M, KeyCode::M},
    {ImGuiKey_N, KeyCode::N},
    {ImGuiKey_O, KeyCode::O},
    {ImGuiKey_P, KeyCode::P},
    {ImGuiKey_Q, KeyCode::Q},
    {ImGuiKey_R, KeyCode::R},
    {ImGuiKey_S, KeyCode::S},
    {ImGuiKey_T, KeyCode::T},
    {ImGuiKey_U, KeyCode::U},
    {ImGuiKey_V, KeyCode::V},
    {ImGuiKey_W, KeyCode::W},
    {ImGuiKey_X, KeyCode::X},
    {ImGuiKey_Y, KeyCode::Y},
    {ImGuiKey_Z, KeyCode::Z},
    {ImGuiKey_Space, KeyCode::Space},
    {ImGuiKey_Backspace, KeyCode::Backspace},
    {ImGuiKey_Tab, KeyCode::Tab},
    {ImGuiKey_Insert, KeyCode::Insert},
    {ImGuiKey_Delete, KeyCode::Delete},
    {ImGuiKey_Home, KeyCode::Home},
    {ImGuiKey_End, KeyCode::End},
    {ImGuiKey_LeftShift, KeyCode::LeftShift},
    {ImGuiKey_LeftAlt, KeyCode::LeftAlt},
    {ImGuiKey_LeftCtrl, KeyCode::LeftControl},
    {ImGuiKey_MouseX1, KeyCode::Mouse4},
    {ImGuiKey_MouseX2, KeyCode::Mouse5},
}};

}  // namespace

KeyCode pressed_key() {
    for (const auto& [key, code] : mappings) {
        if (ImGui::IsKeyPressed(key, false)) {
            return code;
        }
    }

    // the middle button is bindable, the left one is not: it is what the user clicks to
    // start capturing in the first place
    if (ImGui::IsKeyPressed(ImGuiKey_MouseMiddle, false)) {
        return KeyCode::MouseMiddle;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_MouseRight, false)) {
        return KeyCode::MouseRight;
    }

    const float wheel = ImGui::GetIO().MouseWheel;
    if (wheel > 0.0f) {
        return KeyCode::MouseWheelUp;
    }
    if (wheel < 0.0f) {
        return KeyCode::MouseWheelDown;
    }

    return KeyCode::None;
}

}  // namespace dl::config
