# native

C++ port of deadlocked, grown one module at a time. The Rust client in `cheat/` stays the
working one and is not touched by anything here; this tree catches up to it gradually.

Ported so far: the settings window shell, the theme system, and the whole config tree with
its tabs. Everything that needs to read the game is still Rust only, so nothing here acts on
CS2 yet: the Grenades and Application tabs are placeholders, the radar has no connection,
and the status in the footer is a stub.

## Build

Needs a C++23 compiler, CMake 3.24+, and glfw. Dear ImGui, toml++ and glm are fetched at
configure time, pinned to `v1.92.9b`, `v3.4.0` and `1.0.1`.

```sh
cmake -S . -B build -G Ninja
cmake --build build
./build/deadlocked-native
```

Tests are plain executables, no framework:

```sh
ctest --test-dir build --output-on-failure
```

`process_test` reads this very process's memory and resolves symbols out of libc, so it
needs no game running. `config_test` writes into a scratch directory, never the real
config. Nothing above covers `cs2/schema.cpp` or `cs2/offsets.cpp`: they can only be exercised
against a live CS2, and a wrong layout constant shows up as a class that is simply not found.

What can be checked without the game is that the tables the port transcribed still match
the rust originals. A wrong weapon index or a mistyped mangled class name compiles fine and
fails silently at runtime, so both are compared against the source they came from:

```sh
python3 tools/check_offsets.py   # the 115 offsets
python3 tools/check_tables.py    # weapons, bones, class names
```

Define `DL_READ_ONLY` to compile out every write to the game's memory, the same as the
rust client's `read-only` feature.

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
| `src/os/process.cpp` | reading another process's memory, modules, elf lookups, scanning |
| `src/cs2/schema.cpp` | the game's schema system: class field offsets by name |
| `src/cs2/offsets.cpp` | the 115 addresses and field offsets, resolved out of the game |
| `src/cs2/entity.cpp` | entities, players and the planted bomb |
| `src/cs2/game.cpp` | attaching to the game and walking its entity list |

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
5. **os: process access, module lookup, pattern scanning** — done
6. **cs2: the schema reader** — done, but only verifiable against a running game
7. **cs2: the offset table** — done, 115 offsets, cross checked against the rust table
8. **cs2: entities, players, the entity list walk** — done
9. cs2: the rest of the player, bones, visibility and the features on top
10. overlay: the OpenGL esp renderer
11. radar client and the update check
