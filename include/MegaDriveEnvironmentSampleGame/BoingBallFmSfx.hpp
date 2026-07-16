#pragma once

/**
 * @file BoingBallFmSfx.hpp
 * Portable 68000 host for the Boing Ball Z80/YM2612 sound driver.
 */

#include "MegaDriveEnvironmentSampleGame/Memory.hpp"

namespace sample::audio {

/**
 * Loads the Boing Ball FM driver into Z80 RAM and posts bounce commands.
 *
 * Every access is a memory-mapped bus operation through memory::Memory:
 * Z80 RAM at $A00000, bus request at $A11100, reset at $A11200. The Z80
 * program itself programs the YM2612 at $4000 on its own bus. No host sound
 * or Z80 API is used. Only the Boing Ball demo uses this path; the main game
 * still uses PsgSoundEffects.
 */
class BoingBallFmSfx final {
  public:
    /** Command bytes written to the Z80 mailbox (must match z80/boing_ball_sfx.s). */
    static constexpr std::uint8_t kCommandIdle = 0;
    static constexpr std::uint8_t kCommandFloor = 1;
    static constexpr std::uint8_t kCommandWall = 2;

    /** 68000-visible addresses for the Z80 control ports and work RAM window. */
    static constexpr memory::Address kZ80RamBase = 0xA00000;
    static constexpr memory::Address kZ80BusRequest = 0xA11100;
    static constexpr memory::Address kZ80Reset = 0xA11200;
    static constexpr std::uint16_t kMailboxOffset = 0x1E00;
    static constexpr std::uint16_t kStatusOffset = 0x1E01;

    explicit BoingBallFmSfx(memory::Memory &memory);

    /**
     * Requests the Z80 bus, copies the ROM-resident driver into Z80 RAM,
     * clears the mailbox, and releases the Z80 to run.
     */
    void initialize();

    /** Posts the floor-bounce command (last command wins). */
    void playFloorBounce();

    /** Posts the wall-bounce command (last command wins). */
    void playWallBounce();

    /** True after a successful initialize(). */
    [[nodiscard]] bool ready() const;

  private:
    void requestBus();
    void releaseBus();
    void holdReset();
    void releaseReset();
    void writeCommand(std::uint8_t command);

    memory::Memory &memory_;
    bool ready_ = false;
};

} // namespace sample::audio
