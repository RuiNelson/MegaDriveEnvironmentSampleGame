# MegaDriveEnvironmentSampleGame

A small C++23 game that runs unchanged inside
[`MegaDriveEnvironment`](https://github.com/RuiNelson/MegaDriveEnvironment) and
on a real Sega Mega Drive. It is both a playable example and a starting point
for new projects.

Move the blue character, collect yellow gems and avoid the red enemy. Press
Start during play to open the software-rendered Boing Ball demo.

## Quick start

Place this repository next to `MegaDriveEnvironment`:

```text
parent/
├── MegaDriveEnvironment/
└── MegaDriveEnvironmentSampleGame/
```

Install CMake 3.24+, a C++23 compiler, SDL3, Python 3 and `z80asm`, then run:

```bash
./build_pc.sh
./run_pc.sh
```

On macOS, the non-compiler dependencies can be installed with Homebrew:

```bash
brew install cmake sdl3 z80asm
```

Useful commands:

```bash
./configure_controls.sh       # configure keyboard/gamepad bindings
./run_pc.sh --debug           # enable environment diagnostics
./run_pc.sh --frames 10       # finite smoke test
./check.sh                    # build and run every test
./build_megadrive.sh          # create a bootable 4 MiB ROM
./run_emulator.sh             # build if needed and open an emulator
```

See [Building and running](docs/BUILDING.md) for toolchains, CMake presets,
custom paths and emulator configuration.

## Controls

- D-pad: move in the game; Up/Down changes Boing Ball zoom.
- A: accept the opening notice or reset the current round.
- Start: accept the notice, enter the demo, or return to the game.

The default PC keyboard mapping uses the arrow keys. Local bindings are stored
in the ignored `controls.yaml` file.

## What this demonstrates

- one allocation-free game core compiled for PC and M68000;
- VBlank/HBlank callbacks shared by emulation and real hardware;
- VDP palettes, planes, text, sprites, DMA and raster effects;
- memory-mapped three-button controller input;
- SN76489 effects and a Z80/YM2612 DAC sample driver;
- generated, named assets in a raw 32-Mbit image;
- a software-rasterized, continuously scalable checker ball;
- deterministic gameplay objects with host-side unit tests;
- a validated Sega header, vector table, checksum and ROM layout.

## Repository map

```text
include/MegaDriveEnvironmentSampleGame/  Public shared interfaces
src/                                     Shared and target-specific C++
config/shared_sources.json               Sources compiled by both targets
cmake/                                   Focused CMake modules
megadrive/                               Startup assembly and linker reference
sound/                                   Z80 source and Amiga PCM assets
tools/                                   Asset and ROM builders
tests/                                   C++ and Python regression tests
docs/                                    Architecture and extension guides
```

Unsuffixed files in `src/` are shared. Files ending in `-PC.cpp` or `-MD.cpp`
are target adapters. Public headers remain flat; namespaces provide logical
grouping without hiding the sample behind a deep directory hierarchy.

## Start extending it

The easiest first changes are:

1. Adjust positions, speeds and dimensions in
   [`GameConfig.hpp`](include/MegaDriveEnvironmentSampleGame/GameConfig.hpp).
2. Add rules and entities to `GameSession`; it has no renderer or platform
   dependency and is straightforward to test.
3. Add rendering/input/audio orchestration to `SampleGame`.
4. Add immutable data through the asset pipeline instead of embedding large
   arrays in C++.
5. Run `./check.sh`; the source-manifest test catches shared `.cpp` files that
   were not included in the real-hardware build.

Keep hardware access behind `memory::Memory`. A feature normally belongs in
shared code; change a target adapter only when bootstrapping or memory access is
actually platform-specific.

## More documentation

- [Architecture](docs/ARCHITECTURE.md) — components, frame flow and extension
  boundaries.
- [Building and running](docs/BUILDING.md) — PC, ROM, presets and troubleshooting.
- [Asset pipeline](docs/ASSETS.md) — adding tiles, programs and sound data.
- [Memory model](docs/MEMORY_MODEL.md) — the 64 KiB Work RAM budget and portable
  storage choices.
- [Contributing](CONTRIBUTING.md) — project checks and change conventions.
