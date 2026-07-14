#include "MegaDriveEnvironmentSampleGame/SampleGame.hpp"

#include "MegaDriveEnvironmentSampleGame/VdpUtils.hpp"

#include <SDL3/SDL.h>

#include <array>
#include <cstdio>
#include <utility>

namespace sample {
namespace {

constexpr std::uint16_t kFontTile = 1;
constexpr std::uint16_t kPlayerTile = 96;
constexpr std::uint16_t kGemTile = 100;
constexpr std::uint16_t kFloorTile = 101;
constexpr std::uint16_t kEnemyTile = kPlayerTile;

constexpr std::uint16_t kFontTileCount = 95;
constexpr std::uint16_t kPlayerRomTile = 95;
constexpr std::uint16_t kGemRomTile = 99;
constexpr std::uint16_t kFloorRomTile = 100;
constexpr std::uint32_t kRomSize = 4 * 1024 * 1024;
constexpr std::uint32_t kRomTileCount = 101;
constexpr std::uint32_t kRomTileData = kRomSize - kRomTileCount * 32;

constexpr std::array<std::uint16_t, 16> kTextPalette{
    0x0000, 0x0EEE, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};
constexpr std::array<std::uint16_t, 16> kPlayerPalette{
    0x0000, 0x0008, 0x00EE, 0x0EEE, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};
constexpr std::array<std::uint16_t, 16> kGemPalette{
    0x0000, 0x0080, 0x00E0, 0x00EE, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};
constexpr std::array<std::uint16_t, 16> kFloorPalette{
    0x0000, 0x0222, 0x000E, 0x0EEE, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

} // namespace

SampleGame::SampleGame(std::string romPath, unsigned frameLimit)
    : MegaDriveEnvironment(VDP::VSync, VDP::Integer), romPath_(std::move(romPath)), frameLimit_(frameLimit),
      gameMemory_(memory()), player1Controller_(gameMemory_, controllers::Player::One), soundEffects_(gameMemory_) {
}

void SampleGame::run() {
    loadROM(romPath_);
    player1Controller_.initialize();
    soundEffects_.initialize();
    initializeGraphics();
    render();

    while (!shouldQuit() && (frameLimit_ == 0 || frameCount_ < frameLimit_)) {
        waitForVBlank();
        if (shouldQuit()) {
            break;
        }
        update();
        render();
        ++frameCount_;
    }
}

void SampleGame::vSync() {
    frameReady_ = true;
}

void SampleGame::waitForVBlank() {
    VDP::Interrupt pending;
    while (vdp().popInterrupt(pending)) {
    }

    frameReady_ = false;
    while (!frameReady_ && !shouldQuit()) {
        runVDPInterrupts();
        if (!frameReady_) {
            SDL_Delay(1);
        }
    }
}

void SampleGame::initializeGraphics() {
    auto &video = vdp();
    vdp::initialize(video);
    vdp::loadPalette(video, 0, kTextPalette);
    vdp::loadPalette(video, 1, kPlayerPalette);
    vdp::loadPalette(video, 2, kGemPalette);
    vdp::loadPalette(video, 3, kFloorPalette);
    vdp::loadTilesFromRom(video, gameMemory_, kRomTileData, kFontTile, kFontTileCount);
    vdp::loadTilesFromRom(video, gameMemory_, kRomTileData + kPlayerRomTile * 32, kPlayerTile, 4);
    vdp::loadTilesFromRom(video, gameMemory_, kRomTileData + kGemRomTile * 32, kGemTile, 1);
    vdp::loadTilesFromRom(video, gameMemory_, kRomTileData + kFloorRomTile * 32, kFloorTile, 1);

    vdp::fillPlane(video, vdp::kPlaneA, vdp::tileDescriptor(0));
    vdp::fillPlane(video, vdp::kPlaneB, vdp::tileDescriptor(kFloorTile, 3));
    vdp::writeText(video, vdp::kPlaneA, 2, 1, "MEGADRIVE ENVIRONMENT SAMPLE", kFontTile);
    vdp::writeText(video, vdp::kPlaneA, 2, 26, "D-PAD MOVE   A/START RESET", kFontTile);
}

void SampleGame::update() {
    soundEffects_.update();
    const auto events = session_.update(player1Controller_.read());
    if (events.restarted()) {
        soundEffects_.playRestart();
    } else if (events.gameOverStarted()) {
        soundEffects_.playGameOver();
    } else if (events.collectedGem()) {
        soundEffects_.playGemCollected();
    }
}

void SampleGame::render() {
    char scoreText[16];
    std::snprintf(scoreText, sizeof(scoreText), "SCORE %03u", static_cast<unsigned>(session_.score()));
    vdp::writeText(vdp(), vdp::kPlaneA, 15, 3, scoreText, kFontTile);
    const char *message =
        session_.phase() == game::Phase::GameOver ? "GAME OVER  A/START RESTART" : "                           ";
    vdp::writeText(vdp(), vdp::kPlaneA, 7, 13, message, kFontTile);

    const auto &player = session_.player();
    const auto &gem = session_.gem();
    const auto &enemy = session_.enemy();
    vdp::writeSprite(vdp(), 0, player.x(), player.y(), 2, 2, kPlayerTile, 1, 1);
    vdp::writeSprite(vdp(), 1, gem.x(), gem.y(), 1, 1, kGemTile, 2, 2);
    vdp::writeSprite(vdp(), 2, enemy.x(), enemy.y(), 2, 2, kEnemyTile, 3, 0);
}

} // namespace sample
