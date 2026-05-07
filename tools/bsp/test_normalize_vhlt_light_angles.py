#!/usr/bin/env python3
"""Regression tests for VHLT light angle normalization."""

from __future__ import annotations

import subprocess
import sys
import tempfile
from pathlib import Path


def run_normalizer(repo: Path, map_path: Path, *args: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [sys.executable, str(repo / "tools/bsp/normalize_vhlt_light_angles.py"), str(map_path), *args],
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )


def assert_contains(text: str, needle: str) -> None:
    if needle not in text:
        raise AssertionError(f"missing {needle!r} in:\n{text}")


def assert_not_contains(text: str, needle: str) -> None:
    if needle in text:
        raise AssertionError(f"unexpected {needle!r} in:\n{text}")


def main() -> int:
    repo = Path(__file__).resolve().parents[2]
    source = '''{
"classname" "worldspawn"
}
{
"classname" "light_spot"
"angles" "270 90 15"
}
{
"classname" "light_spot"
"pitch" "90"
"angle" "45"
"target" "spot_target"
}
{
"classname" "info_null"
"targetname" "spot_target"
}
{
"classname" "light_environment"
}
{
"classname" "info_player_start"
"angles" "270 180 30"
}
{
"classname" "light"
"pitch" "123"
"angle" "234"
}
'''
    with tempfile.TemporaryDirectory() as tmp:
        map_path = Path(tmp) / "lights.map"
        map_path.write_text(source, encoding="utf-8")

        dry = run_normalizer(repo, map_path, "--dry-run", "--json")
        if dry.returncode != 0:
            raise AssertionError(dry.stderr)
        if map_path.read_text(encoding="utf-8") != source:
            raise AssertionError("--dry-run modified the map")

        first = run_normalizer(repo, map_path)
        if first.returncode != 0:
            raise AssertionError(first.stderr)
        if "targeted light_spot" not in first.stderr:
            raise AssertionError(f"expected targeted light_spot warning, got: {first.stderr}")
        rewritten = map_path.read_text(encoding="utf-8")

        assert_contains(rewritten, '"pitch" "90"\n"angle" "90"\n"angles" "90 90 0"')
        assert_contains(rewritten, '"pitch" "-90"\n"angle" "45"\n"angles" "-90 45 0"')
        if rewritten.count('"_cone" "30"') != 2:
            raise AssertionError(f"expected two injected _cone defaults in:\n{rewritten}")
        if rewritten.count('"_cone2" "45"') != 2:
            raise AssertionError(f"expected two injected _cone2 defaults in:\n{rewritten}")
        assert_contains(rewritten, '"target" "spot_target"')
        assert_contains(rewritten, '"classname" "light_environment"\n"pitch" "-45"\n"angle" "0"\n"angles" "-45 0 0"')
        assert_contains(rewritten, '"_stellar_vhlt_angles_normalized" "1"')
        assert_contains(rewritten, '"classname" "info_player_start"\n"angles" "270 180 30"')
        assert_contains(rewritten, '"classname" "light"\n"pitch" "123"\n"angle" "234"')

        second = run_normalizer(repo, map_path)
        if second.returncode != 0:
            raise AssertionError(second.stderr)
        if map_path.read_text(encoding="utf-8") != rewritten:
            raise AssertionError("normalizer is not idempotent with marker")

        no_marker_path = Path(tmp) / "no_marker.map"
        no_marker_path.write_text('''{
"classname" "light_environment"
"pitch" "45"
"angle" "-90"
}
''', encoding="utf-8")
        no_marker = run_normalizer(repo, no_marker_path, "--no-marker")
        if no_marker.returncode != 0:
            raise AssertionError(no_marker.stderr)
        no_marker_text = no_marker_path.read_text(encoding="utf-8")
        assert_contains(no_marker_text, '"angle" "270"')
        assert_contains(no_marker_text, '"angles" "-45 270 0"')
        assert_not_contains(no_marker_text, "_stellar_vhlt_angles_normalized")

        invalid_path = Path(tmp) / "invalid.map"
        invalid_path.write_text('''{
"classname" "light_spot"
"angles" "90 not-a-number 0"
}
''', encoding="utf-8")
        invalid = run_normalizer(repo, invalid_path)
        if invalid.returncode == 0:
            raise AssertionError("invalid angles unexpectedly passed")
        if "angles yaw" not in invalid.stderr:
            raise AssertionError(f"invalid diagnostic did not identify field: {invalid.stderr}")

    print("VHLT light angle normalization regression passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
