#include "MegaDriveEnvironmentSampleGame/FreestandingStd.hpp"
#include "MegaDriveEnvironmentSampleGame/audio/PsgSoundEffects.hpp"
#include "MegaDriveEnvironmentSampleGame/controllers/ControllerReader.hpp"
#include "MegaDriveEnvironmentSampleGame/game/GameSession.hpp"
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
constexpr u16 kEnemyTile = kPlayerTile;
constexpr u16 kRomTileCount = 101;

constexpr u16 kPalettes[4][16]{
    {0x0000, 0x0EEE, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0x0000, 0x0008, 0x00EE, 0x0EEE, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0x0000, 0x0080, 0x00E0, 0x00EE, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0x0000, 0x0222, 0x000E, 0x0EEE, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
};

extern "C" const u16 asset_tiles[];

class CartridgeVideo final {
  public:
    void initialize() {
        // Select the full 3-bit-per-channel CRAM palette. With bit 2 clear,
        // real VDPs only use one bit from each colour component.
        writeRegister(0x00, 0x04);
        writeRegister(0x01, 0x14); // display disabled, DMA, Mode 5
        writeRegister(0x02, 0x30); // Plane A at 0xC000
        writeRegister(0x03, 0x2C); // Window at 0xB000
        writeRegister(0x04, 0x07); // Plane B at 0xE000
        writeRegister(0x05, 0x68); // sprite table at 0xD000
        writeRegister(0x07, 0x00); // backdrop palette 0, color 0
        writeRegister(0x0A, 0xFF); // HBlank counter (IRQ remains disabled)
        writeRegister(0x0B, 0x00); // full-screen scrolling
        writeRegister(0x0C, 0x81); // H40, non-interlaced
        writeRegister(0x0D, 0x3C); // HScroll table at 0xF000
        writeRegister(0x0F, 0x02); // auto-increment by one word
        writeRegister(0x10, 0x01); // planes are 64 x 32 cells
        writeRegister(0x11, 0x00);
        writeRegister(0x12, 0x00);

        clearVram();
        loadPalettes();
        loadTiles();
        fillPlane(kPlaneA, 0);
        fillPlane(kPlaneB, tileDescriptor(kFloorTile, 3));

        setVramWrite(kHScrollTable);
        dataPort() = 0;
        dataPort() = 0;

        writeText(2, 1, "MEGADRIVE ENVIRONMENT SAMPLE");
        writeText(2, 26, "D-PAD MOVE   A/START RESET");
        writeRegister(0x01, 0x54); // display enabled, DMA, Mode 5
    }

    void waitForVBlank() const {
        while ((controlPort() & 0x0008u) != 0) {
        }
        while ((controlPort() & 0x0008u) == 0) {
        }
    }

    void render(const sample::game::GameSession &session) {
        writeScore(session.score());
        writeText(7, 13,
                  session.phase() == sample::game::Phase::GameOver ? "GAME OVER  A/START RESTART"
                                                                   : "                           ");

        const auto &player = session.player();
        const auto &gem = session.gem();
        const auto &enemy = session.enemy();
        writeSprite(0, player.x(), player.y(), 2, 2, kPlayerTile, 1, 1);
        writeSprite(1, gem.x(), gem.y(), 1, 1, kGemTile, 2, 2);
        writeSprite(2, enemy.x(), enemy.y(), 2, 2, kEnemyTile, 3, 0);
    }

  private:
    [[nodiscard]] static volatile u16 &port16(u32 address) {
        return *reinterpret_cast<volatile u16 *>(static_cast<uptr>(address));
    }

    [[nodiscard]] static volatile u16 &dataPort() {
        return port16(kVdpDataPort);
    }

    [[nodiscard]] static volatile u16 &controlPort() {
        return port16(kVdpControlPort);
    }

    static void writeRegister(u8 reg, u8 value) {
        controlPort() = static_cast<u16>(0x8000u | (static_cast<u16>(reg) << 8) | value);
    }

    static void setVdpWrite(u8 code, u16 address) {
        const u16 first = static_cast<u16>(((code & 0x03u) << 14) | (address & 0x3FFFu));
        const u16 second = static_cast<u16>(((code & 0x3Cu) << 2) | ((address >> 14) & 0x03u));
        controlPort() = first;
        controlPort() = second;
    }

    static void setVramWrite(u16 address) {
        setVdpWrite(0x01, address);
    }

    static void setCramWrite(u16 address) {
        setVdpWrite(0x03, address);
    }

    [[nodiscard]] static constexpr u16 tileDescriptor(u16 tile, u8 palette = 0, bool priority = false) {
        return static_cast<u16>((priority ? 0x8000u : 0u) | ((static_cast<u16>(palette) & 3u) << 13) |
                                (tile & 0x07FFu));
    }

    static void clearVram() {
        setVramWrite(0);
        for (u32 word = 0; word < 32768; ++word) {
            dataPort() = 0;
        }
    }

    static void loadPalettes() {
        setCramWrite(0);
        for (u16 palette = 0; palette < 4; ++palette) {
            for (u16 color = 0; color < 16; ++color) {
                dataPort() = kPalettes[palette][color];
            }
        }
    }

    static void loadTiles() {
        setVramWrite(static_cast<u16>(kFontTile * 32));
        for (u16 word = 0; word < kRomTileCount * 16; ++word) {
            dataPort() = asset_tiles[word];
        }
    }

    static void fillPlane(u16 plane, u16 descriptor) {
        setVramWrite(plane);
        for (u16 cell = 0; cell < kPlaneWidth * kPlaneHeight; ++cell) {
            dataPort() = descriptor;
        }
    }

    static void writePlaneTile(u16 plane, int column, int row, u16 descriptor) {
        const u16 cell = static_cast<u16>((row << 6) + column);
        setVramWrite(static_cast<u16>(plane + cell * 2));
        dataPort() = descriptor;
    }

    static void writeText(int column, int row, const char *text) {
        while (*text != '\0') {
            const unsigned char glyph = static_cast<unsigned char>(*text++);
            const u16 tile = glyph >= 0x20 && glyph <= 0x7E ? static_cast<u16>(kFontTile + glyph - 0x20) : 0;
            writePlaneTile(kPlaneA, column++, row, tileDescriptor(tile, 0, true));
        }
    }

    static void writeScore(u16 score) {
        writeText(15, 3, "SCORE ");

        char hundreds = '0';
        char tens = '0';
        while (score >= 100) {
            ++hundreds;
            score = static_cast<u16>(score - 100);
        }
        while (score >= 10) {
            ++tens;
            score = static_cast<u16>(score - 10);
        }
        writeGlyph(21, 3, hundreds);
        writeGlyph(22, 3, tens);
        writeGlyph(23, 3, static_cast<char>('0' + score));
    }

    static void writeGlyph(int column, int row, char character) {
        writePlaneTile(kPlaneA, column, row, tileDescriptor(static_cast<u16>(kFontTile + character - 0x20), 0, true));
    }

    static void writeSprite(int index, int x, int y, int widthInTiles, int heightInTiles, u16 firstTile, u8 palette,
                            int nextSprite) {
        setVramWrite(static_cast<u16>(kSpriteTable + index * 8));
        dataPort() = static_cast<u16>((y + 128) & 0x01FF);

        const u16 width = static_cast<u16>((widthInTiles - 1) & 3);
        const u16 height = static_cast<u16>((heightInTiles - 1) & 3);
        dataPort() = static_cast<u16>(((width << 2) | height) << 8 | (nextSprite & 0x7F));
        dataPort() = tileDescriptor(firstTile, palette, true);
        dataPort() = static_cast<u16>((x + 128) & 0x01FF);
    }
};

class CartridgeGame final {
  public:
    CartridgeGame() : controller_(memory_, sample::controllers::Player::One), soundEffects_(memory_) {
    }

    [[noreturn]] void run() {
        controller_.initialize();
        soundEffects_.initialize();
        video_.initialize();
        video_.render(session_);

        for (;;) {
            video_.waitForVBlank();
            soundEffects_.update();
            handleEvents(session_.update(controller_.read()));
            video_.render(session_);
        }
    }

  private:
    void handleEvents(const sample::game::Events &events) {
        if (events.restarted()) {
            soundEffects_.playRestart();
        } else if (events.gameOverStarted()) {
            soundEffects_.playGameOver();
        } else if (events.collectedGem()) {
            soundEffects_.playGemCollected();
        }
    }

    sample::platform::megadrive::HardwareMemory memory_;
    sample::controllers::ControllerReader controller_;
    sample::game::GameSession session_;
    sample::audio::PsgSoundEffects soundEffects_;
    CartridgeVideo video_;
};

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
    CartridgeGame game;
    game.run();
}
