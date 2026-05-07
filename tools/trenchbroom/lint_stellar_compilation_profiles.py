#!/usr/bin/env python3
"""Lint Stellar TrenchBroom compilation tool/profile declarations."""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

TOOL_NAME_RE = re.compile(r"^[A-Z][A-Z0-9_]*$")
TOOL_REF_RE = re.compile(r"^\$\{([A-Z][A-Z0-9_]*)\}$")
VARIABLE_REF_RE = re.compile(r"\$\{([A-Za-z_][A-Za-z0-9_]*)")

WORKDIR_ALLOWED_VARIABLES = {
    "MAP_DIR_PATH",
    "MAP_BASE_NAME",
    "MAP_FULL_NAME",
    "GAME_DIR_PATH",
    "MODS",
    "APP_DIR_PATH",
}

TOOL_ALLOWED_VARIABLES = WORKDIR_ALLOWED_VARIABLES | {
    "WORK_DIR_PATH",
    "CPU_COUNT",
}


def strip_json_comments(text: str) -> str:
    lines: list[str] = []
    for raw in text.splitlines():
        in_quote = False
        escaped = False
        out: list[str] = []
        i = 0
        while i < len(raw):
            char = raw[i]
            if escaped:
                out.append(char)
                escaped = False
                i += 1
                continue
            if char == "\\" and in_quote:
                out.append(char)
                escaped = True
                i += 1
                continue
            if char == '"':
                in_quote = not in_quote
                out.append(char)
                i += 1
                continue
            if not in_quote and raw[i:i + 2] == "//":
                break
            out.append(char)
            i += 1
        lines.append("".join(out))
    return "\n".join(lines)


def load_cfg(path: Path) -> object:
    try:
        return json.loads(strip_json_comments(path.read_text(encoding="utf-8")))
    except OSError as exc:
        raise ValueError(f"{path}: unable to read cfg: {exc}") from exc
    except json.JSONDecodeError as exc:
        raise ValueError(f"{path}: invalid JSON-style cfg: {exc}") from exc


def require_dict(value: object, label: str) -> dict[str, object]:
    if not isinstance(value, dict):
        raise ValueError(f"{label} must be an object")
    return value


def require_list(value: object, label: str) -> list[object]:
    if not isinstance(value, list):
        raise ValueError(f"{label} must be an array")
    return value


def validate_variables(label: str, value: str, allowed_variables: set[str]) -> list[str]:
    errors: list[str] = []
    if "${MAP_FULL_PATH}" in value:
        errors.append(
            f"{label} uses unsupported TrenchBroom variable ${{MAP_FULL_PATH}}; "
            f"use ${{MAP_DIR_PATH}}/${{MAP_FULL_NAME}}"
        )
    for variable in VARIABLE_REF_RE.findall(value):
        if variable not in allowed_variables:
            errors.append(f"{label} uses unsupported variable ${{{variable}}}")
    return errors


def iter_string_fields(value: object, label: str) -> list[tuple[str, str]]:
    fields: list[tuple[str, str]] = []
    if isinstance(value, dict):
        for key, child in value.items():
            child_label = f"{label}.{key}"
            fields.extend(iter_string_fields(child, child_label))
    elif isinstance(value, list):
        for index, child in enumerate(value):
            child_label = f"{label}[{index}]"
            fields.extend(iter_string_fields(child, child_label))
    elif isinstance(value, str):
        fields.append((label, value))
    return fields


