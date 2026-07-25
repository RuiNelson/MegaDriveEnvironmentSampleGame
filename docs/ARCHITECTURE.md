# Architecture

## Design goals

The project deliberately keeps a small number of explicit layers:

- game rules do not know about the VDP, SDL or the host;
- shared hardware code only sees the Mega Drive address bus;
- target code only boots that shared game and supplies a memory backend;
- generated asset addresses come from one build artifact;
- shared code is allocation-free and valid for a freestanding M68000 build.

This is a sample, so traceability is more valuable than a large abstraction
framework. New layers should solve a demonstrated problem.

## The two targets

| Concern | PC | Mega Drive |
| --- | --- | --- |
| Entry point | `src/main-PC.cpp` | `src/main-MD.cpp` + `megadrive/header.s` |
| Memory backend | `src/Memory-PC.cpp` | `src/Memory-MD.cpp` |
| Target definition | `PC=1` | `MEGADRIVE=1` |
| Frame callback | environment `vSync` | IRQ6 bridge |
| Game, VDP, input, audio | Shared C++ | The same shared C++ |

`config/shared_sources.json` is the canonical list of shared implementations.
CMake and `tools/build_megadrive_rom.py` both read it. Adding a new unsuffixed
`.cpp` without updating the manifest fails `sample_source_manifest_test`.

## Frame flow

Initialization configures the controller and audio drivers, generates the demo
background, loads tiles from ROM, prepares planes and finally enables display.

During execution:

1. VBlank sets a pending-frame flag and immediately returns from IRQ6.
2. The target main loop consumes that flag, samples input and advances the model.
3. One-frame events select sound effects.
4. The active screen writes bounded planes, sprites or VBlank DMA state.
5. The selection menu uses short HBlank callbacks for its upper-half sky;
   either game disables HINT before taking ownership of the VDP.
6. The Boing Ball renderer uses visible-line CPU time for bounded raster work.

The PC application and real hardware IRQ6 both schedule the same `SampleGame`
frame method, so there is no second game loop or renderer. IRQ4 is active only
for the menu sky: every eight lines it advances the backdrop from light blue
to white, then disables itself at the 112-pixel ocean horizon. Keeping long
work outside IRQ6 is essential: the Boing Ball rasterizer may intentionally
run until NTSC line 192.

## Main components

- `GameConfig.hpp` is the public customization point for basic screen, entity,
  speed and score values.
- `GameSession` owns the player, collectible, enemy, collision rules, score and
  phase. It consumes a plain `ControllerState` and emits one-frame `Events`.
- `SampleGame` composes input, rules, audio and rendering. It switches between
  the front-layer Window menu, main game and demo, records VBlank, owns the
  menu-only sky IRQ, and runs pending frames outside IRQ.
- `ControllerReader` implements the standard three-button protocol through
  `$A10003/$A10005` and `$A10009/$A1000B`.
- `VdpUtils` provides target-neutral VDP operations for registers, VRAM, CRAM,
  planes, sprites, text and DMA.
- `PsgSoundEffects` contains frame-driven SN76489 sequences for the main game.
- `BoingBallFmSfx` installs the Z80 DAC driver and posts floor/wall commands
  through its mailbox.
- `BoingBallDemo` owns the fixed-point simulation and software tile rasterizer.
  It creates only the visible tile rectangle, learns transparent corners and
  double-buffers a bounded Work RAM surface before VDP DMA. Uploads are capped
  at 160 tiles (5120 bytes) per NTSC VBlank, so a 128x128 surface spans two.
- The menu reserves Plane B for scenery: transparent cells expose the
  HBlank-colored sky over rows 0..13, while authored ocean tiles cover rows
  14..27. Window covers the screen as the priority text layer.

## The memory boundary

Shared code calls free functions in `sample::memory`. Words and long words are
big-endian and addresses are normalized to the 24-bit bus.

On PC, `memory::bind(SystemMemory&)` (or a test `Backend`) routes those calls
to the environment's thread-safe map. On hardware, the same functions are
always-inlined volatile bus accesses. Never execute the `MEGADRIVE`
implementation on a host: it dereferences physical bus addresses directly.

See [Memory model](MEMORY_MODEL.md) before adding buffers, pools or recursion.

## Extension patterns

### Add a gameplay object

Put its state and rules in `GameSession` or a new shared class. Give it fixed,
bounded storage; expose read-only state to `SampleGame`; add a deterministic
test; then render it through `VdpUtils`.

### Add a screen

Extend `SampleGame::Screen`, add explicit activation/update/render methods and
make input transitions edge-triggered. Reset any VDP state changed by the old
screen—palettes, Window mode, planes and sprite links do not reset themselves.

### Add input support

Extend the plain controller state first, then implement the memory-mapped
protocol in `ControllerReader`. Do not call SDL or host controller APIs from
shared code.

### Add an asset

Follow [Asset pipeline](ASSETS.md). Runtime code should use generated constants
from `AssetLayout.hpp`, never a copied ROM offset.

### Add target-specific behavior

First ask whether it can be expressed as a bus operation or shared policy. A
new `-PC`/`-MD` pair is appropriate for target bootstrapping or a genuinely
different memory implementation, not for gameplay or rendering shortcuts.
