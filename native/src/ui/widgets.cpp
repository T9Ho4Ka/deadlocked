#include "ui/widgets.hpp"

#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <cmath>

namespace dl::ui {
namespace {

/// only one keybind can be listening at a time
ImGuiID capturing = 0;

std::uint8_t to_byte(float value) {
    return static_cast<std::uint8_t>(std::lround(std::clamp(value, 0.0f, 1.0f) * 255.0f));
}

}  // namespace

void open_url(const char* url) {
    const pid_t pid = fork();
    if (pid == 0) {
        execlp("xdg-open", "xdg-open", url, nullptr);
        _exit(127);
    }
    if (pid > 0) {
        // the child execs a detaching launcher, so this returns immediately
        int status = 0;
        waitpid(pid, &status, 0);
    }
}

bool section(const char* title, const Palette& palette, Color accent, float scale) {
    ImGui::PushStyleColor(ImGuiCol_Text, heading_color(palette, accent).vec4());
    ImGui::PushFont(nullptr, (body_size + 2.0f) * scale);
    const bool open = ImGui::CollapsingHeader(title, ImGuiTreeNodeFlags_DefaultOpen);
    ImGui::PopFont();
    ImGui::PopStyleColor();
    return open;
}

bool checkbox(const char* label, bool& value, const char* hint) {
    const bool changed = ImGui::Checkbox(label, &value);
    if (hint != nullptr && ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", hint);
    }
    return changed;
}

bool drag_float(const char* label, float& value, float speed, float min, float max,
                const char* format) {
    return ImGui::DragFloat(label, &value, speed, min, max, format,
                            ImGuiSliderFlags_AlwaysClamp);
}

bool drag_uint(const char* label, unsigned& value, float speed, unsigned min, unsigned max) {
    return ImGui::DragScalar(label, ImGuiDataType_U32, &value, speed, &min, &max, "%u",
                             ImGuiSliderFlags_AlwaysClamp);
}

bool color_picker(const char* label, Color& color) {
    float rgba[4]{color.r / 255.0f, color.g / 255.0f, color.b / 255.0f, color.a / 255.0f};
    constexpr ImGuiColorEditFlags flags =
        ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar |
        ImGuiColorEditFlags_AlphaPreviewHalf;
    if (!ImGui::ColorEdit4(label, rgba, flags)) {
        return false;
    }
    color = Color(to_byte(rgba[0]), to_byte(rgba[1]), to_byte(rgba[2]), to_byte(rgba[3]));
    return true;
}

bool keybind(const char* label, config::KeyCode& code) {
    ImGui::PushID(label);
    const ImGuiID id = ImGui::GetID("##keybind");
    const bool listening = capturing == id;

    const std::string text =
        listening ? std::string("press a key...") : std::string(config::enum_name(code));
    if (ImGui::Button(text.c_str(), ImVec2(140.0f, 0.0f))) {
        capturing = listening ? 0 : id;
    }
    ImGui::SameLine();
    ImGui::TextUnformatted(label);
    ImGui::PopID();

    if (!listening) {
        return false;
    }

    if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
        capturing = 0;
        const bool changed = code != config::KeyCode::None;
        code = config::KeyCode::None;
        return changed;
    }

    const config::KeyCode pressed = config::pressed_key();
    if (pressed == config::KeyCode::None) {
        return false;
    }
    capturing = 0;
    if (pressed == code) {
        return false;
    }
    code = pressed;
    return true;
}

}  // namespace dl::ui
