#!/usr/bin/env python3
"""Synchronize the BSP compatibility FGD with the TrenchBroom package FGD."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

HEADER = """// Stellar BSP authoring helper.
//
// Generated/copied by tools/bsp/sync_stellar_fgd.py from the authoritative
// tools/trenchbroom/Stellar/stellar_entities.fgd editor contract.
// Keep this compatibility path for existing BSP tooling and user workflows.
"""


def repo_root() -> Path:
    return Path(__file__).resolve().parents[2]


def strip_leading_comment_preamble(text: str) -> str:
    lines = text.splitlines(keepends=True)
    for index, line in enumerate(lines):
        if line.startswith("// Policy:"):
            return "".join(lines[index:])
    index = 0
    while index < len(lines) and lines[index].startswith("//"):
        index += 1
    while index < len(lines) and lines[index].strip() == "":
        index += 1
    return "".join(lines[index:])


def generated_contents(authoritative_text: str) -> str:
    body = strip_leading_comment_preamble(authoritative_text)
    return HEADER + "//\n" + body


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--check", action="store_true", help="fail when the compatibility FGD is stale")
    mode.add_argument("--write", action="store_true", help="rewrite the compatibility FGD")
    root = repo_root()
    parser.add_argument(
        "--authoritative",
        type=Path,
        default=root / "tools/trenchbroom/Stellar/stellar_entities.fgd",
        help="authoritative TrenchBroom package FGD",
    )
    parser.add_argument(
        "--compatibility",
        type=Path,
        default=root / "tools/bsp/stellar_entities.fgd",
        help="BSP compatibility FGD copy",
    )
    args = parser.parse_args()

    expected = generated_contents(args.authoritative.read_text(encoding="utf-8"))
    if args.write:
        args.compatibility.write_text(expected, encoding="utf-8")
        return 0

    actual = args.compatibility.read_text(encoding="utf-8")
    if actual != expected:
        print(
            f"{args.compatibility} is out of sync with {args.authoritative}; "
            "run tools/bsp/sync_stellar_fgd.py --write",
            file=sys.stderr,
        )
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
