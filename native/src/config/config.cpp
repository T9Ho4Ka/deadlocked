#include "config/config.hpp"

#include <algorithm>
#include <ranges>
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

toml::table weapon_config_to_toml(const WeaponConfig& weapon) {
    const AimbotConfig& aimbot = weapon.aimbot;
    toml::array bones;
    for (const Bones bone : aimbot.bones) {
        bones.push_back(std::string(enum_key(bone)));
    }

    return toml::table{
        {"aimbot", toml::table{
                       {"enable_override", aimbot.enable_override},
                       {"enabled", aimbot.enabled},
                       {"mode", std::string(enum_key(aimbot.mode))},
                       {"target_friendlies", aimbot.target_friendlies},
                       {"distance_adjusted_fov", aimbot.distance_adjusted_fov},
                       {"start_bullet", static_cast<std::int64_t>(aimbot.start_bullet)},
                       {"visibility_check", aimbot.visibility_check},
                       {"flash_check", aimbot.flash_check},
                       {"fov", aimbot.fov},
                       {"smooth", aimbot.smooth},
                       {"inertia", aimbot.inertia},
                       {"prediction_time", aimbot.prediction_time},
                       {"bones", std::move(bones)},
                       {"targeting_mode", std::string(enum_key(aimbot.targeting_mode))},
                   }},
        {"rcs", toml::table{
                    {"enable_override", weapon.rcs.enable_override},
                    {"enabled", weapon.rcs.enabled},
                    {"strength", toml::array{weapon.rcs.strength.x, weapon.rcs.strength.y}},
                }},
        {"triggerbot",
         toml::table{
             {"enable_override", weapon.triggerbot.enable_override},
             {"enabled", weapon.triggerbot.enabled},
             {"delay", toml::array{static_cast<std::int64_t>(weapon.triggerbot.delay.start),
                                   static_cast<std::int64_t>(weapon.triggerbot.delay.end)}},
             {"shot_duration", static_cast<std::int64_t>(weapon.triggerbot.shot_duration)},
             {"mode", std::string(enum_key(weapon.triggerbot.mode))},
             {"flash_check", weapon.triggerbot.flash_check},
             {"scope_check", weapon.triggerbot.scope_check},
             {"velocity_check", weapon.triggerbot.velocity_check},
             {"velocity_threshold", weapon.triggerbot.velocity_threshold},
             {"head_only", weapon.triggerbot.head_only},
         }},
    };
}

