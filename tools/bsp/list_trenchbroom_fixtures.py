#!/usr/bin/env python3
"""List and validate Stellar TrenchBroom fixture manifest data."""

from __future__ import annotations

import argparse
import json
import pathlib
import re
import sys
from typing import Any

README_BEGIN = "<!-- STELLAR_TRENCHBROOM_FIXTURE_TABLE_BEGIN -->"
README_END = "<!-- STELLAR_TRENCHBROOM_FIXTURE_TABLE_END -->"
DOCS_BEGIN = "<!-- STELLAR_TRENCHBROOM_FIXTURE_SUMMARY_BEGIN -->"
DOCS_END = "<!-- STELLAR_TRENCHBROOM_FIXTURE_SUMMARY_END -->"


def fail(message: str) -> None:
    print(f"list_trenchbroom_fixtures.py: {message}", file=sys.stderr)
    raise SystemExit(1)


def load_manifest(path: pathlib.Path) -> dict[str, Any]:
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except OSError as exc:
        fail(f"could not read manifest {path}: {exc}")
    except json.JSONDecodeError as exc:
        fail(f"invalid JSON in {path}: {exc}")

    fixtures = data.get("fixtures")
    if not isinstance(fixtures, list):
        fail("manifest must contain a fixtures array")

    seen: set[str] = set()
    for fixture in fixtures:
        if not isinstance(fixture, dict):
            fail("fixture entries must be objects")
        name = fixture.get("name")
        kind = fixture.get("kind")
        source = fixture.get("source")
        if not isinstance(name, str) or not name:
            fail("fixture entry has missing name")
        if name in seen:
            fail(f"duplicate fixture name: {name}")
        seen.add(name)
        if kind not in {"positive", "negative"}:
            fail(f"fixture {name} has invalid kind: {kind}")
        if not isinstance(source, str) or not source.startswith("src/") or not source.endswith(".map"):
            fail(f"fixture {name} has invalid source path: {source}")
        if kind == "negative":
            if not isinstance(fixture.get("preflight_expected_regex"), str):
                fail(f"negative fixture {name} is missing preflight_expected_regex")
            if not isinstance(fixture.get("strict_textures"), bool):
                fail(f"negative fixture {name} is missing strict_textures boolean")
    return data


def fixtures_by_kind(data: dict[str, Any], kind: str) -> list[dict[str, Any]]:
    return [fixture for fixture in data["fixtures"] if fixture["kind"] == kind]


def shell_quote(value: str) -> str:
    return "'" + value.replace("'", "'\\''") + "'"


def emit_names(fixtures: list[dict[str, Any]]) -> str:
    return "\n".join(fixture["name"] for fixture in fixtures) + "\n"


def vhlt_matrix_fixtures(data: dict[str, Any], kind: str) -> list[dict[str, Any]]:
    return [
        fixture
        for fixture in fixtures_by_kind(data, kind)
        if fixture.get("vhlt_matrix", True)
    ]


def emit_shell_arrays(data: dict[str, Any]) -> str:
    positives = vhlt_matrix_fixtures(data, "positive")
    negatives = vhlt_matrix_fixtures(data, "negative")
    lines: list[str] = ["positive_fixtures=("]
    lines.extend(f"    {shell_quote(fixture['name'])}" for fixture in positives)
    lines.append(")")
    lines.append("negative_fixtures=(")
    lines.extend(f"    {shell_quote(fixture['name'])}" for fixture in negatives)
    lines.append(")")
    lines.append("declare -A negative_regex=(")
    for fixture in negatives:
        lines.append(
            f"    [{fixture['name']}]={shell_quote(fixture['preflight_expected_regex'])}"
        )
    lines.append(")")
    lines.append("declare -A negative_strict_textures=(")
    for fixture in negatives:
        if fixture.get("strict_textures"):
            lines.append(f"    [{fixture['name']}]=1")
    lines.append(")")
    lines.append("declare -A negative_compile_after_preflight_failure=(")
    for fixture in negatives:
        if fixture.get("compile_after_preflight_failure"):
            lines.append(f"    [{fixture['name']}]=1")
    lines.append(")")
    return "\n".join(lines) + "\n"


