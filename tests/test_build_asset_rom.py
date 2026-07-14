#!/usr/bin/env python3
"""Regression checks for the raw asset ROM tile encoding."""

from __future__ import annotations

import sys
from pathlib import Path


REPOSITORY = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(REPOSITORY / "tools"))

from build_asset_rom import (  # noqa: E402
    CUSTOM_TILE_ROWS,
    FONT_TILE_COUNT,
    TILE_SIZE,
    build_tile_data,
    encode_font_tile,
)


def main() -> int:
    if len(sys.argv) != 2:
        raise SystemExit("usage: test_build_asset_rom.py FONT_DATA_HPP")

    # font8x8_basic bit 0 is leftmost. Each successive row moves that pixel
    # right, so the VDP nibbles must be 10, 01, 10, 01 across the four bytes.
    diagonal = encode_font_tile((0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80))
    expected = bytes.fromhex(
        "10 00 00 00 01 00 00 00 00 10 00 00 00 01 00 00 "
        "00 00 10 00 00 00 01 00 00 00 00 10 00 00 00 01"
    )
    assert diagonal == expected

    tile_data = build_tile_data(Path(sys.argv[1]))
    assert len(tile_data) == (FONT_TILE_COUNT + len(CUSTOM_TILE_ROWS)) * TILE_SIZE

    # Colored rows already contain left-to-right VDP nibbles and must remain
    # byte-for-byte big-endian rather than being horizontally reversed.
    first_custom = tile_data[FONT_TILE_COUNT * TILE_SIZE : (FONT_TILE_COUNT + 1) * TILE_SIZE]
    expected_custom = b"".join(row.to_bytes(4, "big") for row in CUSTOM_TILE_ROWS[0])
    assert first_custom == expected_custom
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
