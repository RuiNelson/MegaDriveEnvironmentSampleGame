#pragma once

/**
 * @file BoingBallFmSfx.hpp
 * Portable 68000 host for the Boing Ball Z80/YM2612 DAC sample driver.
 */

#include "MegaDriveEnvironmentSampleGame/Memory.hpp"

namespace sample::audio {

/**
 * Loads the Boing Ball Z80 driver, points it at the ROM-resident Amiga impact
 * sample, and posts bounce commands through a Z80 RAM mailbox.
 *
 * Every access is a memory-mapped bus operation through sample::memory:
 * Z80 RAM at $A00000, bus request at $A11100, reset at $A11200. The Z80
 * streams PCM from the cartridge via its bank window and writes the YM2612
 * DAC. No host sound or Z80 API is used.
 */
class BoingBallFmSfx final {
  public:
    /** Command bytes written to the Z80 mailbox (must match sound/z80/boing_ball_sfx.s). */
    static constexpr std::uint8_t kCommandIdle = 0;
    static constexpr std::uint8_t kCommandFloor = 1;
    static constexpr std::uint8_t kCommandWall = 2;

    /** 68000-visible addresses for the Z80 control ports and work RAM window. */
    static constexpr memory::Address kZ80RamBase = 0xA00000;
    static constexpr memory::Address kZ80BusRequest = 0xA11100;
    static constexpr memory::Address kZ80Reset = 0xA11200;
    static constexpr std::uint16_t kMailboxOffset = 0x1E00;
    static constexpr std::uint16_t kStatusOffset = 0x1E01;
    /** Little-endian parameter block written after the driver image. */
    static constexpr std::uint16_t kPcmBankOffset = 0x1E02;
    static constexpr std::uint16_t kPcmPtrOffset = 0x1E04;
    static constexpr std::uint16_t kPcmLenOffset = 0x1E06;

    BoingBallFmSfx() = default;

    /**
     * Requests the Z80 bus, copies the driver, installs PCM bank/pointer/
     * length, and releases the Z80 to run.
     */
    void initialize();

    /** Posts the floor-bounce command (Amiga period 255 rate). */
    void playFloorBounce();

    /** Posts the wall-bounce command (Amiga period 160 rate). */
    void playWallBounce();

    /** True after a successful initialize(). */
    [[nodiscard]] bool ready() const;

  private:
    void requestBus();
    void releaseBus();
    void holdReset();
    void releaseReset();
    void writeCommand(std::uint8_t command);
    void writeZ80WordLE(std::uint16_t offset, std::uint16_t value);

    bool ready_ = false;
};

} // namespace sample::audio
