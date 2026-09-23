use std::fmt::Display;

use egui::{
    Color32, CornerRadius, Mesh, Painter, Pos2, Rect, Shadow, Stroke, Style, TextStyle, Vec2,
};
use serde::{Deserialize, Serialize};
use strum::EnumIter;

const fn rgb(r: u8, g: u8, b: u8) -> Color32 {
    Color32::from_rgb(r, g, b)
}

/// Every structural color of the gui window. Accent colors from [`crate::ui::color::Colors`]
/// stay independent of this, they are picked by the user.
#[derive(Clone, Copy)]
pub struct Palette {
    pub dark: bool,
    /// deepest background, used for text edits and scroll troughs
    pub backdrop: Color32,
    /// default panel and window background
    pub base: Color32,
    /// idle widget background
    pub surface: Color32,
    /// hovered widget background
    pub overlay: Color32,
    /// secondary text: hints, units, inactive labels
    pub subtext: Color32,
    /// primary text
    pub text: Color32,
    /// accent the theme ships with, applied when the theme is picked
    pub accent: Color32,
    /// default gradient stops of the theme
    pub gradient: [Color32; 2],
}

#[derive(Debug, Clone, Copy, Default, PartialEq, EnumIter, Serialize, Deserialize)]
pub enum Theme {
    #[default]
    Midnight,
    Mocha,
    TokyoNight,
    Nord,
    Gruvbox,
    RosePine,
    Dracula,
    Daylight,
    Custom,
}

impl Display for Theme {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.write_str(match self {
            Self::Midnight => "Midnight",
            Self::Mocha => "Mocha",
            Self::TokyoNight => "Tokyo Night",
            Self::Nord => "Nord",
            Self::Gruvbox => "Gruvbox",
            Self::RosePine => "Rosé Pine",
            Self::Dracula => "Dracula",
            Self::Daylight => "Daylight",
            Self::Custom => "Custom",
        })
    }
}

impl Theme {
    /// Palette of a built in theme. [`Theme::Custom`] has no palette of its own, it falls back
    /// to the default one as a starting point, see [`ThemeConfig::palette`].
    pub fn palette(&self) -> Palette {
        match self {
            Self::Midnight | Self::Custom => Palette {
                dark: true,
                backdrop: rgb(15, 15, 19),
                base: rgb(26, 26, 32),
                surface: rgb(46, 46, 58),
                overlay: rgb(72, 72, 90),
                subtext: rgb(150, 152, 165),
                text: rgb(246, 247, 251),
                accent: rgb(100, 150, 240),
                gradient: [rgb(18, 18, 26), rgb(42, 36, 64)],
            },
            Self::Mocha => Palette {
                dark: true,
                backdrop: rgb(17, 17, 27),
                base: rgb(24, 24, 37),
                surface: rgb(49, 50, 68),
                overlay: rgb(76, 78, 102),
                subtext: rgb(166, 173, 200),
                text: rgb(223, 230, 252),
                accent: rgb(203, 166, 247),
                gradient: [rgb(17, 17, 27), rgb(54, 42, 78)],
            },
            Self::TokyoNight => Palette {
                dark: true,
                backdrop: rgb(19, 20, 28),
                base: rgb(26, 27, 38),
                surface: rgb(41, 46, 66),
                overlay: rgb(67, 75, 108),
                subtext: rgb(154, 165, 208),
                text: rgb(208, 216, 252),
                accent: rgb(122, 162, 247),
                gradient: [rgb(19, 20, 28), rgb(36, 42, 82)],
            },
            Self::Nord => Palette {
                dark: true,
                backdrop: rgb(36, 41, 51),
                base: rgb(46, 52, 64),
                surface: rgb(67, 76, 94),
                overlay: rgb(90, 101, 124),
                subtext: rgb(192, 201, 216),
                text: rgb(240, 243, 249),
                accent: rgb(136, 192, 208),
                gradient: [rgb(36, 41, 51), rgb(62, 78, 102)],
            },
            Self::Gruvbox => Palette {
                dark: true,
                backdrop: rgb(22, 24, 25),
                base: rgb(33, 35, 35),
                surface: rgb(60, 56, 54),
                overlay: rgb(92, 84, 76),
                subtext: rgb(176, 161, 138),
                text: rgb(248, 234, 200),
                accent: rgb(250, 189, 47),
                gradient: [rgb(22, 24, 25), rgb(68, 54, 40)],
            },
            Self::RosePine => Palette {
                dark: true,
                backdrop: rgb(18, 16, 28),
                base: rgb(25, 23, 36),
                surface: rgb(40, 37, 60),
                overlay: rgb(70, 66, 92),
                subtext: rgb(152, 148, 180),
                text: rgb(233, 231, 250),
                accent: rgb(196, 167, 231),
                gradient: [rgb(18, 16, 28), rgb(62, 42, 70)],
            },
            Self::Dracula => Palette {
                dark: true,
                backdrop: rgb(25, 26, 34),
                base: rgb(33, 34, 44),
                surface: rgb(58, 61, 80),
                overlay: rgb(92, 96, 120),
                subtext: rgb(158, 162, 188),
                text: rgb(250, 250, 246),
                accent: rgb(189, 147, 249),
                gradient: [rgb(25, 26, 34), rgb(68, 50, 94)],
            },
            Self::Daylight => Palette {
                dark: false,
                backdrop: rgb(220, 224, 233),
                base: rgb(250, 251, 253),
                surface: rgb(230, 234, 242),
                overlay: rgb(202, 209, 223),
                subtext: rgb(94, 101, 119),
                text: rgb(16, 18, 26),
                accent: rgb(45, 98, 214),
                gradient: [rgb(238, 241, 248), rgb(206, 218, 242)],
            },
        }
    }
}

