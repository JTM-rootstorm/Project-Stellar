#!/usr/bin/env python3
"""Check generated/checkable BSP and TrenchBroom documentation snippets."""

from __future__ import annotations

import json
import pathlib
import re
import sys
from typing import Any


REPO_ROOT = pathlib.Path(__file__).resolve().parents[2]
DEVELOPER_TEXTURES = REPO_ROOT / "tools/bsp/developer_textures.json"
FIXTURES = REPO_ROOT / "tests/fixtures/trenchbroom/fixtures.json"

MATERIAL_BEGIN = "<!-- STELLAR_DEVELOPER_MATERIAL_TABLE_BEGIN -->"
MATERIAL_END = "<!-- STELLAR_DEVELOPER_MATERIAL_TABLE_END -->"
FIXTURE_TABLE_BEGIN = "<!-- STELLAR_TRENCHBROOM_FIXTURE_TABLE_BEGIN -->"
FIXTURE_TABLE_END = "<!-- STELLAR_TRENCHBROOM_FIXTURE_TABLE_END -->"
FIXTURE_SUMMARY_BEGIN = "<!-- STELLAR_TRENCHBROOM_FIXTURE_SUMMARY_BEGIN -->"
FIXTURE_SUMMARY_END = "<!-- STELLAR_TRENCHBROOM_FIXTURE_SUMMARY_END -->"

ACTIVE_STALE_CHECK_PATHS = [
    REPO_ROOT / "docs/TrenchBroom.md",
    REPO_ROOT / "docs/BspAuthoring.md",
    REPO_ROOT / "docs/Design.md",
    REPO_ROOT / "docs/ImplementationStatus.md",
    REPO_ROOT / "Plans/NEXT.md",
    REPO_ROOT / "tests/fixtures/trenchbroom/README.md",
]


def load_json(path: pathlib.Path) -> dict[str, Any]:
    return json.loads(path.read_text(encoding="utf-8"))


def material_table(data: dict[str, Any]) -> str:
    lines = [
        "| Runtime material | Source alias | Compiler alias | Scale cue |",
        "| --- | --- | --- | --- |",
    ]
    for entry in data["textures"]:
        lines.append(
            "| `{}` | `{}` | `{}` | {} |".format(
                entry["canonical"],
                entry["source_alias"],
                entry["compiler_alias"],
                entry.get("intended_scale_cue", ""),
            )
        )
    return "\n".join(lines) + "\n"


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
    positives = ", ".join(
        f"`{fixture['name']}`" for fixture in data["fixtures"] if fixture["kind"] == "positive"
    )
    negatives = ", ".join(
        f"`{fixture['name']}`" for fixture in data["fixtures"] if fixture["kind"] == "negative"
    )
    return (
        "The complete positive source fixture matrix is: "
        f"{positives}.\n\n"
        "Negative source fixtures are: "
        f"{negatives}.\n"
    )


def replace_block(text: str, begin: str, end: str, replacement: str) -> str:
    pattern = re.compile(re.escape(begin) + r".*?" + re.escape(end), re.DOTALL)
    if not pattern.search(text):
        raise ValueError(f"missing generated block markers: {begin} / {end}")
    block = f"{begin}\n{replacement.rstrip()}\n{end}"
    return pattern.sub(block, text, count=1)


def check_block(path: pathlib.Path, begin: str, end: str, generated: str) -> list[str]:
    text = path.read_text(encoding="utf-8")
    try:
        expected = replace_block(text, begin, end, generated)
    except ValueError as exc:
        return [f"{path.relative_to(REPO_ROOT)}: {exc}"]
    if text != expected:
        return [f"{path.relative_to(REPO_ROOT)}: generated block is out of sync"]
    return []


def check_stale_branch_name() -> list[str]:
    failures: list[str] = []
    for path in ACTIVE_STALE_CHECK_PATHS:
        text = path.read_text(encoding="utf-8")
        for line_number, line in enumerate(text.splitlines(), start=1):
            if "trenchbroom-compat" in line:
                failures.append(
                    f"{path.relative_to(REPO_ROOT)}:{line_number}: stale branch name "
                    "`trenchbroom-compat` in active docs"
                )
    return failures


def main() -> int:
    material_data = load_json(DEVELOPER_TEXTURES)
    fixture_data = load_json(FIXTURES)

    failures: list[str] = []
    failures.extend(check_stale_branch_name())

    generated_materials = material_table(material_data)
    failures.extend(
        check_block(
            REPO_ROOT / "docs/TrenchBroom.md",
            MATERIAL_BEGIN,
            MATERIAL_END,
            generated_materials,
        )
    )
    failures.extend(
        check_block(
            REPO_ROOT / "docs/BspAuthoring.md",
            MATERIAL_BEGIN,
            MATERIAL_END,
            generated_materials,
        )
    )

    failures.extend(
        check_block(
            REPO_ROOT / "tests/fixtures/trenchbroom/README.md",
            FIXTURE_TABLE_BEGIN,
            FIXTURE_TABLE_END,
            fixture_table(fixture_data),
        )
    )
    failures.extend(
        check_block(
            REPO_ROOT / "docs/TrenchBroom.md",
            FIXTURE_SUMMARY_BEGIN,
            FIXTURE_SUMMARY_END,
            fixture_summary(fixture_data),
        )
    )

    if failures:
        for failure in failures:
            print(f"check_docs_consistency.py: {failure}", file=sys.stderr)
        return 1

    print("Verified active BSP/TrenchBroom docs consistency.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
