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
// Font glyphs stay resident across the menu and every game; other pattern tiles
// are loaded on enter and wiped on exit.
constexpr std::uint16_t kFontTile = 1;
constexpr std::uint16_t kFontTileCount = 95;
constexpr std::uint16_t kFirstReusableTile = kFontTile + kFontTileCount;
// Pattern data ends at the Window plane base (0xB000). Clearing this span
// removes game tiles without touching name tables or the SAT.
constexpr std::uint16_t kPatternTileLimit = vdp::kWindowPlane / 32;
constexpr std::uint16_t kReusableTileCount =
    static_cast<std::uint16_t>(kPatternTileLimit - kFirstReusableTile);

constexpr std::uint16_t kPlayerTile = 96;
constexpr std::uint16_t kGemTile = 100;
constexpr std::uint16_t kFloorTile = 101;
constexpr std::uint16_t kEnemyTile = kPlayerTile;

constexpr std::uint16_t kPlayerRomTile = 95;
constexpr std::uint16_t kGemRomTile = 99;
constexpr std::uint16_t kFloorRomTile = 100;

constexpr int kCookieBannerFirstRow = 7;
constexpr int kCookieBannerLastRow = 20;

// Menu sky gradient: one CRAM update every eight scanlines (HINT reload = 7).
// 28 bands × 8 lines = 224 visible NTSC lines. Games leave HINT disabled so the
// Boing Ball rasterizer keeps its full visible-line budget.
constexpr std::uint8_t kMenuGradientBandCount = 28;
constexpr std::uint8_t kMenuHintReload = 7; // interrupt every (reload + 1) lines

// CRAM words use the Mega Drive's 0000BBB0GGG0RRR0 channel layout.
// Band 0 is pure blue (B=7); band 27 is white (R=G=B=7). Intermediate bands
// raise red and green together so the sky stays in the blue family.
consteval std::uint16_t menuGradientColor(unsigned band) {
    constexpr unsigned kLastBand = kMenuGradientBandCount - 1u;
    const unsigned t = (band * 7u) / kLastBand;
    return static_cast<std::uint16_t>(0x0E00u | (t << 5) | (t << 1));
}

struct MenuGradientTable {
    std::uint16_t colors[kMenuGradientBandCount]{};
    consteval MenuGradientTable() {
        for (unsigned band = 0; band < kMenuGradientBandCount; ++band) {
            colors[band] = menuGradientColor(band);
        }
    }
};
inline constexpr MenuGradientTable kMenuGradient{};

constexpr std::uint16_t kTextPalette[16]{
    0x0000, 0x0EEE, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};
// Palette 0 is reserved for the menu backdrop, whose color 0 is changed by
// the HBlank gradient. Keep menu and consent text in a separate palette so
// raster updates can never affect its colour.
constexpr std::uint8_t kMenuTextPalette = 1;
// Explicit zeros keep freestanding builds from emitting memset.
constexpr std::uint16_t kBlackPalette[16]{
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
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

/** Writes palette 0, color 0 (backdrop) with three fixed VDP port stores. */
[[gnu::always_inline]] inline void writeBackdropColor(std::uint16_t color) {
    // CRAM write address 0: control words are constant, so the HBlank path
    // never recomputes command encoding.
    memory::write16(vdp::kControlPort, 0xC000);
    memory::write16(vdp::kControlPort, 0x0000);
    memory::write16(vdp::kDataPort, color);
}

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
    if (screen_ == Screen::Menu) {
        // Restart the gradient for the new frame. Band 0 covers lines 0-7;
        // subsequent bands are applied by onHSync() every eight lines.
        menuGradientBand_ = 1;
        writeBackdropColor(kMenuGradient.colors[0]);
        vdp::writeRegister(0x0A, kMenuHintReload);
        vdp::writeRegister(0x00, 0x14); // full CRAM + HINT
    }
    framePending_ = true;
}

