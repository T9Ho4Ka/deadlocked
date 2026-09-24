#!/usr/bin/env python3
"""Cross-checks the constant tables the port transcribed from the rust sources.

A wrong weapon index, a renamed bone or a mistyped mangled class name all compile fine and
fail silently at runtime, so they are compared against the rust originals here instead.

Run from anywhere:  python3 native/tools/check_tables.py
"""

from __future__ import annotations

import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]


def read(path: str) -> str:
    return (ROOT / path).read_text()


def check(ok: bool, what: str, detail: str = "") -> bool:
    print(f"{what:<52} {'ok' if ok else 'FAIL'}")
    if not ok and detail:
        print(f"    {detail}")
    return ok


def main() -> int:
    ok = True
    rust_weapon = read("shared/src/weapon.rs")
    rust_bones = read("shared/src/bones.rs")
    rust_consts = read("cheat/src/constants.rs")
    cpp_weapon = read("native/src/config/weapon.hpp")
    cpp_index = read("native/src/cs2/weapon_index.hpp")
    cpp_types = read("native/src/cs2/types.hpp")

    # --- weapon variants and display names ---
    body = rust_weapon[rust_weapon.index("pub enum Weapon {") :]
    body = body[: body.index("\n}")]
    variants = re.findall(r"^\s{4}([A-Za-z0-9_]+),?\s*$", body, re.M)
    display = rust_weapon[rust_weapon.index("impl std::fmt::Display for Weapon") :]
    names = dict(re.findall(r"Self::([A-Za-z0-9_]+) => \"([^\"]*)\"", display))

    cpp_pairs = re.findall(r"\{Weapon::(\w+), \"([^\"]*)\", \"([^\"]*)\"\}", cpp_weapon)
    ok &= check(len(cpp_pairs) == len(variants), "weapon count matches",
                f"rust {len(variants)}, c++ {len(cpp_pairs)}")
    mismatched = [v for v, shown, _ in cpp_pairs if names.get(v) != shown]
    ok &= check(not mismatched, "weapon display names match", ", ".join(mismatched[:5]))
    ok &= check([v for v, _, _ in cpp_pairs] == variants, "weapon order matches")

    # --- item definition indices ---
    from_index = rust_weapon[rust_weapon.index("pub fn from_index") :]
    from_index = from_index[: from_index.index("\n    }")]
    rust_map = {int(i): n for i, n in re.findall(r"(\d+)\s*=>\s*Self::(\w+)", from_index)}
    cpp_map = {int(i): n for i, n in re.findall(r"\{(\d+), config::Weapon::(\w+)\}", cpp_index)}
    ok &= check(rust_map == cpp_map, "item definition indices match",
                f"only in rust: {sorted(set(rust_map) - set(cpp_map))[:5]}, "
                f"only in c++: {sorted(set(cpp_map) - set(rust_map))[:5]}")

    # --- bones, whose numbering has deliberate gaps ---
    bbody = rust_bones[rust_bones.index("pub enum Bones {") :]
    bbody = bbody[: bbody.index("\n}")]
    rust_bone = {n: int(v) for n, v in re.findall(r"^\s{4}(\w+) = (\d+),", bbody, re.M)}
    cpp_bone = {n: int(v) for n, v in re.findall(r"^\s{4}(\w+) = (\d+),", cpp_weapon, re.M)}
    ok &= check(rust_bone == cpp_bone, "bone indices match",
                f"rust {len(rust_bone)}, c++ {len(cpp_bone)}")

    # --- mangled class names ---
    cbody = rust_consts[rust_consts.index("pub mod class {") :]
    cbody = cbody[: cbody.index("\n    }")]
    rust_class = {n.lower(): v for n, v in re.findall(r'pub const (\w+): &str = "([^"]*)"', cbody)}
    cpp_class = dict(
        re.findall(r'inline constexpr std::string_view (\w+) = "([^"]*)"', cpp_types)
    )
    ok &= check(rust_class == cpp_class, "mangled class names match",
                f"differing: {sorted(set(rust_class.items()) ^ set(cpp_class.items()))[:4]}")

    print("\nin sync" if ok else "\nMISMATCH")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
