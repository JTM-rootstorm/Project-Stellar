#!/usr/bin/env python3
"""Regression tests for VHLT map rewrite helpers."""

from __future__ import annotations

import struct
import subprocess
import sys
import tempfile
from pathlib import Path


def run_tool(repo: Path, *args: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [sys.executable, str(repo / "tools/bsp/map_rewrite.py"), *args],
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )


def require_success(result: subprocess.CompletedProcess[str]) -> None:
    if result.returncode != 0:
        raise AssertionError(result.stderr)


def assert_contains(text: str, needle: str) -> None:
    if needle not in text:
        raise AssertionError(f"missing {needle!r} in:\n{text}")


def main() -> int:
    repo = Path(__file__).resolve().parents[2]
    with tempfile.TemporaryDirectory() as tmp:
        tmp_path = Path(tmp)
        map_path = tmp_path / "rewrite.map"
        map_path.write_text(
            '''{
"classname" "worldspawn"
"wad" "old.wad"
}
{
"classname" "func_wall"
( 0 0 0 ) ( 64 0 0 ) ( 64 64 0 ) dev/grid_12 [ 1 0 0 0 ] [ 0 1 0 0 ] 45 0.5 0.25 // keep
( 0 0 0 ) ( 0 64 0 ) ( 0 64 64 ) stellar_dev_grid_16 0 0 0 1 1
}
''',
            encoding="utf-8",
        )

        require_success(run_tool(repo, "inject-mapversion-key", str(map_path)))
        require_success(run_tool(repo, "inject-wad-key", str(map_path), str(tmp_path / "stellar_dev.wad")))
        require_success(run_tool(repo, "inject-worldspawn-key", str(map_path), "_stellar_lighting_mode", "baked"))
        require_success(run_tool(repo, "rewrite-vhlt-texture-names-only", str(map_path)))
        require_success(run_tool(repo, "rewrite-valve220-to-classic-texture-axes", str(map_path)))

        rewritten = map_path.read_text(encoding="utf-8")
        assert_contains(rewritten, '"mapversion" "220"')
        assert_contains(rewritten, f'"wad" "{tmp_path / "stellar_dev.wad"}"')
        assert_contains(rewritten, '"_stellar_lighting_mode" "baked"')
        assert_contains(rewritten, "dev_grid_12 0 0 45 0.5 0.25 // keep")
        assert_contains(rewritten, "stellar_dev_grid_16 0 0 0 1 1")

        missing_worldspawn = tmp_path / "missing_worldspawn.map"
        missing_worldspawn.write_text('''{
"classname" "info_player_start"
}
''', encoding="utf-8")
        failed = run_tool(repo, "inject-mapversion-key", str(missing_worldspawn))
        if failed.returncode == 0 or "worldspawn entity not found" not in failed.stderr:
            raise AssertionError(f"unexpected missing worldspawn result: {failed}")

        bsp_path = tmp_path / "entity_lump.bsp"
        entity = b'{"classname" "worldspawn"}\x00\x00\x00'
        header = struct.pack("<iii", 30, 12, len(entity))
        bsp_path.write_bytes(header + entity + b"tail")
        require_success(run_tool(repo, "normalize-bsp-entity-lump", str(bsp_path)))
        normalized = bsp_path.read_bytes()
        version, offset, length = struct.unpack_from("<iii", normalized, 0)
        if (version, offset, length) != (30, 12, len(entity) - 3):
            raise AssertionError(f"unexpected BSP header: {(version, offset, length)}")

        invalid_bsp = tmp_path / "invalid.bsp"
        invalid_bsp.write_bytes(struct.pack("<iii", 29, 12, 0))
        invalid = run_tool(repo, "normalize-bsp-entity-lump", str(invalid_bsp))
        if invalid.returncode == 0 or "expected BSP30" not in invalid.stderr:
            raise AssertionError(f"unexpected invalid BSP result: {invalid}")

    print("VHLT map rewrite regression passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
