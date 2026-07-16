/**
 * @file main-MD.cpp
 * @brief Minimal freestanding bootstrap for the shared sample::SampleGame on real Mega Drive hardware.
 *
 * ## Role in the dual-target architecture
 *
 * This translation unit is compiled only for the MEGADRIVE target. It is the
 * C++ half of the real-hardware entry path:
 *
 * 1. `megadrive/header.s` owns the vector table, Sega cartridge header, TMSS
 *    unlock, exception/IRQ stubs and the `wait_for_interrupt` STOP helper.
 * 2. After reset, the assembler jumps to `game_main()` defined here.
 * 3. IRQ6 (VBlank) and IRQ4 (HBlank) assembly handlers call `game_vsync()` and
 *    `game_hsync()`, which forward into the same `SampleGame` methods used by
 *    the PC build (`main-PC.cpp`).
 *
 * There is deliberately no second game loop or renderer: shared gameplay,
 * input, audio and VDP code live in `sample::SampleGame` and are driven only
 * through VBlank/HBlank callbacks on both targets.
 *
 * ## Freestanding constraints
 *
 * The real-hardware build does not provide `operator new` / `operator delete`.
 * All game objects therefore use automatic or embedded fixed-capacity storage.
 * Accidental heap use fails at link time instead of silently requiring an
 * incomplete freestanding allocator.
 *
 * Work RAM layout used by this file (see also docs/MEMORY_MODEL.md):
 *
 * | Range            | Owner / purpose                                      |
 * | ---------------- | ---------------------------------------------------- |
 * | `$FF0000-$FF0003`| Active `SampleGame*` for the IRQ bridge              |
 * | `$FF1000-$FF2FFF`| `BoingBallDemo` DMA tile buffer (not touched here)   |
 * | stack from `$FFFFFC` downwards | `SampleGame` + locals for `game_main` lifetime |
 *
 * The HBlank line counter lives inside `SampleGame` (not Work RAM): the VDP
 * HINT does not report a scanline, so the game advances 0, 16, 32, … itself.
 *
 * The game object itself remains on the supervisor stack for the entire
 * lifetime of `game_main`. The IRQ bridge cannot take a C++ reference across
 * the assembly boundary, so the pointer is stored at a fixed Work RAM address.
 */

#include "MegaDriveEnvironmentSampleGame/SampleGame.hpp"
#include "MegaDriveEnvironmentSampleGame/Memory.hpp"

// The real-hardware build deliberately defines no operator new/delete. All
// game objects have automatic or embedded storage, so accidental heap use fails
// at link time instead of silently introducing an incomplete allocator.

/**
 * @brief C++ pure-virtual call trap required by the freestanding ABI.
 *
 * The compiler may emit calls to `__cxa_pure_virtual` when a pure virtual
 * function is invoked (for example through a half-constructed or destroyed
 * object). On a hosted OS this typically aborts; with no OS or debugger
 * transport on cartridge hardware, the only safe reaction is a deterministic
 * infinite halt so the machine does not run off into undefined execution.
 *
 * Marked `extern "C"` so the symbol matches the Itanium/GCC freestanding ABI
 * expected by the m68k-elf toolchain.
 */
extern "C" void __cxa_pure_virtual() {
    // Reaching a pure virtual call indicates a fatal program error. With no OS
    // or debugger transport available, halt real hardware deterministically.
    for (;;) {
    }
}

namespace {

// Work RAM holds the IRQ bridge pointer. BoingBallDemo separately reserves
// $FF1000-$FF2FFF as its DMA tile buffer. The game object itself remains on the
// supervisor stack for the lifetime of game_main.

/** Fixed Work RAM address of the active `SampleGame*` used by IRQ handlers. */
constexpr std::uintptr_t kActiveGameAddress = 0x00FF0000;

/**
 * @brief Publishes the live game object so IRQ handlers can reach it.
 *
 * Assembly IRQ stubs only jump to C entry points; they cannot capture a C++
 * reference. Storing a pointer at a known physical address keeps the bridge
 * allocation-free and independent of the C++ ABI.
 *
 * @param game Pointer to the stack-allocated `SampleGame` that outlives all
 *             subsequent IRQs (typically until the cartridge is powered off).
 */
void setActiveGame(sample::SampleGame *game) {
    *reinterpret_cast<sample::SampleGame *volatile *>(kActiveGameAddress) = game;
}

/**
 * @brief Returns the game object previously published by `setActiveGame`.
 *
 * Called only from IRQ paths after `game_main` has installed a valid pointer.
 * Volatile qualification prevents the compiler from caching the pointer across
 * interrupt boundaries where another context might have updated it.
 *
 * @return Reference to the active `SampleGame`.
 */
sample::SampleGame &activeGame() {
    return **reinterpret_cast<sample::SampleGame *volatile *>(kActiveGameAddress);
}

} // namespace

/**
 * @brief Assembly helper: enable maskable IRQs and sleep until the next one.
 *
 * Implemented in `megadrive/header.s` as `stop #0x2000` (supervisor mode, IRQ
 * mask clear) followed by `rts`. Control returns after the IRQ handler's `rte`
 * resumes the stopped instruction stream. The C++ main loop calls this instead
 * of busy-waiting so the 68000 idles between HBlank/VBlank work.
 */
extern "C" void wait_for_interrupt();

/**
 * @brief VBlank (IRQ6) C entry point called from `irq_level_6` in header.s.
 *
 * The assembler saves caller-saved registers, acknowledges the VDP status
 * port (required to clear the pending IRQ; not used as a line source), then
 * jumps here. Forwards into `SampleGame::onVSync()`.
 */
extern "C" void game_vsync() {
    activeGame().onVSync();
}

/**
 * @brief HBlank (IRQ4) C entry point called from `irq_level_4` in header.s.
 *
 * Forwards into `SampleGame::onHSync()`. The hardware interrupt does not carry
 * a scanline index; the game tracks the next H-scroll block itself.
 */
extern "C" void game_hsync() {
    activeGame().onHSync();
}

/**
 * @brief Cartridge C++ entry point; never returns.
 *
 * Called from `reset_entry` in `megadrive/header.s` after the stack pointer is
 * set (`$FFFFFC`), interrupts are masked, and TMSS (if present) has been
 * unlocked. Startup sequence:
 *
 * 1. Construct target-specific `PlatformMemory` (direct 68000 bus accessors).
 * 2. Construct the shared `SampleGame` on the supervisor stack.
 * 3. Publish the game pointer for IRQ handlers.
 * 4. Run one-shot hardware/game initialization (controllers, audio, VDP, scene).
 * 5. Sleep forever via `wait_for_interrupt()`; all further work is IRQ-driven.
 *
 * Marked `[[noreturn]]` because the infinite STOP loop is the intended
 * lifetime of a cartridge program. A trailing branch in the assembler also
 * traps if this function ever returned.
 */
extern "C" [[noreturn]] void game_main() {
    // header.s transfers control here after setting the stack and hardware.
    sample::platform::PlatformMemory memory;
    sample::SampleGame game{memory};
    setActiveGame(&game);
    game.initialize();

    for (;;) {
        // STOP enables interrupts and sleeps until the next HBlank/VBlank IRQ;
        // the assembly handlers then call the shared SampleGame callbacks.
        wait_for_interrupt();
    }
}