/// User editable palette behind [`Theme::Custom`]. Kept as plain colors so it round trips
/// through the config file unchanged.
#[derive(Clone, Serialize, Deserialize)]
#[serde(default)]
pub struct CustomPalette {
    pub dark: bool,
    pub backdrop: Color32,
    pub base: Color32,
    pub surface: Color32,
    pub overlay: Color32,
    pub subtext: Color32,
    pub text: Color32,
}

impl Default for CustomPalette {
    fn default() -> Self {
        Self::from_palette(&Theme::Midnight.palette())
    }
}

impl CustomPalette {
    pub fn from_palette(palette: &Palette) -> Self {
        Self {
            dark: palette.dark,
            backdrop: palette.backdrop,
            base: palette.base,
            surface: palette.surface,
            overlay: palette.overlay,
            subtext: palette.subtext,
            text: palette.text,
        }
    }

    fn to_palette(&self, accent: Color32, gradient: [Color32; 2]) -> Palette {
        Palette {
            dark: self.dark,
            backdrop: self.backdrop,
            base: self.base,
            surface: self.surface,
            overlay: self.overlay,
            subtext: self.subtext,
            text: self.text,
            accent,
            gradient,
        }
    }
}

#[derive(Debug, Clone, Copy, Default, PartialEq, EnumIter, Serialize, Deserialize)]
pub enum GradientDirection {
    #[default]
    Vertical,
    Horizontal,
    DiagonalDown,
    DiagonalUp,
}

impl Display for GradientDirection {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.write_str(match self {
            Self::Vertical => "Vertical",
            Self::Horizontal => "Horizontal",
            Self::DiagonalDown => "Diagonal Down",
            Self::DiagonalUp => "Diagonal Up",
        })
    }
}

impl GradientDirection {
    /// unit vector the gradient runs along, in screen space (y points down)
    fn axis(&self) -> Vec2 {
        const DIAGONAL: f32 = std::f32::consts::FRAC_1_SQRT_2;
        match self {
            Self::Vertical => Vec2::new(0.0, 1.0),
            Self::Horizontal => Vec2::new(1.0, 0.0),
            Self::DiagonalDown => Vec2::new(DIAGONAL, DIAGONAL),
            Self::DiagonalUp => Vec2::new(DIAGONAL, -DIAGONAL),
        }
    }
}

/// Type sizes in points, before [`ThemeConfig::text_scale`] is applied. The spread between
/// them is what gives the window a reading order instead of one flat wall of text.
const HEADING_SIZE: f32 = 21.0;
const BODY_SIZE: f32 = 15.0;
const BUTTON_SIZE: f32 = 15.0;
const SMALL_SIZE: f32 = 12.0;
const MONOSPACE_SIZE: f32 = 14.0;

#[derive(Clone, Serialize, Deserialize)]
#[serde(default)]
pub struct ThemeConfig {
    pub theme: Theme,
    pub custom: CustomPalette,
    pub gradient: bool,
    pub gradient_start: Color32,
    pub gradient_end: Color32,
    pub gradient_direction: GradientDirection,
    /// how opaque the panels sit on top of the gradient, 0..=1
    pub panel_opacity: f32,
    /// multiplier on every text size, 1.0 is the designed scale
    pub text_scale: f32,
    /// pulls secondary text towards the primary text color, 0..=1
    pub text_contrast: f32,
    pub corner_radius: u8,
    pub shadows: bool,
}

