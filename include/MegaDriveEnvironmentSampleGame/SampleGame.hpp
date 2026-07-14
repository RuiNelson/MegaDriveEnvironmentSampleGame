#pragma once

/**
 * @file SampleGame.hpp
 * Complete platform-independent sample game.
 */

#include "MegaDriveEnvironmentSampleGame/ControllerReader.hpp"
#include "MegaDriveEnvironmentSampleGame/GameSession.hpp"
#include "MegaDriveEnvironmentSampleGame/Memory.hpp"
#include "MegaDriveEnvironmentSampleGame/PsgSoundEffects.hpp"

namespace sample {

/**
 * Owns the entire game, including input, simulation, sound and VDP rendering.
 *
 * All hardware communication goes through memory::Memory. The exact same class
 * and implementation are therefore compiled for MegaDriveEnvironment and for
 * real hardware; only the injected Memory implementation changes.
 */
class SampleGame final {
  public:
    /** Retains `memory` by reference; the backend must outlive the game. */
    explicit SampleGame(memory::Memory &memory);

    /** Configures the controller, PSG, VDP and initial scene. */
    void initialize();

    /**
     * Waits for VBlank, then advances and renders exactly one frame.
     * @return false only when a host backend cancels its cooperative wait.
     */
    [[nodiscard]] bool runFrame();

    /**
     * Runs the common frame loop.
     *
     * @param frameLimit Number of frames to run, or zero to run forever.
     */
    void run(unsigned frameLimit = 0);

  private:
    /** Configures palettes, tile data, planes and static HUD text. */
    void initializeGraphics();

    /** Samples input, advances gameplay/audio and handles one-frame events. */
    void update();

    /** Writes the current model state to Plane A and the sprite table. */
    void render();

    /** Shared bus used for ROM, controller, PSG and VDP accesses. */
    memory::Memory &memory_;
    /** Memory-mapped three-button controller decoder. */
    controllers::ControllerReader player1Controller_;
    /** Platform-independent entities, scoring, collision and phase state. */
    game::GameSession session_;
    /** Frame-driven SN76489 effect sequencer. */
    audio::PsgSoundEffects soundEffects_;
};

} // namespace sample
