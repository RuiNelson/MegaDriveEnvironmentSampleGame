#include "MegaDriveEnvironmentSampleGame/FreestandingStd.hpp"
#include "MegaDriveEnvironmentSampleGame/controllers/ControllerReader.hpp"
#include "MegaDriveEnvironmentSampleGame/platform/megadrive/HardwareMemory.hpp"

namespace {

using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using uptr = std::uintptr_t;

constexpr u32 kVdpDataPort = 0x00C00000;
constexpr u32 kVdpControlPort = 0x00C00004;

constexpr u16 kPlaneA = 0xC000;
constexpr u16 kSpriteTable = 0xD000;
constexpr u16 kPlaneB = 0xE000;
constexpr u16 kHScrollTable = 0xF000;
constexpr int kPlaneWidth = 64;
constexpr int kPlaneHeight = 32;

constexpr u16 kFontTile = 1;
constexpr u16 kPlayerTile = 96;
constexpr u16 kGemTile = 100;
constexpr u16 kFloorTile = 101;
constexpr u16 kRomTileCount = 101;

constexpr int kPlayerSize = 16;
constexpr int kGemSize = 8;
constexpr int kScreenWidth = 320;
constexpr int kScreenHeight = 224;
constexpr int kHudHeight = 32;

constexpr u16 kPalettes[4][16]{
    {0x0000, 0x0EEE, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0x0000, 0x0008, 0x00EE, 0x0EEE, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0x0000, 0x0080, 0x00E0, 0x00EE, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0x0000, 0x0222, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
};

extern "C" const u16 asset_tiles[];

[[nodiscard]] volatile u16 &port16(u32 address) {
    return *reinterpret_cast<volatile u16 *>(static_cast<uptr>(address));
}

void writeVdpRegister(u8 reg, u8 value) {
    port16(kVdpControlPort) = static_cast<u16>(0x8000u | (static_cast<u16>(reg) << 8) | value);
}

void setVdpWrite(u8 code, u16 address) {
    const u16 first = static_cast<u16>(((code & 0x03u) << 14) | (address & 0x3FFFu));
    const u16 second = static_cast<u16>(((code & 0x3Cu) << 2) | ((address >> 14) & 0x03u));
    port16(kVdpControlPort) = first;
    port16(kVdpControlPort) = second;
}

void setVramWrite(u16 address) {
    setVdpWrite(0x01, address);
}

void setCramWrite(u16 address) {
    setVdpWrite(0x03, address);
}

[[nodiscard]] constexpr u16 tileDescriptor(u16 tile, u8 palette = 0, bool priority = false) {
    return static_cast<u16>((priority ? 0x8000u : 0u) |
                            ((static_cast<u16>(palette) & 3u) << 13) |
                            (tile & 0x07FFu));
}

void clearVram() {
    setVramWrite(0);
    for (u32 word = 0; word < 32768; ++word) {
        port16(kVdpDataPort) = 0;
    }
}

void loadPalettes() {
    setCramWrite(0);
    for (u16 palette = 0; palette < 4; ++palette) {
        for (u16 color = 0; color < 16; ++color) {
            port16(kVdpDataPort) = kPalettes[palette][color];
        }
    }
}

void loadTiles() {
    setVramWrite(static_cast<u16>(kFontTile * 32));
    for (u16 word = 0; word < kRomTileCount * 16; ++word) {
        port16(kVdpDataPort) = asset_tiles[word];
    }
}

void fillPlane(u16 plane, u16 descriptor) {
    setVramWrite(plane);
    for (u16 cell = 0; cell < kPlaneWidth * kPlaneHeight; ++cell) {
        port16(kVdpDataPort) = descriptor;
    }
}

void writePlaneTile(u16 plane, int column, int row, u16 descriptor) {
    const u16 cell = static_cast<u16>((row << 6) + column);
    setVramWrite(static_cast<u16>(plane + cell * 2));
    port16(kVdpDataPort) = descriptor;
}

void writeText(int column, int row, const char *text) {
    while (*text != '\0') {
        const unsigned char glyph = static_cast<unsigned char>(*text++);
        const u16 tile = glyph >= 0x20 && glyph <= 0x7E
                             ? static_cast<u16>(kFontTile + glyph - 0x20)
                             : 0;
        writePlaneTile(kPlaneA, column++, row, tileDescriptor(tile, 0, true));
    }
}

void writeSprite(int index,
                 int x,
                 int y,
                 int widthInTiles,
                 int heightInTiles,
                 u16 firstTile,
                 u8 palette,
                 int nextSprite) {
    setVramWrite(static_cast<u16>(kSpriteTable + index * 8));
    port16(kVdpDataPort) = static_cast<u16>((y + 128) & 0x01FF);

    const u16 width = static_cast<u16>((widthInTiles - 1) & 3);
    const u16 height = static_cast<u16>((heightInTiles - 1) & 3);
    port16(kVdpDataPort) = static_cast<u16>(((width << 2) | height) << 8 |
                                           (nextSprite & 0x7F));
    port16(kVdpDataPort) = tileDescriptor(firstTile, palette, true);
    port16(kVdpDataPort) = static_cast<u16>((x + 128) & 0x01FF);
}

void initializeVdp() {
    writeVdpRegister(0x00, 0x00);
    writeVdpRegister(0x01, 0x14); // display disabled, DMA, Mode 5
    writeVdpRegister(0x02, 0x30); // Plane A at 0xC000
    writeVdpRegister(0x03, 0x2C); // Window at 0xB000
    writeVdpRegister(0x04, 0x07); // Plane B at 0xE000
    writeVdpRegister(0x05, 0x68); // sprite table at 0xD000
    writeVdpRegister(0x07, 0x00); // backdrop palette 0, color 0
    writeVdpRegister(0x0A, 0xFF); // HBlank counter (IRQ remains disabled)
    writeVdpRegister(0x0B, 0x00); // full-screen scrolling
    writeVdpRegister(0x0C, 0x81); // H40, non-interlaced
    writeVdpRegister(0x0D, 0x3C); // HScroll table at 0xF000
    writeVdpRegister(0x0F, 0x02); // auto-increment by one word
    writeVdpRegister(0x10, 0x01); // planes are 64 x 32 cells
    writeVdpRegister(0x11, 0x00);
    writeVdpRegister(0x12, 0x00);

    clearVram();
    loadPalettes();
    loadTiles();
    fillPlane(kPlaneA, 0);
    fillPlane(kPlaneB, tileDescriptor(kFloorTile, 3));

    setVramWrite(kHScrollTable);
    port16(kVdpDataPort) = 0;
    port16(kVdpDataPort) = 0;

    writeText(2, 1, "MEGADRIVE ENVIRONMENT SAMPLE");
    writeText(2, 26, "D-PAD MOVE   A/START RESET");
    writeVdpRegister(0x01, 0x54); // display enabled, DMA, Mode 5
}

void waitForVBlank() {
    while ((port16(kVdpControlPort) & 0x0008u) != 0) {
    }
    while ((port16(kVdpControlPort) & 0x0008u) == 0) {
    }
}

[[nodiscard]] bool overlaps(int ax,
                            int ay,
                            int aw,
                            int ah,
                            int bx,
                            int by,
                            int bw,
                            int bh) {
    return ax < bx + bw && ax + aw > bx && ay < by + bh && ay + ah > by;
}

struct GameState {
    int playerX;
    int playerY;
    int gemX;
    int gemY;
    u16 score;
    u16 gemStepX;
    u16 gemStepY;
    bool resetWasPressed;
};

void reset(GameState &state) {
    state.playerX = 40;
    state.playerY = 104;
    state.gemX = 240;
    state.gemY = 104;
    state.score = 0;
    state.gemStepX = 224;
    state.gemStepY = 64;
    state.resetWasPressed = false;
}

void moveGem(GameState &state) {
    state.gemStepX = static_cast<u16>(state.gemStepX + 73);
    if (state.gemStepX >= 280) {
        state.gemStepX = static_cast<u16>(state.gemStepX - 280);
    }
    state.gemStepY = static_cast<u16>(state.gemStepY + 47);
    if (state.gemStepY >= 168) {
        state.gemStepY = static_cast<u16>(state.gemStepY - 168);
    }
    state.gemX = 16 + state.gemStepX;
    state.gemY = kHudHeight + 8 + state.gemStepY;
}

void update(GameState &state, const sample::controllers::ControllerState &controls) {
    constexpr int speed = 2;
    state.playerX += (controls.right ? speed : 0) - (controls.left ? speed : 0);
    state.playerY += (controls.down ? speed : 0) - (controls.up ? speed : 0);

    if (state.playerX < 0) {
        state.playerX = 0;
    } else if (state.playerX > kScreenWidth - kPlayerSize) {
        state.playerX = kScreenWidth - kPlayerSize;
    }
    if (state.playerY < kHudHeight) {
        state.playerY = kHudHeight;
    } else if (state.playerY > kScreenHeight - kPlayerSize) {
        state.playerY = kScreenHeight - kPlayerSize;
    }

    const bool resetPressed = controls.a || controls.start;
    if (resetPressed && !state.resetWasPressed) {
        reset(state);
    }
    state.resetWasPressed = resetPressed;

    if (overlaps(state.playerX,
                 state.playerY,
                 kPlayerSize,
                 kPlayerSize,
                 state.gemX,
                 state.gemY,
                 kGemSize,
                 kGemSize)) {
        ++state.score;
        if (state.score >= 1000) {
            state.score = 0;
        }
        moveGem(state);
    }
}

void render(const GameState &state) {
    writeText(15, 3, "SCORE ");

    u16 value = state.score;
    char hundreds = '0';
    char tens = '0';
    while (value >= 100) {
        ++hundreds;
        value = static_cast<u16>(value - 100);
    }
    while (value >= 10) {
        ++tens;
        value = static_cast<u16>(value - 10);
    }
    writePlaneTile(kPlaneA,
                   21,
                   3,
                   tileDescriptor(static_cast<u16>(kFontTile + hundreds - 0x20), 0, true));
    writePlaneTile(kPlaneA,
                   22,
                   3,
                   tileDescriptor(static_cast<u16>(kFontTile + tens - 0x20), 0, true));
    writePlaneTile(kPlaneA,
                   23,
                   3,
                   tileDescriptor(static_cast<u16>(kFontTile + value + '0' - 0x20), 0, true));
    writeSprite(0, state.playerX, state.playerY, 2, 2, kPlayerTile, 1, 1);
    writeSprite(1, state.gemX, state.gemY, 1, 1, kGemTile, 2, 0);
}

} // namespace

// A freestanding link has no allocator. These symbols only satisfy the virtual
// destructor entries of the shared Memory interface; game objects live on the
// cartridge stack and are never dynamically allocated.
void operator delete(void *) noexcept {
}

void operator delete(void *, std::size_t) noexcept {
}

extern "C" void __cxa_pure_virtual() {
    for (;;) {
    }
}

extern "C" [[noreturn]] void game_main() {
    sample::platform::megadrive::HardwareMemory memory;
    sample::controllers::ControllerReader controller(memory, sample::controllers::Player::One);
    GameState state{};

    controller.initialize();
    initializeVdp();
    reset(state);
    render(state);

    for (;;) {
        waitForVBlank();
        update(state, controller.read());
        render(state);
    }
}
