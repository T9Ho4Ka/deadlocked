# native

C++ port of deadlocked, grown one module at a time. The Rust client in `cheat/` stays the
working one and is not touched by anything here; this tree catches up to it gradually.

Ported so far: the settings window shell, the theme system, and the whole config tree with
its tabs. Everything that needs to read the game is still Rust only, so nothing here acts on
CS2 yet: the Grenades and Application tabs are placeholders, the radar has no connection,
and the status in the footer is a stub.

## Build

Needs a C++23 compiler, CMake 3.24+, and glfw. Dear ImGui and toml++ are fetched at configure
time, pinned to `v1.92.9b` and `v3.4.0`.

```sh
cmake -S . -B build -G Ninja
cmake --build build
./build/deadlocked-native
```

Fedora dependencies: `sudo dnf install gcc-c++ cmake ninja-build glfw-devel mesa-libGL-devel`.

## Layout

| path | what |
| --- | --- |
| `src/app.cpp` | window, gl context, ImGui setup, frame loop |
| `src/ui/theme.cpp` | palettes, gradient, ImGui style, port of `cheat/src/ui/theme.rs` |
| `src/ui/sidebar.cpp` | title block, tab list, status footer |
| `src/ui/tabs.cpp` | tab bodies: aimbot, player, hud, unsafe, radar, appearance |
| `src/ui/widgets.cpp` | shared controls: sections, drags, color pickers, keybinds |
| `src/config/game.hpp` | player, hud, unsafe and radar config trees and their enums |
| `src/config/aim.hpp` | aimbot, triggerbot and rcs, global and per weapon |
| `src/config/weapon.hpp` | the weapon and bone enums, generated from `shared/` |
| `src/config/text.hpp` | the eleven overlay text slots and their styling |
| `src/config/font.hpp` | the six bundled typefaces |
| `src/config/keycode.cpp` | key codes and capturing a pressed key |
| `src/config/config.cpp` | TOML load and save |

## Config

A weapon left at its defaults is not written out, so a config file carries only the weapons
the user actually overrode rather than 67 near identical blocks. A missing weapon entry
reads back as the defaults, which is the same thing it would have been written as.

Written to `$XDG_CONFIG_HOME/deadlocked/native.toml`, deliberately a **different file** from
the Rust client's `deadlocked.toml`. A half ported native build must not be able to corrupt a
config the working client depends on. The two formats get merged once the config module is
fully ported.

Fonts are read from `../cheat/assets` through the `DL_ASSETS_DIR` define rather than being
duplicated here. A build that cannot find them falls back to the ImGui built in font.

## Porting order

1. **ui shell and theme** — done
2. **config: player, unsafe, radar** — done
3. **config: hud, overlay text, fonts** — done
4. **config: aim, weapons, bones** — done, the config tree is complete
5. cs2: process access, offsets, entity reading
6. overlay: the OpenGL esp renderer
7. radar client and the update check
