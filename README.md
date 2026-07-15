# MegaDriveEnvironmentSampleGame

A tiny C++23 game showing how to build a new Sega Mega Drive project with
[`MegaDriveEnvironment`](https://github.com/RuiNelson/MegaDriveEnvironment).

Move the blue character with the D-pad (arrow keys in the default keyboard
configuration), collect the yellow gems, and avoid the red enemy. It becomes a
little faster with every collected gem. A collision ends the game; press A to
restart. During play, Start opens a second software-rendered demo screen; press
Start there to return.

## What the sample demonstrates

- consuming `MegaDriveEnvironment` as a CMake subdirectory;
- subclassing `MegaDriveEnvironment` and starting it with `boot()`;
- running game logic on the environment CPU thread and on the real-hardware
  68000;
- overriding `vSync()` and `hSync(int)` in the PC environment application;
- forwarding real-hardware IRQ6/IRQ4 to the same shared VBlank/HBlank methods;
- creating a visible HBlank raster-wave effect on the Plane B floor;
- rendering a rotating, bouncing 96x96 white/red/blue checker ball in software;
- expanding a 48x48 logical raster 2x2 and transferring its 144 dynamic tiles
  from fixed Work RAM to VRAM with VDP DMA;
- composing the ball and its dithered ground shadow from linked hardware sprites;
- measuring the 30/25 software-rasterized frames over a 60/50-VBlank window;
- drawing an upright wall grid and a perspective floor grid on separate planes;
- rotating the checker texture around both theta and phi axes;
- playing distinct PSG impacts for the floor and side walls;
- configuring the VDP for H40 / Mode 5 output;
- loading palettes, font data, and 4-bpp tiles;
- drawing Plane A/Plane B text and backgrounds;
- updating linked entries in the sprite attribute table;
- reading Player 1 through the memory-mapped controller ports;
- modelling the player, collectible, enemy, collisions, and game session with
  portable C++ objects shared by both targets;
- playing SN76489 PSG effects through the memory-mapped sound port;
- running the exact same interrupt-driven game callbacks and VDP renderer on
  both targets through one 24-bit memory API;
- generating and loading a raw 32-Mbit asset ROM;
- compiling the C++ game to M68000 assembly and producing a bootable ROM for
  real hardware.

The helper code in `VdpUtils` is intentionally explicit. It writes the VDP's
memory-mapped ports through `memory::Memory`, so the same implementation runs
inside MegaDriveEnvironment and on the physical console.

## Software 3D demo

After accepting the opening notice, press Start from gameplay to open the
Boing Ball screen. The ball is 96 pixels wide—30% of the H40 display—and uses
no pre-rendered animation frames. A 48x48 fixed-point sphere lookup supplies
pre-shaded white, red and blue colours, longitude and latitude; the shared C++
renderer rotates its checker texture around theta and phi and expands each logical pixel to a 2x2
block, and packs the result as 144 VDP tiles. Nine 4x4 sprites display those
tiles as one large ball, while three more sprites form the 96x8 dithered
shadow. Plane B draws the upright grid and the Window plane draws the
perspective floor below the horizon. Floor and wall impacts trigger distinct
SN76489 PSG effects.

To stay within the real 68000/VDP budget, the renderer writes one bounded
4608-byte tile buffer at `$FF1000-$FF21FF` and launches one 68000-to-VRAM DMA.
The sprite position and bounce advance on every VBlank (60 Hz NTSC / 50 Hz
PAL); the more expensive rotating surface is rebuilt every other VBlank and
persists in VRAM between updates. The FPS display counts those completed
software rasters over one 60/50-VBlank interval, so the normal result is 30
FPS on NTSC and 25 FPS on PAL. It does not use either Yamaha timer. PC and Mega
Drive execute this same code and perform every RAM, controller and VDP access
through `memory::Memory`.

## Requirements

- CMake 3.24 or newer;
- a C++23 compiler;
- SDL3 installed on the host;
- `MegaDriveEnvironment` and this repository checked out side by side.

The build for real hardware additionally requires Python 3 and the GNU M68k
cross tools: `m68k-elf-g++`, `m68k-elf-as`, `m68k-elf-ld`, and
`m68k-elf-objcopy`.

### Install the GNU cross tools

Homebrew provides the GCC and binutils cross tools. On macOS, install them with:

```bash
brew install m68k-elf-binutils m68k-elf-gcc
```

### What `build_megadrive.sh` expects

With no options, the shell/Python build pipeline expects all of the following:

- `python3` in `PATH`, or another interpreter selected with `PYTHON3=/path/to/python3`;
- `m68k-elf-g++`, `m68k-elf-as`, `m68k-elf-ld`, and `m68k-elf-objcopy` in
  `PATH`;
- the sibling `MegaDriveEnvironment` checkout, used to read
  `include/MegaDriveEnvironment/util/font/FontData.hpp`.

The tool names can be overridden with `--assembler` and `--tool-prefix`; the
font path can be overridden with `--font-data`.

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

## Build the Mega Drive ROM for real hardware

```bash
./build_megadrive.sh
```

The final image is written to:

```text
build/megadrive/MegaDriveEnvironmentSampleGame.bin
```

It is an exact 4 MiB / 32 Mbit ROM image with a standard Sega header,
64-entry vector table, reset startup, IRQ vectors, M68000 game code, checksum,
and the tile blob at `$3FF360-$3FFFFF`. It can be loaded directly by Mega Drive
emulators or run on compatible real hardware—including by burning it to a
physical ROM chip, like it's the '90s again. 👌

The final ELF is linked from three separately assembled inputs:

- `megadrive/header.s` — hand-written vectors, Sega header, reset and IRQ handlers;
- `build/megadrive/generated/code.s` — generated by `m68k-elf-g++` from the
  same freestanding C++ game used on PC plus the hardware memory backend;
- `build/megadrive/generated/blobs.s` — generated from the trailing tiles in
  `build/sample_game_assets.bin`.

`tools/build_megadrive_rom.py` commands the complete process: it regenerates the
asset-only ROM, compiles C++ to `code.s`, creates `blobs.s`, invokes
`m68k-elf-as`, links the ELF, emits the binary, patches the Sega checksum, and
validates the header, vectors, size, checksum, and byte-for-byte asset payload.
The generated ELF and link map remain under `build/megadrive/` for inspection.

Alternative paths and cross-tool names can be supplied directly:

```bash
./build_megadrive.sh \
  --output /tmp/sample-game.bin \
  --assembler /path/to/m68k-elf-as \
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
the real-hardware executable code. CMake runs the script automatically, and the
environment loads the generated binary before `SampleGame::initialize()`.

Run the script directly with:

```bash
python3 tools/build_asset_rom.py \
  --output build/sample_game_assets.bin
```

An alternative image can be selected with `--rom FILE`.

## One memory API, target-specific cost

Game code uses the common `memory::Memory` contract. It defines 8/16/32-bit
big-endian access, 24-bit address normalization, block reads/writes, overlapping
copies, fills, and masked waits on memory-mapped registers.

- `src/Memory-PC.cpp` forwards PC operations to the thread-safe `SystemMemory`
  owned by `MegaDriveEnvironment` and yields during any masked register wait.
- In the real-hardware build, `Memory.hpp` turns that same API into a
  concrete, stateless type. Its primitive `volatile` reads, writes, and masked
  busy-wait are forced inline, so there is no backend object, virtual dispatch,
  or helper subroutine around an individual 68000 bus instruction.

`Memory.hpp` exposes the PC adapter in a host build and an alias to
`memory::Memory` in a real-hardware build. The build defines `MEGADRIVE` for
real hardware and `PC` for MegaDriveEnvironment. `Memory-PC.cpp` implements
cooperative emulation access; `Memory-MD.cpp` implements direct, always-inline
68000 access. Defining both targets, or neither target, is a compile-time error.

```bash
ctest --test-dir build --output-on-failure
```

`SampleGame`, `BoingBallDemo`, `VdpUtils`, `ControllerReader`, `GameSession`,
and `PsgSoundEffects` are compiled unchanged for both targets. On PC,
`EnvironmentApplication::vSync()` and `hSync(int)` forward to
`SampleGame::onVSync()` and `onHSync()`. On real hardware, IRQ6 and IRQ4 forward
to those same methods. The PSG player writes to `$C00011`, which reaches
emulated audio on PC and the chip on a real Mega Drive.

## Memory management and the 64 KiB Work RAM budget

There are two separate ideas called "memory" in this project:

- `memory::Memory` represents the Mega Drive address bus. Calls such as
  `write16(0xC00004, value)` communicate with mapped hardware; they do not
  allocate or own C++ objects.
- Storage management decides where mutable game state and temporary data live,
  how much space they may use, and when that space can be reused.

A real Mega Drive gives the 68000 only 64 KiB of Work RAM at
`$FF0000-$FFFFFF`. The stack, persistent game state, decompression workspaces,
staging buffers, and any custom allocator must all fit in that same physical
region. The initial stack pointer is `$FFFFFC` and the stack grows downwards;
there is no guard page to stop it from overwriting another Work RAM region.
The real-hardware bootstrap reserves `$FF0000-$FF0003` for the active
`SampleGame` pointer and `$FF0004-$FF0005` for the HBlank scanline shim. The
software 3D demo reserves `$FF1000-$FF21FF` as its dynamic tile/DMA buffer;
future Work RAM layouts must preserve or deliberately relocate these regions.

The correct design is therefore not necessarily "put everything on the stack"
or "allocate everything manually". Decide a Work RAM budget before building the
game and give every use a bounded region and lifetime:

| Use | Typical storage choice |
| --- | --- |
| Function calls and small local values | 68000 stack, with reserved headroom |
| Persistent player, world, and subsystem state | Fixed objects or a reserved static region |
| Temporary decompression or conversion data | Reusable scratch arena in Work RAM |
| Variable enemies, projectiles, or effects | Fixed-capacity typed pools |
| Immutable compressed assets and programs | Game ROM until consumed |

Direct members and fixed arrays are useful because their maximum size is known,
but they are not automatically free of RAM cost. Their storage follows their
owner: in this sample `SampleGame` is created on the stack, so its embedded
`GameSession`, controller, and sound state also consume stack space for the
whole run. Large buffers should not become members of that stack object merely
to avoid writing an allocator.

A game that chooses a reserved static region must describe it in the linker
script and initialize it during startup on real hardware. Zero-initialized BSS
must be cleared in Work RAM, while mutable initialized data normally needs an
initial copy stored in ROM and copied to Work RAM. This sample needs neither
operation and deliberately makes non-empty BSS a link error.

### Compressed ROM data

Compressed graphics, sound data, and Z80 programs are good examples of data
that need an explicit transfer plan. Keeping the compressed bytes in ROM saves
ROM space, but decompression may require mutable state or an output
workspace before the result is sent to VRAM or Z80 RAM.

Prefer a streaming decoder when the format permits it: read compressed bytes
from ROM and emit decoded words directly to the VDP data port or, after taking
the Z80 bus, to Z80 RAM. Only the decoder's small history/state then needs Work
RAM. If the algorithm requires a complete output block, back-references, or a
conversion pass, reserve a bounded scratch arena in Work RAM, decompress there,
transfer the result, and reuse the arena after the transfer. A large local C++
array is not an appropriate substitute because it silently consumes the stack.

A game may deliberately implement manual allocation for these workspaces. It
can expose explicit `allocate/release` operations, typed pools, or complete
`operator new/delete` implementations backed by reserved Work RAM. Such an
allocator must define its address range, alignment, capacity, out-of-memory
behaviour, reset rules, and relationship with the descending stack. The current
sample instead chooses no heap, supplies no `operator new/delete`, and rejects
BSS through its linker script; those are sample policies, not limitations of
MegaDriveEnvironment or of Mega Drive software.

### Important PC versus real hardware difference

On PC, shared C++ functions execute on the native host thread and their local
variables use the native host stack. That stack is separate from the 64 KiB Work
RAM array modelled by MegaDriveEnvironment and is normally much larger. A game
can therefore run perfectly in MegaDriveEnvironment while a large local buffer,
deep call chain, or recursion overflows the 68000 stack on real hardware.

`memory::Memory` cannot fix this difference: it controls explicit bus reads and
writes, not where the C++ implementation places automatic variables. Portable
C++ provides no way to redirect ordinary function frames into
MegaDriveEnvironment's emulated Work RAM or to give the host stack exactly the
68000 ABI's size and layout. Overriding `operator new` would only affect dynamic
allocation, not local variables. An OS-specific small host thread stack can
catch some mistakes, but it is not equivalent to the 68000 stack on real
hardware because the host ABI and runtime use different frame sizes.

Explicit arenas and pools can still behave identically on both targets: give
the PC version the same capacities and failure rules, and represent scratch
storage as Work RAM addresses accessed through `memory::Memory` rather than as
an unlimited host `std::vector`. Stack safety must additionally be checked from
the m68k build itself by budgeting worst-case call depth and frame sizes,
avoiding recursion and large locals, reserving headroom, and optionally checking
a canary at the chosen lower stack boundary on hardware.

Before choosing automatic storage, static regions, or manual allocation,
document at least:

- the maximum stack budget and its lower boundary;
- persistent Work RAM regions and their owners;
- scratch arenas, their largest operation, and when they may be reused;
- pool capacities and what happens when a pool is full;
- transfers from ROM to Work RAM, VRAM, CRAM, and Z80 RAM;
- the checks used on PC and in the m68k build to enforce those limits.

## Shared controller reader

`ControllerReader` is a small target-independent library built on
`memory::Memory`. It configures TH through `$A10009/$A1000B`, samples the data
ports at `$A10003/$A10005`, decodes the active-low three-button protocol, and
returns a plain `ControllerState`.

Both builds use this reader unchanged. On real hardware the same operations go
through the bare-metal `PlatformMemory`, so controller logic does not need a second
implementation. Six-button controller negotiation can be added to this library
later without changing game code.

## Code tour

- `include/MegaDriveEnvironmentSampleGame/` contains one flat set of public
  headers; namespaces provide the logical grouping without more directories.
- `src/` contains all implementations. Unsuffixed files are shared unchanged
  by both targets; target-only files end in `-PC.cpp` or `-MD.cpp`.
- `src/main-PC.cpp` parses the host CLI, overrides `vSync()`/`hSync(int)`, and
  supplies the adapter implemented by `src/Memory-PC.cpp` before calling
  `boot()`.
- `src/main-MD.cpp` creates the zero-cost real-hardware `PlatformMemory` alias
  and uses the direct bus primitives from `src/Memory-MD.cpp`.
- `SampleGame` owns the shared VBlank/HBlank callbacks, input, audio, and
  renderer used by both targets.
- `GameSession.hpp` contains the shared `Player`, `Enemy`, `Collectible`,
  collision rules, scoring, and game-over state.
- `PsgSoundEffects.hpp` contains the shared PSG effects for collecting a
  gem, colliding with the enemy, and restarting.
- `VdpUtils` contains shared memory-mapped VDP operations.
- `megadrive/header.s` contains the real-hardware IRQ4/IRQ6 bridges.
- `tools/build_megadrive_rom.py` builds and validates the ROM image for real
  hardware.
- `Memory.hpp` is the only memory header; `Memory-PC.cpp` and `Memory-MD.cpp`
  provide the two target implementations.
- `ControllerReader.hpp` decodes controllers through that contract.

New features belong in the shared game and renderer. Platform code should only
change when memory access or target bootstrapping itself changes.
