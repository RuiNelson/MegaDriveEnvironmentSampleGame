# Agent guide

Instructions for automated contributors working in
`MegaDriveEnvironmentSampleGame`.

## Purpose and non-negotiable architecture

This repository is a small game whose core C++ runs in two environments:

- as a native PC program hosted by `MegaDriveEnvironment`;
- as a bootable ROM on real Mega Drive hardware.

The shared game is the product. Keep target adapters narrow and explicit; do
not create a second renderer, input implementation, game loop, or behavior for
the PC target.

`../MegaDriveEnvironment` is the owned sibling runtime. `../Genesis-Plus-GX`
is an upstream, read-only reference: never edit, patch, reformat, commit, or
update it.

Before changing files, inspect repository status, preserve unrelated work, and
read the relevant documents under `docs/`:

- `ARCHITECTURE.md` for target boundaries and runtime flow;
- `MEMORY_MODEL.md` for portable memory access;
- `ASSETS.md` for generated asset ownership;
- `BUILDING.md` for PC and real-hardware commands.

## Repository layout

| Path | Responsibility |
| --- | --- |
| `include/MegaDriveEnvironmentSampleGame/` | Flat public/shared headers |
| `src/` | Shared and target-specific implementations |
| `src/runtime_tests/` | PC-only VDP, controller, sound, and font diagnostics |
| `config/shared_sources.json` | Canonical shared `.cpp` manifest |
| `cmake/` | PC build, assets, tests, and project options |
| `megadrive/` | Vector table, Sega header, startup/linker material |
| `sound/z80/` | Z80 audio driver source |
| `sound/tools/` | Sound conversion and Z80 assembly helpers |
| `tools/` | Asset packer and real-hardware ROM builder |
| `tests/` | C++ gameplay tests and Python asset/build tests |

Shared source files use plain names. A source file that exists for only one
target uses `-PC` or `-MD`, such as `Memory-PC.cpp` and `Memory-MD.cpp`.

## Shared-code rules

- Use C++23 and keep shared gameplay allocation-free.
- Compile shared C++ with exactly one target definition: `PC` or `MEGADRIVE`.
- Include only public `MegaDriveEnvironment` headers.
- Keep `SampleGame`, `VdpUtils`, controllers, gameplay, and audio source
  unchanged between targets.
- Drive shared behavior through VBlank/HBlank. The PC entry point overrides
  `vSync()`/`hSync()`; hardware IRQ6/IRQ4 forwards to the same `SampleGame`
  methods.
- Access runtime hardware only through the free functions in
  `sample::memory`. `Memory.hpp` is the single API; `Memory-PC.cpp` binds a host
  backend and `Memory-MD.cpp` accesses the 68000 bus.
- Never execute the `MEGADRIVE` memory implementation on the host because it
  dereferences the real console address map.
- Read controller state through `ControllerReader` and memory-mapped I/O, not
  SDL or `Controllers::getCurrentState()` from shared game code.
- Keep command-line parsing dependency-free in `src/main-PC.cpp`.
- Keep `config/shared_sources.json` authoritative; both CMake and the ROM
  builder consume it.

The hardware build intentionally provides no `operator new/delete`. Prefer
automatic storage and embedded fixed-capacity structures.

## Memory and audio invariants

Preserve these Work RAM reservations:

- `$FF0000-$FF0003`: IRQ bridge pointer;
- `$FF1000-$FF2FFF`: `BoingBallDemo` dynamic tile/DMA buffer.

HBlank line state belongs to `SampleGame`, not to a fixed Work RAM shim.

The Boing Ball bounce effect streams
`sound/amiga_assets/boing_pcm.bin` through the Z80 and YM2612 DAC using
`sound/z80/boing_ball_sfx.s` and `BoingBallFmSfx`. The main game uses
`PsgSoundEffects` on the SN76489. Shared audio code is memory-mapped only:

- PSG `$C00011`;
- Z80 RAM `$A00000`;
- bus request `$A11100`;
- reset `$A11200`;
- Z80 bank register `$6000`;
- Z80-local YM2612 `$4000`.

Do not call host sound or Z80 APIs from shared audio code.

## Asset pipeline

`tools/build_assets.py` owns the raw, headerless 32-Mbit asset ROM. It
assembles Z80 source via `sound/tools/assemble_z80.py`, packs named blobs at
the end of the image, and emits `AssetLayout.hpp` and `asset_layout.json`.
`tools/build_asset_rom.py` is a compatibility wrapper.

Normal builds consume the checked-in
`sound/amiga_assets/boing_pcm.bin` and write generated files only below the
build directory. PCM regeneration is an explicit maintenance action, not part
of an ordinary build.

The real-hardware builder embeds the packed asset tail in the bootable ROM.
Keep the hand-written vector table and Sega header in `megadrive/header.s`.
`code.o`, `code.disasm`, `blobs.s`, ELF/map files, and built ROMs are generated
artifacts.

## Build the PC target

Requirements are CMake 3.24+, a C++23 compiler, Python 3, SDL3, `z80asm`, and a
sibling `MegaDriveEnvironment` checkout.

Use the repository entry points:

```bash
./build_pc.sh
./run_pc.sh
./configure_controls.sh
```

Or use CMake directly:

```bash
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Debug \
  -DMEGADRIVE_ENVIRONMENT_DIR=../MegaDriveEnvironment
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Useful presets:

```bash
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
```

The PC target links shared `MegaDriveEnvironment` by default and sets
`CMAKE_LINK_DEPENDS_NO_SHARED`, so an implementation-only runtime rebuild does
not force the game executable to relink.

## Build the real-hardware target

The ROM pipeline requires Python 3, `z80asm`, and the `m68k-elf` GCC/binutils
toolchain, including `m68k-elf-as`:

```bash
./build_megadrive.sh
```

`tools/build_megadrive_rom.py` owns compilation, assembly, LTO link, Sega
checksum, asset embedding, and structural validation. Shared C++ is compiled
with `-Os -flto`; preserve `-Os` as the established size/speed policy unless a
task explicitly evaluates a different tradeoff.

Do not treat an emulator-only result as hardware validation. When a change
touches memory layout, interrupts, DMA, or audio timing, validate the ROM
structure and report which runtime environments were actually tested.

## Validation

Use the consolidated check for changes spanning game logic and tools:

```bash
./check.sh
```

For focused work, run the smallest relevant C++ or Python test first, then:

```bash
cmake --build --preset dev
ctest --preset dev
python3 -m unittest discover -s tests -p 'test_*.py'
```

Build both targets when shared code, the source manifest, memory access, asset
layout, controller code, interrupts, or audio changes. A PC-only UI/CLI change
does not require a hardware ROM rebuild unless it alters shared files.

## Generated files and delivery

Do not commit build directories, fetched dependencies, caches, screenshots,
generated ROMs, object/disassembly files, or locally regenerated PCM unless
the task explicitly calls for a versioned source asset update.

After validation, commit and push this repository to `main` automatically
unless the user explicitly asks not to publish. When checked out as a
submodule, publish this repository first and then update the parent gitlink.
Preserve unrelated work and never force-push or rewrite history. Report which
PC tests, ROM validations, emulators, or real hardware were used.
