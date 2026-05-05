#!/usr/bin/env python3
"""Generate deterministic retro placeholder footstep WAV one-shots."""

from __future__ import annotations

import argparse
import math
import struct
import wave
from pathlib import Path

SAMPLE_RATE = 22050
VARIANT_COUNT = 2

SURFACES = {
    "generic": {"duration": 0.10, "base": 95.0, "noise": 0.36, "tone": 0.14},
    "concrete": {"duration": 0.09, "base": 120.0, "noise": 0.30, "tone": 0.18},
    "metal": {"duration": 0.16, "base": 520.0, "noise": 0.18, "tone": 0.38},
    "wood": {"duration": 0.12, "base": 190.0, "noise": 0.20, "tone": 0.30},
    "stone": {"duration": 0.08, "base": 155.0, "noise": 0.28, "tone": 0.22},
    "dirt": {"duration": 0.13, "base": 75.0, "noise": 0.48, "tone": 0.07},
    "grass": {"duration": 0.14, "base": 65.0, "noise": 0.42, "tone": 0.05},
    "water": {"duration": 0.15, "base": 105.0, "noise": 0.34, "tone": 0.16},
}


def lcg(seed: int) -> int:
    return (seed * 1664525 + 1013904223) & 0xFFFFFFFF


def sample_noise(seed: int) -> tuple[int, float]:
    seed = lcg(seed)
    value = ((seed >> 8) & 0xFFFF) / 32767.5 - 1.0
    return seed, value


def surface_seed(surface: str, variant: int) -> int:
    seed = 2166136261
    for byte in f"{surface}:{variant}".encode("ascii"):
        seed ^= byte
        seed = (seed * 16777619) & 0xFFFFFFFF
    return seed


def synthesize(surface: str, variant: int) -> bytes:
    spec = SURFACES[surface]
    sample_count = int(SAMPLE_RATE * spec["duration"])
    seed = surface_seed(surface, variant)
    frames = bytearray()
    freq = spec["base"] * (1.0 + 0.08 * variant)
    ring_freq = freq * (2.4 if surface == "metal" else 1.6)

    for index in range(sample_count):
        t = index / SAMPLE_RATE
        phase = index / max(1, sample_count - 1)
        attack = min(1.0, phase / 0.08)
        decay = math.exp(-phase * (7.0 if surface != "metal" else 4.5))
        envelope = attack * decay
        seed, noise = sample_noise(seed)
        tone = math.sin(2.0 * math.pi * freq * t)
        ring = math.sin(2.0 * math.pi * ring_freq * t) * (0.35 if surface == "metal" else 0.12)
        splash = math.sin(2.0 * math.pi * (freq * (1.0 + phase * 1.8)) * t)
        if surface == "water":
            tone = 0.45 * tone + 0.55 * splash
        value = envelope * (spec["noise"] * noise + spec["tone"] * tone + ring)
        value = max(-0.95, min(0.95, value))
        frames.extend(struct.pack("<h", int(value * 32767)))
    return bytes(frames)


def write_wav(path: Path, pcm: bytes) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with wave.open(str(path), "wb") as wav:
        wav.setnchannels(1)
        wav.setsampwidth(2)
        wav.setframerate(SAMPLE_RATE)
        wav.writeframes(pcm)


def generate(out_dir: Path) -> None:
    for surface in SURFACES:
        for variant in range(VARIANT_COUNT):
            write_wav(out_dir / f"footstep_{surface}_{variant}.wav", synthesize(surface, variant))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", type=Path, required=True, help="Output directory for WAV files")
    args = parser.parse_args()
    generate(args.out)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
