#!/usr/bin/env python3
"""Cross-checks the ported offset table against the rust one.

The table in ../cheat/src/cs2/offsets.rs is a declarative macro, and the C++ side mirrors it
by hand. This script parses both and reports any offset that was dropped, renamed, or added
along the way, which is the failure mode a compiler cannot catch.

Run from anywhere:  python3 native/tools/check_offsets.py
"""

from __future__ import annotations

import json
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
RUST = ROOT / "cheat" / "src" / "cs2" / "offsets.rs"
CPP = ROOT / "native" / "src" / "cs2" / "offsets.cpp"
HPP = ROOT / "native" / "src" / "cs2" / "offsets.hpp"


def parse_rust() -> list[dict]:
    src = RUST.read_text()
    block = src[src.index("schema! {") + len("schema! {") :]
    block = block[: block.index("\nimpl CS2")]
    block = block[: block.rindex("}")]

    groups: list[dict] = []
    scope = None
    current = None
    for line in block.split("\n"):
        text = line.strip()
        if not text:
            continue
        if match := re.match(r"^(client|physics):\s*\{$", text):
            scope = match.group(1)
            continue
        if match := re.match(r"^(\w+):\s*(\w+)\s*\{$", text):
            current = {"field": match.group(1), "struct": match.group(2), "scope": scope, "buf": "", "fields": []}
            groups.append(current)
            continue
        if text == "}":
            if current is not None:
                current = None
            else:
                scope = None
            continue
        if current is not None:
            current["buf"] += " " + text

    for group in groups:
        buf = group.pop("buf")
        for match in re.finditer(r"(\w+)\s*:\s*(\w+)\((.*?)\)\s*,(?=\s*(?:\w+\s*:|$))", buf, re.S):
            group["fields"].append((match.group(1), match.group(2), match.group(3).strip()))
    return groups


def main() -> int:
    groups = parse_rust()
    rust = {f"{g['field']}.{name}" for g in groups for name, _, _ in g["fields"]}

    cpp = CPP.read_text()
    assigned = {f"{a}.{b}" for a, b in re.findall(r"offsets\.(\w+)\.(\w+)\s*=", cpp)}

    struct_of = {g["struct"]: g["field"] for g in groups}
    declared = set()
    current_struct = None
    for line in HPP.read_text().split("\n"):
        if match := re.match(r"^struct (\w+) \{", line):
            current_struct = match.group(1)
        elif match := re.match(r"^\s+std::uintptr_t (\w+) = 0;", line):
            if current_struct in struct_of:
                declared.add(f"{struct_of[current_struct]}.{match.group(1)}")
        elif line == "};":
            current_struct = None

    problems = {
        "declared in the header but not in the rust table": sorted(declared - rust),
        "in the rust table but never declared": sorted(rust - declared),
        "in the rust table but never assigned": sorted(rust - assigned),
        "assigned but not in the rust table": sorted(assigned - rust),
    }

    print(f"rust offsets:      {len(rust)}")
    print(f"c++ declared:      {len(declared)}")
    print(f"c++ assigned:      {len(assigned)}")

    failed = False
    for title, entries in problems.items():
        if entries:
            failed = True
            print(f"\n{title}:")
            for entry in entries:
                print(f"  {entry}")

    print("\nMISMATCH" if failed else "\nin sync")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
