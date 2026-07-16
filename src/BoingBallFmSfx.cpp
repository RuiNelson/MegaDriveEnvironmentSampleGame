/**
 * @file BoingBallFmSfx.cpp
 * Memory-mapped 68000 bootstrap and mailbox for the Boing Ball Z80 DAC driver.
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

constexpr int kBusAckSpinLimit = 0x8000;

void yieldToZ80() {
    for (int spin = 0; spin < 64; ++spin) {
        volatile int sink = spin;
        (void)sink;
    }
}

} // namespace

BoingBallFmSfx::BoingBallFmSfx(memory::Memory &memory) : memory_(memory) {
}

void BoingBallFmSfx::writeZ80WordLE(std::uint16_t offset, std::uint16_t value) {
    // Z80 is little-endian; write low byte at the lower address.
    memory_.write8(kZ80RamBase + offset, static_cast<std::uint8_t>(value & 0xFFu));
    memory_.write8(kZ80RamBase + offset + 1, static_cast<std::uint8_t>((value >> 8) & 0xFFu));
}

void BoingBallFmSfx::initialize() {
    // Standard 68000 Z80-bus protocol (memory map only):
    //  1. Assert /BUSREQ and wait for /BUSACK.
    //  2. Release /RESET so the 68K window at $A00000 is live.
    //  3. Copy the driver and install PCM parameters.
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

    // Point the Z80 at the ROM-resident Amiga sample through its 32 KiB bank.
    const auto pcmOffset = assets::kBoingPcmOffset;
    const auto pcmSize = static_cast<std::uint16_t>(assets::kBoingPcmSize);
    const auto bank = static_cast<std::uint16_t>(pcmOffset >> 15);
    const auto ptr = static_cast<std::uint16_t>(0x8000u | (pcmOffset & 0x7FFFu));
    writeZ80WordLE(kPcmBankOffset, bank);
    writeZ80WordLE(kPcmPtrOffset, ptr);
    writeZ80WordLE(kPcmLenOffset, pcmSize);

    holdReset();
    releaseReset();
    releaseBus();

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

    requestBus();
    memory_.write8(kZ80RamBase + kMailboxOffset, command);
    releaseBus();
}

} // namespace sample::audio