impl Default for ThemeConfig {
    fn default() -> Self {
        let palette = Theme::default().palette();
        Self {
            theme: Theme::default(),
            custom: CustomPalette::from_palette(&palette),
            gradient: true,
            gradient_start: palette.gradient[0],
            gradient_end: palette.gradient[1],
            gradient_direction: GradientDirection::DiagonalDown,
            panel_opacity: 0.72,
            text_scale: 1.0,
            text_contrast: 0.0,
            corner_radius: 6,
            shadows: true,
        }
    }
}

impl ThemeConfig {
    /// Resolved palette: the preset, or the user's own colors when [`Theme::Custom`] is picked.
    /// The accent stays owned by `Config::accent_color`, so it is not part of this.
    pub fn palette(&self) -> Palette {
        let preset = self.theme.palette();
        let gradient = [self.gradient_start, self.gradient_end];
        match self.theme {
            Theme::Custom => self.custom.to_palette(preset.accent, gradient),
            _ => Palette { gradient, ..preset },
        }
    }

    /// pull gradient stops from the current theme, used when the theme changes
    pub fn sync_gradient(&mut self) {
        let palette = self.theme.palette();
        self.gradient_start = palette.gradient[0];
        self.gradient_end = palette.gradient[1];
    }

    /// seed the custom palette from a built in theme, so editing starts somewhere sane
    pub fn seed_custom(&mut self, from: Theme) {
        self.custom = CustomPalette::from_palette(&from.palette());
    }

    /// panel background, translucent while a gradient is showing through
    pub fn panel_fill(&self, base: Color32) -> Color32 {
        if !self.gradient {
            return base;
        }
        let alpha = (self.panel_opacity.clamp(0.0, 1.0) * 255.0).round() as u8;
        Color32::from_rgba_unmultiplied(base.r(), base.g(), base.b(), alpha)
    }

    /// secondary text color, pulled towards the primary one by `text_contrast`
    fn secondary_text(&self, palette: &Palette) -> Color32 {
        lerp_color(
            palette.subtext,
            palette.text,
            self.text_contrast.clamp(0.0, 1.0),
        )
    }
}

pub fn lerp_color(from: Color32, to: Color32, t: f32) -> Color32 {
    let t = t.clamp(0.0, 1.0);
    let lerp = |a: u8, b: u8| (a as f32 + (b as f32 - a as f32) * t).round() as u8;
    Color32::from_rgb(
        lerp(from.r(), to.r()),
        lerp(from.g(), to.g()),
        lerp(from.b(), to.b()),
    )
}

/// Color for section headers: the primary text color tinted towards the accent, so headers
/// read as their own level instead of blending into the body text.
pub fn heading_color(style: &Style) -> Color32 {
    let text = style.visuals.widgets.noninteractive.fg_stroke.color;
    let accent = style.visuals.selection.bg_fill;
    lerp_color(text, accent, 0.55)
}

/// Font size for section headers, one step above body text.
pub fn heading_size(style: &Style) -> f32 {
    style
        .text_styles
        .get(&TextStyle::Body)
        .map_or(BODY_SIZE, |font| font.size)
        + 2.0
}

/// Black or white, whichever stays readable on top of `color`. Used for text drawn directly
/// on the accent fill, where a bright accent would otherwise get bright text on it.
fn contrasting_text(color: Color32, palette: &Palette) -> Color32 {
    let luminance =
        0.2126 * color.r() as f32 + 0.7152 * color.g() as f32 + 0.0722 * color.b() as f32;
    if luminance > 140.0 {
        palette.backdrop
    } else {
        palette.text
    }
}

/// Paints the window background gradient as a single quad. The corner colors are the
/// gradient sampled at each corner's projection onto the gradient axis, so the linear
/// interpolation the gpu does between them reproduces the gradient exactly.
pub fn paint_gradient(painter: &Painter, rect: Rect, config: &ThemeConfig) {
    if !config.gradient || rect.width() <= 0.0 || rect.height() <= 0.0 {
        return;
    }

    let axis = config.gradient_direction.axis();
    let corners = [
        rect.left_top(),
        rect.right_top(),
        rect.right_bottom(),
        rect.left_bottom(),
    ];
    let projections = corners.map(|corner: Pos2| corner.to_vec2().dot(axis));
    let min = projections.iter().copied().fold(f32::INFINITY, f32::min);
    let max = projections
        .iter()
        .copied()
        .fold(f32::NEG_INFINITY, f32::max);
    let span = max - min;

    let mut mesh = Mesh::default();
    for (corner, projection) in corners.iter().zip(projections) {
        let t = if span > f32::EPSILON {
            (projection - min) / span
        } else {
            0.0
        };
        mesh.colored_vertex(
            *corner,
            lerp_color(config.gradient_start, config.gradient_end, t),
        );
    }
    mesh.add_triangle(0, 1, 2);
    mesh.add_triangle(0, 2, 3);

    painter.add(mesh);
}

