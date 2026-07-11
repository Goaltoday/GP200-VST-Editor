#!/usr/bin/env python3
"""Static consistency checks for the generated GP-200 catalog sources."""

from __future__ import annotations

import re
import sys
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
EFFECT_DB = ROOT / "source/libgp200/GP200EffectDatabase.cpp"
PARAM_DB = ROOT / "source/libgp200/GP200EffectParamDatabase.cpp"


def parse_hex(value: str) -> int:
    return int(value.lower().removesuffix("u"), 16)


def check_sorted_unique(name: str, values: list[int], errors: list[str]) -> None:
    duplicates = [value for value, count in Counter(values).items() if count > 1]
    if duplicates:
        errors.append(f"{name}: duplicate IDs: " + ", ".join(f"0x{x:08X}" for x in duplicates))
    if values != sorted(values):
        errors.append(f"{name}: IDs are not sorted in ascending order")


def main() -> int:
    errors: list[str] = []
    effect_text = EFFECT_DB.read_text(encoding="utf-8")
    param_text = PARAM_DB.read_text(encoding="utf-8")

    effect_map_section = re.search(
        r"constexpr\s+GP200EffectInfo\s+effectMap\[\]\s*=\s*\{(.*?)\n\};",
        effect_text,
        re.S,
    )
    description_section = re.search(
        r"constexpr\s+GP200EffectDescription\s+effectDescriptions\[\]\s*=\s*\{(.*?)\n\};",
        effect_text,
        re.S,
    )
    param_sets_section = re.search(
        r"constexpr\s+GP200EffectParamSet\s+paramSets\[\]\s*=\s*\{(.*?)\n\};",
        param_text,
        re.S,
    )

    if not effect_map_section or not description_section or not param_sets_section:
        print("ERROR: one or more catalog arrays could not be located", file=sys.stderr)
        return 1

    effect_ids = [
        parse_hex(value)
        for value in re.findall(r"\{\s*(0[xX][0-9A-Fa-f]+u)\s*,\s*\"", effect_map_section.group(1))
    ]
    description_ids = [
        parse_hex(value)
        for value in re.findall(r"\{\s*(0[xX][0-9A-Fa-f]+u)\s*,\s*\"", description_section.group(1))
    ]

    layout_bodies = {
        name: body
        for name, body in re.findall(
            r"constexpr\s+GP200EffectParamInfo\s+(paramLayout_\d+)\[\]\s*=\s*\{(.*?)\n\};",
            param_text,
            re.S,
        )
    }
    layout_sizes = {
        name: len(re.findall(r"\{\s*-?\d+\s*,\s*\"", body))
        for name, body in layout_bodies.items()
    }

    param_sets = [
        (parse_hex(effect_id), layout_name, int(count))
        for effect_id, layout_name, count in re.findall(
            r"\{\s*(0[xX][0-9A-Fa-f]+u)\s*,\s*(paramLayout_\d+)\s*,\s*(\d+)\s*\}",
            param_sets_section.group(1),
        )
    ]
    param_ids = [entry[0] for entry in param_sets]

    check_sorted_unique("effectMap", effect_ids, errors)
    check_sorted_unique("effectDescriptions", description_ids, errors)
    check_sorted_unique("paramSets", param_ids, errors)

    effect_id_set = set(effect_ids)
    for label, ids in (("effectDescriptions", description_ids), ("paramSets", param_ids)):
        missing = sorted(set(ids) - effect_id_set)
        if missing:
            errors.append(
                f"{label}: IDs absent from effectMap: "
                + ", ".join(f"0x{x:08X}" for x in missing)
            )

    for effect_id, layout_name, declared_count in param_sets:
        actual_count = layout_sizes.get(layout_name)
        if actual_count is None:
            errors.append(f"paramSets: {layout_name} is referenced but not declared")
        elif actual_count != declared_count:
            errors.append(
                f"paramSets: 0x{effect_id:08X} declares {declared_count} parameters "
                f"but {layout_name} contains {actual_count}"
            )

    unused_layouts = sorted(set(layout_sizes) - {layout for _, layout, _ in param_sets})
    if unused_layouts:
        errors.append("unused parameter layouts: " + ", ".join(unused_layouts))

    if '"combox"' in param_text or '"combo"' in param_text or '"knob"' in param_text:
        errors.append("legacy string parameter kinds are still present")

    if errors:
        for error in errors:
            print(f"ERROR: {error}", file=sys.stderr)
        return 1

    print(
        "Catalog validation passed: "
        f"{len(effect_ids)} effects, {len(description_ids)} descriptions, "
        f"{len(param_sets)} parameter mappings, {len(layout_sizes)} shared layouts."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
