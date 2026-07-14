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
- reading Player 1 through `controllers().getCurrentState()`.

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
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
./build/mega_drive_environment_sample_game
```

If the runtime is elsewhere, pass its path explicitly:

```bash
cmake -S . -B build \
  -DMEGADRIVE_ENVIRONMENT_DIR=/path/to/MegaDriveEnvironment
```

Useful development options:

```bash
./build/mega_drive_environment_sample_game --debug
./build/mega_drive_environment_sample_game --frames 10
```

`--frames N` exits after N frames and is useful for smoke tests and CI.

## Code tour

- `src/main.cpp` parses the tiny host-side CLI and calls `boot()`.
- `SampleGame` owns game state, input, collision, and the frame loop.
- `VdpUtils` contains reusable, hardware-facing VDP operations.

Start by changing the tile patterns and palettes in `SampleGame.cpp`, then add
new game state in `update()` and render it in `render()`.
