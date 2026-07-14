# MegaDriveEnvironmentSampleGame

A tiny C++23 game showing how to build a new Sega Mega Drive project with
[`MegaDriveEnvironment`](https://github.com/RuiNelson/MegaDriveEnvironment).

Move the blue character with the D-pad (arrow keys in the default keyboard
configuration), collect the yellow gems, and avoid the red enemy that slowly
chases you. A collision ends the game; press A or Start to restart.

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
- modelling the player, collectible, enemy, collisions, and game session with
  portable C++ objects shared by both targets;
- playing SN76489 PSG effects through the memory-mapped sound port;
- sharing one 24-bit memory API between the PC environment and real hardware;
- generating and loading a raw 32-Mbit asset ROM;
- compiling the C++ game to M68000 assembly and producing a bootable cartridge ROM.

The helper code in `VdpUtils` is intentionally explicit. It exposes the VDP
register and port operations so the project can serve as a starting point for
new games rather than hiding the hardware model behind a larger engine.

## Requirements

- CMake 3.24 or newer;
- a C++23 compiler;
- SDL3 installed on the host;
- `MegaDriveEnvironment` and this repository checked out side by side.

The real Mega Drive build additionally requires Python 3, a host C compiler and
`make`, plus:

- [vasm](http://sun.hasenbraten.de/vasm), built as `vasmm68k_std`;
- `m68k-elf-g++`, `m68k-elf-ld`, and `m68k-elf-objcopy`.

### Install the GNU cross tools

Homebrew provides the GCC and binutils cross tools. On macOS, install them with:

```bash
brew install m68k-elf-binutils m68k-elf-gcc
```

### Install vasm from its official source

vasm is not a Homebrew formula. Download its source archive directly from the
[official vasm site](http://sun.hasenbraten.de/vasm/release/vasm.tar.gz),
extract it, and build the M68k standard-syntax executable:

```bash
curl -LO http://sun.hasenbraten.de/vasm/release/vasm.tar.gz
tar -xzf vasm.tar.gz
cd vasm
make CPU=m68k SYNTAX=std
```

This produces `vasmm68k_std` in the source directory. The `std` variant is
required because it accepts the GNU-as-style assembly emitted by
`m68k-elf-g++`; `vasmm68k_mot` is not what this build script invokes.

Either copy the executable into a directory already present in `PATH`:

```bash
install -m 755 vasmm68k_std /opt/homebrew/bin/vasmm68k_std
```

or leave it in its build directory and pass the full path when building the ROM:

```bash
./build_megadrive.sh --assembler /path/to/vasm/vasmm68k_std
```

Review the vasm license in the official source distribution; its default grant
is for non-commercial use outside the documented Amiga/M68k exception.

### What `build_megadrive.sh` expects

With no options, the shell/Python build pipeline expects all of the following:

- `python3` in `PATH`, or another interpreter selected with `PYTHON3=/path/to/python3`;
- `vasmm68k_std` in `PATH`;
- `m68k-elf-g++`, `m68k-elf-ld`, and `m68k-elf-objcopy` in `PATH`;
- the sibling `MegaDriveEnvironment` checkout, used to read
  `include/MegaDriveEnvironment/util/font/FontData.hpp`.

The tool names can be overridden with `--assembler` and `--tool-prefix`; the
font path can be overridden with `--font-data`. The script deliberately does
not use `m68k-elf-as`: vasm performs the assembly step.

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

`build_pc.sh` first runs `tools/build_asset_rom.py` to generate the raw 32-Mbit
ROM at `build/sample_game_assets.bin`, then configures and compiles the PC
executable.

Environment variables customize the PC build:

```bash
BUILD_TYPE=Release ./build_pc.sh
BUILD_DIR=/tmp/sample-build ./build_pc.sh
MEGADRIVE_ENVIRONMENT_DIR=/path/to/MegaDriveEnvironment ./build_pc.sh
PYTHON3=/path/to/python3 ./build_pc.sh
```

Useful development options:

```bash
./configure_controls.sh
./run_pc.sh --debug
./run_pc.sh --frames 10
```

`--frames N` exits after N frames and is useful for smoke tests and CI.

## Build the Mega Drive cartridge ROM

```bash
./build_megadrive.sh
```

The final image is written to:

```text
build/megadrive/MegaDriveEnvironmentSampleGame.bin
```

It is an exact 4 MiB / 32 Mbit cartridge image with a standard Sega header,
64-entry vector table, reset startup, IRQ vectors, M68000 game code, checksum,
and the tile blob at `$3FF360-$3FFFFF`. It can be loaded directly by Mega Drive
emulators or written to compatible flash-cartridge hardware.

The final object is assembled by vasm from four assembly inputs:

- `megadrive/header.s` — hand-written vectors, Sega header, reset and IRQ handlers;
- `build/megadrive/generated/code.s` — generated by `m68k-elf-g++` from the
  freestanding C++ game, hardware memory backend, and shared controller reader;
- `build/megadrive/generated/blobs.s` — generated from the trailing tiles in
  `build/sample_game_assets.bin`;
- `megadrive/main.s` — includes the preceding three inputs in one assembly unit.

`tools/build_megadrive_rom.py` commands the complete process: it regenerates the
asset-only ROM, compiles C++ to `code.s`, creates `blobs.s`, invokes vasm, links
the ELF, emits the binary, patches the Sega checksum, and validates the header,
vectors, size, checksum, and byte-for-byte asset payload. The generated ELF and
link map remain under `build/megadrive/` for inspection.

Alternative paths and cross-tool names can be supplied directly:

```bash
./build_megadrive.sh \
  --output /tmp/sample-game.bin \
  --assembler /path/to/vasmm68k_std \
  --tool-prefix /path/to/toolchain/bin/m68k-elf-
```

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
3232 bytes, at `$3FF360-$3FFFFF`. This keeps the lower ROM area available for
the executable cartridge code. CMake runs the script automatically, and the
environment loads the generated binary at the beginning of `SampleGame::run()`.

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

The cartridge target uses `HardwareMemory`, the same `ControllerReader`, the
same object-oriented game session, and the same PSG sound-effect player as the
PC target. Its VDP startup, rendering loop, vectors, and interrupts are
implemented explicitly for the real address map. The PSG player writes to the
SN76489 port at `$C00011`, which is routed to emulated audio on PC and directly
to the chip on a real Mega Drive.

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
- `SampleGame` owns the environment-specific frame loop and renderer.
- `game/GameSession.hpp` contains the shared `Player`, `Enemy`, `Collectible`,
  collision rules, scoring, and game-over state.
- `audio/PsgSoundEffects.hpp` contains the shared PSG effects for collecting a
  gem, colliding with the enemy, and restarting.
- `VdpUtils` contains the environment target's VDP operations.
- `src/megadrive/CartridgeGame.cpp` provides object-oriented real-hardware game
  and VDP classes plus the freestanding entry point.
- `megadrive/header.s` and `megadrive/main.s` are the hand-written assembly inputs.
- `tools/build_megadrive_rom.py` builds and validates the real cartridge image.
- `memory/Memory.hpp` is the platform-neutral memory contract.
- `controllers/ControllerReader.hpp` decodes controllers through that contract.
- `platform/megadrive_environment` and `platform/megadrive` contain the two
  memory implementations.

Start by changing shared rules in `GameSession`, then adapt only the two renderers
when a feature needs new VDP presentation.
