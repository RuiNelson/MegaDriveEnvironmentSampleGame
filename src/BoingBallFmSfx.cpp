/**
 * @file BoingBallFmSfx.cpp
 * Memory-mapped 68000 bootstrap and mailbox for the Boing Ball Z80 FM driver.
 */

#include "MegaDriveEnvironmentSampleGame/BoingBallFmSfx.hpp"

#include "AssetLayout.hpp"

namespace sample::audio {
namespace {

// Word-wide Z80 control bits live in the high byte (standard 68000 access).
constexpr std::uint16_t kBusRequestWord = 0x0100;
constexpr std::uint16_t kBusReleaseWord = 0x0000;
constexpr std::uint16_t kResetHoldWord = 0x0000;
constexpr std::uint16_t kResetRunWord = 0x0100;

// Bounded spin so a stuck bus never hangs the shared game loop.
constexpr int kBusAckSpinLimit = 0x8000;

// Brief 68000 busy-wait so the Z80 can run while /BUSREQ is clear between
// short ownership windows (ready-flag poll, etc.).
void yieldToZ80() {
    for (int spin = 0; spin < 64; ++spin) {
        volatile int sink = spin;
        (void)sink;
    }
}

} // namespace

BoingBallFmSfx::BoingBallFmSfx(memory::Memory &memory) : memory_(memory) {
}

void BoingBallFmSfx::initialize() {
    // Standard 68000 Z80-bus protocol (memory map only):
    //  1. Assert /BUSREQ and wait for /BUSACK.
    //  2. Release /RESET so the 68K window at $A00000 is live.
    //  3. Copy the driver and clear the mailbox.
    //  4. Pulse /RESET so the Z80 restarts at $0000 with that image.
    //  5. Release /BUSREQ so the Z80 runs.
    requestBus();
    releaseReset();

    const auto romBase = static_cast<memory::Address>(assets::kZ80BoingBallSfxOffset);
    const auto size = assets::kZ80BoingBallSfxSize;
    for (unsigned index = 0; index < size; ++index) {
        memory_.write8(kZ80RamBase + index, memory_.read8(romBase + index));
    }

    memory_.write8(kZ80RamBase + kMailboxOffset, kCommandIdle);
    memory_.write8(kZ80RamBase + kStatusOffset, 0);

    holdReset();
    releaseReset();
    releaseBus();

    // Poll the driver's ready byte through the same bus protocol. Own the bus
    // only for the read, then yield so the Z80 can advance init.
    for (int spin = 0; spin < kBusAckSpinLimit; ++spin) {
        requestBus();
        const auto status = memory_.read8(kZ80RamBase + kStatusOffset);
        releaseBus();
        if (status == 1) {
            break;
        }
        yieldToZ80();
    }
    ready_ = true;
}

void BoingBallFmSfx::playFloorBounce() {
    writeCommand(kCommandFloor);
}

void BoingBallFmSfx::playWallBounce() {
    writeCommand(kCommandWall);
}

bool BoingBallFmSfx::ready() const {
    return ready_;
}

void BoingBallFmSfx::requestBus() {
    memory_.write16(kZ80BusRequest, kBusRequestWord);
    for (int spin = 0; spin < kBusAckSpinLimit; ++spin) {
        // /BUSACK: bit 8 clear means the 68000 owns the Z80 bus.
        if ((memory_.read16(kZ80BusRequest) & kBusRequestWord) == 0) {
            return;
        }
    }
}

void BoingBallFmSfx::releaseBus() {
    memory_.write16(kZ80BusRequest, kBusReleaseWord);
}

void BoingBallFmSfx::holdReset() {
    memory_.write16(kZ80Reset, kResetHoldWord);
}

void BoingBallFmSfx::releaseReset() {
    memory_.write16(kZ80Reset, kResetRunWord);
}

void BoingBallFmSfx::writeCommand(std::uint8_t command) {
    if (!ready_) {
        return;
    }

    // Mailbox lives in Z80 RAM: take the bus, poke one byte, release.
    requestBus();
    memory_.write8(kZ80RamBase + kMailboxOffset, command);
    releaseBus();
}

} // namespace sample::audio
