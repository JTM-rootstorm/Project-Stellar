#!/usr/bin/env python3
"""Generate and verify Project Stellar developer textures as deterministic WAD3."""

from __future__ import annotations

import argparse
import binascii
import os
import struct
import sys
import zlib
from pathlib import Path

TEXTURES = (
    "dev/grid_12",
    "dev/grid_16",
    "dev/grid_32",
    "dev/grid_64",
    "dev/player_72",
    "dev/wall_96",
    "dev_grid_12",
    "dev_grid_16",
    "dev_grid_32",
    "dev_grid_64",
    "dev_player_72",
    "dev_wall_96",
)

WAD3_MIPTEX_TYPE = 0x43
PALETTE_SIZE = 256
TEXTURE_SIZE = 128

STANDARD_SAMPLE_COORDINATES = (
    (0, 0),
    (1, 1),
    (16, 16),
    (32, 32),
    (64, 64),
    (96, 96),
    (127, 127),
)

TEXTURE_SAMPLE_COORDINATES = {
    "dev/grid_64": ((64, 10),),
    "dev/wall_96": ((8, 48),),
}


def build_palette() -> bytes:
    """Return a fixed 256-color RGB palette."""
    colors: list[tuple[int, int, int]] = [
        (0, 0, 0),
        (32, 32, 36),
        (72, 72, 80),
        (112, 112, 124),
        (176, 176, 188),
        (224, 224, 232),
        (32, 64, 128),
        (48, 96, 192),
        (96, 144, 224),
        (160, 192, 240),
        (96, 64, 32),
        (144, 96, 48),
        (192, 144, 72),
        (224, 192, 112),
        (128, 48, 48),
        (208, 80, 72),
        (72, 84, 96),
        (88, 102, 116),
        (122, 148, 176),
        (255, 176, 48),
        (255, 224, 128),
        (246, 214, 168),
        (184, 116, 64),
        (112, 64, 40),
        (240, 64, 48),
        (255, 248, 220),
    ]
    while len(colors) < PALETTE_SIZE:
        i = len(colors)
        colors.append(((i * 37) & 0xFF, (i * 67) & 0xFF, (i * 97) & 0xFF))
    return b"".join(bytes(rgb) for rgb in colors)


def palette_color(index: int, palette: bytes) -> tuple[int, int, int, int]:
    """Return an RGBA color tuple for a palette index."""
    offset = index * 3
    return (palette[offset], palette[offset + 1], palette[offset + 2], 255)


def validate_texture_name(name: str) -> bytes:
    encoded = name.encode("ascii")
    if not encoded or len(encoded) > 15:
        raise ValueError(f"WAD3 miptex name must be 1..15 ASCII bytes: {name!r}")
    return encoded.ljust(16, b"\0")


def canonical_source_name(name: str) -> str:
    if name.startswith("stellar_dev_"):
        return "dev/" + name.removeprefix("stellar_dev_")
    if name.startswith("dev_"):
        return "dev/" + name.removeprefix("dev_")
    return name


