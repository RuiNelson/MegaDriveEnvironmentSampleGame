"""VDP tile encoding shared by the asset pack builder."""

from __future__ import annotations

import re
from pathlib import Path

TILE_SIZE = 32
FONT_TILE_COUNT = 95

# VDP sprite tiles are ordered by column: top-left, bottom-left,
# top-right, bottom-right.
CUSTOM_TILE_ROWS = (
    (0x00011111, 0x00112222, 0x01122222, 0x01222222, 0x11222222, 0x12222222, 0x12221111, 0x12211111),
    (0x12211111, 0x12222222, 0x01222222, 0x01122222, 0x00112222, 0x00011222, 0x00001111, 0x00000110),
    (0x11111000, 0x22221100, 0x22222110, 0x22222110, 0x22222211, 0x22222221, 0x11112221, 0x11111221),
    (0x11111221, 0x22222221, 0x22222210, 0x22222110, 0x22221100, 0x22211000, 0x11110000, 0x01100000),
    (0x00011000, 0x00122100, 0x01233210, 0x12333321, 0x01233210, 0x00122100, 0x00011000, 0x00000000),
    (0x11111111, 0x10000001, 0x10011001, 0x10100101, 0x10100101, 0x10011001, 0x10000001, 0x11111111),
)


def parse_font(font_data_path: Path) -> list[tuple[int, ...]]:
    """Read the 95 eight-byte glyph rows from MegaDriveEnvironment's font."""
    glyphs: list[tuple[int, ...]] = []
    row_pattern = re.compile(r"^\s*\{([^{}]+)\}")
    byte_pattern = re.compile(r"0x([0-9A-Fa-f]{1,2})")

    for line in font_data_path.read_text(encoding="utf-8").splitlines():
        match = row_pattern.match(line)
        if not match:
            continue
        values = tuple(int(value, 16) for value in byte_pattern.findall(match.group(1)))
        if len(values) == 8:
            glyphs.append(values)

    if len(glyphs) != FONT_TILE_COUNT:
        raise ValueError(
            f"expected {FONT_TILE_COUNT} glyphs in {font_data_path}, found {len(glyphs)}"
        )
    return glyphs


def encode_font_tile(glyph: tuple[int, ...]) -> bytes:
    """Convert one monochrome 8x8 glyph to a Mega Drive 4-bpp tile."""
    tile = bytearray()
    for row in glyph:
        # font8x8_basic stores the leftmost pixel in bit 0. The VDP stores the
        # left pixel in the high nibble and the adjacent right pixel in the low
        # nibble, matching Font::fontCharToVDPTile in MegaDriveEnvironment.
        for pixel in range(0, 8, 2):
            left = (row >> pixel) & 1
            right = (row >> (pixel + 1)) & 1
            tile.append((left << 4) | right)
    if len(tile) != TILE_SIZE:
        raise AssertionError("font tile encoding did not produce 32 bytes")
    return bytes(tile)


def encode_color_tile(rows: tuple[int, ...]) -> bytes:
    """Encode eight rows containing one hexadecimal palette index per pixel."""
    if len(rows) != 8:
        raise ValueError("a tile must contain exactly eight rows")
    return b"".join(row.to_bytes(4, byteorder="big") for row in rows)


def build_tile_data(font_data_path: Path) -> bytes:
    font_tiles = (encode_font_tile(glyph) for glyph in parse_font(font_data_path))
    custom_tiles = (encode_color_tile(rows) for rows in CUSTOM_TILE_ROWS)
    tile_data = b"".join((*font_tiles, *custom_tiles))

    expected_tiles = FONT_TILE_COUNT + len(CUSTOM_TILE_ROWS)
    if len(tile_data) != expected_tiles * TILE_SIZE:
        raise AssertionError("unexpected tile data size")
    return tile_data
