#pragma once

#include <array>
#include <cstdint>
#include <string_view>

#include <imgui.h>

namespace dl::ui {

/// rgba8, the same shape as egui's Color32 so palettes port over unchanged
struct Color {
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;
    std::uint8_t a = 255;

    constexpr Color() = default;
    constexpr Color(std::uint8_t red, std::uint8_t green, std::uint8_t blue, std::uint8_t alpha = 255)
        : r(red), g(green), b(blue), a(alpha) {}

    [[nodiscard]] ImU32 u32() const { return IM_COL32(r, g, b, a); }
    [[nodiscard]] ImVec4 vec4() const {
        return ImVec4(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f);
    }
    [[nodiscard]] Color with_alpha(std::uint8_t alpha) const { return Color(r, g, b, alpha); }

    friend bool operator==(const Color&, const Color&) = default;
};

Color lerp(Color from, Color to, float t);

/// Every structural color of the gui window. The accent is owned by the config, not by the
/// palette, so the user can pick a theme and an accent independently.
struct Palette {
    bool dark = true;
    Color backdrop;  ///< deepest background: text fields, scroll troughs
    Color base;      ///< panel and window background
    Color surface;   ///< idle widget background
    Color overlay;   ///< hovered widget background, borders
    Color subtext;   ///< secondary text
    Color text;      ///< primary text
    Color accent;    ///< accent the theme ships with
    std::array<Color, 2> gradient{};
};

enum class Theme : int {
    Midnight,
    Mocha,
    TokyoNight,
    Nord,
    Gruvbox,
    RosePine,
    Dracula,
    Daylight,
    Custom,
};

inline constexpr int theme_count = static_cast<int>(Theme::Custom) + 1;

std::string_view theme_name(Theme theme);
Palette theme_palette(Theme theme);

enum class GradientDirection : int {
    Vertical,
    Horizontal,
    DiagonalDown,
    DiagonalUp,
};

inline constexpr int gradient_direction_count = static_cast<int>(GradientDirection::DiagonalUp) + 1;

std::string_view gradient_direction_name(GradientDirection direction);

/// User editable palette behind Theme::Custom.
struct CustomPalette {
    bool dark = true;
    Color backdrop;
    Color base;
    Color surface;
    Color overlay;
    Color subtext;
    Color text;

    static CustomPalette from_palette(const Palette& palette);
};

/// Type sizes in points, before text_scale is applied. The spread between them is what gives
/// the window a reading order instead of one flat wall of text.
inline constexpr float heading_size = 21.0f;
inline constexpr float body_size = 15.0f;
inline constexpr float small_size = 12.0f;

struct ThemeConfig {
    Theme theme = Theme::Midnight;
    CustomPalette custom;
    bool gradient = true;
    Color gradient_start;
    Color gradient_end;
    GradientDirection gradient_direction = GradientDirection::DiagonalDown;
    /// how opaque the panels sit on top of the gradient, 0..=1
    float panel_opacity = 0.72f;
    /// multiplier on every text size, 1.0 is the designed scale
    float text_scale = 1.0f;
    /// pulls secondary text towards the primary text color, 0..=1
    float text_contrast = 0.0f;
    float corner_radius = 6.0f;
    /// egui draws a real blurred drop shadow; imgui has only a border shadow, so this
    /// is the same setting with a weaker effect rather than the same look
    bool shadows = true;

    ThemeConfig();

    /// the preset, or the user's own colors when Theme::Custom is picked
    [[nodiscard]] Palette palette() const;
    /// pull gradient stops from the current theme, used when the theme changes
    void sync_gradient();
    /// seed the custom palette from a built in theme, so editing starts somewhere sane
    void seed_custom(Theme from);
    /// panel background, translucent while a gradient is showing through
    [[nodiscard]] Color panel_fill(Color base) const;
    /// secondary text color, pulled towards the primary one by text_contrast
    [[nodiscard]] Color secondary_text(const Palette& palette) const;
};

/// Colors a button takes in each state.
///
/// egui gives every widget state its own border and text colour. ImGui has one global
/// colour for each, so a button drawn straight through it never changes its border or
/// brightens its label. These are kept here for the button helper to push by hand.
struct ButtonColors {
    Color border_idle;
    Color border_hovered;
    Color border_active;
    Color text_idle;
    Color text_hovered;
};

/// What the last call to `apply` worked out.
[[nodiscard]] const ButtonColors& button_colors();

/// Color for section headers: primary text tinted towards the accent, so headers read as
/// their own level instead of blending into the body text.
Color heading_color(const Palette& palette, Color accent);

/// Pushes the theme into the current ImGui style.
void apply(const ThemeConfig& config, Color accent);

/// Paints the window background gradient into the background draw list.
void paint_gradient(const ThemeConfig& config, ImVec2 min, ImVec2 max);

}  // namespace dl::ui
