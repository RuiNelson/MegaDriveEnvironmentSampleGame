#pragma once

/**
 * @file SampleGame.hpp
 * @brief MegaDriveEnvironment host application for the sample game.
 */

#include "MegaDriveEnvironmentSampleGame/audio/PsgSoundEffects.hpp"
#include "MegaDriveEnvironmentSampleGame/controllers/ControllerReader.hpp"
#include "MegaDriveEnvironmentSampleGame/game/GameSession.hpp"
#include "MegaDriveEnvironmentSampleGame/platform/megadrive_environment/EnvironmentMemory.hpp"
#include "system/MegaDriveEnvironment.hpp"

#include <string>

namespace sample {

/**
 * Runs the portable game model inside MegaDriveEnvironment.
 *
 * This class owns the host-specific frame loop and VDP presentation. Gameplay,
 * controller decoding, memory access, and PSG sequences remain in shared
 * objects that are also compiled into the real cartridge ROM.
 */
class SampleGame final : public MegaDriveEnvironment {
  public:
    /**
     * @param romPath Path to the raw 32-Mbit asset ROM loaded into emulated ROM.
     * @param frameLimit Optional number of frames to run; zero means unlimited.
     */
    explicit SampleGame(std::string romPath, unsigned frameLimit = 0);

  protected:
    /** CPU-thread entry point containing the initialize/update/render loop. */
    void run() override;

    /** VBlank callback; releases the CPU thread waiting for the next frame. */
    void vSync() override;

  private:
    /** Configures Mode 5, palettes, tile data, planes, and static HUD text. */
    void initializeGraphics();

    /** Samples input, advances gameplay/audio by one frame, and handles events. */
    void update();

    /** Writes the current model state to Plane A and the sprite table. */
    void render();

    /** Drains stale interrupts and blocks cooperatively until the next VBlank. */
    void waitForVBlank();

    /** Asset-only ROM path retained until the CPU thread starts. */
    std::string romPath_;
    /** Test/CI frame cap; zero keeps the interactive game running. */
    unsigned frameLimit_ = 0;
    /** Number of completed update/render iterations. */
    unsigned frameCount_ = 0;
    /** Set by vSync() and consumed by waitForVBlank(). */
    bool frameReady_ = false;

    /** Adapts the environment address space to the portable memory contract. */
    platform::megadrive_environment::EnvironmentMemory gameMemory_;
    /** Shared memory-mapped three-button controller decoder. */
    controllers::ControllerReader player1Controller_;
    /** Platform-independent entities, scoring, collision, and phase state. */
    game::GameSession session_;
    /** Frame-driven SN76489 effect sequencer using the same memory bus. */
    audio::PsgSoundEffects soundEffects_;
};

} // namespace sample
