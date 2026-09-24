#pragma once

#include <array>
#include <cstddef>
#include <string_view>

#include "config/enums.hpp"

namespace dl::config {

/// Ported from cheat/src/font.rs. The rust client embeds these, the native one reads them
/// from the shared asset directory until it grows its own embedding step.
enum class Font : std::size_t {
    DMSans,
    FiraSans,
    Inter,
    Nunito,
    Ubuntu,
    JetBrainsMono,
};

inline constexpr std::size_t font_count = 6;

inline constexpr std::array<EnumEntry<Font>, font_count> font_entries{{
    {Font::DMSans, "DM Sans", "dm_sans"},
    {Font::FiraSans, "Fira Sans", "fira_sans"},
    {Font::Inter, "Inter", "inter"},
    {Font::Nunito, "Nunito", "nunito"},
    {Font::Ubuntu, "Ubuntu", "ubuntu"},
    {Font::JetBrainsMono, "JetBrains Mono", "jetbrainsmono"},
}};
constexpr const auto& enum_entries(Font) { return font_entries; }

/// file name inside the asset directory, indexed the same way as the enum
inline constexpr std::array<std::string_view, font_count> font_files{{
    "DMSans.ttf", "FiraSans.ttf", "Inter.ttf", "Nunito.ttf", "Ubuntu.ttf", "JetBrainsMono.ttf",
}};

}  // namespace dl::config
