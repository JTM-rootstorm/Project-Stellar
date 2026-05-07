#!/usr/bin/env python3
"""Compare Stellar readback histogram JSON reports."""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path


CHANNELS = ("r", "g", "b", "a")


def load_report(path: Path) -> dict:
    with path.open("r", encoding="utf-8") as handle:
        return json.load(handle)


def channel_total(report: dict, channel: str) -> int:
    return int(sum(report["histogram"][channel]["bins"]))


def compare_channel(actual: dict, reference: dict, channel: str, tolerance: float) -> list[str]:
    actual_bins = actual["histogram"][channel]["bins"]
    reference_bins = reference["histogram"][channel]["bins"]
    if len(actual_bins) != 256 or len(reference_bins) != 256:
        return [f"{channel}: expected 256 histogram bins"]

    actual_total = channel_total(actual, channel)
    reference_total = channel_total(reference, channel)
    if actual_total <= 0 or reference_total <= 0:
        return [f"{channel}: empty histogram"]

    failures: list[str] = []
    for value, (actual_count, reference_count) in enumerate(
        zip(actual_bins, reference_bins, strict=True)
    ):
        actual_ratio = float(actual_count) / float(actual_total)
        reference_ratio = float(reference_count) / float(reference_total)
        delta = abs(actual_ratio - reference_ratio)
        if delta > tolerance:
            failures.append(
                f"{channel}[{value}]: delta={delta:.6f} "
                f"actual={actual_ratio:.6f} reference={reference_ratio:.6f}"
            )
    return failures


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("actual", type=Path)
    parser.add_argument("reference", type=Path)
    parser.add_argument("--tolerance", type=float, default=0.02)
    args = parser.parse_args()

    actual = load_report(args.actual)
    reference = load_report(args.reference)
    failures: list[str] = []
    for channel in CHANNELS:
        failures.extend(compare_channel(actual, reference, channel, args.tolerance))

    if failures:
        print("Readback comparison failed:", file=sys.stderr)
        for failure in failures[:40]:
            print(f"  {failure}", file=sys.stderr)
        if len(failures) > 40:
            print(f"  ... {len(failures) - 40} more differences", file=sys.stderr)
        return 1

    print(
        f"Readback comparison passed for {args.actual} "
        f"against {args.reference} (tolerance={args.tolerance})"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