void weapon_config_from_toml(const toml::node_view<const toml::node>& node,
                             WeaponConfig& weapon) {
    const toml::node_view<const toml::node> aimbot_node = node["aimbot"];
    AimbotConfig& aimbot = weapon.aimbot;
    aimbot.enable_override = aimbot_node["enable_override"].value_or(aimbot.enable_override);
    aimbot.enabled = aimbot_node["enabled"].value_or(aimbot.enabled);
    aimbot.mode = enum_from_key(aimbot_node["mode"].value_or(std::string_view{}), aimbot.mode);
    aimbot.target_friendlies =
        aimbot_node["target_friendlies"].value_or(aimbot.target_friendlies);
    aimbot.distance_adjusted_fov =
        aimbot_node["distance_adjusted_fov"].value_or(aimbot.distance_adjusted_fov);
    aimbot.start_bullet = aimbot_node["start_bullet"].value_or(aimbot.start_bullet);
    aimbot.visibility_check = aimbot_node["visibility_check"].value_or(aimbot.visibility_check);
    aimbot.flash_check = aimbot_node["flash_check"].value_or(aimbot.flash_check);
    aimbot.fov = aimbot_node["fov"].value_or(aimbot.fov);
    aimbot.smooth = aimbot_node["smooth"].value_or(aimbot.smooth);
    aimbot.inertia = aimbot_node["inertia"].value_or(aimbot.inertia);
    aimbot.prediction_time = aimbot_node["prediction_time"].value_or(aimbot.prediction_time);
    aimbot.targeting_mode = enum_from_key(
        aimbot_node["targeting_mode"].value_or(std::string_view{}), aimbot.targeting_mode);

    if (const toml::array* bones = aimbot_node["bones"].as_array(); bones != nullptr) {
        // an explicitly empty list means the user aims at nothing, so it is honoured as is
        aimbot.bones.clear();
        for (const toml::node& entry : *bones) {
            const std::optional<std::string_view> key = entry.value<std::string_view>();
            if (!key.has_value()) {
                continue;
            }
            const Bones bone = enum_from_key(*key, Bones::Head);
            if (enum_key(bone) == *key) {
                aimbot.bones.push_back(bone);
            }
        }
    }

    const toml::node_view<const toml::node> rcs_node = node["rcs"];
    weapon.rcs.enable_override =
        rcs_node["enable_override"].value_or(weapon.rcs.enable_override);
    weapon.rcs.enabled = rcs_node["enabled"].value_or(weapon.rcs.enabled);
    if (const toml::array* strength = rcs_node["strength"].as_array();
        strength != nullptr && strength->size() >= 2) {
        weapon.rcs.strength.x = strength->get(0)->value_or(weapon.rcs.strength.x);
        weapon.rcs.strength.y = strength->get(1)->value_or(weapon.rcs.strength.y);
    }

    const toml::node_view<const toml::node> trigger_node = node["triggerbot"];
    TriggerbotConfig& trigger = weapon.triggerbot;
    trigger.enable_override =
        trigger_node["enable_override"].value_or(trigger.enable_override);
    trigger.enabled = trigger_node["enabled"].value_or(trigger.enabled);
    if (const toml::array* delay = trigger_node["delay"].as_array();
        delay != nullptr && delay->size() >= 2) {
        trigger.delay.start = delay->get(0)->value_or(trigger.delay.start);
        trigger.delay.end = delay->get(1)->value_or(trigger.delay.end);
    }
    trigger.shot_duration = trigger_node["shot_duration"].value_or(trigger.shot_duration);
    trigger.mode = enum_from_key(trigger_node["mode"].value_or(std::string_view{}), trigger.mode);
    trigger.flash_check = trigger_node["flash_check"].value_or(trigger.flash_check);
    trigger.scope_check = trigger_node["scope_check"].value_or(trigger.scope_check);
    trigger.velocity_check = trigger_node["velocity_check"].value_or(trigger.velocity_check);
    trigger.velocity_threshold =
        trigger_node["velocity_threshold"].value_or(trigger.velocity_threshold);
    trigger.head_only = trigger_node["head_only"].value_or(trigger.head_only);
}