void SampleGame::onHSync() {
    // Games keep HINT disabled, so this body only runs on the menu. Keep every
    // path branch-light: one load, three port writes, one increment, and at
    // most one register write when the last band has been painted.
    const auto band = menuGradientBand_;
    if (band >= kMenuGradientBandCount) {
        return;
    }

    writeBackdropColor(kMenuGradient.colors[band]);
    const auto next = static_cast<std::uint8_t>(band + 1u);
    menuGradientBand_ = next;
    if (next >= kMenuGradientBandCount) {
        // No further palette work this frame; free the rest of the active
        // display for the main loop until the next VBlank re-arms HINT.
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

    // Only the font glyphs are shared by the menu and every game. Game-specific
    // pattern tiles are uploaded when a screen is activated.
    const auto tileRom = static_cast<memory::Address>(assets::kTilesOffset);
    vdp::loadTilesFromRom(tileRom, kFontTile, kFontTileCount);

    activateMenu();
    vdp::finishInitialization();
}

void SampleGame::activateGameScreen() {
    disableMenuHBlank();

    // Blank while patterns and planes change so the transfer is never visible.
    vdp::writeRegister(0x01, 0x14); // display off, DMA, Mode 5

    // Player, gem and floor patterns live only for this screen.
    const auto tileRom = static_cast<memory::Address>(assets::kTilesOffset);
    vdp::loadTilesFromRom(tileRom + kPlayerRomTile * 32, kPlayerTile, 4);
    vdp::loadTilesFromRom(tileRom + kGemRomTile * 32, kGemTile, 1);
    vdp::loadTilesFromRom(tileRom + kFloorRomTile * 32, kFloorTile, 1);

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

    vdp::writeRegister(0x01, 0x74); // display, DMA, Mode 5, VBlank IRQ
}

void SampleGame::returnToMenu() {
    disableMenuHBlank();
    // Blank the display so the player never sees partial plane or tile cleanup.
    vdp::writeRegister(0x01, 0x14); // display off, DMA, Mode 5
    vdp::clearTiles(kFirstReusableTile, kReusableTileCount);
    screen_ = Screen::Menu;
    activateMenu();
    vdp::writeRegister(0x01, 0x74); // display, DMA, Mode 5, VBlank IRQ
}

void SampleGame::activateMenu() {
    vdp::writeRegister(0x07, 0x00); // backdrop = palette 0, color 0
    vdp::writeRegister(0x11, 0x00);
    vdp::writeRegister(0x12, 0x00);
    vdp::loadPalette(0, kBlackPalette);
    // Palette 0 is reserved for the HBlank-driven backdrop. Keep menu text in
    // palette 1 so the gradient cannot change its colour.
    vdp::loadPalette(kMenuTextPalette, kTextPalette);
    vdp::loadPalette(2, kBlackPalette);
    vdp::loadPalette(3, kBlackPalette);

    // Blank tiles are transparent, so the HBlank-driven backdrop shows through.
    vdp::fillPlaneArea(vdp::kPlaneA, 0, 0, 40, 28, vdp::tileDescriptor(0));
    vdp::fillPlaneArea(vdp::kPlaneB, 0, 0, 40, 28, vdp::tileDescriptor(0));
    // Hide every sprite the games may have linked, including Boing Ball's shadow.
    for (int i = 0; i < 20; ++i) {
        vdp::writeSprite(i, -32, -32, 1, 1, 0, 0, 0);
    }

    enableMenuHBlank();
}

void SampleGame::disableMenuHBlank() {
    vdp::writeRegister(0x00, 0x04); // full CRAM, HINT off
    menuGradientBand_ = 1;
}

void SampleGame::enableMenuHBlank() {
    menuGradientBand_ = 1;
    writeBackdropColor(kMenuGradient.colors[0]);
    vdp::writeRegister(0x0A, kMenuHintReload);
    vdp::writeRegister(0x00, 0x14); // full CRAM + HINT
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
            // Drop HINT before any game owns the VDP or spends visible-line CPU.
            disableMenuHBlank();
            if (menuSelection_ == 0) {
                screen_ = Screen::Game;
                activateGameScreen();
            } else {
                screen_ = Screen::BoingBall;
                boingBallDemo_.activate();
            }
        }
        return;
    }

    if (screen_ == Screen::BoingBall) {
        if (startPressed) {
            returnToMenu();
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
        returnToMenu();
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
    vdp::writeText(vdp::kPlaneA, 12, 8, "SELECT A GAME", kFontTile, kMenuTextPalette);

    const char *gemCursor = (menuSelection_ == 0) ? ">" : " ";
    const char *boingCursor = (menuSelection_ == 1) ? ">" : " ";
    vdp::writeText(vdp::kPlaneA, 11, 12, gemCursor, kFontTile, kMenuTextPalette);
    vdp::writeText(vdp::kPlaneA, 13, 12, "GEM COLLECTING", kFontTile, kMenuTextPalette);
    vdp::writeText(vdp::kPlaneA, 11, 14, boingCursor, kFontTile, kMenuTextPalette);
    vdp::writeText(vdp::kPlaneA, 13, 14, "BOING BALL", kFontTile, kMenuTextPalette);

    vdp::writeText(vdp::kPlaneA, 7, 20, "UP/DOWN SELECT   A START", kFontTile, kMenuTextPalette);
}

void SampleGame::renderCookieBanner() {
    vdp::writeText(vdp::kPlaneA, 1, 7, "+------------------------------------+", kFontTile, kMenuTextPalette);
    vdp::writeText(vdp::kPlaneA, 1, 8, "|        COOKIE CONSENT              |", kFontTile, kMenuTextPalette);
    vdp::writeText(vdp::kPlaneA, 1, 9, "|                                    |", kFontTile, kMenuTextPalette);
    vdp::writeText(vdp::kPlaneA, 1, 10, "| THIS GAME WAS MADE IN THE          |", kFontTile, kMenuTextPalette);
    vdp::writeText(vdp::kPlaneA, 1, 11, "| EUROPEAN UNION.                    |", kFontTile, kMenuTextPalette);
    vdp::writeText(vdp::kPlaneA, 1, 12, "|                                    |", kFontTile, kMenuTextPalette);
    vdp::writeText(vdp::kPlaneA, 1, 13, "| WE USE ESSENTIAL COOKIES TO        |", kFontTile, kMenuTextPalette);
    vdp::writeText(vdp::kPlaneA, 1, 14, "| REMEMBER YOUR HIGH SCORE.          |", kFontTile, kMenuTextPalette);
    vdp::writeText(vdp::kPlaneA, 1, 15, "|                                    |", kFontTile, kMenuTextPalette);
    vdp::writeText(vdp::kPlaneA, 1, 16, "| [A] ACCEPT ALL                     |", kFontTile, kMenuTextPalette);
    vdp::writeText(vdp::kPlaneA, 1, 17, "| [START] ALSO ACCEPT ALL            |", kFontTile, kMenuTextPalette);
    vdp::writeText(vdp::kPlaneA, 1, 18, "|                                    |", kFontTile, kMenuTextPalette);
    vdp::writeText(vdp::kPlaneA, 1, 19, "| *YOUR CHOICE IS VERY IMPORTANT     |", kFontTile, kMenuTextPalette);
    vdp::writeText(vdp::kPlaneA, 1, 20, "+------------------------------------+", kFontTile, kMenuTextPalette);

    // An empty sprite list keeps the world hidden while consent blocks play.
    vdp::writeSprite(0, -32, -32, 1, 1, 0, 0, 0);
}

void SampleGame::clearCookieBanner() {
    // Clear the name-table cells directly. Writing space glyphs leaves the
    // font tile and its attributes in place, which can expose stale banner
    // pixels while the next screen is being drawn.
    vdp::fillPlaneArea(vdp::kPlaneA,
                       0,
                       kCookieBannerFirstRow,
                       vdp::kPlaneWidth,
                       kCookieBannerLastRow - kCookieBannerFirstRow + 1,
                       vdp::tileDescriptor(0));
}

} // namespace sample