def scale_nearest(base: list[int], base_width: int, width: int, height: int) -> bytes:
    base_height = len(base) // base_width
    out = bytearray(width * height)
    for y in range(height):
        src_y = min((y * base_height) // height, base_height - 1)
        for x in range(width):
            src_x = min((x * base_width) // width, base_width - 1)
            out[y * width + x] = base[src_y * base_width + src_x]
    return bytes(out)


def grid_pattern(spacing: int) -> list[int]:
    pixels: list[int] = []
    major = max(spacing, 1) * 4
    for y in range(TEXTURE_SIZE):
        for x in range(TEXTURE_SIZE):
            if spacing == 64:
                if x % 64 <= 1 or y % 64 <= 1:
                    pixels.append(20)
                elif x % 16 == 0 or y % 16 == 0:
                    pixels.append(18)
                elif ((x // 16) + (y // 16)) & 1:
                    pixels.append(17)
                else:
                    pixels.append(16)
                continue
            if x % major == 0 or y % major == 0:
                pixels.append(9)
            elif x % spacing == 0 or y % spacing == 0:
                pixels.append(7)
            elif ((x // spacing) + (y // spacing)) & 1:
                pixels.append(2)
            else:
                pixels.append(1)
    return pixels


def player_pattern() -> list[int]:
    pixels: list[int] = []
    center = TEXTURE_SIZE // 2
    half_width = 16
    half_height = 36
    for y in range(TEXTURE_SIZE):
        for x in range(TEXTURE_SIZE):
            grid = x % 16 == 0 or y % 16 == 0
            in_marker = abs(x - center) <= half_width and abs(y - center) <= half_height
            edge = in_marker and (abs(x - center) == half_width or abs(y - center) == half_height)
            if edge:
                pixels.append(15)
            elif in_marker:
                pixels.append(14)
            elif grid:
                pixels.append(6)
            else:
                pixels.append(1)
    return pixels


def wall_pattern() -> list[int]:
    """Return the semantic 96-inch wall-height developer reference.

    The WAD/PNG remains 128x128 for BSP30 compatibility. The top 96 pixels are
    the authored-inch reference that a default 96-inch wall samples at 1:1 scale;
    the remaining 32 pixels are a darker overflow band so accidental repeats are
    obvious in runtime and editor previews.
    """
    pixels: list[int] = []
    for y in range(TEXTURE_SIZE):
        for x in range(TEXTURE_SIZE):
            if y in (0, 95, 96):
                pixels.append(24)
            elif y < 96 and y % 12 == 0:
                pixels.append(20)
            elif y < 96 and x in (8, 9, 10, 64):
                pixels.append(25)
            elif y < 96 and (x % 32 == 0 or y % 32 == 0):
                pixels.append(22)
            elif y > 96 and (y % 8 == 0 or x % 16 == 0):
                pixels.append(23)
            elif y > 96:
                pixels.append(10)
            elif ((x // 16) + (y // 16)) & 1:
                pixels.append(21)
            else:
                pixels.append(22)
    return pixels


def texture_pixels(name: str) -> list[int]:
    normalized_name = canonical_source_name(name)
    if normalized_name == "dev/grid_12":
        return grid_pattern(12)
    if normalized_name == "dev/grid_16":
        return grid_pattern(16)
    if normalized_name == "dev/grid_32":
        return grid_pattern(32)
    if normalized_name == "dev/grid_64":
        return grid_pattern(64)
    if normalized_name == "dev/player_72":
        return player_pattern()
    if normalized_name == "dev/wall_96":
        return wall_pattern()
    raise ValueError(f"unhandled texture name: {name}")


def build_miptex(name: str, palette: bytes) -> bytes:
    width = TEXTURE_SIZE
    height = TEXTURE_SIZE
    base_pixels = texture_pixels(name)
    sizes = ((width, height), (width // 2, height // 2), (width // 4, height // 4), (width // 8, height // 8))
    header_size = 16 + 4 + 4 + 4 * 4
    mip_data = [scale_nearest(base_pixels, width, mip_width, mip_height) for mip_width, mip_height in sizes]
    offsets: list[int] = []
    cursor = header_size
    for mip in mip_data:
        offsets.append(cursor)
        cursor += len(mip)
    miptex = bytearray()
    miptex += validate_texture_name(name)
    miptex += struct.pack("<II4I", width, height, *offsets)
    for mip in mip_data:
        miptex += mip
    miptex += struct.pack("<H", PALETTE_SIZE)
    miptex += palette
    return bytes(miptex)


def build_wad() -> bytes:
    palette = build_palette()
    lumps = [(name, build_miptex(name, palette)) for name in TEXTURES]
    data = bytearray()
    directory: list[tuple[int, int, str]] = []
    data += struct.pack("<4sII", b"WAD3", len(lumps), 0)
    for name, lump in lumps:
        filepos = len(data)
        data += lump
        directory.append((filepos, len(lump), name))
    directory_offset = len(data)
    for filepos, size, name in directory:
        data += struct.pack("<IIIbbbb16s", filepos, size, size, WAD3_MIPTEX_TYPE, 0, 0, 0, validate_texture_name(name))
    struct.pack_into("<I", data, 8, directory_offset)
    return bytes(data)


def sample_coordinates(name: str) -> tuple[tuple[int, int], ...]:
    """Return deterministic sample coordinates for manifest color checks."""
    normalized_name = canonical_source_name(name)
    coordinates = list(STANDARD_SAMPLE_COORDINATES)
    specific_coordinates = TEXTURE_SAMPLE_COORDINATES.get(normalized_name, ())
    for coordinate in specific_coordinates:
        if coordinate not in coordinates:
            coordinates.append(coordinate)
    return tuple(coordinates)


def build_manifest(entries: list[tuple[str, int, int, int]]) -> str:
    """Return deterministic texture metadata and sample-color manifest text."""
    palette = build_palette()
    lines: list[str] = []
    for name, width, height, size in entries:
        lines.append(f"{name} {width}x{height} {size}")
        pixels = texture_pixels(name)
        for x, y in sample_coordinates(name):
            index = pixels[y * TEXTURE_SIZE + x]
            r, g, b, a = palette_color(index, palette)
            lines.append(f"sample {name} {x},{y} rgba={r},{g},{b},{a} palette={index}")
    return "\n".join(lines) + "\n"


def png_chunk(kind: bytes, payload: bytes) -> bytes:
    crc = binascii.crc32(kind)
    crc = binascii.crc32(payload, crc) & 0xFFFFFFFF
    return struct.pack(">I", len(payload)) + kind + payload + struct.pack(">I", crc)


def indexed_pixels_to_rgba_rows(pixels: list[int], palette: bytes) -> bytes:
    rows = bytearray()
    for y in range(TEXTURE_SIZE):
        rows.append(0)  # PNG filter: none.
        for x in range(TEXTURE_SIZE):
            index = pixels[y * TEXTURE_SIZE + x]
            offset = index * 3
            rows += palette[offset : offset + 3]
            rows.append(255)
    return bytes(rows)


def build_png(name: str) -> bytes:
    palette = build_palette()
    payload = indexed_pixels_to_rgba_rows(texture_pixels(name), palette)
    data = bytearray(b"\x89PNG\r\n\x1a\n")
    data += png_chunk(b"IHDR", struct.pack(">IIBBBBB", TEXTURE_SIZE, TEXTURE_SIZE, 8, 6, 0, 0, 0))
    data += png_chunk(b"IDAT", zlib.compress(payload, level=9))
    data += png_chunk(b"IEND", b"")
    return bytes(data)


def write_png_previews(root: Path) -> None:
    for name in TEXTURES:
        if name.startswith("dev/"):
            path = root / f"{name}.png"
            write_atomic(path, build_png(name))
        elif name.startswith("dev_"):
            canonical = canonical_source_name(name)
            path = root / f"stellar_dev_{name.removeprefix('dev_')}.png"
            write_atomic(path, build_png(canonical))


def verify_png_previews(root: Path) -> None:
    for name in TEXTURES:
        if name.startswith("dev/"):
            path = root / f"{name}.png"
            expected = build_png(name)
        elif name.startswith("dev_"):
            path = root / f"stellar_dev_{name.removeprefix('dev_')}.png"
            expected = build_png(canonical_source_name(name))
        else:
            continue
        if path.read_bytes() != expected:
            raise ValueError(f"PNG preview does not match deterministic generated output: {path}")


def inspect_wad(data: bytes) -> list[tuple[str, int, int, int]]:
    if len(data) < 12:
        raise ValueError("WAD is too small")
    magic, count, directory_offset = struct.unpack_from("<4sII", data, 0)
    if magic != b"WAD3":
        raise ValueError(f"expected WAD3 magic, found {magic!r}")
    if directory_offset + count * 32 > len(data):
        raise ValueError("WAD directory is outside file bounds")
    entries: list[tuple[str, int, int, int]] = []
    for i in range(count):
        filepos, disksize, size, lump_type, compression, _pad1, _pad2, raw_name = struct.unpack_from(
            "<IIIbbbb16s", data, directory_offset + i * 32
        )
        if lump_type != WAD3_MIPTEX_TYPE or compression != 0:
            raise ValueError(f"unsupported lump metadata at index {i}")
        if filepos + disksize > len(data) or disksize != size:
            raise ValueError(f"invalid lump bounds at index {i}")
        name = raw_name.split(b"\0", 1)[0].decode("ascii")
        tex_name, width, height = struct.unpack_from("<16sII", data, filepos)
        embedded_name = tex_name.split(b"\0", 1)[0].decode("ascii")
        if embedded_name != name:
            raise ValueError(f"directory name {name!r} disagrees with miptex {embedded_name!r}")
        palette_count_offset = filepos + disksize - (PALETTE_SIZE * 3 + 2)
        palette_count = struct.unpack_from("<H", data, palette_count_offset)[0]
        if palette_count != PALETTE_SIZE:
            raise ValueError(f"texture {name} has palette size {palette_count}")
        entries.append((name, width, height, disksize))
    return entries


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Generate or verify Project Stellar deterministic developer WAD3 textures.")
    parser.add_argument("--out", type=Path, help="Output WAD3 path.")
    parser.add_argument("--verify", type=Path, help="Verify an existing WAD3 against deterministic generated bytes.")
    parser.add_argument("--list", action="store_true", help="List deterministic developer texture names and dimensions.")
    parser.add_argument("--manifest", type=Path, help="Write a text manifest of generated WAD entries.")
    parser.add_argument("--png-source", type=Path, help="Accepted for workflow documentation; generation is procedural and deterministic.")
    parser.add_argument("--png-out", type=Path, help="Write deterministic TrenchBroom PNG previews under this materials directory.")
    parser.add_argument("--verify-png", type=Path, help="Verify deterministic TrenchBroom PNG previews under this materials directory.")
    return parser.parse_args(argv)


def write_atomic(path: Path, data: bytes) -> None:
    if path.exists() and path.is_dir():
        raise OSError(f"output path is a directory: {path}")
    path.parent.mkdir(parents=True, exist_ok=True)
    tmp_path = path.with_name(f".{path.name}.tmp")
    tmp_path.write_bytes(data)
    os.replace(tmp_path, path)


def main(argv: list[str]) -> int:
    try:
        args = parse_args(argv)
        generated = build_wad()
        entries = inspect_wad(generated)
        if args.png_source is not None and not args.png_source.exists():
            raise OSError(f"--png-source path does not exist: {args.png_source}")
        if args.list:
            for name, width, height, _size in entries:
                print(f"{name} {width}x{height}")
        if args.manifest is not None:
            write_atomic(args.manifest, build_manifest(entries).encode("utf-8"))
        if args.verify is not None:
            actual = args.verify.read_bytes()
            inspect_wad(actual)
            if actual != generated:
                raise ValueError(f"WAD does not match deterministic generated output: {args.verify}")
            print(f"Verified deterministic Stellar developer WAD: {args.verify}")
        if args.out is not None:
            write_atomic(args.out, generated)
        if args.png_out is not None:
            write_png_previews(args.png_out)
        if args.verify_png is not None:
            verify_png_previews(args.verify_png)
            print(f"Verified deterministic Stellar developer PNG previews: {args.verify_png}")
        if (
            args.out is None
            and args.verify is None
            and not args.list
            and args.manifest is None
            and args.png_out is None
            and args.verify_png is None
        ):
            raise ValueError("one of --out, --verify, --list, --manifest, --png-out, or --verify-png is required")
    except (OSError, ValueError, struct.error, UnicodeDecodeError) as exc:
        print(f"create_stellar_dev_wad.py: error: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
