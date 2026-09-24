#pragma once

#include <array>
#include <cstddef>
#include <string_view>

namespace dl::constants {

/// Ported from cheat/src/constants.rs
namespace cs2 {
inline constexpr std::string_view process_name = "cs2";
inline constexpr std::string_view client_lib = "libclient.so";
inline constexpr std::string_view engine_lib = "libengine2.so";
inline constexpr std::string_view tier0_lib = "libtier0.so";
inline constexpr std::string_view input_lib = "libinputsystem.so";
inline constexpr std::string_view sdl_lib = "libSDL3.so.0";
inline constexpr std::string_view schema_lib = "libschemasystem.so";
inline constexpr std::string_view physics_lib = "libvphysics2.so";

inline constexpr std::array<std::string_view, 6> libs{
    client_lib, engine_lib, tier0_lib, input_lib, sdl_lib, schema_lib,
};

inline constexpr std::size_t mesh_skeleton_bone_count = 96;
}  // namespace cs2

/// Offsets into an ELF64 header, used to work out how big a mapped module is.
namespace elf {
inline constexpr std::size_t program_header_offset = 0x20;
inline constexpr std::size_t program_header_entry_size = 0x36;
inline constexpr std::size_t program_header_num_entries = 0x38;

inline constexpr std::size_t section_header_offset = 0x28;
inline constexpr std::size_t section_header_entry_size = 0x3A;
inline constexpr std::size_t section_header_num_entries = 0x3C;

inline constexpr std::size_t dynamic_section_pht_type = 0x02;
}  // namespace elf

}  // namespace dl::constants
