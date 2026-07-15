# CLAUDE.md

Guidance for agents working in `MegaDriveEnvironmentSampleGame`.

## Purpose

This repository is a deliberately small dual-target game for the local
`MegaDriveEnvironment` and real Mega Drive hardware. Keep the shared game code
portable and the target-specific implementations explicit. The environment
target and the bootable ROM for real hardware both demonstrate VDP setup, controller
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

The real-hardware pipeline requires the `m68k-elf` GCC and binutils tools,
including `m68k-elf-as`. `tools/build_megadrive_rom.py` owns the compilation,
assembly, link, checksum, and structural validation steps.

```bash
cmake -S . -B build \
  -DMEGADRIVE_ENVIRONMENT_DIR=/path/to/MegaDriveEnvironment
```

## Conventions

- Use C++23.
- Include only public `MegaDriveEnvironment` headers.
- Compile `SampleGame`, `VdpUtils`, controllers, gameplay, and audio unchanged
  for both targets. Do not add a second renderer or target-specific game loop.
- Drive the shared game through VBlank and HBlank callbacks: the PC entry point
  overrides `vSync()`/`hSync()`, while the real-hardware IRQ6/IRQ4 handlers
  forward to the same `SampleGame` methods.
- All runtime hardware access from shared game code must go through
  `memory::Memory`.
- Keep public headers flat under `include/MegaDriveEnvironmentSampleGame/` and
  implementations flat under `src/`. Shared files have plain names; source
  files that only build for one target use the `-PC` or `-MD` suffix.
- Build shared C++ with exactly one target definition: `PC` for
  MegaDriveEnvironment or `MEGADRIVE` for real hardware.
- Keep `Memory.hpp` as the single memory API and target declarations. Its two
  implementations are `src/Memory-PC.cpp` and `src/Memory-MD.cpp`.
- Controller code must use `ControllerReader` and memory-mapped I/O,
  not SDL or `Controllers::getCurrentState()` directly.
- Keep command-line processing dependency-free; extend the manual parser in
  `src/main-PC.cpp` instead of adding CLI11 or another argument library.
- `tools/build_asset_rom.py` owns the raw, headerless asset ROM layout. The
  separate real-hardware builder embeds its trailing tile blob in a bootable ROM.
- Keep the hand-written vector table and Sega header in `megadrive/header.s`.
  `code.s` and `blobs.s` are generated build artifacts and must not be committed.
- Never execute the `MEGADRIVE` branch of `Memory.hpp` on the
  host; it dereferences the real 68000 address map directly.
- Keep the shared game allocation-free. The real-hardware build intentionally
  provides no `operator new/delete`, so use automatic or embedded
  fixed-capacity storage.
- Preserve the Work RAM reservations at `$FF0000-$FF0005` for the IRQ bridge
  and `$FF1000-$FF2FFF` for `BoingBallDemo`'s dynamic tile/DMA buffer.
- Do not commit build outputs, fetched dependencies, screenshots, or caches.
