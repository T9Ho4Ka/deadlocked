#include "ui/theme.hpp"

#include <algorithm>
#include <cmath>

namespace dl::ui {
namespace {

constexpr Color rgb(std::uint8_t r, std::uint8_t g, std::uint8_t b) { return Color(r, g, b); }

/// black or white, whichever stays readable on top of `color`. used for text drawn directly
/// on the accent fill, where a bright accent would otherwise get bright text on it
Color contrasting_text(Color color, const Palette& palette) {
    const float luminance =
        0.2126f * static_cast<float>(color.r) + 0.7152f * static_cast<float>(color.g) +
        0.0722f * static_cast<float>(color.b);
    return luminance > 140.0f ? palette.backdrop : palette.text;
}

void set(ImGuiStyle& style, ImGuiCol slot, Color color) { style.Colors[slot] = color.vec4(); }

}  // namespace

Color lerp(Color from, Color to, float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    const auto mix = [t](std::uint8_t a, std::uint8_t b) {
        const float value =
            static_cast<float>(a) + (static_cast<float>(b) - static_cast<float>(a)) * t;
        return static_cast<std::uint8_t>(std::lround(value));
    };
    return Color(mix(from.r, to.r), mix(from.g, to.g), mix(from.b, to.b), mix(from.a, to.a));
}

std::string_view theme_name(Theme theme) {
    switch (theme) {
        case Theme::Midnight: return "Midnight";
        case Theme::Mocha: return "Mocha";
        case Theme::TokyoNight: return "Tokyo Night";
        case Theme::Nord: return "Nord";
        case Theme::Gruvbox: return "Gruvbox";
        case Theme::RosePine: return "Rose Pine";
        case Theme::Dracula: return "Dracula";
        case Theme::Daylight: return "Daylight";
        case Theme::Custom: return "Custom";
    }
    return "Midnight";
}

Palette theme_palette(Theme theme) {
    switch (theme) {
        case Theme::Midnight:
        case Theme::Custom:
            return Palette{true,          rgb(15, 15, 19),    rgb(26, 26, 32),
                           rgb(46, 46, 58), rgb(72, 72, 90),  rgb(150, 152, 165),
                           rgb(246, 247, 251), rgb(100, 150, 240),
                           {rgb(18, 18, 26), rgb(42, 36, 64)}};
        case Theme::Mocha:
            return Palette{true,          rgb(17, 17, 27),    rgb(24, 24, 37),
                           rgb(49, 50, 68), rgb(76, 78, 102), rgb(166, 173, 200),
                           rgb(223, 230, 252), rgb(203, 166, 247),
                           {rgb(17, 17, 27), rgb(54, 42, 78)}};
        case Theme::TokyoNight:
            return Palette{true,          rgb(19, 20, 28),     rgb(26, 27, 38),
                           rgb(41, 46, 66), rgb(67, 75, 108),  rgb(154, 165, 208),
                           rgb(208, 216, 252), rgb(122, 162, 247),
                           {rgb(19, 20, 28), rgb(36, 42, 82)}};
        case Theme::Nord:
            return Palette{true,          rgb(36, 41, 51),     rgb(46, 52, 64),
                           rgb(67, 76, 94), rgb(90, 101, 124), rgb(192, 201, 216),
                           rgb(240, 243, 249), rgb(136, 192, 208),
                           {rgb(36, 41, 51), rgb(62, 78, 102)}};
        case Theme::Gruvbox:
            return Palette{true,          rgb(22, 24, 25),    rgb(33, 35, 35),
                           rgb(60, 56, 54), rgb(92, 84, 76),  rgb(176, 161, 138),
                           rgb(248, 234, 200), rgb(250, 189, 47),
                           {rgb(22, 24, 25), rgb(68, 54, 40)}};
        case Theme::RosePine:
            return Palette{true,          rgb(18, 16, 28),    rgb(25, 23, 36),
                           rgb(40, 37, 60), rgb(70, 66, 92),  rgb(152, 148, 180),
                           rgb(233, 231, 250), rgb(196, 167, 231),
                           {rgb(18, 16, 28), rgb(62, 42, 70)}};
        case Theme::Dracula:
            return Palette{true,          rgb(25, 26, 34),    rgb(33, 34, 44),
                           rgb(58, 61, 80), rgb(92, 96, 120), rgb(158, 162, 188),
                           rgb(250, 250, 246), rgb(189, 147, 249),
                           {rgb(25, 26, 34), rgb(68, 50, 94)}};
        case Theme::Daylight:
            return Palette{false,              rgb(220, 224, 233), rgb(250, 251, 253),
                           rgb(230, 234, 242), rgb(202, 209, 223), rgb(94, 101, 119),
                           rgb(16, 18, 26),    rgb(45, 98, 214),
                           {rgb(238, 241, 248), rgb(206, 218, 242)}};
    }
    return theme_palette(Theme::Midnight);
}

std::string_view gradient_direction_name(GradientDirection direction) {
    switch (direction) {
        case GradientDirection::Vertical: return "Vertical";
        case GradientDirection::Horizontal: return "Horizontal";
        case GradientDirection::DiagonalDown: return "Diagonal Down";
        case GradientDirection::DiagonalUp: return "Diagonal Up";
    }
    return "Vertical";
}

CustomPalette CustomPalette::from_palette(const Palette& palette) {
    return CustomPalette{palette.dark,    palette.backdrop, palette.base,
                         palette.surface, palette.overlay,  palette.subtext,
                         palette.text};
}

ThemeConfig::ThemeConfig() {
    const Palette palette = theme_palette(Theme::Midnight);
    custom = CustomPalette::from_palette(palette);
    gradient_start = palette.gradient[0];
    gradient_end = palette.gradient[1];
}

Palette ThemeConfig::palette() const {
    Palette resolved = theme_palette(theme);
    if (theme == Theme::Custom) {
        resolved.dark = custom.dark;
        resolved.backdrop = custom.backdrop;
        resolved.base = custom.base;
        resolved.surface = custom.surface;
        resolved.overlay = custom.overlay;
        resolved.subtext = custom.subtext;
        resolved.text = custom.text;
    }
    resolved.gradient = {gradient_start, gradient_end};
    return resolved;
}

void ThemeConfig::sync_gradient() {
    const Palette palette = theme_palette(theme);
    gradient_start = palette.gradient[0];
    gradient_end = palette.gradient[1];
}

void ThemeConfig::seed_custom(Theme from) { custom = CustomPalette::from_palette(theme_palette(from)); }

Color ThemeConfig::panel_fill(Color base) const {
    if (!gradient) {
        return base;
    }
    const float clamped = std::clamp(panel_opacity, 0.0f, 1.0f);
    return base.with_alpha(static_cast<std::uint8_t>(std::lround(clamped * 255.0f)));
}

Color ThemeConfig::secondary_text(const Palette& palette) const {
    return lerp(palette.subtext, palette.text, std::clamp(text_contrast, 0.0f, 1.0f));
}

Color heading_color(const Palette& palette, Color accent) {
    return lerp(palette.text, accent, 0.55f);
}

void apply(const ThemeConfig& config, Color accent) {
    const Palette palette = config.palette();
    const Color secondary = config.secondary_text(palette);
    const Color hovered = palette.overlay;
    const Color active = lerp(palette.overlay, accent, 0.45f);

    ImGuiStyle& style = ImGui::GetStyle();

    // one font at a base size, headings and hints push their own size on top of it
    style.FontSizeBase = body_size * std::clamp(config.text_scale, 0.7f, 2.0f);

    style.WindowPadding = ImVec2(10.0f, 10.0f);
    style.FramePadding = ImVec2(10.0f, 4.0f);
    style.ItemSpacing = ImVec2(8.0f, 6.0f);
    style.ItemInnerSpacing = ImVec2(6.0f, 4.0f);
    style.IndentSpacing = 18.0f;
    style.ScrollbarSize = 11.0f;
    style.GrabMinSize = 10.0f;

    style.WindowRounding = config.corner_radius;
    style.ChildRounding = config.corner_radius;
    style.FrameRounding = config.corner_radius;
    style.PopupRounding = config.corner_radius;
    style.GrabRounding = config.corner_radius;
    style.ScrollbarRounding = config.corner_radius;
    style.TabRounding = config.corner_radius;

    style.WindowBorderSize = 0.0f;
    style.ChildBorderSize = 0.0f;
    style.FrameBorderSize = 1.0f;
    style.PopupBorderSize = 1.0f;

    set(style, ImGuiCol_Text, palette.text);
    set(style, ImGuiCol_TextDisabled, secondary);
    set(style, ImGuiCol_WindowBg, config.panel_fill(palette.base));
    // children stay transparent so the window background, gradient included, shows through
    set(style, ImGuiCol_ChildBg, Color(0, 0, 0, 0));
    // popups and dropdowns stay opaque so their text keeps its contrast
    set(style, ImGuiCol_PopupBg, palette.base);
    set(style, ImGuiCol_Border, palette.overlay);
    set(style, ImGuiCol_BorderShadow, Color(0, 0, 0, 0));

    set(style, ImGuiCol_FrameBg, palette.surface);
    set(style, ImGuiCol_FrameBgHovered, hovered);
    set(style, ImGuiCol_FrameBgActive, active);

    set(style, ImGuiCol_TitleBg, palette.base);
    set(style, ImGuiCol_TitleBgActive, palette.surface);
    set(style, ImGuiCol_TitleBgCollapsed, palette.base);
    set(style, ImGuiCol_MenuBarBg, palette.surface);

    set(style, ImGuiCol_ScrollbarBg, Color(0, 0, 0, 0));
    set(style, ImGuiCol_ScrollbarGrab, palette.surface);
    set(style, ImGuiCol_ScrollbarGrabHovered, hovered);
    set(style, ImGuiCol_ScrollbarGrabActive, accent);

    set(style, ImGuiCol_CheckMark, accent);
    set(style, ImGuiCol_SliderGrab, accent);
    set(style, ImGuiCol_SliderGrabActive, lerp(accent, palette.text, 0.25f));

    set(style, ImGuiCol_Button, palette.surface);
    set(style, ImGuiCol_ButtonHovered, hovered);
    set(style, ImGuiCol_ButtonActive, active);

    set(style, ImGuiCol_Header, palette.surface);
    set(style, ImGuiCol_HeaderHovered, hovered);
    set(style, ImGuiCol_HeaderActive, active);

    set(style, ImGuiCol_Separator, palette.overlay);
    set(style, ImGuiCol_SeparatorHovered, lerp(palette.overlay, accent, 0.5f));
    set(style, ImGuiCol_SeparatorActive, accent);

    set(style, ImGuiCol_ResizeGrip, palette.surface);
    set(style, ImGuiCol_ResizeGripHovered, hovered);
    set(style, ImGuiCol_ResizeGripActive, accent);

    set(style, ImGuiCol_Tab, palette.surface);
    set(style, ImGuiCol_TabHovered, hovered);
    set(style, ImGuiCol_TabSelected, accent);

    set(style, ImGuiCol_TextLink, accent);
    set(style, ImGuiCol_TextSelectedBg, accent);
    set(style, ImGuiCol_InputTextCursor, palette.text);
    set(style, ImGuiCol_NavCursor, accent);

    // text drawn straight on the accent, so a bright accent does not get bright text
    set(style, ImGuiCol_DragDropTarget, contrasting_text(accent, palette));
}

void paint_gradient(const ThemeConfig& config, ImVec2 min, ImVec2 max) {
    if (!config.gradient || max.x <= min.x || max.y <= min.y) {
        return;
    }

    // corner colors are the gradient sampled at each corner's projection onto the gradient
    // axis, so the linear interpolation the gpu does between them reproduces the gradient
    constexpr float diagonal = 0.70710678f;
    ImVec2 axis;
    switch (config.gradient_direction) {
        case GradientDirection::Vertical: axis = ImVec2(0.0f, 1.0f); break;
        case GradientDirection::Horizontal: axis = ImVec2(1.0f, 0.0f); break;
        case GradientDirection::DiagonalDown: axis = ImVec2(diagonal, diagonal); break;
        case GradientDirection::DiagonalUp: axis = ImVec2(diagonal, -diagonal); break;
    }

    const std::array<ImVec2, 4> corners{
        ImVec2(min.x, min.y),
        ImVec2(max.x, min.y),
        ImVec2(max.x, max.y),
        ImVec2(min.x, max.y),
    };

    std::array<float, 4> projections{};
    for (std::size_t i = 0; i < corners.size(); ++i) {
        projections[i] = corners[i].x * axis.x + corners[i].y * axis.y;
    }
    const float lowest = *std::ranges::min_element(projections);
    const float highest = *std::ranges::max_element(projections);
    const float span = highest - lowest;

    std::array<ImU32, 4> colors{};
    for (std::size_t i = 0; i < colors.size(); ++i) {
        const float t = span > 1e-6f ? (projections[i] - lowest) / span : 0.0f;
        colors[i] = lerp(config.gradient_start, config.gradient_end, t).u32();
    }

    ImGui::GetBackgroundDrawList()->AddRectFilledMultiColor(min, max, colors[0], colors[1],
                                                            colors[2], colors[3]);
}

}  // namespace dl::ui
