#!/usr/bin/env python3
"""Preflight Stellar TrenchBroom .map source before BSP30 compilation."""

from __future__ import annotations

import argparse
import json
import math
import re
import sys
from dataclasses import dataclass, asdict
from pathlib import Path

KNOWN_CLASSES = {
    "worldspawn", "info_player_start", "info_player_deathmatch", "info_stellar_spawn",
    "trigger_multiple", "trigger_once", "trigger_stellar", "trigger_stellar_point",
    "trigger_multiple_point", "trigger_once_point", "stellar_object_collider",
    "stellar_object_collider_point", "stellar_sprite", "env_sprite", "func_wall",
    "func_illusionary", "func_detail", "func_door", "func_button", "target_stellar_relay",
    "light", "light_spot", "light_environment", "stellar_global_light", "info_null",
}
KNOWN_STELLAR_KEYS = {
    "stellar.script", "_stellar_script", "stellar.table", "_stellar_table",
    "stellar.extents", "_stellar_extents", "stellar.once", "_stellar_once",
    "stellar.enabled", "_stellar_enabled", "stellar.collider", "_stellar_collider",
    "stellar.sprite", "_stellar_sprite", "stellar.texture", "_stellar_texture",
    "stellar.size", "_stellar_size", "stellar.alpha", "_stellar_alpha",
    "stellar.collision", "_stellar_collision",
}
FACE_RE = re.compile(
    r"^\s*\(([^)]*)\)\s*\(([^)]*)\)\s*\(([^)]*)\)\s+([^\s]+)\s+"
    r"\[([^]]*)\]\s+\[([^]]*)\]\s+([-+]?\d+(?:\.\d+)?)\s+"
    r"([-+]?\d+(?:\.\d+)?)\s+([-+]?\d+(?:\.\d+)?)(?:\s|$)"
)
KV_RE = re.compile(r'^\s*"([^"]+)"\s+"([^"]*)"\s*$')
DEV_ALIASES = {
    "dev/grid_12", "dev/grid_16", "dev/grid_32", "dev/grid_64", "dev/player_72", "dev/wall_96",
    "dev_grid_12", "dev_grid_16", "dev_grid_32", "dev_grid_64", "dev_player_72", "dev_wall_96",
    "stellar_dev_grid_12", "stellar_dev_grid_16", "stellar_dev_grid_32", "stellar_dev_grid_64",
    "stellar_dev_player_72", "stellar_dev_wall_96",
}
IGNORE_TEXTURES = {"NULL", "CLIP", "SKIP", "ORIGIN", "AAATRIGGER"}


@dataclass
class Diagnostic:
    severity: str
    line: int
    column: int
    message: str


def strip_comment(line: str) -> str:
    in_quote = False
    i = 0
    while i + 1 < len(line):
        if line[i] == '"':
            in_quote = not in_quote
        if not in_quote and line[i:i + 2] == "//":
            return line[:i]
        i += 1
    return line


def parse_floats(text: str, count: int) -> list[float] | None:
    parts = text.split()
    if len(parts) != count:
        return None
    values: list[float] = []
    try:
        for part in parts:
            value = float(part)
            if not math.isfinite(value):
                return None
            values.append(value)
    except ValueError:
        return None
    return values


def parse_finite_float(text: str) -> float | None:
    try:
        value = float(text)
    except ValueError:
        return None
    return value if math.isfinite(value) else None


def validate_light_angles(entity: dict[str, str], lines_by_key: dict[str, int], entity_line: int,
                          diagnostics: list[Diagnostic]) -> None:
    classname = entity.get("classname", "")
    if classname not in {"light_spot", "light_environment"}:
        return
    angles_valid = False
    if "angles" in entity:
        line = lines_by_key.get("angles", entity_line)
        angles = parse_floats(entity["angles"], 3)
        if angles is None:
            diagnostics.append(Diagnostic("error", line, 1, "angles must contain exactly three finite numbers"))
        else:
            angles_valid = True
            if "pitch" in entity or "angle" in entity:
                diagnostics.append(Diagnostic(
                    "warning", line, 1,
                    "angles is present and takes precedence over separate pitch/angle fields for VHLT normalization"))
            if abs(angles[2]) > 1e-12:
                diagnostics.append(Diagnostic("warning", line, 1, "nonzero light roll is ignored by VHLT normalization"))
    if "pitch" in entity:
        line = lines_by_key.get("pitch", entity_line)
        if parse_finite_float(entity["pitch"]) is None:
            diagnostics.append(Diagnostic("error", line, 1, "pitch must be a finite number"))
    if "angle" in entity:
        line = lines_by_key.get("angle", entity_line)
        if parse_finite_float(entity["angle"]) is None:
            diagnostics.append(Diagnostic("error", line, 1, "angle must be a finite number"))
    if classname == "light_spot" and entity.get("target"):
        diagnostics.append(Diagnostic(
            "warning", lines_by_key.get("target", entity_line), 1,
            "targeted light_spot may ignore angle fields in VHLT; target is preserved"))


