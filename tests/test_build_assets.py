#!/usr/bin/env python3
"""Regression checks for the multi-blob asset pack and Z80 assembly step."""

from __future__ import annotations

import json
import subprocess
import sys
import tempfile
from pathlib import Path


REPOSITORY = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(REPOSITORY / "tools"))

from assemble_z80 import assemble_z80  # noqa: E402
from asset_pack import AssetBlob, AssetLayout, pack_blobs  # noqa: E402
from boing_pcm import load_boing_pcm  # noqa: E402
from build_assets import build_asset_image, write_outputs  # noqa: E402
from tiles import (  # noqa: E402
    CUSTOM_TILE_ROWS,
    FONT_TILE_COUNT,
    TILE_SIZE,
    build_tile_data,
    encode_font_tile,
)


def main() -> int:
    if len(sys.argv) != 2:
        raise SystemExit("usage: test_build_assets.py FONT_DATA_HPP")

    font_data = Path(sys.argv[1])

    # font8x8_basic bit 0 is leftmost. Each successive row moves that pixel
    # right, so the VDP nibbles must be 10, 01, 10, 01 across the four bytes.
    diagonal = encode_font_tile((0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80))
    expected = bytes.fromhex(
        "10 00 00 00 01 00 00 00 00 10 00 00 00 01 00 00 "
        "00 00 10 00 00 00 01 00 00 00 00 10 00 00 00 01"
    )
    assert diagonal == expected

    tile_data = build_tile_data(font_data)
    assert len(tile_data) == (FONT_TILE_COUNT + len(CUSTOM_TILE_ROWS)) * TILE_SIZE

    z80_source = REPOSITORY / "z80" / "boing_ball_sfx.s"
    boing_samples = REPOSITORY / "assets" / "boing.samples"

    with tempfile.TemporaryDirectory() as temporary:
        work = Path(temporary)
        boing_pcm_path = work / "boing_pcm.bin"
        subprocess.run(
            [
                sys.executable,
                str(REPOSITORY / "tools" / "convert_boing_pcm.py"),
                "--input",
                str(boing_samples),
                "--output",
                str(boing_pcm_path),
                "--target-rate",
                "8000",
            ],
            check=True,
        )
        boing_pcm = load_boing_pcm(boing_pcm_path)
        assert len(boing_pcm) > 1000
        # Resampled 14 kHz → 8 kHz must shrink the Amiga 24706-byte source.
        assert len(boing_pcm) < 20000
        assert abs(sum(boing_pcm) / len(boing_pcm) - 128) < 4

        z80_binary = assemble_z80(z80_source, work / "driver.bin")
        assert len(z80_binary) > 32
        assert z80_binary[0] == 0xF3

        image, layout = pack_blobs(
            (
                AssetBlob("boing_pcm", boing_pcm),
                AssetBlob("z80_boing_ball_sfx", z80_binary),
                AssetBlob("tiles", tile_data),
            )
        )
        assert len(image) == layout.rom_size
        tiles = layout.blob("tiles")
        z80 = layout.blob("z80_boing_ball_sfx")
        pcm = layout.blob("boing_pcm")
        assert image[tiles.offset : tiles.offset + tiles.size] == tile_data
        assert image[z80.offset : z80.offset + z80.size] == z80_binary
        assert image[pcm.offset : pcm.offset + pcm.size] == boing_pcm
        assert tiles.offset + tiles.aligned_size == layout.rom_size
        assert z80.offset + z80.aligned_size == tiles.offset
        assert pcm.offset + pcm.aligned_size == z80.offset
        bank_base = pcm.offset & ~0x7FFF
        assert pcm.offset + pcm.size <= bank_base + 0x8000

        image2, layout2, z802 = build_asset_image(
            font_data_path=font_data,
            z80_source=z80_source,
            boing_pcm_path=boing_pcm_path,
            work_dir=work / "assets",
        )
        assert z802 == z80_binary
        assert layout2.blob("tiles").size == len(tile_data)
        assert layout2.blob("boing_pcm").size == len(boing_pcm)

        rom_path = work / "rom.bin"
        header_path = work / "AssetLayout.hpp"
        json_path = work / "asset_layout.json"
        pack_path = work / "pack.bin"
        write_outputs(
            image=image2,
            layout=layout2,
            output_rom=rom_path,
            layout_header=header_path,
            layout_json=json_path,
            pack_binary=pack_path,
        )
        assert rom_path.stat().st_size == layout2.rom_size
        assert pack_path.read_bytes() == image2[layout2.pack_offset :]
        loaded = AssetLayout.from_json(json.loads(json_path.read_text(encoding="utf-8")))
        assert loaded.pack_offset == layout2.pack_offset
        header = header_path.read_text(encoding="utf-8")
        assert "kZ80BoingBallSfxOffset" in header
        assert "kBoingPcmOffset" in header
        assert "kTilesOffset" in header
        assert f"{layout2.blob('tiles').offset}u" in header

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
