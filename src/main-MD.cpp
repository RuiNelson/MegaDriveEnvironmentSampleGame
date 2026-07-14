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

extern "C" [[noreturn]] void game_main() {
    // header.s transfers control here after setting the stack and hardware.
    sample::platform::PlatformMemory memory;
    sample::SampleGame game{memory};
    game.run();

    for (;;) {
    }
}
