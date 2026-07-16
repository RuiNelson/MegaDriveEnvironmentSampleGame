/**
 * @file BoingBallFmSfx.cpp
 * 68000-side bootstrap and mailbox for the Boing Ball Z80 FM driver.
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

// Bounded spin so a stuck bus never hangs the game loop on either target.
constexpr int kBusAckSpinLimit = 0x8000;

// Give the Z80 time to run between short 68K bus grabs. Accurate emulators and
// real hardware only advance the Z80 while /BUSREQ is clear.
void delayZ80Window() {
    for (int spin = 0; spin < 64; ++spin) {
        // Volatile so -Os cannot delete the wait on either target.
        volatile int sink = spin;
        (void)sink;
    }
}

} // namespace

BoingBallFmSfx::BoingBallFmSfx(memory::Memory &memory) : memory_(memory) {
}

void BoingBallFmSfx::initialize() {
    // Accurate Z80 bus controller (Genesis Plus GX / real hardware):
    //   zstate bit0 = /RESET released, bit1 = /BUSREQ asserted
    // 68K may read/write Z80 RAM only in zstate 3 (both bits set).
    // From cold boot (zstate 0): request bus, then *release* reset so access
    // is granted. Holding reset while requesting bus leaves writes discarded.
    requestBus();
    releaseReset();

    const auto romBase = static_cast<memory::Address>(assets::kZ80BoingBallSfxOffset);
    const auto size = assets::kZ80BoingBallSfxSize;
    for (unsigned index = 0; index < size; ++index) {
        const auto byte = memory_.read8(romBase + index);
        memory_.write8(kZ80RamBase + index, byte);
    }

    memory_.write8(kZ80RamBase + kMailboxOffset, kCommandIdle);
    memory_.write8(kZ80RamBase + kStatusOffset, 0);

    // Pulse reset while still holding the bus so the Z80 restarts at $0000 with
    // the freshly loaded image. Access stays granted across the pulse only if
    // we end with /RESET released before dropping /BUSREQ.
    holdReset();
    releaseReset();
    releaseBus();

    // Wait until the driver finishes YM init and raises its ready flag. Each
    // probe briefly owns the bus, then yields so the Z80 can make progress.
    for (int spin = 0; spin < kBusAckSpinLimit; ++spin) {
        requestBus();
        const auto status = memory_.read8(kZ80RamBase + kStatusOffset);
        releaseBus();
        if (status == 1) {
            break;
        }
        delayZ80Window();
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
        // BUSACK: bit 8 clear means the 68K owns the Z80 bus.
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

    // Own the Z80 bus for the mailbox poke; required on real hardware and on
    // accurate emulators (writes are ignored while /BUSREQ is clear).
    requestBus();
    memory_.write8(kZ80RamBase + kMailboxOffset, command);
    releaseBus();
}

} // namespace sample::audio