toml::table text_category_to_toml(const TextCategory& category) {
    return toml::table{
        {"font_size", category.font_size},
        {"color", to_toml(category.color)},
        {"use_player_color", category.use_player_color},
        {"position", std::string(enum_key(category.position))},
        {"align", std::string(enum_key(category.align))},
    };
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

std::filesystem::path config_dir() { return base_path() / "native"; }

namespace {

/// which profile is selected, remembered in a file of its own so it survives a restart and
/// is not itself part of any profile
std::filesystem::path selected_marker() { return base_path() / "native-current"; }

std::string read_selected() {
    std::ifstream in(selected_marker());
    std::string name;
    if (in) {
        std::getline(in, name);
    }
    // a name with a separator in it would escape the config directory
    if (name.empty() || name.find('/') != std::string::npos || name == "." || name == "..") {
        return "deadlocked.toml";
    }
    return name;
}

}  // namespace

/// Earlier builds kept a single native.toml next to the directory profiles now live in.
/// It is moved in as the default profile the first time, rather than being left behind.
void migrate_single_config() {
    const std::filesystem::path legacy = base_path() / "native.toml";
    const std::filesystem::path destination = config_dir() / "deadlocked.toml";
    std::error_code error;
    if (!std::filesystem::exists(legacy, error) || std::filesystem::exists(destination, error)) {
        return;
    }
    std::filesystem::rename(legacy, destination, error);
    if (!error) {
        std::fprintf(stderr, "moved the old native.toml in as %s\n",
                     destination.filename().c_str());
    }
}

std::vector<std::filesystem::path> available_configs() {
    std::vector<std::filesystem::path> found;
    std::error_code error;
    std::filesystem::create_directories(config_dir(), error);
    migrate_single_config();

    for (const auto& entry : std::filesystem::directory_iterator(config_dir(), error)) {
        if (entry.is_regular_file(error) && entry.path().extension() == ".toml") {
            found.push_back(entry.path());
        }
    }
    std::ranges::sort(found);

    if (found.empty()) {
        // there is always at least one profile to be editing
        const std::filesystem::path fallback = config_dir() / "deadlocked.toml";
        save_to(Config{}, fallback);
        found.push_back(fallback);
    }
    return found;
}

std::filesystem::path config_path() { return config_dir() / read_selected(); }

void select_config(const std::filesystem::path& path) {
    std::error_code error;
    std::filesystem::create_directories(base_path(), error);
    std::ofstream out(selected_marker(), std::ios::trunc);
    if (out) {
        out << path.filename().string() << "\n";
    }
}

bool delete_config(const std::filesystem::path& path) {
    if (available_configs().size() <= 1) {
        return false;
    }
    std::error_code error;
    const bool removed = std::filesystem::remove(path, error);
    if (removed && path == config_path()) {
        select_config(available_configs().front());
    }
    return removed;
}

Config load() { return load_from(config_path()); }

Config load_from(const std::filesystem::path& path) {
    Config config;

    // a missing or broken file simply leaves every field at its default
    const toml::table table = [&path] {
        try {
            return toml::parse_file(path.string());
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
    target.shadows = theme["shadows"].value_or(target.shadows);

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

    const toml::node_view<const toml::node> aim = table["aim"];
    config.aim.aimbot_hotkey = read_enum(aim["aimbot_hotkey"], config.aim.aimbot_hotkey);
    config.aim.triggerbot_hotkey =
        read_enum(aim["triggerbot_hotkey"], config.aim.triggerbot_hotkey);
    weapon_config_from_toml(aim["global"], config.aim.global);

    const toml::node_view<const toml::node> weapons = aim["weapons"];
    for (std::size_t i = 0; i < weapon_count; ++i) {
        // a weapon left at its defaults is not written out, so a missing entry is not an error
        weapon_config_from_toml(weapons[weapon_entries[i].key], config.aim.weapons[i]);
    }

    const toml::node_view<const toml::node> hud = table["hud"];
    HudConfig& h = config.hud;
    h.bomb_timer = hud["bomb_timer"].value_or(h.bomb_timer);
    h.fov_circle = hud["fov_circle"].value_or(h.fov_circle);
    h.dropped_weapons = hud["dropped_weapons"].value_or(h.dropped_weapons);
    h.keybind_list = hud["keybind_list"].value_or(h.keybind_list);
    h.spectator_list = hud["spectator_list"].value_or(h.spectator_list);
    h.text_outline = hud["text_outline"].value_or(h.text_outline);
    h.line_width = hud["line_width"].value_or(h.line_width);
    h.debug = hud["debug"].value_or(h.debug);

    const toml::node_view<const toml::node> crosshair = hud["sniper_crosshair"];
    CrosshairConfig& c = h.sniper_crosshair;
    c.enabled = crosshair["enabled"].value_or(c.enabled);
    c.color = color_from(crosshair["color"], c.color);
    c.line_length = crosshair["line_length"].value_or(c.line_length);
    c.line_width = crosshair["line_width"].value_or(c.line_width);
    c.gap = crosshair["gap"].value_or(c.gap);

    const toml::node_view<const toml::node> trails = hud["grenade_trails"];
    TrailConfig& t = h.grenade_trails;
    t.enabled = trails["enabled"].value_or(t.enabled);
    t.inferno_poly = trails["inferno_poly"].value_or(t.inferno_poly);
    t.smoke = color_from(trails["smoke"], t.smoke);
    t.molotov = color_from(trails["molotov"], t.molotov);
    t.incendiary = color_from(trails["incendiary"], t.incendiary);
    t.flash = color_from(trails["flash"], t.flash);
    t.he = color_from(trails["he"], t.he);
    t.decoy = color_from(trails["decoy"], t.decoy);

    const toml::node_view<const toml::node> text = hud["overlay_text"];
    for (std::size_t i = 0; i < text_slot_count; ++i) {
        TextCategory& category = h.overlay_text.categories[i];
        const toml::node_view<const toml::node> entry = text[text_slots[i].key];
        category.font_size = entry["font_size"].value_or(category.font_size);
        category.color = color_from(entry["color"], category.color);
        category.use_player_color =
            entry["use_player_color"].value_or(category.use_player_color);
        category.position = read_enum(entry["position"], category.position);
        category.align = read_enum(entry["align"], category.align);
    }

    config.font = read_enum(table["font"], config.font);
    config.fps = table["fps"].value_or(config.fps);

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

bool save(const Config& config) { return save_to(config, config_path()); }

bool save_to(const Config& config, const std::filesystem::path& path) {
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
        {"shadows", theme.shadows},
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

    // only weapons that actually differ from the defaults are written, otherwise every
    // config file would carry 67 near identical blocks
    toml::table weapons_table;
    const WeaponConfig default_weapon;
    for (std::size_t i = 0; i < weapon_count; ++i) {
        if (config.aim.weapons[i] == default_weapon) {
            continue;
        }
        weapons_table.insert(weapon_entries[i].key,
                             weapon_config_to_toml(config.aim.weapons[i]));
    }

    toml::table aim_table{
        {"aimbot_hotkey", write_enum(config.aim.aimbot_hotkey)},
        {"triggerbot_hotkey", write_enum(config.aim.triggerbot_hotkey)},
        {"global", weapon_config_to_toml(config.aim.global)},
        {"weapons", std::move(weapons_table)},
    };

    const HudConfig& h = config.hud;
    toml::table text_table;
    for (std::size_t i = 0; i < text_slot_count; ++i) {
        text_table.insert(text_slots[i].key,
                          text_category_to_toml(h.overlay_text.categories[i]));
    }

    toml::table hud_table{
        {"bomb_timer", h.bomb_timer},
        {"fov_circle", h.fov_circle},
        {"dropped_weapons", h.dropped_weapons},
        {"keybind_list", h.keybind_list},
        {"spectator_list", h.spectator_list},
        {"text_outline", h.text_outline},
        {"line_width", h.line_width},
        {"debug", h.debug},
        {"sniper_crosshair", toml::table{
                                 {"enabled", h.sniper_crosshair.enabled},
                                 {"color", to_toml(h.sniper_crosshair.color)},
                                 {"line_length", h.sniper_crosshair.line_length},
                                 {"line_width", h.sniper_crosshair.line_width},
                                 {"gap", h.sniper_crosshair.gap},
                             }},
        {"grenade_trails", toml::table{
                               {"enabled", h.grenade_trails.enabled},
                               {"inferno_poly", h.grenade_trails.inferno_poly},
                               {"smoke", to_toml(h.grenade_trails.smoke)},
                               {"molotov", to_toml(h.grenade_trails.molotov)},
                               {"incendiary", to_toml(h.grenade_trails.incendiary)},
                               {"flash", to_toml(h.grenade_trails.flash)},
                               {"he", to_toml(h.grenade_trails.he)},
                               {"decoy", to_toml(h.grenade_trails.decoy)},
                           }},
        {"overlay_text", std::move(text_table)},
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
        {"font", write_enum(config.font)},
        {"fps", static_cast<std::int64_t>(config.fps)},
        {"aim", std::move(aim_table)},
        {"player", std::move(player_table)},
        {"hud", std::move(hud_table)},
        {"misc", std::move(misc_table)},
        {"radar", std::move(radar_table)},
    };

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
