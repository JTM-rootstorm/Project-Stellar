#!/usr/bin/env python3
"""Normalize TrenchBroom/VHLT light_spot and light_environment pitch conventions."""

from __future__ import annotations

import argparse
import json
import math
import re
import sys
from dataclasses import asdict, dataclass
from pathlib import Path

KV_RE = re.compile(r'^(\s*)"([^"]+)"\s+"([^"]*)"(\s*(?://.*)?)$')
TARGET_CLASSES = {"light_spot", "light_environment"}
MARKER_KEY = "_stellar_vhlt_angles_normalized"
DEFAULT_LIGHT_SPOT_CONE = "30"
DEFAULT_LIGHT_SPOT_CONE2 = "45"


@dataclass
class EntityBlock:
    start: int
    end: int
    pairs: dict[str, str]
    key_lines: dict[str, int]


@dataclass
class NormalizedEntity:
    index: int
    classname: str
    line: int
    editor_pitch: float
    compiler_pitch: float
    yaw: float
    roll: float
    skipped_marker: bool = False


@dataclass
class WarningDiagnostic:
    line: int
    message: str


class MapParseError(Exception):
    pass


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


def parse_entities(lines: list[str]) -> list[EntityBlock]:
    entities: list[EntityBlock] = []
    depth = 0
    start: int | None = None
    pairs: dict[str, str] = {}
    key_lines: dict[str, int] = {}
    for index, raw in enumerate(lines):
        stripped = strip_comment(raw).strip()
        if not stripped:
            continue
        if stripped == "{":
            depth += 1
            if depth == 1:
                start = index
                pairs = {}
                key_lines = {}
            continue
        if stripped == "}":
            if depth <= 0:
                raise MapParseError(f"line {index + 1}: closing brace without matching block")
            if depth == 1:
                if start is None:
                    raise MapParseError(f"line {index + 1}: internal parser error for entity block")
                entities.append(EntityBlock(start, index, dict(pairs), dict(key_lines)))
                start = None
                pairs = {}
                key_lines = {}
            depth -= 1
            continue
        if depth == 0:
            raise MapParseError(f"line {index + 1}: content outside entity block")
        if depth == 1:
            match = KV_RE.match(strip_comment(raw).rstrip("\n"))
            if match:
                _, key, value, _ = match.groups()
                pairs[key] = value
                key_lines[key] = index
    if depth != 0:
        line = (start + 1) if start is not None else len(lines)
        raise MapParseError(f"line {line}: unclosed entity or brush block")
    return entities


def parse_float(value: str, field: str, line: int) -> float:
    try:
        parsed = float(value)
    except ValueError as exc:
        raise MapParseError(f"line {line}: {field} must be a finite number, got '{value}'") from exc
    if not math.isfinite(parsed):
        raise MapParseError(f"line {line}: {field} must be finite, got '{value}'")
    return parsed


def parse_angles(value: str, line: int) -> tuple[float, float, float]:
    parts = value.split()
    if len(parts) != 3:
        raise MapParseError(f"line {line}: angles must contain exactly three finite numbers")
    return (
        parse_float(parts[0], "angles pitch", line),
        parse_float(parts[1], "angles yaw", line),
        parse_float(parts[2], "angles roll", line),
    )


def normalize_signed(value: float) -> float:
    normalized = math.fmod(value, 360.0)
    if normalized > 180.0:
        normalized -= 360.0
    if normalized < -180.0:
        normalized += 360.0
    if normalized == -0.0:
        normalized = 0.0
    return normalized


def normalize_yaw(value: float) -> float:
    normalized = math.fmod(value, 360.0)
    if normalized < 0.0:
        normalized += 360.0
    if abs(normalized - 360.0) < 1e-12 or normalized == -0.0:
        normalized = 0.0
    return normalized


def format_number(value: float) -> str:
    if abs(value) < 1e-12:
        value = 0.0
    if float(value).is_integer():
        return str(int(value))
    return f"{value:.6f}".rstrip("0").rstrip(".")


def key_line(key: str, value: str) -> str:
    return f'"{key}" "{value}"\n'


def replace_or_insert(lines: list[str], entity: EntityBlock, key: str, value: str,
                      insert_after: str | None = None) -> None:
    if key in entity.key_lines:
        lines[entity.key_lines[key]] = key_line(key, value)
        return
    insert_at = entity.start + 1
    if insert_after and insert_after in entity.key_lines:
        insert_at = entity.key_lines[insert_after] + 1
    elif "classname" in entity.key_lines:
        insert_at = entity.key_lines["classname"] + 1
    lines.insert(insert_at, key_line(key, value))

