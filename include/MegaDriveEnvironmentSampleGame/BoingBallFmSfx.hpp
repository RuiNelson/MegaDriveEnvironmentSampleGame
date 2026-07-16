#pragma once

/**
 * @file BoingBallFmSfx.hpp
 * 68000 host for the Boing Ball Z80/YM2612 sound driver.
 */

#include "MegaDriveEnvironmentSampleGame/Memory.hpp"

namespace sample::audio {

/**
 * Loads the Boing Ball FM driver into Z80 RAM and posts bounce commands.
 *
 * All busreq/reset/Z80-RAM/YM traffic goes through memory::Memory so the same
 * class runs on MegaDriveEnvironment and on real hardware. Only the Boing Ball
 * demo uses this path; the main game still uses PsgSoundEffects.
 */
class BoingBallFmSfx final {
  public:
    /** Command bytes written to the Z80 mailbox (must match z80/boing_ball_sfx.s). */
    static constexpr std::uint8_t kCommandIdle = 0;
    static constexpr std::uint8_t kCommandFloor = 1;
    static constexpr std::uint8_t kCommandWall = 2;

    /** Z80 work-RAM addresses observed by the 68000 at $A00000+. */
    static constexpr memory::Address kZ80RamBase = 0xA00000;
    static constexpr memory::Address kZ80BusRequest = 0xA11100;
    static constexpr memory::Address kZ80Reset = 0xA11200;
    static constexpr std::uint16_t kMailboxOffset = 0x1FF0;
    static constexpr std::uint16_t kStatusOffset = 0x1FF1;

    explicit BoingBallFmSfx(memory::Memory &memory);

    /**
     * Bus-requests the Z80, copies the ROM-resident driver, clears the mailbox
     * and releases the Z80 to run.
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