def emit_cmake_fragments(data: dict[str, Any]) -> str:
    positives = ";".join(fixture["name"] for fixture in fixtures_by_kind(data, "positive"))
    negatives = ";".join(fixture["name"] for fixture in fixtures_by_kind(data, "negative"))
    all_names = ";".join(fixture["name"] for fixture in data["fixtures"])
    return (
        f'set(STELLAR_TRENCHBROOM_POSITIVE_FIXTURES "{positives}")\n'
        f'set(STELLAR_TRENCHBROOM_NEGATIVE_FIXTURES "{negatives}")\n'
        f'set(STELLAR_TRENCHBROOM_ALL_FIXTURES "{all_names}")\n'
    )


def fixture_table(data: dict[str, Any]) -> str:
    lines = [
        "| Fixture | Source | Generated/compiled BSP | Expected BSP version | Expected outcome |",
        "| --- | --- | --- | --- | --- |",
    ]
    for fixture in data["fixtures"]:
        bsp = fixture.get("generated_bsp") or fixture.get("vhlt_bsp") or "none"
        version = fixture.get("expected_bsp_version")
        lines.append(
            "| "
            f"`{fixture['name']}` | `{fixture['source']}` | `{bsp}` | "
            f"{version if version is not None else 'n/a'} | {fixture['expected_outcome']} |"
        )
    return "\n".join(lines) + "\n"


def fixture_summary(data: dict[str, Any]) -> str:
    positives = ", ".join(f"`{fixture['name']}`" for fixture in fixtures_by_kind(data, "positive"))
    negatives = ", ".join(f"`{fixture['name']}`" for fixture in fixtures_by_kind(data, "negative"))
    return (
        "The complete positive source fixture matrix is: "
        f"{positives}.\n\n"
        "Negative source fixtures are: "
        f"{negatives}.\n"
    )


def replace_block(text: str, begin: str, end: str, replacement: str) -> str:
    pattern = re.compile(re.escape(begin) + r".*?" + re.escape(end), re.DOTALL)
    block = f"{begin}\n{replacement.rstrip()}\n{end}"
    if not pattern.search(text):
        fail(f"missing generated block markers: {begin} / {end}")
    return pattern.sub(block, text, count=1)


def check_block(path: pathlib.Path, begin: str, end: str, generated: str) -> bool:
    try:
        original = path.read_text(encoding="utf-8")
    except OSError as exc:
        fail(f"could not read {path}: {exc}")
    expected = replace_block(original, begin, end, generated)
    if original == expected:
        return True
    print(f"Generated fixture content is out of sync: {path}", file=sys.stderr)
    return False


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("manifest", type=pathlib.Path)
    modes = parser.add_mutually_exclusive_group(required=True)
    modes.add_argument("--positive-names", action="store_true")
    modes.add_argument("--negative-names", action="store_true")
    modes.add_argument("--shell-arrays", action="store_true")
    modes.add_argument("--json", action="store_true")
    modes.add_argument("--cmake-fragments", action="store_true")
    modes.add_argument("--markdown-table", action="store_true")
    modes.add_argument("--markdown-summary", action="store_true")
    modes.add_argument("--check-docs", nargs="*", metavar="PATH")
    args = parser.parse_args()

    data = load_manifest(args.manifest)

    if args.positive_names:
        sys.stdout.write(emit_names(fixtures_by_kind(data, "positive")))
    elif args.negative_names:
        sys.stdout.write(emit_names(fixtures_by_kind(data, "negative")))
    elif args.shell_arrays:
        sys.stdout.write(emit_shell_arrays(data))
    elif args.json:
        sys.stdout.write(json.dumps(data, indent=2, sort_keys=True) + "\n")
    elif args.cmake_fragments:
        sys.stdout.write(emit_cmake_fragments(data))
    elif args.markdown_table:
        sys.stdout.write(fixture_table(data))
    elif args.markdown_summary:
        sys.stdout.write(fixture_summary(data))
    elif args.check_docs is not None:
        paths = [pathlib.Path(path) for path in args.check_docs]
        if not paths:
            root = args.manifest.resolve().parents[3]
            paths = [
                root / "tests/fixtures/trenchbroom/README.md",
                root / "docs/TrenchBroom.md",
            ]
        ok = True
        for path in paths:
            if path.name == "README.md":
                ok = check_block(path, README_BEGIN, README_END, fixture_table(data)) and ok
            elif path.name == "TrenchBroom.md":
                ok = check_block(path, DOCS_BEGIN, DOCS_END, fixture_summary(data)) and ok
            else:
                fail(f"unsupported docs check path: {path}")
        return 0 if ok else 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
