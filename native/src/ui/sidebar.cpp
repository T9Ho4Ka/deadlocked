#include "ui/sidebar.hpp"

#include <imgui.h>

#include "app.hpp"
#include "ui/theme.hpp"
#include "ui/widgets.hpp"

namespace dl::ui {
namespace {

constexpr const char* issues_url = "https://github.com/avitran0/deadlocked/issues";

bool tab_button(std::string_view label, bool selected, float width, const Palette& palette,
                Color accent) {
    const int styles = selected ? 3 : 0;
    if (selected) {
        const Color active = lerp(accent, palette.text, 0.1f);
        ImGui::PushStyleColor(ImGuiCol_Button, accent.vec4());
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, active.vec4());
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, active.vec4());
    }
    const bool clicked = ImGui::Button(label.data(), ImVec2(width, 30.0f));
    ImGui::PopStyleColor(styles);
    return clicked;
}

}  // namespace

void draw_sidebar(AppState& state) {
    const ThemeConfig& theme = state.config.theme;
    const Palette palette = theme.palette();
    const Color accent = state.config.accent_color;
    const Color secondary = theme.secondary_text(palette);
    const ImGuiStyle& style = ImGui::GetStyle();

    ImGui::BeginChild("##sidebar", ImVec2(sidebar_width, 0.0f));

    ImGui::PushFont(nullptr, heading_size * theme.text_scale);
    ImGui::TextColored(accent.vec4(), "deadlocked");
    ImGui::PopFont();

    ImGui::PushFont(nullptr, small_size * theme.text_scale);
    ImGui::TextColored(secondary.vec4(), "v%s", "1.3.0-native");
    ImGui::PopFont();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // the footer's height is reserved before the tab list is laid out, so a short window
    // shrinks the scrolling tab list instead of letting the footer run over the tabs
    const float line = ImGui::GetTextLineHeightWithSpacing();
    const float footer_height = style.ItemSpacing.y * 2.0f + 1.0f  // separator
                                + line * 2.0f                      // status and frame time
                                + style.ItemSpacing.y + ImGui::GetFrameHeight();  // button
    const float tabs_height = ImGui::GetContentRegionAvail().y - footer_height;

    if (ImGui::BeginChild("##tabs", ImVec2(0.0f, tabs_height > 0.0f ? tabs_height : 0.0f))) {
        const float width = ImGui::GetContentRegionAvail().x;
        for (const auto& [tab, label] : tabs) {
            if (tab_button(label, state.tab == tab, width, palette, accent)) {
                state.tab = tab;
            }
        }
    }
    ImGui::EndChild();

    ImGui::Separator();

    // status is stubbed until the cs2 layer is ported, the native client reads no memory yet
    const Color status_color(240, 200, 120);
    const ImVec2 dot = ImGui::GetCursorScreenPos();
    const float radius = 4.0f;
    ImGui::GetWindowDrawList()->AddCircleFilled(
        ImVec2(dot.x + radius, dot.y + line * 0.4f), radius, status_color.u32());
    ImGui::Dummy(ImVec2(radius * 2.0f, 0.0f));
    ImGui::SameLine();
    ImGui::PushFont(nullptr, small_size * theme.text_scale);
    ImGui::TextColored(status_color.vec4(), "Not Started");
    ImGui::TextColored(secondary.vec4(), "%.1f ms", 1000.0f * ImGui::GetIO().DeltaTime);
    ImGui::PopFont();

    if (ImGui::Button("Report Issue", ImVec2(ImGui::GetContentRegionAvail().x, 0.0f))) {
        open_url(issues_url);
    }

    ImGui::EndChild();
}

}  // namespace dl::ui
