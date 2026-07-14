#include "MegaDriveEnvironmentSampleGame/SampleGame.hpp"

#include "MegaDriveEnvironmentSampleGame/VdpUtils.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <utility>

namespace sample {
namespace {

constexpr std::uint16_t kFontTile = 1;
constexpr std::uint16_t kPlayerTile = 96;
constexpr std::uint16_t kGemTile = 100;
constexpr std::uint16_t kFloorTile = 101;

constexpr std::uint16_t kFontTileCount = 95;
constexpr std::uint16_t kPlayerRomTile = 95;
constexpr std::uint16_t kGemRomTile = 99;
constexpr std::uint16_t kFloorRomTile = 100;
constexpr std::uint32_t kRomSize = 4 * 1024 * 1024;
constexpr std::uint32_t kRomTileCount = 101;
constexpr std::uint32_t kRomTileData = kRomSize - kRomTileCount * 32;

constexpr int kPlayerSize = 16;
constexpr int kGemSize = 8;
constexpr int kScreenWidth = 320;
constexpr int kScreenHeight = 224;
constexpr int kHudHeight = 32;

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
    0x0000, 0x0222, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

bool overlaps(int ax, int ay, int aw, int ah, int bx, int by, int bw, int bh) {
    return ax < bx + bw && ax + aw > bx && ay < by + bh && ay + ah > by;
}

} // namespace

SampleGame::SampleGame(std::string romPath, unsigned frameLimit)
    : MegaDriveEnvironment(VDP::VSync, VDP::Integer),
      romPath_(std::move(romPath)),
      frameLimit_(frameLimit),
      gameMemory_(memory()),
      player1Controller_(gameMemory_, controllers::Player::One) {
}

void SampleGame::run() {
    loadROM(romPath_);
    player1Controller_.initialize();
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
    vdp::loadTilesFromRom(
        video, gameMemory_, kRomTileData + kPlayerRomTile * 32, kPlayerTile, 4);
    vdp::loadTilesFromRom(video, gameMemory_, kRomTileData + kGemRomTile * 32, kGemTile, 1);
    vdp::loadTilesFromRom(video, gameMemory_, kRomTileData + kFloorRomTile * 32, kFloorTile, 1);

    vdp::fillPlane(video, vdp::kPlaneA, vdp::tileDescriptor(0));
    vdp::fillPlane(video, vdp::kPlaneB, vdp::tileDescriptor(kFloorTile, 3));
    vdp::writeText(video, vdp::kPlaneA, 2, 1, "MEGADRIVE ENVIRONMENT SAMPLE", kFontTile);
    vdp::writeText(video, vdp::kPlaneA, 2, 26, "D-PAD MOVE   A/START RESET", kFontTile);
}

void SampleGame::update() {
    const auto controls = player1Controller_.read();
    constexpr int speed = 2;

    playerX_ += (controls.right ? speed : 0) - (controls.left ? speed : 0);
    playerY_ += (controls.down ? speed : 0) - (controls.up ? speed : 0);
    playerX_ = std::clamp(playerX_, 0, kScreenWidth - kPlayerSize);
    playerY_ = std::clamp(playerY_, kHudHeight, kScreenHeight - kPlayerSize);

    const bool resetPressed = controls.a || controls.start;
    if (resetPressed && !resetWasPressed_) {
        reset();
    }
    resetWasPressed_ = resetPressed;

    if (overlaps(playerX_, playerY_, kPlayerSize, kPlayerSize, gemX_, gemY_, kGemSize, kGemSize)) {
        ++score_;
        moveGem();
    }
}

void SampleGame::render() {
    char scoreText[16];
    std::snprintf(scoreText, sizeof(scoreText), "SCORE %03u", score_ % 1000);
    vdp::writeText(vdp(), vdp::kPlaneA, 15, 3, scoreText, kFontTile);
    vdp::writeSprite(vdp(), 0, playerX_, playerY_, 2, 2, kPlayerTile, 1, 1);
    vdp::writeSprite(vdp(), 1, gemX_, gemY_, 1, 1, kGemTile, 2, 0);
}

void SampleGame::moveGem() {
    // Deterministic placement keeps the example reproducible without a random dependency.
    gemX_ = 16 + static_cast<int>((score_ * 73) % 280);
    gemY_ = kHudHeight + 8 + static_cast<int>((score_ * 47) % 168);
}

void SampleGame::reset() {
    playerX_ = 40;
    playerY_ = 104;
    score_ = 0;
    gemX_ = 240;
    gemY_ = 104;
}

} // namespace sample
