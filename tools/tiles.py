"""VDP tile encoding shared by the asset pack builder."""

from __future__ import annotations

import re
from pathlib import Path

TILE_SIZE = 32
FONT_TILE_COUNT = 95

# VDP sprite tiles are ordered by column: top-left, bottom-left,
# top-right, bottom-right.
CUSTOM_TILE_ROWS = (
    # Player, gem and floor tiles.
    (0x00011111, 0x00112222, 0x01122222, 0x01222222, 0x11222222, 0x12222222, 0x12221111, 0x12211111),
    (0x12211111, 0x12222222, 0x01222222, 0x01122222, 0x00112222, 0x00011222, 0x00001111, 0x00000110),
    (0x11111000, 0x22221100, 0x22222110, 0x22222110, 0x22222211, 0x22222221, 0x11112221, 0x11111221),
    (0x11111221, 0x22222221, 0x22222210, 0x22222110, 0x22221100, 0x22211000, 0x11110000, 0x01100000),
    (0x00011000, 0x00122100, 0x01233210, 0x12333321, 0x01233210, 0x00122100, 0x00011000, 0x00000000),
    (0x11111111, 0x10000001, 0x10011001, 0x10100101, 0x10100101, 0x10011001, 0x10000001, 0x11111111),
    # Menu ocean: five four-tile water/crest families derived from the layered
    # cyan, blue and white wave language in sea.png. The image remains a visual
    # reference only; these compact 8x8 patterns are authored for the VDP.
    (0x55555555, 0x55556655, 0x55677665, 0x67777776, 0x66666666, 0x55555555, 0x44555544, 0x44444444),
    (0x55555555, 0x66555555, 0x76655567, 0x77766677, 0x66666666, 0x55555555, 0x55444555, 0x44444444),
    (0x55555555, 0x55555566, 0x67556677, 0x77667777, 0x66666666, 0x55555555, 0x45554445, 0x44444444),
    (0x55555555, 0x55665555, 0x66776555, 0x77777667, 0x66666666, 0x55555555, 0x54445554, 0x44444444),
    (0x33333333, 0x33444433, 0x34555543, 0x45577554, 0x57788775, 0x78888887, 0x56677665, 0x45566554),
    (0x33333333, 0x44333334, 0x55433445, 0x77544557, 0x88775778, 0x88888788, 0x77665667, 0x66554456),
    (0x33333333, 0x33334443, 0x43345555, 0x55457755, 0x77578887, 0x88788888, 0x66567766, 0x54456654),
    (0x33333333, 0x44433333, 0x55554433, 0x75575445, 0x87788775, 0x78888888, 0x66776656, 0x45665445),
    (0x22222222, 0x22333322, 0x23444432, 0x34466443, 0x46677664, 0x67788776, 0x78888887, 0x45566554),
    (0x22222222, 0x33222223, 0x44322334, 0x66433446, 0x77644667, 0x88767788, 0x88887888, 0x66554456),
    (0x22222222, 0x22223332, 0x32234444, 0x44346644, 0x66467776, 0x77678888, 0x88788888, 0x54456654),
    (0x22222222, 0x33322222, 0x44443322, 0x64464334, 0x76677664, 0x87788776, 0x78888888, 0x45665445),
    (0x44444444, 0x44455544, 0x44566554, 0x45677664, 0x56666665, 0x45555554, 0x34444443, 0x33333333),
    (0x44444444, 0x55444445, 0x66544556, 0x77655667, 0x66666666, 0x55544555, 0x44433344, 0x33333333),
    (0x44444444, 0x44444555, 0x45445666, 0x66456776, 0x56666665, 0x45555554, 0x34444334, 0x33333333),
    (0x44444444, 0x55544444, 0x66655444, 0x77666556, 0x66666666, 0x55555545, 0x44333444, 0x33333333),
    (0x22222222, 0x22233322, 0x22344332, 0x23455443, 0x34566554, 0x45666665, 0x34455443, 0x23344332),
    (0x22222222, 0x33222223, 0x44322334, 0x55433445, 0x66544556, 0x66655666, 0x55433445, 0x44322334),
    (0x22222222, 0x22222333, 0x23223444, 0x44324555, 0x55435666, 0x66546666, 0x34445543, 0x23334432),
    (0x22222222, 0x33322222, 0x44433222, 0x55544332, 0x66655443, 0x66666554, 0x45543344, 0x34432233),
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
