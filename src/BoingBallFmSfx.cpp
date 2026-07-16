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

} // namespace

BoingBallFmSfx::BoingBallFmSfx(memory::Memory &memory) : memory_(memory) {
}

void BoingBallFmSfx::initialize() {
    requestBus();
    holdReset();

    const auto romBase = static_cast<memory::Address>(assets::kZ80BoingBallSfxOffset);
    const auto size = assets::kZ80BoingBallSfxSize;
    for (unsigned index = 0; index < size; ++index) {
        const auto byte = memory_.read8(romBase + index);
        memory_.write8(kZ80RamBase + index, byte);
    }

    memory_.write8(kZ80RamBase + kMailboxOffset, kCommandIdle);
    memory_.write8(kZ80RamBase + kStatusOffset, 0);

    // Match the environment self-test order: drop busreq, then leave reset so
    // the Z80 begins execution at address 0 with a clean program image.
    releaseBus();
    releaseReset();

    // Wait until the driver finishes YM init and raises its ready flag. Without
    // this, early mailbox posts can race a still-booting Z80 on a slow host.
    for (int spin = 0; spin < kBusAckSpinLimit; ++spin) {
        requestBus();
        const auto status = memory_.read8(kZ80RamBase + kStatusOffset);
        releaseBus();
        if (status == 1) {
            break;
        }
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

    // Brief bus request so the 68000 owns Z80 RAM while poking the mailbox.
    requestBus();
    memory_.write8(kZ80RamBase + kMailboxOffset, command);
    releaseBus();
}

} // namespace sample::audio
