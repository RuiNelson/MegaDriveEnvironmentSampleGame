#pragma once

/**
 * @file SampleGame.hpp
 * Complete platform-independent sample game.
 */

#include "MegaDriveEnvironmentSampleGame/ControllerReader.hpp"
#include "MegaDriveEnvironmentSampleGame/BoingBallDemo.hpp"
#include "MegaDriveEnvironmentSampleGame/BoingBallFmSfx.hpp"
#include "MegaDriveEnvironmentSampleGame/GameSession.hpp"
#include "MegaDriveEnvironmentSampleGame/Memory.hpp"
#include "MegaDriveEnvironmentSampleGame/PsgSoundEffects.hpp"

namespace sample {

/**
 * Owns the entire game, including input, simulation, sound and VDP rendering.
 *
 * All hardware communication goes through sample::memory free functions. The
 * exact same class is compiled for MegaDriveEnvironment and real hardware;
 * only the bound memory backend changes on PC.
 */
class SampleGame final {
  public:
    SampleGame();

    /** Configures the controller, PSG, Z80 FM demo driver, VDP and initial scene. */
    void initialize();

    /**
     * Advances input, gameplay, sound and rendering once per VBlank IRQ.
     * Also finishes the last H-scroll block and resets the HBlank line counter.
     */
    void onVSync();

    /**
     * Applies a raster scroll effect when the VDP raises an HBlank IRQ.
     *
     * The hardware HINT does not report a scanline index, and the host VDP's
     * line argument is not required either: this method advances an internal
     * first-line counter (0, 16, 32, …) so both targets stay in lockstep.
     */
    void onHSync();

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

    /** Memory-mapped three-button controller decoder. */
    controllers::ControllerReader player1Controller_;
    /** Platform-independent entities, scoring, collision and phase state. */
    game::GameSession session_;
    /** Frame-driven SN76489 effect sequencer for the main game. */
    audio::PsgSoundEffects soundEffects_;
    /** Z80/YM2612 bounce effects used only by the Boing Ball demo. */
    audio::BoingBallFmSfx boingBallFmSfx_;
    /** Shared fixed-point and software-rendered Start-screen demo. */
    demo::BoingBallDemo boingBallDemo_;
    /** Moves the Plane B wave vertically without scrolling Plane A. */
    std::uint8_t backgroundWavePhase_ = 0;
    /**
     * First scanline of the next H-scroll block written by onHSync().
     * Reset to 0 on each VBlank; advanced by kHSyncLineBatch per HBlank.
     */
    int nextHScrollLine_ = 0;
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
