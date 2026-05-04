#!/usr/bin/env python3
"""Small map/BSP rewrites used by the VHLT BSP30 wrapper."""

from __future__ import annotations

import argparse
import re
import struct
from pathlib import Path

DEV_TEXTURE_NAMES = ("grid_12", "grid_16", "grid_32", "grid_64", "player_72", "wall_96")


def inject_worldspawn_key(map_path: Path, key: str, value: str) -> None:
    lines = map_path.read_text(encoding="utf-8").splitlines(keepends=True)

    depth = 0
    world_start: int | None = None
    world_end: int | None = None
    classname_at_depth_one = False
    for i, line in enumerate(lines):
        stripped = line.strip()
        if stripped == "{":
            depth += 1
            if depth == 1 and world_start is None:
                world_start = i
            continue
        if stripped == "}":
            if depth == 1 and classname_at_depth_one and world_end is None:
                world_end = i
                break
            depth -= 1
            if depth == 0 and world_start is not None and world_end is None:
                world_start = None
                classname_at_depth_one = False
            continue
        if depth == 1 and re.match(r'^"classname"\s+"worldspawn"\s*$', stripped):
            classname_at_depth_one = True

    if world_start is None or world_end is None:
        raise SystemExit("worldspawn entity not found")

    key_line = f'"{key}" "{value}"\n'
    key_re = re.compile(r'^\s*"' + re.escape(key) + r'"\s+".*"\s*$')
    for i in range(world_start + 1, world_end):
        if key_re.match(lines[i].strip()):
            lines[i] = key_line
            break
    else:
        insert_at = world_start + 1
        for i in range(world_start + 1, world_end):
            if re.match(r'^\s*"classname"\s+"worldspawn"\s*$', lines[i].strip()):
                insert_at = i + 1
                break
        lines.insert(insert_at, key_line)

    map_path.write_text("".join(lines), encoding="utf-8")


def rewrite_vhlt_texture_names_only(map_path: Path) -> None:
    lines = map_path.read_text(encoding="utf-8").splitlines(keepends=True)
    converted = []
    for line in lines:
        for name in DEV_TEXTURE_NAMES:
            line = line.replace(f"dev/{name}", f"dev_{name}")
        converted.append(line)
    map_path.write_text("".join(converted), encoding="utf-8")


def rewrite_valve220_to_classic_texture_axes(map_path: Path) -> None:
    lines = map_path.read_text(encoding="utf-8").splitlines(keepends=True)
    converted = []
    face = re.compile(
        r'^(\s*\([^)]*\)\s*\([^)]*\)\s*\([^)]*\)\s+)'
        r'(\S+)\s+\[[^\]]*\]\s+\[[^\]]*\]\s+'
        r'([-+]?\d+(?:\.\d+)?)\s+([-+]?\d+(?:\.\d+)?)\s+([-+]?\d+(?:\.\d+)?)(\s*(?://.*)?\n?)$'
    )
    for line in lines:
        match = face.match(line)
        if match:
            prefix, texture, rotation, scale_x, scale_y, suffix = match.groups()
            line = f"{prefix}{texture} 0 0 {rotation} {scale_x} {scale_y}{suffix}"
        converted.append(line)
    map_path.write_text("".join(converted), encoding="utf-8")


def normalize_bsp_entity_lump(bsp_path: Path) -> None:
    data = bytearray(bsp_path.read_bytes())
    if len(data) < 12:
        raise SystemExit("BSP is too small")
    version = struct.unpack_from("<i", data, 0)[0]
    if version != 30:
        raise SystemExit(f"expected BSP30 before entity normalization, found {version}")
    entity_offset, entity_length = struct.unpack_from("<ii", data, 4)
    if entity_offset < 0 or entity_length < 0 or entity_offset + entity_length > len(data):
        raise SystemExit("invalid BSP entity lump bounds")
    end = entity_offset + entity_length
    while entity_length > 0 and data[end - 1] == 0:
        entity_length -= 1
        end -= 1
    struct.pack_into("<ii", data, 4, entity_offset, entity_length)
    bsp_path.write_bytes(data)


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)

    inject = subparsers.add_parser("inject-worldspawn-key")
    inject.add_argument("map", type=Path)
    inject.add_argument("key")
    inject.add_argument("value")

    wad = subparsers.add_parser("inject-wad-key")
    wad.add_argument("map", type=Path)
    wad.add_argument("wad")

    mapversion = subparsers.add_parser("inject-mapversion-key")
    mapversion.add_argument("map", type=Path)

    textures = subparsers.add_parser("rewrite-vhlt-texture-names-only")
    textures.add_argument("map", type=Path)

    axes = subparsers.add_parser("rewrite-valve220-to-classic-texture-axes")
    axes.add_argument("map", type=Path)

    bsp = subparsers.add_parser("normalize-bsp-entity-lump")
    bsp.add_argument("bsp", type=Path)
    return parser


def main() -> int:
    args = build_parser().parse_args()
    if args.command == "inject-worldspawn-key":
        inject_worldspawn_key(args.map, args.key, args.value)
    elif args.command == "inject-wad-key":
        inject_worldspawn_key(args.map, "wad", args.wad)
    elif args.command == "inject-mapversion-key":
        inject_worldspawn_key(args.map, "mapversion", "220")
    elif args.command == "rewrite-vhlt-texture-names-only":
        rewrite_vhlt_texture_names_only(args.map)
    elif args.command == "rewrite-valve220-to-classic-texture-axes":
        rewrite_valve220_to_classic_texture_axes(args.map)
    elif args.command == "normalize-bsp-entity-lump":
        normalize_bsp_entity_lump(args.bsp)
    else:
        raise AssertionError(args.command)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