/// Applies a theme to an egui style. Replaces the previous hardcoded dark palette.
pub fn apply(style: &mut Style, config: &ThemeConfig, accent: Color32) {
    let palette = config.palette();
    let radius = CornerRadius::same(config.corner_radius);
    let secondary = config.secondary_text(&palette);

    style.interaction.selectable_labels = false;

    let scale = config.text_scale.clamp(0.7, 2.0);
    for (text_style, font) in style.text_styles.iter_mut() {
        font.size = scale
            * match text_style {
                TextStyle::Heading => HEADING_SIZE,
                TextStyle::Body => BODY_SIZE,
                TextStyle::Button => BUTTON_SIZE,
                TextStyle::Small => SMALL_SIZE,
                TextStyle::Monospace => MONOSPACE_SIZE,
                TextStyle::Name(_) => BODY_SIZE,
            };
    }

    style.spacing.item_spacing = Vec2::new(8.0, 6.0);
    style.spacing.button_padding = Vec2::new(10.0, 4.0);
    style.spacing.menu_margin = egui::Margin::same(6);
    style.spacing.indent = 18.0;

    style.visuals.dark_mode = palette.dark;
    // panels let the gradient through, floating windows and dropdowns stay opaque so
    // their text keeps its contrast
    style.visuals.window_fill = palette.base;
    style.visuals.panel_fill = config.panel_fill(palette.base);
    style.visuals.extreme_bg_color = palette.backdrop;
    style.visuals.faint_bg_color = palette.surface;
    style.visuals.code_bg_color = palette.backdrop;
    style.visuals.hyperlink_color = accent;
    style.visuals.weak_text_color = Some(secondary);
    style.visuals.window_corner_radius = radius;
    style.visuals.menu_corner_radius = radius;
    style.visuals.window_stroke = Stroke::new(1.0, palette.overlay);

    style.visuals.window_shadow = if config.shadows {
        Shadow {
            offset: [0, 6],
            blur: 18,
            spread: 0,
            color: Color32::from_black_alpha(96),
        }
    } else {
        Shadow::NONE
    };
    style.visuals.popup_shadow = style.visuals.window_shadow;

    let text_stroke = Stroke::new(1.0, palette.text);
    let secondary_stroke = Stroke::new(1.0, secondary);
    let border_stroke = Stroke::new(1.0, palette.overlay);

    style.visuals.selection.bg_fill = accent;
    style.visuals.selection.stroke = Stroke::new(1.0, contrasting_text(accent, &palette));

    let widgets = &mut style.visuals.widgets;

    // labels, separators and other non widget text: the brightest text in the window
    widgets.noninteractive.bg_fill = palette.surface;
    widgets.noninteractive.weak_bg_fill = palette.surface;
    widgets.noninteractive.bg_stroke = border_stroke;
    widgets.noninteractive.fg_stroke = text_stroke;
    widgets.noninteractive.corner_radius = radius;

    // idle controls sit one step back from labels
    widgets.inactive.bg_fill = palette.surface;
    widgets.inactive.weak_bg_fill = palette.surface;
    widgets.inactive.bg_stroke = border_stroke;
    widgets.inactive.fg_stroke = secondary_stroke;
    widgets.inactive.corner_radius = radius;

    widgets.hovered.bg_fill = palette.overlay;
    widgets.hovered.weak_bg_fill = palette.overlay;
    widgets.hovered.bg_stroke = Stroke::new(1.0, lerp_color(palette.overlay, accent, 0.5));
    widgets.hovered.fg_stroke = text_stroke;
    widgets.hovered.corner_radius = radius;
    widgets.hovered.expansion = 1.0;

    widgets.active.bg_fill = lerp_color(palette.overlay, accent, 0.45);
    widgets.active.weak_bg_fill = lerp_color(palette.overlay, accent, 0.45);
    widgets.active.bg_stroke = Stroke::new(1.0, accent);
    widgets.active.fg_stroke = text_stroke;
    widgets.active.corner_radius = radius;
    widgets.active.expansion = 1.0;

    widgets.open.bg_fill = palette.overlay;
    widgets.open.weak_bg_fill = palette.overlay;
    widgets.open.bg_stroke = border_stroke;
    widgets.open.fg_stroke = text_stroke;
    widgets.open.corner_radius = radius;
}

/// Reapplies a theme to a live context, for when the user changes it at runtime.
pub fn apply_to_ctx(ctx: &egui::Context, config: &ThemeConfig, accent: Color32) {
    ctx.all_styles_mut(|style| apply(style, config, accent));
}
