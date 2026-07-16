/**
 * @file SampleGame.cpp
 * Complete game implementation shared by PC and real-hardware builds.
 */

#include "MegaDriveEnvironmentSampleGame/SampleGame.hpp"

#include "MegaDriveEnvironmentSampleGame/VdpUtils.hpp"

#include "AssetLayout.hpp"

namespace sample {
namespace {

// Tile zero remains blank. The asset ROM stores tiles densely from zero, while
// VRAM starts them at one, so the following ROM and VRAM indices differ by one.
constexpr std::uint16_t kFontTile = 1;
constexpr std::uint16_t kPlayerTile = 96;
constexpr std::uint16_t kGemTile = 100;
constexpr std::uint16_t kFloorTile = 101;
constexpr std::uint16_t kEnemyTile = kPlayerTile;

constexpr std::uint16_t kFontTileCount = 95;
constexpr std::uint16_t kPlayerRomTile = 95;
constexpr std::uint16_t kGemRomTile = 99;
constexpr std::uint16_t kFloorRomTile = 100;

constexpr int kCookieBannerFirstRow = 7;
constexpr int kCookieBannerLastRow = 20;
constexpr const char *kBlankScreenRow = "                                        ";
constexpr int kVisibleScanlineCount = 224;

// CRAM words use the Mega Drive's 0000BBB0GGG0RRR0 channel layout.
constexpr std::uint16_t kTextPalette[16]{
    0x0000, 0x0EEE, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};
constexpr std::uint16_t kPlayerPalette[16]{
    0x0000, 0x0008, 0x00EE, 0x0EEE, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};
constexpr std::uint16_t kGemPalette[16]{
    0x0000, 0x0080, 0x00E0, 0x00EE, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};
constexpr std::uint16_t kFloorPalette[16]{
    0x0000, 0x0222, 0x000E, 0x0EEE, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

} // namespace

SampleGame::SampleGame() : player1Controller_(controllers::Player::One) {
}

void SampleGame::initialize() {
    player1Controller_.initialize();
    soundEffects_.initialize();
    boingBallFmSfx_.initialize();
    initializeGraphics();
    render();
}

void SampleGame::onVSync() {
    // IRQ6 can pre-empt the final IRQ4 at line 223 on real hardware. Populate
    // that last block here during VBlank so both targets complete all 224
    // per-line entries before the next displayed frame.
    writeBackgroundWaveBlock(kVisibleScanlineCount - vdp::kHSyncLineBatch);
    // Restart the software line counter; HINT itself never reports a line index.
    nextHScrollLine_ = 0;
    update();
    render();
    backgroundWavePhase_ = static_cast<std::uint8_t>((backgroundWavePhase_ + 1u) & 0x3Fu);
}

void SampleGame::onHSync() {
    // Skip once the final visible block is reserved for onVSync() (lines 208-223).
    if (nextHScrollLine_ >= kVisibleScanlineCount - vdp::kHSyncLineBatch) {
        return;
    }

    // One address command starts a contiguous block, then VDP auto-increment
    // advances through all Plane A / Plane B pairs. This keeps the real 68000
    // IRQ short enough while preserving an independent offset for every line.
    writeBackgroundWaveBlock(nextHScrollLine_);
    nextHScrollLine_ += vdp::kHSyncLineBatch;
}

void SampleGame::writeBackgroundWaveBlock(int firstScanline) {
    vdp::beginHorizontalScrollLines(firstScanline);
    const int endScanline = firstScanline + vdp::kHSyncLineBatch;
    if (screen_ == Screen::BoingBall) {
        for (int currentScanline = firstScanline; currentScanline < endScanline; ++currentScanline) {
            vdp::appendHorizontalScrollLine(0, 0);
        }
        return;
    }

    for (int currentScanline = firstScanline; currentScanline < endScanline; ++currentScanline) {
        const auto step = static_cast<unsigned>((currentScanline + backgroundWavePhase_) & 0x3F);
        int offset;
        if (step < 16u) {
            offset = static_cast<int>(step);
        } else if (step < 32u) {
            offset = static_cast<int>(32u - step);
        } else if (step < 48u) {
            offset = -static_cast<int>(step - 32u);
        } else {
            offset = -static_cast<int>(64u - step);
        }
        vdp::appendHorizontalScrollLine(0, static_cast<std::uint16_t>(offset));
    }
}

void SampleGame::initializeGraphics() {
    vdp::initialize();
    boingBallDemo_.initialize();

    // Copy only the required spans even though all assets share one ROM blob.
    const auto tileRom = static_cast<memory::Address>(assets::kTilesOffset);
    vdp::loadTilesFromRom(tileRom, kFontTile, kFontTileCount);
    vdp::loadTilesFromRom(tileRom + kPlayerRomTile * 32, kPlayerTile, 4);
    vdp::loadTilesFromRom(tileRom + kGemRomTile * 32, kGemTile, 1);
    vdp::loadTilesFromRom(tileRom + kFloorRomTile * 32, kFloorTile, 1);

    activateGameScreen();
    vdp::finishInitialization();
}

void SampleGame::activateGameScreen() {
    vdp::writeRegister(0x07, 0x00);
    vdp::writeRegister(0x11, 0x00);
    vdp::writeRegister(0x12, 0x00); // disable the demo's bottom Window plane
    vdp::loadPalette(0, kTextPalette);
    vdp::loadPalette(1, kPlayerPalette);
    vdp::loadPalette(2, kGemPalette);
    vdp::loadPalette(3, kFloorPalette);

    vdp::fillPlaneArea(vdp::kPlaneA, 0, 0, 40, 28, vdp::tileDescriptor(0));
    vdp::fillPlaneArea(vdp::kPlaneB, 0, 0, 40, 28,
                       vdp::tileDescriptor(kFloorTile, 3));
    vdp::writeText(vdp::kPlaneA, 2, 1, "MEGADRIVE ENVIRONMENT SAMPLE", kFontTile);
    vdp::writeText(vdp::kPlaneA, 2, 26, "D-PAD MOVE   A RESET   START DEMO", kFontTile);
}

void SampleGame::update() {
    // Advance the previous effect before a newly emitted event replaces it.
    soundEffects_.update();
    auto controls = player1Controller_.read();
    const bool startPressed = controls.start && !startWasDown_;
    startWasDown_ = controls.start;

    if (!cookieConsentAccepted_) {
        // In keeping with the joke, A and Start are presented as different
        // choices but both accept exactly the same terms.
        if (controls.a || controls.start) {
            cookieConsentAccepted_ = true;
            waitingForConsentButtonRelease_ = true;
            cookieBannerNeedsClear_ = true;
        }
        return;
    }

    // Do not leak the acceptance input into GameSession, where A/Start is the
    // reset command. Resume only after the player releases the chosen button.
    if (waitingForConsentButtonRelease_) {
        if (controls.a || controls.start) {
            return;
        }
        waitingForConsentButtonRelease_ = false;
    }

    if (screen_ == Screen::BoingBall) {
        if (startPressed) {
            screen_ = Screen::Game;
            activateGameScreen();
            return;
        }
        const auto events = boingBallDemo_.update(controls.up, controls.down);
        if (events.hitFloor) {
            boingBallFmSfx_.playFloorBounce();
        } else if (events.hitWall) {
            boingBallFmSfx_.playWallBounce();
        }
        return;
    }

    if (startPressed) {
        screen_ = Screen::BoingBall;
        boingBallDemo_.activate();
        return;
    }

    // Start belongs to screen navigation; A remains the gameplay reset input.
    controls.start = false;

    const auto events = session_.update(controls);
    if (events.restarted()) {
        soundEffects_.playRestart();
    } else if (events.gameOverStarted()) {
        soundEffects_.playGameOver();
    } else if (events.collectedGem()) {
        soundEffects_.playGemCollected();
    }
}

void SampleGame::render() {
    if (screen_ == Screen::BoingBall) {
        boingBallDemo_.render();
        return;
    }

    if (!cookieConsentAccepted_) {
        renderCookieBanner();
        return;
    }

    if (cookieBannerNeedsClear_) {
        clearCookieBanner();
        cookieBannerNeedsClear_ = false;
    }

    // Avoid snprintf, division and initialized local arrays (which can make a
    // freestanding compiler request memcpy) in this shared renderer.
    vdp::writeText(vdp::kPlaneA, 15, 3, "SCORE ", kFontTile);
    auto score = session_.score();
    char hundreds = '0';
    char tens = '0';
    while (score >= 100) {
        ++hundreds;
        score = static_cast<std::uint16_t>(score - 100);
    }
    while (score >= 10) {
        ++tens;
        score = static_cast<std::uint16_t>(score - 10);
    }
    const char ones = static_cast<char>('0' + score);
    vdp::writePlaneTile(vdp::kPlaneA, 21, 3,
                        vdp::tileDescriptor(static_cast<std::uint16_t>(kFontTile + hundreds - 0x20), 0, true));
    vdp::writePlaneTile(vdp::kPlaneA, 22, 3,
                        vdp::tileDescriptor(static_cast<std::uint16_t>(kFontTile + tens - 0x20), 0, true));
    vdp::writePlaneTile(vdp::kPlaneA, 23, 3,
                        vdp::tileDescriptor(static_cast<std::uint16_t>(kFontTile + ones - 0x20), 0, true));

    const char *message =
        session_.phase() == game::Phase::GameOver ? "GAME OVER  A/START RESTART" : "                           ";
    vdp::writeText(vdp::kPlaneA, 7, 13, message, kFontTile);

    const auto &player = session_.player();
    const auto &gem = session_.gem();
    const auto &enemy = session_.enemy();
    // SAT links form 0 -> 1 -> 2 -> 0; link zero terminates traversal.
    vdp::writeSprite(0, player.x(), player.y(), 2, 2, kPlayerTile, 1, 1);
    vdp::writeSprite(1, gem.x(), gem.y(), 1, 1, kGemTile, 2, 2);
    vdp::writeSprite(2, enemy.x(), enemy.y(), 2, 2, kEnemyTile, 3, 0);
}

void SampleGame::renderCookieBanner() {
    vdp::writeText(vdp::kPlaneA, 1, 7, "+------------------------------------+", kFontTile);
    vdp::writeText(vdp::kPlaneA, 1, 8, "|        COOKIE CONSENT              |", kFontTile);
    vdp::writeText(vdp::kPlaneA, 1, 9, "|                                    |", kFontTile);
    vdp::writeText(vdp::kPlaneA, 1, 10, "| THIS GAME WAS MADE IN THE          |", kFontTile);
    vdp::writeText(vdp::kPlaneA, 1, 11, "| EUROPEAN UNION.                    |", kFontTile);
    vdp::writeText(vdp::kPlaneA, 1, 12, "|                                    |", kFontTile);
    vdp::writeText(vdp::kPlaneA, 1, 13, "| WE USE ESSENTIAL COOKIES TO        |", kFontTile);
    vdp::writeText(vdp::kPlaneA, 1, 14, "| REMEMBER YOUR HIGH SCORE.          |", kFontTile);
    vdp::writeText(vdp::kPlaneA, 1, 15, "|                                    |", kFontTile);
    vdp::writeText(vdp::kPlaneA, 1, 16, "| [A] ACCEPT ALL                     |", kFontTile);
    vdp::writeText(vdp::kPlaneA, 1, 17, "| [START] ALSO ACCEPT ALL            |", kFontTile);
    vdp::writeText(vdp::kPlaneA, 1, 18, "|                                    |", kFontTile);
    vdp::writeText(vdp::kPlaneA, 1, 19, "| *YOUR CHOICE IS VERY IMPORTANT     |", kFontTile);
    vdp::writeText(vdp::kPlaneA, 1, 20, "+------------------------------------+", kFontTile);

    // An empty sprite list keeps the world hidden while consent blocks play.
    vdp::writeSprite(0, -32, -32, 1, 1, 0, 0, 0);
}

void SampleGame::clearCookieBanner() {
    for (int row = kCookieBannerFirstRow; row <= kCookieBannerLastRow; ++row) {
        vdp::writeText(vdp::kPlaneA, 0, row, kBlankScreenRow, kFontTile);
    }
}

} // namespace sample
