#include "MegaDriveEnvironmentSampleGame/SampleGame.hpp"

#include "MegaDriveEnvironmentSampleGame/VdpUtils.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <cstdio>

namespace sample {
namespace {

constexpr std::uint16_t kFontTile = 1;
constexpr std::uint16_t kPlayerTile = 96;
constexpr std::uint16_t kGemTile = 100;
constexpr std::uint16_t kFloorTile = 101;

constexpr int kPlayerSize = 16;
constexpr int kGemSize = 8;
constexpr int kScreenWidth = 320;
constexpr int kScreenHeight = 224;
constexpr int kHudHeight = 32;

// Each uint32_t is one 8-pixel row: one hexadecimal nibble per palette index.
constexpr std::array<std::array<std::uint32_t, 8>, 4> kPlayerTiles{{
    {0x00011111, 0x00112222, 0x01122222, 0x01222222, 0x11222222, 0x12222222, 0x12221111, 0x12211111},
    {0x12211111, 0x12222222, 0x01222222, 0x01122222, 0x00112222, 0x00011222, 0x00001111, 0x00000110},
    {0x11111000, 0x22221100, 0x22222110, 0x22222110, 0x22222211, 0x22222221, 0x11112221, 0x11111221},
    {0x11111221, 0x22222221, 0x22222210, 0x22222110, 0x22221100, 0x22211000, 0x11110000, 0x01100000},
}};

constexpr std::array<std::uint32_t, 8> kGemRows{
    0x00011000, 0x00122100, 0x01233210, 0x12333321, 0x01233210, 0x00122100, 0x00011000, 0x00000000,
};

constexpr std::array<std::uint32_t, 8> kFloorRows{
    0x11111111, 0x10000001, 0x10011001, 0x10100101, 0x10100101, 0x10011001, 0x10000001, 0x11111111,
};

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

SampleGame::SampleGame(unsigned frameLimit)
    : MegaDriveEnvironment(VDP::VSync, VDP::Integer), frameLimit_(frameLimit) {
}

void SampleGame::run() {
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
    vdp::loadFont(video, memory(), kFontTile);

    for (std::size_t index = 0; index < kPlayerTiles.size(); ++index) {
        vdp::loadTile(video, static_cast<std::uint16_t>(kPlayerTile + index), kPlayerTiles[index]);
    }
    vdp::loadTile(video, kGemTile, kGemRows);
    vdp::loadTile(video, kFloorTile, kFloorRows);

    vdp::fillPlane(video, vdp::kPlaneA, vdp::tileDescriptor(0));
    vdp::fillPlane(video, vdp::kPlaneB, vdp::tileDescriptor(kFloorTile, 3));
    vdp::writeText(video, vdp::kPlaneA, 2, 1, "MEGADRIVE ENVIRONMENT SAMPLE", kFontTile);
    vdp::writeText(video, vdp::kPlaneA, 2, 26, "D-PAD MOVE   A/START RESET", kFontTile);
}

void SampleGame::update() {
    const auto controls = controllers().getCurrentState().player1;
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
