/**
 * @file main-MD.cpp
 * Minimal freestanding bootstrap for the shared sample::SampleGame.
 */

#include "MegaDriveEnvironmentSampleGame/SampleGame.hpp"
#include "MegaDriveEnvironmentSampleGame/Memory.hpp"

// The real-hardware build deliberately defines no operator new/delete. All
// game objects have automatic or embedded storage, so accidental heap use fails
// at link time instead of silently introducing an incomplete allocator.

extern "C" void __cxa_pure_virtual() {
    // Reaching a pure virtual call indicates a fatal program error. With no OS
    // or debugger transport available, halt real hardware deterministically.
    for (;;) {
    }
}

namespace {

// The first four Work RAM bytes are an explicit IRQ bridge slot. The game
// object itself remains on the supervisor stack for the lifetime of game_main.
constexpr std::uintptr_t kActiveGameAddress = 0x00FF0000;
constexpr std::uintptr_t kHvCounterAddress = 0x00C00008;

void setActiveGame(sample::SampleGame *game) {
    *reinterpret_cast<sample::SampleGame *volatile *>(kActiveGameAddress) = game;
}

sample::SampleGame &activeGame() {
    return **reinterpret_cast<sample::SampleGame *volatile *>(kActiveGameAddress);
}

} // namespace

extern "C" void wait_for_interrupt();

extern "C" void game_vsync() {
    activeGame().onVSync();
}

extern "C" void game_hsync() {
    const auto hvCounter = *reinterpret_cast<volatile std::uint16_t *>(kHvCounterAddress);
    activeGame().onHSync(static_cast<int>(hvCounter >> 8));
}

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
