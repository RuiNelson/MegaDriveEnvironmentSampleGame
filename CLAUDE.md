# CLAUDE.md

Guidance for agents working in `MegaDriveEnvironmentSampleGame`.

## Purpose

This repository is a deliberately small dual-target game for the local
`MegaDriveEnvironment` and real Mega Drive hardware. Keep the shared game code
portable and the target-specific implementations explicit. The environment
target and the bootable cartridge ROM both demonstrate VDP setup, controller
input, game state, sprites, and text.

## Build

The `MegaDriveEnvironment` repository must be available next to this one, or
its location must be supplied explicitly:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
./build/mega_drive_environment_sample_game
```

Prefer the repository entry points for normal PC workflows:

```bash
./build_pc.sh
./configure_controls.sh
./run_pc.sh
```

Build the real Mega Drive ROM with:

```bash
./build_megadrive.sh
```

The cartridge pipeline requires `vasmm68k_std` plus the `m68k-elf` GCC and
binutils tools. `tools/build_megadrive_rom.py` owns the compilation, assembly,
link, checksum, and structural validation steps.

```bash
cmake -S . -B build \
  -DMEGADRIVE_ENVIRONMENT_DIR=/path/to/MegaDriveEnvironment
```

## Conventions

- Use C++23.
- Include only public `MegaDriveEnvironment` headers.
- Compile `SampleGame`, `VdpUtils`, controllers, gameplay, and audio unchanged
  for both targets. Do not add a second renderer or target-specific game loop.
- All runtime hardware access from shared game code must go through
  `memory::Memory`. Platform implementations may differ in access and waiting:
  real hardware busy-waits, while MegaDriveEnvironment yields cooperatively.
- Keep shared platform contracts under `memory/` and target implementations
  under `platform/megadrive_environment/` or `platform/megadrive/`.
- Controller code must use `controllers/ControllerReader` and memory-mapped I/O,
  not SDL or `Controllers::getCurrentState()` directly.
- Keep command-line processing dependency-free; extend the manual parser in
  `src/main-PC.cpp` instead of adding CLI11 or another argument library.
- `tools/build_asset_rom.py` owns the raw, headerless asset ROM layout. The
  separate cartridge builder embeds its trailing tile blob in a bootable ROM.
- Keep the hand-written vector table and Sega header in `megadrive/header.s`.
  `code.s` and `blobs.s` are generated build artifacts and must not be committed.
- Never execute the `SAMPLE_FREESTANDING` branch of `memory/Memory.hpp` on the
  host; it dereferences the real 68000 address map directly.
- Do not commit build outputs, fetched dependencies, screenshots, or caches.
