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
     * Records that a new video frame began; safe to call directly from IRQ6.
     * On the menu, also advances the vertically animated backdrop gradient.
     */
    void onVSync();

    /**
     * Menu-only HBlank work: advances one scanline of the seven-level blue
     * backdrop gradient. Must stay extremely short; games keep HINT disabled
     * so this is never on the hot path during gameplay or the Boing Ball
     * rasterizer.
     */
    void onHSync();

    /**
     * Runs one pending frame outside interrupt context.
     *
     * Returns false when no VBlank has requested work. The Boing Ball renderer
     * may intentionally occupy visible-line CPU time, so this must never be
     * called from the level-6 interrupt handler.
     */
    [[nodiscard]] bool runPendingFrame();

  private:
    enum class Screen : std::uint8_t {
        Menu,
        Game,
        BoingBall,
    };

    /** Configures the VDP, loads persistent font tiles and shows the menu. */
    void initializeGraphics();

    /** Loads game tiles and restores gameplay palettes, planes and HUD text. */
    void activateGameScreen();

    /**
     * Darkens the display, clears non-font pattern tiles and returns to the
     * menu. Font glyphs remain resident in VRAM across screen transitions.
     */
    void returnToMenu();

    /** Samples input, advances gameplay/audio and handles one-frame events. */
    void update();

    /** Writes the current model state to Plane A and the sprite table. */
    void render();

    /** Sets up the menu palettes, clears planes and enables the sky gradient. */
    void activateMenu();

    /** Turns off HBlank IRQs before a game takes ownership of the VDP. */
    void disableMenuHBlank();

    /** Arms the eight-line backdrop gradient for the menu screen. */
    void enableMenuHBlank();

    /** Draws the menu title, game list and selection cursor. */
    void renderMenu();

    /** Draws the deliberately unavoidable EU cookie-consent notice. */
    void renderCookieBanner();

    /** Erases the cookie notice before normal gameplay is shown. */
    void clearCookieBanner();

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
    /** Set by IRQ6 and consumed before normal frame work begins. */
    volatile bool framePending_ = false;
    /**
     * Next gradient scanline written by onHSync(). Reset to 1 on each menu
     * VBlank; line 0 is applied during VBlank. Volatile because IRQ4 and the
     * main loop both touch it.
     */
    volatile std::uint16_t menuGradientLine_ = 1;
    /** Vertical animation offset, advanced once per menu frame. */
    volatile std::uint16_t menuGradientOffset_ = 0;
    /** Keeps gameplay paused until the player accepts the satirical notice. */
    bool cookieConsentAccepted_ = false;
    /** Prevents the acceptance press from also resetting the game. */
    bool waitingForConsentButtonRelease_ = false;
    /** Requests one cleanup pass when gameplay replaces the notice. */
    bool cookieBannerNeedsClear_ = false;
    /** Current renderer selected by an edge-triggered Start press. */
    Screen screen_ = Screen::Menu;
    /** Converts the level-sensitive Start bit into screen-toggle edges. */
    bool startWasDown_ = false;
    /** Converts the level-sensitive A bit into menu-select edges. */
    bool aWasDown_ = false;
    /** Which game the cursor points to in the menu (0 = Gem, 1 = Boing Ball). */
    std::uint8_t menuSelection_ = 0;
};

} // namespace sample
