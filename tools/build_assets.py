#!/usr/bin/env python3
"""Build the raw 32-Mbit asset ROM and generated layout metadata.

Packs every named blob (Z80 programs, PCM samples, VDP tiles, …) at the end of
a headerless 4 MiB image. Emits:

  - the full asset ROM used by the PC loader
  - AssetLayout.hpp with freestanding C++ constants
  - asset_layout.json for the Mega Drive ROM builder
  - the packed tail binary for the .assets linker section
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

from assemble_z80 import assemble_z80
from asset_pack import (
    ROM_SIZE,
    AssetBlob,
    pack_blobs,
    print_asset_layout_summary,
    write_layout_header,
    write_layout_json,
    write_pack_binary,
)
from boing_pcm import load_boing_pcm
from tiles import build_tile_data

# convert_boing_pcm is the only path that produces assets/boing_pcm.bin


def repository_root() -> Path:
    return Path(__file__).resolve().parent.parent


def default_font_data() -> Path:
    return (
        repository_root().parent
        / "MegaDriveEnvironment"
        / "include"
        / "MegaDriveEnvironment"
        / "util"
        / "font"
        / "FontData.hpp"
    )


def build_asset_image(
    *,
    font_data_path: Path,
    z80_source: Path,
    boing_pcm_path: Path,
    work_dir: Path,
) -> tuple[bytes, object, bytes]:
    """Return (rom_image, layout, z80_binary)."""
    work_dir.mkdir(parents=True, exist_ok=True)
    z80_binary_path = work_dir / "z80_boing_ball_sfx.bin"
    z80_list_path = work_dir / "z80_boing_ball_sfx.lst"
    z80_binary = assemble_z80(z80_source, z80_binary_path, list_file=z80_list_path)
    tile_data = build_tile_data(font_data_path)
    boing_pcm = load_boing_pcm(boing_pcm_path)

    # Order matters: last blob sits at the absolute end of the ROM. The PCM
    # sample is packed before the Z80 driver so both live in the final 32 KiB
    # bank window when possible (Z80 streams PCM via $6000 banking).
    image, layout = pack_blobs(
        (
            AssetBlob("boing_pcm", boing_pcm, align=2),
            AssetBlob("z80_boing_ball_sfx", z80_binary, align=2),
            AssetBlob("tiles", tile_data, align=2),
        )
    )

    pcm = layout.blob("boing_pcm")
    bank_base = pcm.offset & ~0x7FFF
    if pcm.offset + pcm.size > bank_base + 0x8000:
        raise RuntimeError(
            f"boing PCM [{pcm.offset:#x}..{pcm.offset + pcm.size:#x}) spans a "
            f"32 KiB bank boundary at {bank_base + 0x8000:#x}; adjust packing"
        )
    return image, layout, z80_binary


def write_outputs(
    *,
    image: bytes,
    layout,
    output_rom: Path,
    layout_header: Path,
    layout_json: Path,
    pack_binary: Path,
) -> None:
    output_rom.parent.mkdir(parents=True, exist_ok=True)
    output_rom.write_bytes(image)
    written = output_rom.read_bytes()
    if len(written) != ROM_SIZE or written != image:
        raise RuntimeError(f"failed to verify generated ROM {output_rom}")

    write_layout_header(layout, layout_header)
    write_layout_json(layout, layout_json)
    write_pack_binary(image, layout, pack_binary)


def parse_args() -> argparse.Namespace:
    repository = repository_root()
    default_build = repository / "build"
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--output",
        type=Path,
        default=default_build / "sample_game_assets.bin",
        help="full 32-Mbit asset ROM path",
    )
    parser.add_argument(
        "--font-data",
        type=Path,
        default=default_font_data(),
        help="path to MegaDriveEnvironment's FontData.hpp",
    )
    parser.add_argument(
        "--z80-source",
        type=Path,
        default=repository / "z80" / "boing_ball_sfx.s",
        help="Boing Ball Z80 driver assembly source",
    )
    parser.add_argument(
        "--boing-pcm",
        type=Path,
        default=repository / "assets" / "boing_pcm.bin",
        help="Amiga Boing impact sample (unsigned 8-bit or signed Amiga PCM)",
    )
    parser.add_argument(
        "--work-dir",
        type=Path,
        default=default_build / "generated" / "assets",
        help="directory for intermediate Z80 binaries and listings",
    )
    parser.add_argument(
        "--layout-header",
        type=Path,
        default=default_build / "generated" / "AssetLayout.hpp",
        help="generated C++ layout header",
    )
    parser.add_argument(
        "--layout-json",
        type=Path,
        default=default_build / "generated" / "asset_layout.json",
        help="generated JSON layout for the Mega Drive builder",
    )
    parser.add_argument(
        "--pack-binary",
        type=Path,
        default=default_build / "generated" / "asset_pack.bin",
        help="packed tail written into the real-hardware .assets section",
    )
    return parser.parse_args()


def main() -> int:
    tools_dir = Path(__file__).resolve().parent
    if str(tools_dir) not in sys.path:
        sys.path.insert(0, str(tools_dir))

    args = parse_args()
    image, layout, z80_binary = build_asset_image(
        font_data_path=args.font_data.resolve(),
        z80_source=args.z80_source.resolve(),
        boing_pcm_path=args.boing_pcm.resolve(),
        work_dir=args.work_dir.resolve(),
    )
    write_outputs(
        image=image,
        layout=layout,
        output_rom=args.output.resolve(),
        layout_header=args.layout_header.resolve(),
        layout_json=args.layout_json.resolve(),
        pack_binary=args.pack_binary.resolve(),
    )

    print(f"Wrote {args.output}")
    print(f"Wrote {args.layout_header}")
    print(f"Wrote {args.layout_json}")
    print(f"Wrote {args.pack_binary}")
    print(f"Assembled Z80 driver: {len(z80_binary)} bytes")
    chart_path = args.layout_json.resolve().with_name("asset_rom_layout.svg")
    print_asset_layout_summary(
        layout,
        title="Asset ROM layout",
        svg_path=chart_path,
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
