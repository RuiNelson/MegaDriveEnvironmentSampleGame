/**
 * @file SampleGame.cpp
 * Complete game implementation shared by PC and real-hardware builds.
 */

#include "MegaDriveEnvironmentSampleGame/SampleGame.hpp"

#include "MegaDriveEnvironmentSampleGame/VdpUtils.hpp"

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

// Both ROM formats are 32 Mbit and place the same packed tile blob at the end.
constexpr std::uint32_t kRomSize = 4 * 1024 * 1024;
constexpr std::uint32_t kRomTileCount = 101;
constexpr std::uint32_t kRomTileData = kRomSize - kRomTileCount * 32;

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

SampleGame::SampleGame(memory::Memory &memory)
    : memory_(memory), player1Controller_(memory, controllers::Player::One), soundEffects_(memory) {
}

void SampleGame::initialize() {
    player1Controller_.initialize();
    soundEffects_.initialize();
    initializeGraphics();
    render();
}

bool SampleGame::runFrame() {
    if (!vdp::waitForVBlank(memory_)) {
        return false;
    }
    update();
    render();
    return true;
}

void SampleGame::run(unsigned frameLimit) {
    initialize();
    unsigned frameCount = 0;
    while (frameLimit == 0 || frameCount < frameLimit) {
        if (!runFrame()) {
            break;
        }
        ++frameCount;
    }
}

void SampleGame::initializeGraphics() {
    vdp::initialize(memory_);
    vdp::loadPalette(memory_, 0, kTextPalette);
    vdp::loadPalette(memory_, 1, kPlayerPalette);
    vdp::loadPalette(memory_, 2, kGemPalette);
    vdp::loadPalette(memory_, 3, kFloorPalette);

    // Copy only the required spans even though all assets share one ROM blob.
    vdp::loadTilesFromRom(memory_, kRomTileData, kFontTile, kFontTileCount);
    vdp::loadTilesFromRom(memory_, kRomTileData + kPlayerRomTile * 32, kPlayerTile, 4);
    vdp::loadTilesFromRom(memory_, kRomTileData + kGemRomTile * 32, kGemTile, 1);
    vdp::loadTilesFromRom(memory_, kRomTileData + kFloorRomTile * 32, kFloorTile, 1);

    vdp::fillPlane(memory_, vdp::kPlaneA, vdp::tileDescriptor(0));
    vdp::fillPlane(memory_, vdp::kPlaneB, vdp::tileDescriptor(kFloorTile, 3));
    vdp::writeText(memory_, vdp::kPlaneA, 2, 1, "MEGADRIVE ENVIRONMENT SAMPLE", kFontTile);
    vdp::writeText(memory_, vdp::kPlaneA, 2, 26, "D-PAD MOVE   A/START RESET", kFontTile);
    vdp::finishInitialization(memory_);
}

void SampleGame::update() {
    // Advance the previous effect before a newly emitted event replaces it.
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
    // Avoid snprintf, division and initialized local arrays (which can make a
    // freestanding compiler request memcpy) in this shared renderer.
    vdp::writeText(memory_, vdp::kPlaneA, 15, 3, "SCORE ", kFontTile);
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
    vdp::writePlaneTile(memory_, vdp::kPlaneA, 21, 3,
                        vdp::tileDescriptor(static_cast<std::uint16_t>(kFontTile + hundreds - 0x20), 0, true));
    vdp::writePlaneTile(memory_, vdp::kPlaneA, 22, 3,
                        vdp::tileDescriptor(static_cast<std::uint16_t>(kFontTile + tens - 0x20), 0, true));
    vdp::writePlaneTile(memory_, vdp::kPlaneA, 23, 3,
                        vdp::tileDescriptor(static_cast<std::uint16_t>(kFontTile + ones - 0x20), 0, true));

    const char *message =
        session_.phase() == game::Phase::GameOver ? "GAME OVER  A/START RESTART" : "                           ";
    vdp::writeText(memory_, vdp::kPlaneA, 7, 13, message, kFontTile);

    const auto &player = session_.player();
    const auto &gem = session_.gem();
    const auto &enemy = session_.enemy();
    // SAT links form 0 -> 1 -> 2 -> 0; link zero terminates traversal.
    vdp::writeSprite(memory_, 0, player.x(), player.y(), 2, 2, kPlayerTile, 1, 1);
    vdp::writeSprite(memory_, 1, gem.x(), gem.y(), 1, 1, kGemTile, 2, 2);
    vdp::writeSprite(memory_, 2, enemy.x(), enemy.y(), 2, 2, kEnemyTile, 3, 0);
}

} // namespace sample
