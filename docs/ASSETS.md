# Asset pipeline

## Overview

`tools/build_assets.py` creates a raw 4 MiB image filled with `0xFF` and packs
named blobs at its end. It currently contains:

1. converted Boing Ball PCM;
2. the assembled Z80 DAC driver;
3. font and authored VDP tiles.

The builder emits four synchronized outputs below the build directory:

- `sample_game_assets.bin` — complete raw image used by the PC environment;
- `generated/AssetLayout.hpp` — offsets and sizes used by shared C++;
- `generated/asset_layout.json` — metadata used by the ROM builder;
- `generated/asset_pack.bin` — contiguous tail embedded in the hardware ROM.

CMake owns this pipeline for PC builds. `tools/build_megadrive_rom.py` invokes
the same builder before linking the bootable image.

## Add a named asset

1. Put the source under an appropriate repository directory such as `sound/`.
2. Load or convert it in `tools/build_assets.py`.
3. Add an `AssetBlob` with a valid C identifier and explicit alignment.
4. Add the source file to CMake's asset command dependencies in
   `cmake/Assets.cmake`.
5. Read its generated `k<Name>Offset` and `k<Name>Size` constants in shared C++.
6. Extend `tests/test_build_assets.py` to verify content, size and any banking
   constraints.

Packing order matters: the final blob ends at `$3FFFFF`; earlier blobs grow
backwards. The Boing PCM must stay inside a single 32 KiB Z80 bank window, and
the builder rejects a layout that violates that rule.

Never copy generated numeric offsets into C++ or assembly. Layout metadata is
the contract between both targets.

## Tiles

`tools/tiles.py` parses the 95 glyphs in `MegaDriveEnvironment`'s
`FontData.hpp`, converts them to Mega Drive 4-bpp tiles and appends the sample's
custom tiles. Each tile is 32 bytes.

Runtime code copies only the needed spans from ROM to VRAM. New authored tiles
can be added to `CUSTOM_TILE_ROWS`; larger art pipelines should produce a blob
instead of turning large binary assets into C++ arrays.

## Z80 and PCM sound

`sound/z80/boing_ball_sfx.s` is assembled by
`sound/tools/assemble_z80.py`. The 68000-side `BoingBallFmSfx` copies that
driver to Z80 RAM, installs the PCM bank/pointer/length and writes commands to
a mailbox.

`sound/amiga_assets/boing_pcm.bin` is a checked-in generated input so normal
builds are reproducible and do not modify source files. Regenerate it only when
changing the original sample or conversion policy:

```bash
python3 sound/tools/convert_boing_pcm.py \
  --input sound/amiga_assets/boing.samples \
  --output sound/amiga_assets/boing_pcm.bin \
  --source-rate 14037.43 \
  --target-rate 8000
```

The conversion removes DC, trims silence, low-pass filters, resamples and
quantizes to unsigned 8-bit YM2612 DAC data. After regeneration, run
`./check.sh` and review the binary diff deliberately.

## Bootable ROM

The raw asset image has no vectors, executable or Sega header. The real-
hardware builder compiles the shared C++ with `-Os -flto`, combines its LTO
object with `megadrive/header.s`, embeds `asset_pack.bin`, links through the GCC
driver using a generated layout, patches the checksum and validates the final
image. `code.disasm` contains a readable disassembly of the final post-LTO ELF.

Generated `code.cpp`, `code.o`, `code.disasm`, `blobs.s`, linker script, ELF,
map, ROMs and SVG charts belong under `build/` and must not be committed.
