#pragma once

#include <string>

#include <imgui.h>

#include "config/enums.hpp"
#include "config/keycode.hpp"
#include "config/text.hpp"
#include "ui/theme.hpp"

namespace dl::ui {

/// Opens a url in the user's browser. No shell in between, so nothing in the url can be
/// interpreted as a command.
void open_url(const char* url);

/// A button that changes its border and its label with its state, the way the rust client's
/// do. ImGui only has one border colour and one text colour, so the state is worked out
/// before the button is drawn and the colours pushed around it.
bool button(const char* label, const ImVec2& size = ImVec2(0.0f, 0.0f));

/// The same, without the frame padding, for buttons that sit beside another control.
bool small_button(const char* label);

/// The same, drawn as selected: filled with the accent instead of outlined.
bool selectable_button(const char* label, bool selected, const ImVec2& size, Color accent,
                       Color selected_text);

/// Section header, the equivalent of the rust `collapsing_open` helper: open by default and
/// drawn in the heading color and size so it reads as its own level.
bool section(const char* title, const Palette& palette, Color accent, float scale);

/// A plain checkbox that reports whether it changed, optionally with a hover hint.
bool checkbox(const char* label, bool& value, const char* hint = nullptr);

bool drag_float(const char* label, float& value, float speed, float min, float max,
                const char* format = "%.2f");
bool drag_uint(const char* label, unsigned& value, float speed, unsigned min, unsigned max);

/// Swatch plus a full picker. The rust client used four drag boxes, a picker is strictly
/// more usable and keeps the same rgba semantics.
bool color_picker(const char* label, Color& color);

/// Click to arm, then press any key or mouse button to bind it. Escape clears the binding.
bool keybind(const char* label, config::KeyCode& code);

/// Gear button that opens the text settings popup for one overlay text slot.
void text_settings_button(config::TextSlot slot, std::string& open_popup);

/// Draws whichever text settings popup is open. Returns true when something changed.
bool text_settings_popup(config::OverlayTextConfig& text, std::string& open_popup);

/// Combo box over any config enum that provides an entry table.
template <typename E>
bool enum_combo(const char* label, E& value) {
    bool changed = false;
    const std::string current(config::enum_name(value));
    if (ImGui::BeginCombo(label, current.c_str())) {
        for (const auto& entry : config::enum_entries(E{})) {
            const std::string name(entry.name);
            if (ImGui::Selectable(name.c_str(), entry.value == value) && entry.value != value) {
                value = entry.value;
                changed = true;
            }
        }
        ImGui::EndCombo();
    }
    return changed;
}

}  // namespace dl::ui