def ensure_spotlight_cone_defaults(lines: list[str], entity_index: int) -> None:
    entity = parse_entities(lines)[entity_index]
    if entity.pairs.get("classname") != "light_spot":
        return
    if "_cone" not in entity.pairs:
        insert_after = "_light" if "_light" in entity.key_lines else "classname"
        replace_or_insert(lines, entity, "_cone", DEFAULT_LIGHT_SPOT_CONE, insert_after)
    entity = parse_entities(lines)[entity_index]
    if "_cone2" not in entity.pairs:
        replace_or_insert(lines, entity, "_cone2", DEFAULT_LIGHT_SPOT_CONE2, "_cone")


def rewrite_map(text: str, add_marker: bool) -> tuple[str, list[NormalizedEntity], list[WarningDiagnostic]]:
    lines = text.splitlines(keepends=True)
    entities = parse_entities(lines)
    normalized: list[NormalizedEntity] = []
    warnings: list[WarningDiagnostic] = []

    for entity_index, entity in enumerate(list(entities)):
        classname = entity.pairs.get("classname", "")
        if classname not in TARGET_CLASSES:
            continue
        if classname == "light_spot" and entity.pairs.get("target"):
            warnings.append(WarningDiagnostic(
                entity.key_lines.get("target", entity.start) + 1,
                "targeted light_spot preserves target; VHLT may ignore angle fields for targeted spotlights",
            ))
        if add_marker and entity.pairs.get(MARKER_KEY) == "1":
            if classname == "light_spot":
                ensure_spotlight_cone_defaults(lines, entity_index)
            normalized.append(NormalizedEntity(entity_index, classname, entity.start + 1, 0, 0, 0, 0, True))
            continue

        if "angles" in entity.pairs:
            editor_pitch, yaw, roll = parse_angles(entity.pairs["angles"], entity.key_lines["angles"] + 1)
        else:
            default_pitch = 90.0 if classname == "light_spot" else 45.0
            if "pitch" in entity.pairs:
                editor_pitch = parse_float(entity.pairs["pitch"], "pitch", entity.key_lines["pitch"] + 1)
            else:
                editor_pitch = default_pitch
            if "angle" in entity.pairs:
                yaw = parse_float(entity.pairs["angle"], "angle", entity.key_lines["angle"] + 1)
            else:
                yaw = 0.0
            roll = 0.0

        compiler_pitch = -normalize_signed(editor_pitch)
        pitch_text = format_number(compiler_pitch)
        compiler_yaw = normalize_yaw(yaw)
        yaw_text = format_number(compiler_yaw)
        roll_text = "0"

        # Reparse after each insertion to keep line indexes valid while preserving entity order.
        entity = parse_entities(lines)[entity_index]
        replace_or_insert(lines, entity, "pitch", pitch_text, "classname")
        entity = parse_entities(lines)[entity_index]
        replace_or_insert(lines, entity, "angle", yaw_text, "pitch")
        entity = parse_entities(lines)[entity_index]
        replace_or_insert(lines, entity, "angles", f"{pitch_text} {yaw_text} {roll_text}", "angle")
        if add_marker:
            entity = parse_entities(lines)[entity_index]
            replace_or_insert(lines, entity, MARKER_KEY, "1", "angles")

        if classname == "light_spot":
            ensure_spotlight_cone_defaults(lines, entity_index)

        normalized.append(NormalizedEntity(
            entity_index, classname, entity.start + 1, editor_pitch, compiler_pitch, compiler_yaw, roll
        ))
        entities = parse_entities(lines)

    return "".join(lines), normalized, warnings


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description="Normalize VHLT light_spot/light_environment angles in a .map file.")
    parser.add_argument("map", type=Path)
    parser.add_argument("--dry-run", action="store_true", help="Validate and report changes without writing the map.")
    parser.add_argument("--json", action="store_true", help="Emit a JSON summary to stdout.")
    parser.add_argument("--no-marker", action="store_true", help="Do not write or honor the idempotency marker.")
    args = parser.parse_args(argv)

    try:
        original = args.map.read_text(encoding="utf-8")
        rewritten, normalized, warnings = rewrite_map(original, add_marker=not args.no_marker)
    except OSError as exc:
        print(f"normalize_vhlt_light_angles.py: failed to read {args.map}: {exc}", file=sys.stderr)
        return 1
    except UnicodeDecodeError as exc:
        print(f"normalize_vhlt_light_angles.py: {args.map}: not valid UTF-8: {exc}", file=sys.stderr)
        return 1
    except MapParseError as exc:
        print(f"normalize_vhlt_light_angles.py: {args.map}: {exc}", file=sys.stderr)
        return 1

    for warning in warnings:
        print(f"{args.map}:{warning.line}: warning: {warning.message}", file=sys.stderr)

    changed = rewritten != original
    if changed and not args.dry_run:
        args.map.write_text(rewritten, encoding="utf-8")

    if args.json:
        print(json.dumps({
            "path": str(args.map),
            "changed": changed,
            "dry_run": args.dry_run,
            "normalized": [asdict(item) for item in normalized],
            "warnings": [asdict(item) for item in warnings],
        }, indent=2))
    elif args.dry_run:
        status = "would update" if changed else "already normalized"
        print(f"{args.map}: {status}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
