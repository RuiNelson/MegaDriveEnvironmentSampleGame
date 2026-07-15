/**
 * @file main-MD.cpp
 * Minimal freestanding bootstrap for the shared sample::SampleGame.
 */

#include "MegaDriveEnvironmentSampleGame/SampleGame.hpp"
#include "MegaDriveEnvironmentSampleGame/Memory.hpp"
#include "MegaDriveEnvironmentSampleGame/VdpUtils.hpp"

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

// Work RAM holds the IRQ bridge state. The game object itself remains on the
// supervisor stack for the lifetime of game_main.
constexpr std::uintptr_t kActiveGameAddress = 0x00FF0000;
constexpr std::uintptr_t kHLineAddress = 0x00FF0004;
constexpr std::uint16_t kHSyncVBlankSentinel = 0xFFFF;

void setActiveGame(sample::SampleGame *game) {
    *reinterpret_cast<sample::SampleGame *volatile *>(kActiveGameAddress) = game;
}

sample::SampleGame &activeGame() {
    return **reinterpret_cast<sample::SampleGame *volatile *>(kActiveGameAddress);
}

volatile std::uint16_t &activeHLine() {
    return *reinterpret_cast<volatile std::uint16_t *>(kHLineAddress);
}

void resetHSyncLineShim() {
    // A final HBlank IRQ remains pending when VBlank begins. Mark it so the
    // shim can discard it instead of treating it as the first visible block.
    activeHLine() = kHSyncVBlankSentinel;
}

int nextHSyncLineFromShim() {
    const auto hLine = activeHLine();
    if (hLine == kHSyncVBlankSentinel) {
        activeHLine() = static_cast<std::uint16_t>(sample::vdp::kHSyncLineBatch - 1);
        return -1;
    }
    activeHLine() = static_cast<std::uint16_t>(hLine + sample::vdp::kHSyncLineBatch);
    return static_cast<int>(hLine);
}

} // namespace

extern "C" void wait_for_interrupt();

extern "C" void game_vsync() {
    resetHSyncLineShim();
    activeGame().onVSync();
}

extern "C" void game_hsync() {
    const int hLine = nextHSyncLineFromShim();
    if (hLine < 0) {
        return;
    }
    activeGame().onHSync(hLine);
}

extern "C" [[noreturn]] void game_main() {
    // header.s transfers control here after setting the stack and hardware.
    sample::platform::PlatformMemory memory;
    sample::SampleGame game{memory};
    setActiveGame(&game);
    activeHLine() = static_cast<std::uint16_t>(sample::vdp::kHSyncLineBatch - 1);
    game.initialize();

    for (;;) {
        // STOP enables interrupts and sleeps until the next HBlank/VBlank IRQ;
        // the assembly handlers then call the shared SampleGame callbacks.
        wait_for_interrupt();
    }
}
