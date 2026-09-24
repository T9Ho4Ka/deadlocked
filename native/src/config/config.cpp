#include "config/config.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <fstream>
#include <optional>
#include <system_error>

#include <toml++/toml.hpp>

namespace dl::config {
namespace {

using ui::CustomPalette;
using ui::GradientDirection;
using ui::Theme;
using ui::ThemeConfig;

std::filesystem::path base_path() {
    if (const char* xdg = std::getenv("XDG_CONFIG_HOME"); xdg != nullptr && *xdg != '\0') {
        return std::filesystem::path(xdg) / "deadlocked";
    }
    if (const char* home = std::getenv("HOME"); home != nullptr && *home != '\0') {
        return std::filesystem::path(home) / ".config" / "deadlocked";
    }
    return std::filesystem::current_path();
}

toml::array to_toml(Color color) {
    return toml::array{color.r, color.g, color.b, color.a};
}

Color color_from(const toml::node_view<const toml::node>& node, Color fallback) {
    const toml::array* values = node.as_array();
    if (values == nullptr || values->size() < 3) {
        return fallback;
    }
    const auto channel = [values](std::size_t index, std::uint8_t fallback_value) {
        const std::optional<std::int64_t> value = values->get(index)->value<std::int64_t>();
        if (!value.has_value()) {
            return fallback_value;
        }
        return static_cast<std::uint8_t>(std::clamp<std::int64_t>(*value, 0, 255));
    };
    return Color(channel(0, fallback.r), channel(1, fallback.g), channel(2, fallback.b),
                 values->size() > 3 ? channel(3, fallback.a) : fallback.a);
}

/// enums round trip as their names, so the file stays readable and reordering the enum
/// later cannot silently change what a saved config means
std::string_view theme_key(Theme theme) {
    switch (theme) {
        case Theme::Midnight: return "midnight";
        case Theme::Mocha: return "mocha";
        case Theme::TokyoNight: return "tokyo_night";
        case Theme::Nord: return "nord";
        case Theme::Gruvbox: return "gruvbox";
        case Theme::RosePine: return "rose_pine";
        case Theme::Dracula: return "dracula";
        case Theme::Daylight: return "daylight";
        case Theme::Custom: return "custom";
    }
    return "midnight";
}

Theme theme_from(std::string_view key, Theme fallback) {
    for (int i = 0; i < ui::theme_count; ++i) {
        const auto theme = static_cast<Theme>(i);
        if (theme_key(theme) == key) {
            return theme;
        }
    }
    return fallback;
}

std::string_view direction_key(GradientDirection direction) {
    switch (direction) {
        case GradientDirection::Vertical: return "vertical";
        case GradientDirection::Horizontal: return "horizontal";
        case GradientDirection::DiagonalDown: return "diagonal_down";
        case GradientDirection::DiagonalUp: return "diagonal_up";
    }
    return "vertical";
}

GradientDirection direction_from(std::string_view key, GradientDirection fallback) {
    for (int i = 0; i < ui::gradient_direction_count; ++i) {
        const auto direction = static_cast<GradientDirection>(i);
        if (direction_key(direction) == key) {
            return direction;
        }
    }
    return fallback;
}

template <typename E>
E read_enum(const toml::node_view<const toml::node>& node, E fallback) {
    return enum_from_key(node.value_or(std::string_view{}), fallback);
}

template <typename E>
std::string write_enum(E value) {
    return std::string(enum_key(value));
}

}  // namespace

std::string new_uuid() {
    std::random_device device;
    std::uniform_int_distribution<unsigned> nibble(0, 15);
    // 8-4-4-4-12 with the version and variant nibbles pinned, as uuid v4 requires
    static constexpr char layout[] = "xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx";
    std::string out;
    out.reserve(sizeof(layout) - 1);
    for (const char slot : std::string_view(layout)) {
        switch (slot) {
            case 'x': out.push_back("0123456789abcdef"[nibble(device)]); break;
            case 'y': out.push_back("89ab"[nibble(device) % 4]); break;
            default: out.push_back(slot); break;
        }
    }
    return out;
}

std::filesystem::path config_path() { return base_path() / "native.toml"; }

Config load() {
    Config config;

    // a missing or broken file simply leaves every field at its default
    const toml::table table = [] {
        try {
            return toml::parse_file(config_path().string());
        } catch (const std::exception&) {
            return toml::table{};
        }
    }();

    config.accent_color = color_from(table["accent_color"], config.accent_color);

    const toml::node_view<const toml::node> theme = table["theme"];
    ThemeConfig& target = config.theme;
    target.theme = theme_from(theme["theme"].value_or(std::string_view{}), target.theme);
    target.gradient = theme["gradient"].value_or(target.gradient);
    target.gradient_start = color_from(theme["gradient_start"], target.gradient_start);
    target.gradient_end = color_from(theme["gradient_end"], target.gradient_end);
    target.gradient_direction =
        direction_from(theme["gradient_direction"].value_or(std::string_view{}),
                       target.gradient_direction);
    target.panel_opacity = theme["panel_opacity"].value_or(target.panel_opacity);
    target.text_scale = theme["text_scale"].value_or(target.text_scale);
    target.text_contrast = theme["text_contrast"].value_or(target.text_contrast);
    target.corner_radius = theme["corner_radius"].value_or(target.corner_radius);

    const toml::node_view<const toml::node> custom = theme["custom"];
    CustomPalette& palette = target.custom;
    palette.dark = custom["dark"].value_or(palette.dark);
    palette.backdrop = color_from(custom["backdrop"], palette.backdrop);
    palette.base = color_from(custom["base"], palette.base);
    palette.surface = color_from(custom["surface"], palette.surface);
    palette.overlay = color_from(custom["overlay"], palette.overlay);
    palette.subtext = color_from(custom["subtext"], palette.subtext);
    palette.text = color_from(custom["text"], palette.text);

    const toml::node_view<const toml::node> player = table["player"];
    PlayerConfig& p = config.player;
    p.enabled = player["enabled"].value_or(p.enabled);
    p.chicken = player["chicken"].value_or(p.chicken);
    p.esp_hotkey = read_enum(player["esp_hotkey"], p.esp_hotkey);
    p.show_friendlies = player["show_friendlies"].value_or(p.show_friendlies);
    p.draw_box = read_enum(player["draw_box"], p.draw_box);
    p.box_mode = read_enum(player["box_mode"], p.box_mode);
    p.box_visible_color = color_from(player["box_visible_color"], p.box_visible_color);
    p.box_invisible_color = color_from(player["box_invisible_color"], p.box_invisible_color);
    p.snaplines = read_enum(player["snaplines"], p.snaplines);
    p.snapline_color = color_from(player["snapline_color"], p.snapline_color);
    p.snapline_anchor = read_enum(player["snapline_anchor"], p.snapline_anchor);
    p.draw_skeleton = read_enum(player["draw_skeleton"], p.draw_skeleton);
    p.skeleton_color = color_from(player["skeleton_color"], p.skeleton_color);
    p.draw_model = read_enum(player["draw_model"], p.draw_model);
    p.model_mode = read_enum(player["model_mode"], p.model_mode);
    p.model_visible_color = color_from(player["model_visible_color"], p.model_visible_color);
    p.model_invisible_color =
        color_from(player["model_invisible_color"], p.model_invisible_color);
    p.head_circle = player["head_circle"].value_or(p.head_circle);
    p.health_bar = player["health_bar"].value_or(p.health_bar);
    p.armor_bar = player["armor_bar"].value_or(p.armor_bar);
    p.player_name = player["player_name"].value_or(p.player_name);
    p.weapon_icon = player["weapon_icon"].value_or(p.weapon_icon);
    p.tags = player["tags"].value_or(p.tags);
    p.visibility = read_enum(player["visibility"], p.visibility);

    const toml::node_view<const toml::node> sound = player["sound"];
    SoundConfig& s = p.sound;
    s.enabled = sound["enabled"].value_or(s.enabled);
    s.footstep_diameter = sound["footstep_diameter"].value_or(s.footstep_diameter);
    s.gunshot_diameter = sound["gunshot_diameter"].value_or(s.gunshot_diameter);
    s.weapon_diameter = sound["weapon_diameter"].value_or(s.weapon_diameter);
    s.fadeout_start = sound["fadeout_start"].value_or(s.fadeout_start);
    s.fadeout_duration = sound["fadeout_duration"].value_or(s.fadeout_duration);
    s.show_visible = sound["show_visible"].value_or(s.show_visible);

    const toml::node_view<const toml::node> misc = table["misc"];
    UnsafeConfig& m = config.misc;
    m.no_flash = misc["no_flash"].value_or(m.no_flash);
    m.max_flash_alpha = misc["max_flash_alpha"].value_or(m.max_flash_alpha);
    m.fov_changer = misc["fov_changer"].value_or(m.fov_changer);
    m.desired_fov = misc["desired_fov"].value_or(m.desired_fov);
    m.no_smoke = misc["no_smoke"].value_or(m.no_smoke);
    m.change_smoke_color = misc["change_smoke_color"].value_or(m.change_smoke_color);
    m.smoke_color = color_from(misc["smoke_color"], m.smoke_color);

    const toml::node_view<const toml::node> radar = table["radar"];
    config.radar.enabled = radar["enabled"].value_or(config.radar.enabled);
    config.radar.url = radar["url"].value_or(config.radar.url);
    config.radar_uuid = table["radar_uuid"].value_or(std::string{});
    if (config.radar_uuid.empty()) {
        config.radar_uuid = new_uuid();
    }

    return config;
}

bool save(const Config& config) {
    const ui::ThemeConfig& theme = config.theme;
    const CustomPalette& palette = theme.custom;

    toml::table custom{
        {"dark", palette.dark},
        {"backdrop", to_toml(palette.backdrop)},
        {"base", to_toml(palette.base)},
        {"surface", to_toml(palette.surface)},
        {"overlay", to_toml(palette.overlay)},
        {"subtext", to_toml(palette.subtext)},
        {"text", to_toml(palette.text)},
    };

    toml::table theme_table{
        {"theme", std::string(theme_key(theme.theme))},
        {"gradient", theme.gradient},
        {"gradient_start", to_toml(theme.gradient_start)},
        {"gradient_end", to_toml(theme.gradient_end)},
        {"gradient_direction", std::string(direction_key(theme.gradient_direction))},
        {"panel_opacity", theme.panel_opacity},
        {"text_scale", theme.text_scale},
        {"text_contrast", theme.text_contrast},
        {"corner_radius", theme.corner_radius},
        {"custom", std::move(custom)},
    };

    const PlayerConfig& p = config.player;
    const SoundConfig& s = p.sound;
    toml::table player_table{
        {"enabled", p.enabled},
        {"chicken", p.chicken},
        {"esp_hotkey", write_enum(p.esp_hotkey)},
        {"show_friendlies", p.show_friendlies},
        {"draw_box", write_enum(p.draw_box)},
        {"box_mode", write_enum(p.box_mode)},
        {"box_visible_color", to_toml(p.box_visible_color)},
        {"box_invisible_color", to_toml(p.box_invisible_color)},
        {"snaplines", write_enum(p.snaplines)},
        {"snapline_color", to_toml(p.snapline_color)},
        {"snapline_anchor", write_enum(p.snapline_anchor)},
        {"draw_skeleton", write_enum(p.draw_skeleton)},
        {"skeleton_color", to_toml(p.skeleton_color)},
        {"draw_model", write_enum(p.draw_model)},
        {"model_mode", write_enum(p.model_mode)},
        {"model_visible_color", to_toml(p.model_visible_color)},
        {"model_invisible_color", to_toml(p.model_invisible_color)},
        {"head_circle", p.head_circle},
        {"health_bar", p.health_bar},
        {"armor_bar", p.armor_bar},
        {"player_name", p.player_name},
        {"weapon_icon", p.weapon_icon},
        {"tags", p.tags},
        {"visibility", write_enum(p.visibility)},
        {"sound", toml::table{
                      {"enabled", s.enabled},
                      {"footstep_diameter", s.footstep_diameter},
                      {"gunshot_diameter", s.gunshot_diameter},
                      {"weapon_diameter", s.weapon_diameter},
                      {"fadeout_start", s.fadeout_start},
                      {"fadeout_duration", s.fadeout_duration},
                      {"show_visible", s.show_visible},
                  }},
    };

    const UnsafeConfig& m = config.misc;
    toml::table misc_table{
        {"no_flash", m.no_flash},
        {"max_flash_alpha", m.max_flash_alpha},
        {"fov_changer", m.fov_changer},
        {"desired_fov", static_cast<std::int64_t>(m.desired_fov)},
        {"no_smoke", m.no_smoke},
        {"change_smoke_color", m.change_smoke_color},
        {"smoke_color", to_toml(m.smoke_color)},
    };

    toml::table radar_table{
        {"enabled", config.radar.enabled},
        {"url", config.radar.url},
    };

    toml::table root{
        {"accent_color", to_toml(config.accent_color)},
        {"radar_uuid", config.radar_uuid},
        {"theme", std::move(theme_table)},
        {"player", std::move(player_table)},
        {"misc", std::move(misc_table)},
        {"radar", std::move(radar_table)},
    };

    const std::filesystem::path path = config_path();
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    if (error) {
        return false;
    }

    std::ofstream out(path, std::ios::trunc);
    if (!out) {
        return false;
    }
    out << root;
    return out.good();
}

}  // namespace dl::config
