# Memory model

## Bus access and object storage are different

`memory::Memory` represents the Mega Drive address bus. A call such as
`write16(0xC00004, value)` communicates with mapped hardware; it does not
allocate or own C++ storage.

Object storage decides where mutable state and temporary buffers live. On real
hardware, the 68000 has only 64 KiB of Work RAM at `$FF0000-$FFFFFF`. The stack,
persistent state, scratch buffers and any allocator share that region.

## Current reservations

- `$FF0000-$FF0003`: active `SampleGame` pointer used by the IRQ bridge;
- `$FF1000-$FF2FFF`: Boing Ball tile/DMA buffer;
- stack starts at `$FFFFFC` and grows downwards.

The HBlank line counter is a normal member of `SampleGame` (on the stack with
the game object), not a fixed Work RAM word. The VDP HINT does not report a
scanline index; the game advances the next H-scroll block start itself.

Future layouts must preserve these ranges or relocate them deliberately in all
code, tests and documentation. There is no guard page between a descending
stack and another Work RAM owner.

## Choosing storage

| Use | Suitable bounded storage |
| --- | --- |
| Small locals and calls | 68000 stack with reserved headroom |
| Persistent world state | Fixed objects or a documented static region |
| Decompression/conversion | Reusable Work RAM scratch arena |
| Enemies/projectiles/effects | Fixed-capacity typed pools |
| Immutable programs and assets | ROM until transfer is required |

Direct members and fixed arrays have predictable maximum size, but they are not
free. Their storage follows their owner. `SampleGame` currently lives on the
stack, so embedded state also occupies the stack for the whole run.

The ROM build deliberately supplies no `operator new/delete`, and the linker
rejects non-empty BSS. These are policies of this sample, not limitations of
the environment or console. A larger game can introduce reserved static data,
arenas or pools, provided startup initialization and overlap rules are explicit.

## Transferring compressed data

Prefer streaming decompression directly from ROM to the VDP or Z80 RAM when a
format permits it. If an algorithm needs a complete output block or history,
reserve a bounded scratch arena, transfer the result and reuse that arena after
the operation. A large local array silently consumes the hardware stack and is
not a safe substitute.

An allocator must document its address range, alignment, capacity,
out-of-memory behavior, reset rules and relationship with the stack.

## PC versus hardware

Shared C++ uses the native host stack on PC, not the environment's emulated
64 KiB Work RAM. A large local buffer, recursion or deep call chain can work on
PC and still overflow on M68000 hardware. `memory::Memory` cannot make the two
ABIs share stack size or frame layout.

Explicit arenas and pools can enforce identical capacities on both targets.
Stack safety must additionally be checked from the M68k build by budgeting
worst-case call depth and frame sizes, avoiding recursion and large locals, and
optionally checking a canary at the chosen lower boundary.

Before adding substantial storage, document:

- maximum stack budget and lower boundary;
- persistent Work RAM regions and owners;
- scratch arena size and reuse points;
- pool capacities and full-pool behavior;
- transfers between ROM, Work RAM, VRAM, CRAM and Z80 RAM;
- checks that enforce those limits on PC and M68k.
