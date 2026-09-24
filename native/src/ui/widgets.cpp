#include "ui/widgets.hpp"

#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <cmath>
#include <utility>

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

namespace {

/// What imgui will lay the button out as, so its state can be known before it is drawn.
ImVec2 button_size(const char* label, const ImVec2& requested) {
    const ImGuiStyle& style = ImGui::GetStyle();
    const ImVec2 text = ImGui::CalcTextSize(label, nullptr, true);
    ImVec2 size = requested;
    if (size.x <= 0.0f) {
        size.x = text.x + style.FramePadding.x * 2.0f;
    }
    if (size.y <= 0.0f) {
        size.y = text.y + style.FramePadding.y * 2.0f;
    }
    return size;
}

/// hovered, and whether the button is being held down
std::pair<bool, bool> button_state(const ImVec2& size) {
    const ImVec2 min = ImGui::GetCursorScreenPos();
    const ImVec2 max(min.x + size.x, min.y + size.y);
    const bool hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows) &&
                         ImGui::IsMouseHoveringRect(min, max);
    return {hovered, hovered && ImGui::IsMouseDown(ImGuiMouseButton_Left)};
}

}  // namespace

bool button(const char* label, const ImVec2& size) {
    const ButtonColors& colors = button_colors();
    const auto [hovered, held] = button_state(button_size(label, size));

    const Color border = held ? colors.border_active
                        : hovered ? colors.border_hovered
                                  : colors.border_idle;
    ImGui::PushStyleColor(ImGuiCol_Border, border.vec4());
    ImGui::PushStyleColor(ImGuiCol_Text,
                          (hovered ? colors.text_hovered : colors.text_idle).vec4());
    const bool clicked = ImGui::Button(label, size);
    ImGui::PopStyleColor(2);
    return clicked;
}

bool small_button(const char* label) {
    const ImGuiStyle& style = ImGui::GetStyle();
    const ImVec2 text = ImGui::CalcTextSize(label, nullptr, true);
    // a small button keeps the horizontal padding and drops the vertical, as imgui does
    const auto [hovered, held] = button_state(ImVec2(text.x + style.FramePadding.x * 2.0f, text.y));

    const ButtonColors& colors = button_colors();
    const Color border = held ? colors.border_active
                        : hovered ? colors.border_hovered
                                  : colors.border_idle;
    ImGui::PushStyleColor(ImGuiCol_Border, border.vec4());
    ImGui::PushStyleColor(ImGuiCol_Text,
                          (hovered ? colors.text_hovered : colors.text_idle).vec4());
    const bool clicked = ImGui::SmallButton(label);
    ImGui::PopStyleColor(2);
    return clicked;
}

bool selectable_button(const char* label, bool selected, const ImVec2& size, Color accent,
                       Color selected_text) {
    if (!selected) {
        return button(label, size);
    }

    // a selected button is filled rather than outlined, and its label has to stay readable
    // on the accent, which a bright accent would otherwise swallow
    const Color hovered = lerp(accent, selected_text, 0.15f);
    ImGui::PushStyleColor(ImGuiCol_Button, accent.vec4());
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hovered.vec4());
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, hovered.vec4());
    ImGui::PushStyleColor(ImGuiCol_Border, accent.vec4());
    ImGui::PushStyleColor(ImGuiCol_Text, selected_text.vec4());
    const bool clicked = ImGui::Button(label, size);
    ImGui::PopStyleColor(5);
    return clicked;
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
    if (button(text.c_str(), ImVec2(140.0f, 0.0f))) {
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

void text_settings_button(config::TextSlot slot, std::string& open_popup) {
    const config::TextSlotInfo& info = config::text_slots[static_cast<std::size_t>(slot)];
    ImGui::PushID(info.key.data(), info.key.data() + info.key.size());
    if (small_button("...")) {
        open_popup = std::string(info.key);
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Text settings");
    }
    ImGui::PopID();
}

bool text_settings_popup(config::OverlayTextConfig& text, std::string& open_popup) {
    if (open_popup.empty()) {
        return false;
    }

    const auto match = std::ranges::find_if(
        config::text_slots, [&](const auto& info) { return info.key == open_popup; });
    if (match == config::text_slots.end()) {
        open_popup.clear();
        return false;
    }

    config::TextCategory& category = text[match->slot];
    const std::string title(match->label);

    bool open = true;
    bool changed = false;
    ImGui::SetNextWindowSize(ImVec2(300.0f, 0.0f), ImGuiCond_Appearing);
    if (ImGui::Begin(title.c_str(), &open, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings)) {
        changed |= drag_float("Font Size", category.font_size, 0.2f, 1.0f, 99.0f, "%.1f");
        changed |= color_picker("Color", category.color);
        if (match->player_color) {
            changed |= checkbox("Use Player Color", category.use_player_color);
        }
        ImGui::Separator();
        changed |= enum_combo("Position", category.position);
        changed |= enum_combo("Align", category.align);
    }
    ImGui::End();

    if (!open) {
        open_popup.clear();
    }
    return changed;
}

}  // namespace dl::ui