def lint(game_config_path: Path, profiles_path: Path) -> list[str]:
    errors: list[str] = []
    try:
        game_config = require_dict(load_cfg(game_config_path), str(game_config_path))
        profiles_config = require_dict(load_cfg(profiles_path), str(profiles_path))
    except ValueError as exc:
        return [str(exc)]

    declared_tools: set[str] = set()
    try:
        compilation_tools = require_list(
            game_config.get("compilationTools"), "GameConfig.cfg compilationTools"
        )
    except ValueError as exc:
        errors.append(str(exc))
        compilation_tools = []
    for index, tool_value in enumerate(compilation_tools):
        if not isinstance(tool_value, dict):
            errors.append(f"compilationTools[{index}] must be an object")
            continue
        name = tool_value.get("name")
        if not isinstance(name, str) or not name:
            errors.append(f"compilationTools[{index}] missing string name")
            continue
        if " " in name:
            errors.append(f"compilationTools[{index}] name contains unsafe spaces: {name!r}")
        if not TOOL_NAME_RE.fullmatch(name):
            errors.append(
                f"compilationTools[{index}] name is not variable-safe uppercase identifier: {name!r}"
            )
        declared_tools.add(name)

    if profiles_config.get("version") != 1:
        errors.append("CompilationProfiles.cfg must declare top-level \"version\": 1")

    profiles = profiles_config.get("profiles")
    if not isinstance(profiles, list) or not profiles:
        errors.append("CompilationProfiles.cfg must declare a non-empty profiles array")
        return errors

    for profile_index, profile_value in enumerate(profiles):
        if not isinstance(profile_value, dict):
            errors.append(f"profiles[{profile_index}] must be an object")
            continue
        profile_name = profile_value.get("name", f"#{profile_index}")
        workdir = profile_value.get("workdir")
        if not isinstance(workdir, str) or not workdir:
            errors.append(f"profile {profile_name!r} must declare a non-empty string workdir")
        else:
            if "${WORK_DIR_PATH}" in workdir:
                errors.append(
                    f"profile {profile_name!r} workdir cannot use ${{WORK_DIR_PATH}}; "
                    f"use ${{MAP_DIR_PATH}} or another Workdir-scope variable"
                )
            errors.extend(
                validate_variables(
                    f"profile {profile_name!r} workdir", workdir, WORKDIR_ALLOWED_VARIABLES
                )
            )
        tasks = profile_value.get("tasks")
        if not isinstance(tasks, list) or not tasks:
            errors.append(f"profile {profile_name!r} must declare a non-empty tasks array")
            continue
        for task_index, task_value in enumerate(tasks):
            if not isinstance(task_value, dict):
                errors.append(f"profile {profile_name!r} tasks[{task_index}] must be an object")
                continue
            if task_value.get("type") != "tool":
                continue
            task_variables = TOOL_ALLOWED_VARIABLES | declared_tools
            for label, value in iter_string_fields(
                task_value, f"profile {profile_name!r} tasks[{task_index}]"
            ):
                errors.extend(validate_variables(label, value, task_variables))
            tool_ref = task_value.get("tool")
            if not isinstance(tool_ref, str) or not tool_ref:
                errors.append(f"profile {profile_name!r} tasks[{task_index}] missing tool reference")
                continue
            if " " in tool_ref:
                errors.append(
                    f"profile {profile_name!r} tasks[{task_index}] tool reference contains spaces: "
                    f"{tool_ref!r}"
                )
            match = TOOL_REF_RE.fullmatch(tool_ref)
            if match is None:
                errors.append(
                    f"profile {profile_name!r} tasks[{task_index}] tool must use ${{TOOL_NAME}} "
                    f"variable reference: {tool_ref!r}"
                )
                continue
            tool_name = match.group(1)
            if tool_name not in declared_tools:
                errors.append(
                    f"profile {profile_name!r} tasks[{task_index}] references undeclared tool "
                    f"{tool_name!r}"
                )

    return errors


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Lint Stellar TrenchBroom compilation tool/profile schema."
    )
    default_package = Path(__file__).resolve().parent / "Stellar"
    parser.add_argument(
        "--game-config",
        type=Path,
        default=default_package / "GameConfig.cfg",
        help="Path to Stellar GameConfig.cfg.",
    )
    parser.add_argument(
        "--profiles",
        type=Path,
        default=default_package / "CompilationProfiles.cfg",
        help="Path to Stellar CompilationProfiles.cfg.",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    errors = lint(args.game_config, args.profiles)
    if errors:
        for error in errors:
            print(f"lint_stellar_compilation_profiles.py: {error}", file=sys.stderr)
        return 1
    print("Stellar TrenchBroom compilation profiles lint passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
