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
constexpr std::uint16_t kMenuOceanFirstTile = 102;
constexpr std::uint16_t kEnemyTile = kPlayerTile;

constexpr std::uint16_t kFontTileCount = 95;
constexpr std::uint16_t kPlayerRomTile = 95;
constexpr std::uint16_t kGemRomTile = 99;
constexpr std::uint16_t kFloorRomTile = 100;
constexpr std::uint16_t kMenuOceanFirstRomTile = 101;
constexpr std::uint16_t kMenuOceanTileCount = 20;

constexpr int kCookieBannerFirstRow = 7;
constexpr int kCookieBannerLastRow = 20;
constexpr int kMenuOceanFirstRow = 14;
constexpr int kMenuOceanRowCount = 14;
constexpr const char *kBlankScreenRow = "                                        ";
// CRAM words use the Mega Drive's 0000BBB0GGG0RRR0 channel layout.
constexpr std::uint16_t kTextPalette[16]{
    0x0000, 0x0EEE, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};
constexpr std::uint16_t kMenuTextPalette[16]{
    0x0000, 0x00EE, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
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
constexpr std::uint16_t kMenuOceanPalette[16]{
    0x0000, 0x0600, 0x0800, 0x0A20, 0x0C40, 0x0E60, 0x0E80, 0x0EA0,
    0x0EEE, 0x00EE, 0, 0, 0, 0, 0, 0,
};
// One backdrop color per eight-pixel sky band. Fourteen bands cover exactly
// the upper 112 pixels; Plane B becomes fully opaque ocean at row 14.
constexpr std::uint16_t kMenuSkyGradient[kMenuOceanFirstRow]{
    0x0E86, 0x0E86, 0x0E88, 0x0E88, 0x0EA8, 0x0EA8, 0x0EAA,
    0x0EAA, 0x0ECA, 0x0ECA, 0x0ECC, 0x0ECC, 0x0EEE, 0x0EEE,
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
    if (screen_ == Screen::Menu && cookieConsentAccepted_) {
        nextMenuSkyBand_ = 1;
        vdp::writePaletteColor(0, 0, kMenuSkyGradient[0]);
        vdp::writeRegister(0x0A, 7);
        vdp::writeRegister(0x00, 0x14);
    }
    framePending_ = true;
}

void SampleGame::onHSync() {
    if (screen_ != Screen::Menu || !cookieConsentAccepted_ ||
        nextMenuSkyBand_ >= kMenuOceanFirstRow) {
        return;
    }
    vdp::writePaletteColor(0, 0, kMenuSkyGradient[nextMenuSkyBand_]);
    ++nextMenuSkyBand_;
    if (nextMenuSkyBand_ == kMenuOceanFirstRow) {
        // No more palette work is needed once the beam reaches the opaque
        // lower-half ocean. VBlank re-enables HINT for the next sky.
        vdp::writeRegister(0x00, 0x04);
    }
}

bool SampleGame::runPendingFrame() {
    if (!framePending_) {
        return false;
    }
    // Consume before doing any work so an overrun into the next VBlank leaves
    // a fresh request pending instead of erasing it on return.
    framePending_ = false;
    update();
    render();
    return true;
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
    vdp::loadTilesFromRom(tileRom + kMenuOceanFirstRomTile * 32,
                          kMenuOceanFirstTile, kMenuOceanTileCount);

    activateMenu();
    vdp::finishInitialization();
}

void SampleGame::activateGameScreen() {
    deactivateMenuRaster();
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
    vdp::writeText(vdp::kPlaneA, 2, 26, "D-PAD MOVE   A RESET   START MENU", kFontTile);
}

void SampleGame::activateMenu() {
    deactivateMenuRaster();
    vdp::writeRegister(0x07, 0x00);
    // The Window plane covers the screen and acts as the frontmost text layer.
    // Its transparent pixels reveal Plane B's sky/ocean composition.
    vdp::writeRegister(0x11, 20); // left of cell 40: full 320-pixel width
    vdp::writeRegister(0x12, 0x00);
    vdp::loadPalette(0, kTextPalette);
    vdp::loadPalette(1, kMenuTextPalette);
    vdp::loadPalette(2, kMenuOceanPalette);
    vdp::loadPalette(3, kFloorPalette);

    vdp::fillPlaneArea(vdp::kPlaneA, 0, 0, 40, 28, vdp::tileDescriptor(0));
    vdp::fillPlaneArea(vdp::kPlaneB, 0, 0, 40, 28, vdp::tileDescriptor(0));
    vdp::fillPlaneArea(vdp::kWindowPlane, 0, 0, 40, 28, vdp::tileDescriptor(0));
    renderMenuOcean();
    vdp::setHorizontalScroll(0, 0);
    vdp::writePaletteColor(0, 0, kMenuSkyGradient[0]);
    // Hide all sprites while on the menu.
    for (int i = 0; i < 3; ++i) {
        vdp::writeSprite(i, -32, -32, 1, 1, 0, 0, 0);
    }
}

void SampleGame::deactivateMenuRaster() {
    vdp::writeRegister(0x00, 0x04); // HBlank IRQ disabled
    nextMenuSkyBand_ = 1;
}

void SampleGame::activateMenuRaster() {
    nextMenuSkyBand_ = 1;
    vdp::writePaletteColor(0, 0, kMenuSkyGradient[0]);
    vdp::writeRegister(0x0A, 7);    // one HBlank IRQ per eight scanlines
    vdp::writeRegister(0x00, 0x14); // enable HBlank IRQ only for this menu
}

void SampleGame::renderMenuOcean() {
    for (int row = 0; row < kMenuOceanRowCount; ++row) {
        int tileGroup = 0; // distant calm water
        if (row == 3 || row == 7) {
            tileGroup = 1; // middle-distance white crest
        } else if (row >= 4 && row <= 6) {
            tileGroup = 3; // calm middle water
        } else if (row == 11) {
            tileGroup = 2; // large foreground white crest
        } else if (row >= 8) {
            tileGroup = 4; // deep foreground water
        }
        for (int column = 0; column < 40; ++column) {
            const auto variant = static_cast<std::uint16_t>((column + row) & 3);
            const auto tile = static_cast<std::uint16_t>(
                kMenuOceanFirstTile + tileGroup * 4 + variant);
            vdp::writePlaneTile(
                vdp::kPlaneB, column, kMenuOceanFirstRow + row,
                vdp::tileDescriptor(tile, 2));
        }
    }
}

void SampleGame::update() {
    // Advance the previous effect before a newly emitted event replaces it.
    soundEffects_.update();
    auto controls = player1Controller_.read();
    const bool startPressed = controls.start && !startWasDown_;
    startWasDown_ = controls.start;
    const bool aPressed = controls.a && !aWasDown_;
    aWasDown_ = controls.a;

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

    if (screen_ == Screen::Menu) {
        if (controls.up && menuSelection_ > 0) {
            --menuSelection_;
        }
        if (controls.down && menuSelection_ < 1) {
            ++menuSelection_;
        }
    }

    // Do not leak the acceptance input into GameSession, where A/Start is the
    // reset command. Resume only after the player releases the chosen button.
    if (waitingForConsentButtonRelease_) {
        if (controls.a || controls.start) {
            return;
        }
        waitingForConsentButtonRelease_ = false;
    }

    if (screen_ == Screen::Menu) {
        if (aPressed) {
            if (menuSelection_ == 0) {
                screen_ = Screen::Game;
                activateGameScreen();
            } else {
                screen_ = Screen::BoingBall;
                deactivateMenuRaster();
                boingBallDemo_.activate();
            }
        }
        return;
    }

    if (screen_ == Screen::BoingBall) {
        if (startPressed) {
            screen_ = Screen::Menu;
            activateMenu();
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
        screen_ = Screen::Menu;
        activateMenu();
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
    if (!cookieConsentAccepted_) {
        renderCookieBanner();
        return;
    }

    if (cookieBannerNeedsClear_) {
        clearCookieBanner();
        cookieBannerNeedsClear_ = false;
    }

    if (screen_ == Screen::Menu) {
        renderMenu();
        return;
    }

    if (screen_ == Screen::BoingBall) {
        boingBallDemo_.render();
        return;
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

void SampleGame::renderMenu() {
    constexpr std::uint8_t kMenuTextPaletteIndex = 1;
    vdp::writeText(vdp::kWindowPlane, 12, 8, "SELECT A GAME", kFontTile,
                   kMenuTextPaletteIndex);

    const char *gemCursor = (menuSelection_ == 0) ? ">" : " ";
    const char *boingCursor = (menuSelection_ == 1) ? ">" : " ";
    vdp::writeText(vdp::kWindowPlane, 11, 12, gemCursor, kFontTile,
                   kMenuTextPaletteIndex);
    vdp::writeText(vdp::kWindowPlane, 13, 12, "GEM COLLECTING", kFontTile,
                   kMenuTextPaletteIndex);
    vdp::writeText(vdp::kWindowPlane, 11, 14, boingCursor, kFontTile,
                   kMenuTextPaletteIndex);
    vdp::writeText(vdp::kWindowPlane, 13, 14, "BOING BALL", kFontTile,
                   kMenuTextPaletteIndex);

    vdp::writeText(vdp::kWindowPlane, 7, 20, "UP/DOWN SELECT   A START",
                   kFontTile, kMenuTextPaletteIndex);
    activateMenuRaster();
}

void SampleGame::renderCookieBanner() {
    vdp::writeText(vdp::kWindowPlane, 1, 7, "+------------------------------------+", kFontTile);
    vdp::writeText(vdp::kWindowPlane, 1, 8, "|        COOKIE CONSENT              |", kFontTile);
    vdp::writeText(vdp::kWindowPlane, 1, 9, "|                                    |", kFontTile);
    vdp::writeText(vdp::kWindowPlane, 1, 10, "| THIS GAME WAS MADE IN THE          |", kFontTile);
    vdp::writeText(vdp::kWindowPlane, 1, 11, "| EUROPEAN UNION.                    |", kFontTile);
    vdp::writeText(vdp::kWindowPlane, 1, 12, "|                                    |", kFontTile);
    vdp::writeText(vdp::kWindowPlane, 1, 13, "| WE USE ESSENTIAL COOKIES TO        |", kFontTile);
    vdp::writeText(vdp::kWindowPlane, 1, 14, "| REMEMBER YOUR HIGH SCORE.          |", kFontTile);
    vdp::writeText(vdp::kWindowPlane, 1, 15, "|                                    |", kFontTile);
    vdp::writeText(vdp::kWindowPlane, 1, 16, "| [A] ACCEPT ALL                     |", kFontTile);
    vdp::writeText(vdp::kWindowPlane, 1, 17, "| [START] ALSO ACCEPT ALL            |", kFontTile);
    vdp::writeText(vdp::kWindowPlane, 1, 18, "|                                    |", kFontTile);
    vdp::writeText(vdp::kWindowPlane, 1, 19, "| *YOUR CHOICE IS VERY IMPORTANT     |", kFontTile);
    vdp::writeText(vdp::kWindowPlane, 1, 20, "+------------------------------------+", kFontTile);

    // An empty sprite list keeps the world hidden while consent blocks play.
    vdp::writeSprite(0, -32, -32, 1, 1, 0, 0, 0);
}

void SampleGame::clearCookieBanner() {
    for (int row = kCookieBannerFirstRow; row <= kCookieBannerLastRow; ++row) {
        vdp::writeText(vdp::kWindowPlane, 0, row, kBlankScreenRow, kFontTile);
    }
}

} // namespace sample
