#pragma once

/**
 * @file SampleGame.hpp
 * Complete platform-independent sample game.
 */

#include "MegaDriveEnvironmentSampleGame/ControllerReader.hpp"
#include "MegaDriveEnvironmentSampleGame/BoingBallDemo.hpp"
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

    /** Advances input, gameplay, sound and rendering once per VBlank IRQ. */
    void onVSync();

    /**
     * Applies a raster scroll effect when the VDP raises an HBlank IRQ.
     * @param scanline Scanline that triggered the interrupt.
     */
    void onHSync(int scanline);

  private:
    enum class Screen : std::uint8_t {
        Game,
        BoingBall,
    };

    /** Configures palettes, tile data, planes and static HUD text. */
    void initializeGraphics();

    /** Restores gameplay palettes, visible Plane A cells and static labels. */
    void activateGameScreen();

    /** Samples input, advances gameplay/audio and handles one-frame events. */
    void update();

    /** Writes the current model state to Plane A and the sprite table. */
    void render();

    /** Draws the deliberately unavoidable EU cookie-consent notice. */
    void renderCookieBanner();

    /** Erases the cookie notice before normal gameplay is shown. */
    void clearCookieBanner();

    /** Writes one contiguous block of Plane B per-scanline wave offsets. */
    void writeBackgroundWaveBlock(int firstScanline);

    /** Shared bus used for ROM, controller, PSG and VDP accesses. */
    memory::Memory &memory_;
    /** Memory-mapped three-button controller decoder. */
    controllers::ControllerReader player1Controller_;
    /** Platform-independent entities, scoring, collision and phase state. */
    game::GameSession session_;
    /** Frame-driven SN76489 effect sequencer. */
    audio::PsgSoundEffects soundEffects_;
    /** Shared fixed-point and software-rendered Start-screen demo. */
    demo::BoingBallDemo boingBallDemo_;
    /** Moves the Plane B wave vertically without scrolling Plane A. */
    std::uint8_t backgroundWavePhase_ = 0;
    /** Keeps gameplay paused until the player accepts the satirical notice. */
    bool cookieConsentAccepted_ = false;
    /** Prevents the acceptance press from also resetting the game. */
    bool waitingForConsentButtonRelease_ = false;
    /** Requests one cleanup pass when gameplay replaces the notice. */
    bool cookieBannerNeedsClear_ = false;
    /** Current renderer selected by an edge-triggered Start press. */
    Screen screen_ = Screen::Game;
    /** Converts the level-sensitive controller bit into screen-toggle edges. */
    bool startWasDown_ = false;
};

} // namespace sample