def script_path_escape(value: str) -> bool:
    path = Path(value)
    if path.is_absolute() or re.match(r"^[A-Za-z]:", value):
        return True
    return any(part == ".." for part in value.replace("\\", "/").split("/"))


def split_wad_entries(value: str) -> list[str]:
    return [item.strip() for item in value.split(";") if item.strip()]


def texture_known_to_preflight(texture: str, wad_entries: list[str], wad_roots: list[Path]) -> bool:
    if texture in DEV_ALIASES or texture.upper() in IGNORE_TEXTURES:
        return True
    if not wad_entries:
        return False
    for entry in wad_entries:
        entry_path = Path(entry)
        if entry_path.is_absolute() or script_path_escape(entry):
            continue
        for root in wad_roots:
            if (root / entry_path).is_file():
                return True
    return False


def validate(path: Path, allow_no_spawn: bool, strict_textures: bool,
             wad_roots: list[Path]) -> list[Diagnostic]:
    diagnostics: list[Diagnostic] = []
    stack: list[dict[str, object]] = []
    entities: list[dict[str, object]] = []
    used_textures: list[tuple[str, int, int]] = []
    try:
        lines = path.read_text(encoding="utf-8").splitlines()
    except UnicodeDecodeError as exc:
        return [Diagnostic("error", 1, 1, f"source map is not valid UTF-8: {exc}")]
    for line_no, raw in enumerate(lines, 1):
        line = strip_comment(raw).strip()
        if not line:
            continue
        if line == "{":
            if not stack:
                stack.append({"kind": "entity", "pairs": {}, "key_lines": {}, "brush_planes": None, "line": line_no})
            elif stack[-1]["kind"] == "entity":
                stack.append({"kind": "brush", "brush_planes": 0, "line": line_no})
            else:
                diagnostics.append(Diagnostic("error", line_no, 1, "nested brush blocks are not supported"))
            continue
        if line == "}":
            if not stack:
                diagnostics.append(Diagnostic("error", line_no, 1, "closing brace without matching block"))
                continue
            block = stack.pop()
            if block["kind"] == "brush" and int(block["brush_planes"]) < 4:
                diagnostics.append(Diagnostic("error", int(block["line"]), 1, "brush has fewer than 4 planes"))
            elif block["kind"] == "entity":
                entities.append({
                    "pairs": dict(block["pairs"]),
                    "key_lines": dict(block["key_lines"]),
                    "line": int(block["line"]),
                })
            continue
        if not stack:
            diagnostics.append(Diagnostic("error", line_no, 1, "content outside entity block"))
            continue
        if stack[-1]["kind"] == "entity":
            match = KV_RE.match(line)
            if not match:
                diagnostics.append(Diagnostic("error", line_no, 1, "expected quoted entity key/value pair or brush block"))
                continue
            key, value = match.groups()
            stack[-1]["pairs"][key] = value
            stack[-1]["key_lines"][key] = line_no
            if key == "wad":
                for item in value.split(";"):
                    if item and (Path(item).is_absolute() or script_path_escape(item)):
                        diagnostics.append(Diagnostic("error", line_no, line.find(value) + 1, "worldspawn wad path must be relative and must not escape with .."))
            if key in ("stellar.script", "_stellar_script") and script_path_escape(value):
                diagnostics.append(Diagnostic("error", line_no, line.find(value) + 1, "script path must not be absolute or contain .."))
            if key in ("stellar.extents", "_stellar_extents") and parse_floats(value, 3) is None:
                diagnostics.append(Diagnostic("error", line_no, line.find(value) + 1, f"{key} must be a finite 3-vector"))
            if key in ("stellar.size", "_stellar_size") and parse_floats(value, 2) is None:
                diagnostics.append(Diagnostic("error", line_no, line.find(value) + 1, f"{key} must be a finite 2-vector"))
            if key in ("stellar.once", "_stellar_once", "stellar.enabled", "_stellar_enabled") and value.lower() not in {"0", "1", "true", "false", "yes", "no"}:
                diagnostics.append(Diagnostic("error", line_no, line.find(value) + 1, f"{key} must be boolean-like"))
            continue
        face = FACE_RE.match(line)
        if not face:
            diagnostics.append(Diagnostic("error", line_no, 1, "expected Valve 220 brush face"))
            continue
        for group in (1, 2, 3):
            if parse_floats(face.group(group), 3) is None:
                diagnostics.append(Diagnostic("error", line_no, 1, "face plane point triple must contain finite numbers"))
        texture = face.group(4)
        used_textures.append((texture, line_no, raw.find(texture) + 1))
        rewritten = texture.replace("dev/", "dev_", 1) if texture.startswith("dev/") else texture
        if not texture or len(rewritten.encode("ascii", "ignore")) > 15:
            diagnostics.append(Diagnostic("error", line_no, raw.find(texture) + 1, "texture name is missing or exceeds BSP30/WAD 15-byte limit after rewrite"))
        for group in (5, 6):
            if parse_floats(face.group(group), 4) is None:
                diagnostics.append(Diagnostic("error", line_no, raw.find("[") + 1, "Valve texture axis must contain four finite numbers"))
        if parse_floats(" ".join(face.group(g) for g in (7, 8, 9)), 3) is None:
            diagnostics.append(Diagnostic("error", line_no, 1, "rotation and texture scales must be finite numbers"))
        stack[-1]["brush_planes"] = int(stack[-1]["brush_planes"]) + 1
    for block in stack:
        diagnostics.append(Diagnostic("error", int(block["line"]), 1, f"unclosed {block['kind']} block"))
    if not entities:
        diagnostics.append(Diagnostic("error", 1, 1, "map has no entities"))
        return diagnostics
    entity_pairs = [entity["pairs"] for entity in entities]
    if entity_pairs[0].get("classname") != "worldspawn":
        diagnostics.append(Diagnostic("error", 1, 1, "first entity must be worldspawn"))
    if not allow_no_spawn and not any(entity.get("classname") in {"info_player_start", "info_player_deathmatch"} for entity in entity_pairs):
        diagnostics.append(Diagnostic("error", 1, 1, "map must contain an info_player_start unless --allow-no-spawn is used"))
    for index, entity_record in enumerate(entities):
        entity = entity_record["pairs"]
        key_lines = entity_record["key_lines"]
        classname = entity.get("classname", "")
        if not classname:
            diagnostics.append(Diagnostic("error", 1, 1, f"entity {index} is missing classname"))
        elif classname not in KNOWN_CLASSES:
            diagnostics.append(Diagnostic("warning", 1, 1, f"entity {index} uses custom classname: {classname}"))
        validate_light_angles(entity, key_lines, int(entity_record["line"]), diagnostics)
        for key in entity:
            if key.startswith("_stellar") or key.startswith("stellar."):
                if key not in KNOWN_STELLAR_KEYS:
                    diagnostics.append(Diagnostic("warning", 1, 1, f"custom Stellar property on {classname}: {key}"))
    targetnames = {entity.get("targetname", "") for entity in entity_pairs if entity.get("targetname")}
    for entity in entity_pairs:
        target = entity.get("target")
        if target and target not in targetnames:
            diagnostics.append(Diagnostic("error", 1, 1,
                                          f"target '{target}' on {entity.get('classname', '<unknown>')} does not match any targetname"))
    if strict_textures:
        world_wads = split_wad_entries(entity_pairs[0].get("wad", "")) if entity_pairs else []
        strict_roots = [path.parent, *wad_roots]
        for texture, line_no, column in used_textures:
            if not texture_known_to_preflight(texture, world_wads, strict_roots):
                diagnostics.append(Diagnostic(
                    "error", line_no, column,
                    f"texture '{texture}' is not a known developer texture and was not resolvable from worldspawn wad"))
    return diagnostics


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description="Validate Stellar TrenchBroom .map source before compile.")
    parser.add_argument("map", type=Path)
    parser.add_argument("--allow-no-spawn", action="store_true")
    parser.add_argument("--strict-textures", action="store_true",
                        help="Fail when a used texture is neither a known developer alias nor resolvable from worldspawn wad entries.")
    parser.add_argument("--wad-root", action="append", type=Path, default=[],
                        help="Additional root to search for relative worldspawn wad entries in strict texture mode.")
    parser.add_argument("--format", choices=("human", "json"), default="human")
    args = parser.parse_args(argv)
    diagnostics = validate(args.map, args.allow_no_spawn, args.strict_textures, args.wad_root)
    if args.format == "json":
        print(json.dumps({"path": str(args.map), "diagnostics": [asdict(d) for d in diagnostics]}, indent=2))
    else:
        for diagnostic in diagnostics:
            print(f"{args.map}:{diagnostic.line}:{diagnostic.column}: {diagnostic.severity}: {diagnostic.message}")
        if not diagnostics:
            print(f"{args.map}: preflight passed")
    return 1 if any(d.severity == "error" for d in diagnostics) else 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
