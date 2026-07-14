/**
 * @file main-MD.cpp
 * Minimal freestanding bootstrap for the shared sample::SampleGame.
 */

#include "MegaDriveEnvironmentSampleGame/SampleGame.hpp"
#include "MegaDriveEnvironmentSampleGame/platform/PlatformMemory.hpp"

// A freestanding link has no allocator. These symbols only satisfy the virtual
// destructor entries of the shared Memory interface; game objects live on the
// cartridge stack and are never dynamically allocated.
void operator delete(void *) noexcept {
}

void operator delete(void *, std::size_t) noexcept {
}

extern "C" void __cxa_pure_virtual() {
    // Reaching a pure virtual call indicates a fatal program error. With no OS
    // or debugger transport available, halt the cartridge deterministically.
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
