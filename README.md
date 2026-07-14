# MegaDriveEnvironmentSampleGame

A tiny C++23 game showing how to build a new Sega Mega Drive project with
[`MegaDriveEnvironment`](https://github.com/RuiNelson/MegaDriveEnvironment).

Move the blue character with the D-pad (arrow keys in the default keyboard
configuration), collect the yellow gems, and press A or Start to reset.

## What the sample demonstrates

- consuming `MegaDriveEnvironment` as a CMake subdirectory;
- subclassing `MegaDriveEnvironment` and starting it with `boot()`;
- running game logic on the cartridge/CPU thread;
- synchronizing the game loop with VBlank;
- configuring the VDP for H40 / Mode 5 output;
- loading palettes, font data, and 4-bpp tiles;
- drawing Plane A/Plane B text and backgrounds;
- updating linked entries in the sprite attribute table;
- reading Player 1 through the memory-mapped controller ports;
- sharing one 24-bit memory API between the PC environment and real hardware;
- generating and loading a raw 32-Mbit asset ROM.

The helper code in `VdpUtils` is intentionally explicit. It exposes the VDP
register and port operations so the project can serve as a starting point for
new games rather than hiding the hardware model behind a larger engine.

## Requirements

- CMake 3.24 or newer;
- a C++23 compiler;
- SDL3 installed on the host;
- `MegaDriveEnvironment` and this repository checked out side by side.

The default directory layout is:

```text
parent/
├── MegaDriveEnvironment/
└── MegaDriveEnvironmentSampleGame/
```

## Build and run

```bash
./build_pc.sh
./run_pc.sh
```

The scripts resolve paths relative to their own location, so they can be called
from any working directory. `configure_controls.sh` and `run_pc.sh` use the
same project-root `controls.yaml`.

Environment variables customize the PC build:

```bash
BUILD_TYPE=Release ./build_pc.sh
BUILD_DIR=/tmp/sample-build ./build_pc.sh
MEGADRIVE_ENVIRONMENT_DIR=/path/to/MegaDriveEnvironment ./build_pc.sh
```

Useful development options:

```bash
./configure_controls.sh
./run_pc.sh --debug
./run_pc.sh --frames 10
```

`--frames N` exits after N frames and is useful for smoke tests and CI.

## Configure controls on PC

Run the interactive keyboard/gamepad configuration UI with:

```bash
./configure_controls.sh
```

`--configControls` is also accepted for compatibility with
`StreetsOfRageRecompilation`. The UI writes `controls.yaml` in the current
working directory and then exits. The next game run loads those bindings
automatically through `MegaDriveEnvironment`; the shared `ControllerReader`
continues to see them through the emulated memory-mapped controller ports.

The command line is parsed directly with the C++ standard library. This sample
does not use CLI11 or another argument-processing dependency.

The equivalent direct executable command remains available:

```bash
./build/mega_drive_environment_sample_game --config-controls
```

## Raw asset ROM

`tools/build_asset_rom.py` generates `build/sample_game_assets.bin`. It is a
raw 4 MiB (32 Mbit) image filled with `0xFF`: it contains no executable code,
ROM header, manifest, vectors, or checksum.

The 95 font tiles followed by the player, gem, and floor tiles occupy the final
3232 bytes, at `$3FF360-$3FFFFF`. This keeps the whole lower ROM area available
for game code later. CMake runs the script automatically, and the environment
loads the generated binary at the beginning of `SampleGame::run()`.

Run the script directly with:

```bash
python3 tools/build_asset_rom.py \
  --output build/sample_game_assets.bin
```

An alternative image can be selected with `--rom FILE`.

## Two memory backends

Game code uses the common `memory::Memory` contract. It defines 8/16/32-bit
big-endian access, 24-bit address normalization, block reads/writes, overlapping
copies, and fills.

- `EnvironmentMemory` forwards those operations to the thread-safe
  `SystemMemory` owned by `MegaDriveEnvironment`.
- `HardwareMemory` performs direct `volatile` reads and writes on the real
  68000 address bus.

The host build compiles both backends and runs tests against the environment
backend. `HardwareMemory` must never be executed on the host.

```bash
ctest --test-dir build --output-on-failure
```

This is the first shared platform layer. Producing a bootable cartridge ROM
still requires real-hardware implementations for startup/vector tables, VDP,
controllers, interrupts and audio, plus a selected 68000 compiler and linker
script. Those pieces are intentionally not hidden behind a fake “ROM build”.

## Shared controller reader

`ControllerReader` is a small target-independent library built on
`memory::Memory`. It configures TH through `$A10009/$A1000B`, samples the data
ports at `$A10003/$A10005`, decodes the active-low three-button protocol, and
returns a plain `ControllerState`.

The game uses this reader in the PC environment today. On real hardware the
same code operates through `HardwareMemory`, so controller logic does not need
a second implementation. Six-button controller negotiation can be added to
this library later without changing game code.

## Code tour

- `src/main.cpp` parses the tiny host-side CLI and calls `boot()`.
- `SampleGame` owns game state, input, collision, and the frame loop.
- `VdpUtils` contains the environment target's VDP operations.
- `memory/Memory.hpp` is the platform-neutral memory contract.
- `controllers/ControllerReader.hpp` decodes controllers through that contract.
- `platform/megadrive_environment` and `platform/megadrive` contain the two
  memory implementations.

Start by changing the tile patterns and palettes in `SampleGame.cpp`, then add
new game state in `update()` and render it in `render()`.
