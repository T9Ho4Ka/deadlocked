#pragma once

#include <array>
#include <cstddef>
#include <string_view>

#include "config/enums.hpp"
#include "ui/theme.hpp"

namespace dl::config {

using ui::Color;

/// where on the entity the text block is anchored
enum class TextPosition {
    TopLeft,
    TopCenter,
    TopRight,
    CenterLeft,
    Center,
    CenterRight,
    BottomLeft,
    BottomCenter,
    BottomRight,
};

inline constexpr std::array<EnumEntry<TextPosition>, 9> text_position_entries{{
    {TextPosition::TopLeft, "Top Left", "top_left"},
    {TextPosition::TopCenter, "Top Center", "top_center"},
    {TextPosition::TopRight, "Top Right", "top_right"},
    {TextPosition::CenterLeft, "Center Left", "center_left"},
    {TextPosition::Center, "Center", "center"},
    {TextPosition::CenterRight, "Center Right", "center_right"},
    {TextPosition::BottomLeft, "Bottom Left", "bottom_left"},
    {TextPosition::BottomCenter, "Bottom Center", "bottom_center"},
    {TextPosition::BottomRight, "Bottom Right", "bottom_right"},
}};
constexpr const auto& enum_entries(TextPosition) { return text_position_entries; }

/// how the text is aligned around that anchor
enum class TextAlign {
    LeftTop,
    LeftCenter,
    LeftBottom,
    CenterTop,
    CenterCenter,
    CenterBottom,
    RightTop,
    RightCenter,
    RightBottom,
};

inline constexpr std::array<EnumEntry<TextAlign>, 9> text_align_entries{{
    {TextAlign::LeftTop, "Left Top", "left_top"},
    {TextAlign::LeftCenter, "Left Center", "left_center"},
    {TextAlign::LeftBottom, "Left Bottom", "left_bottom"},
    {TextAlign::CenterTop, "Center Top", "center_top"},
    {TextAlign::CenterCenter, "Center Center", "center_center"},
    {TextAlign::CenterBottom, "Center Bottom", "center_bottom"},
    {TextAlign::RightTop, "Right Top", "right_top"},
    {TextAlign::RightCenter, "Right Center", "right_center"},
    {TextAlign::RightBottom, "Right Bottom", "right_bottom"},
}};
constexpr const auto& enum_entries(TextAlign) { return text_align_entries; }

struct TextCategory {
    float font_size = 16.0f;
    Color color = Color(255, 255, 255);
    bool use_player_color = false;
    TextPosition position = TextPosition::Center;
    TextAlign align = TextAlign::CenterCenter;
};

/// Every piece of text the overlay draws. Kept as one array rather than eleven named
/// members, so the settings popups and the serializer can both just loop.
enum class TextSlot : std::size_t {
    StatusText,
    PlayerName,
    PlayerTags,
    WeaponIcon,
    AmmoText,
    WeaponName,
    BombTimer,
    GrenadeName,
    GrenadeLineup,
    KeybindList,
    SpectatorList,
};

inline constexpr std::size_t text_slot_count = 11;

struct TextSlotInfo {
    TextSlot slot;
    std::string_view label;
    std::string_view key;
    /// only the per player texts can follow the player's own color
    bool player_color;
    TextCategory defaults;
};

inline constexpr std::array<TextSlotInfo, text_slot_count> text_slots{{
    {TextSlot::StatusText, "Status Text", "status_text", false,
     {16.0f, Color(255, 255, 255), false, TextPosition::Center, TextAlign::LeftTop}},
    {TextSlot::PlayerName, "Player Name", "player_name", true,
     {14.0f, Color(255, 255, 255), false, TextPosition::TopRight, TextAlign::LeftTop}},
    {TextSlot::PlayerTags, "Player Tags", "player_tags", true,
     {20.0f, Color(255, 255, 255), false, TextPosition::TopRight, TextAlign::LeftTop}},
    {TextSlot::WeaponIcon, "Weapon Icon", "weapon_icon", true,
     {20.0f, Color(255, 255, 255), false, TextPosition::BottomCenter, TextAlign::CenterTop}},
    {TextSlot::AmmoText, "Ammo", "ammo_text", true,
     {16.0f, Color(255, 255, 255), false, TextPosition::BottomCenter, TextAlign::CenterTop}},
    {TextSlot::WeaponName, "Weapon Name", "weapon_name", false,
     {16.0f, Color(255, 255, 255), false, TextPosition::Center, TextAlign::CenterCenter}},
    {TextSlot::BombTimer, "Bomb Timer", "bomb_timer", false,
     {24.0f, Color(255, 255, 255), false, TextPosition::Center, TextAlign::CenterCenter}},
    {TextSlot::GrenadeName, "Grenade Name", "grenade_name", false,
     {16.0f, Color(255, 255, 255), false, TextPosition::Center, TextAlign::CenterCenter}},
    {TextSlot::GrenadeLineup, "Grenade Lineup", "grenade_lineup", false,
     {14.0f, Color(255, 255, 255), false, TextPosition::Center, TextAlign::CenterTop}},
    {TextSlot::KeybindList, "Keybind List", "keybind_list", false,
     {16.0f, Color(255, 255, 255), false, TextPosition::CenterLeft, TextAlign::LeftTop}},
    {TextSlot::SpectatorList, "Spectator List", "spectator_list", false,
     {16.0f, Color(255, 255, 255), false, TextPosition::CenterLeft, TextAlign::LeftTop}},
}};

struct OverlayTextConfig {
    std::array<TextCategory, text_slot_count> categories;

    OverlayTextConfig() {
        for (std::size_t i = 0; i < text_slot_count; ++i) {
            categories[i] = text_slots[i].defaults;
        }
    }

    TextCategory& operator[](TextSlot slot) {
        return categories[static_cast<std::size_t>(slot)];
    }
    const TextCategory& operator[](TextSlot slot) const {
        return categories[static_cast<std::size_t>(slot)];
    }
};

}  // namespace dl::config
