#include "MegaDriveEnvironmentSampleGame/VdpUtils.hpp"

#include "system/graphics/VDP.hpp"
#include "system/memory/SystemMemory.hpp"
#include "util/font/Font.hpp"

namespace sample::vdp {
namespace {

constexpr std::uint32_t kFontStagingAddress = 0xFF0000;
constexpr std::uint8_t kVramWrite = 0x01;
constexpr std::uint8_t kCramWrite = 0x03;
constexpr std::uint8_t kDma = 0x20;

std::uint16_t commandWord1(std::uint8_t code, std::uint16_t address) {
    return static_cast<std::uint16_t>(((code & 0x03u) << 14) | (address & 0x3FFFu));
}

std::uint16_t commandWord2(std::uint8_t code, std::uint16_t address) {
    return static_cast<std::uint16_t>(((code & 0x3Cu) << 2) | ((address >> 14) & 0x03u));
}

void dmaFromRam(VDP &vdp, std::uint32_t source, std::uint16_t destination, std::uint16_t wordCount) {
    const std::uint32_t sourceWord = source >> 1;
    writeRegister(vdp, 0x13, static_cast<std::uint8_t>(wordCount));
    writeRegister(vdp, 0x14, static_cast<std::uint8_t>(wordCount >> 8));
    writeRegister(vdp, 0x15, static_cast<std::uint8_t>(sourceWord));
    writeRegister(vdp, 0x16, static_cast<std::uint8_t>(sourceWord >> 8));
    writeRegister(vdp, 0x17, static_cast<std::uint8_t>((sourceWord >> 16) & 0x7F));

    const auto command = static_cast<std::uint8_t>(kVramWrite | kDma);
    vdp.writeControlPort(commandWord1(command, destination));
    vdp.writeControlPort(commandWord2(command, destination));
}

} // namespace

void writeRegister(VDP &vdp, std::uint8_t reg, std::uint8_t value) {
    vdp.writeControlPort(static_cast<std::uint16_t>(0x8000u | (reg << 8) | value));
}

void setVramWrite(VDP &vdp, std::uint16_t address) {
    vdp.writeControlPort(commandWord1(kVramWrite, address));
    vdp.writeControlPort(commandWord2(kVramWrite, address));
}

void setCramWrite(VDP &vdp, std::uint16_t address) {
    vdp.writeControlPort(commandWord1(kCramWrite, address));
    vdp.writeControlPort(commandWord2(kCramWrite, address));
}

void initialize(VDP &vdp) {
    vdp.reset();
    writeRegister(vdp, 0x00, 0x00);
    writeRegister(vdp, 0x01, 0x74); // display, VBlank IRQ, DMA, Mode 5
    writeRegister(vdp, 0x02, 0x30); // Plane A at 0xC000
    writeRegister(vdp, 0x03, 0x2C); // Window at 0xB000
    writeRegister(vdp, 0x04, 0x07); // Plane B at 0xE000
    writeRegister(vdp, 0x05, 0x68); // sprite table at 0xD000
    writeRegister(vdp, 0x07, 0x00); // backdrop palette 0, color 0
    writeRegister(vdp, 0x0B, 0x00); // full-screen scrolling
    writeRegister(vdp, 0x0C, 0x81); // H40 (320 pixels), non-interlaced
    writeRegister(vdp, 0x0D, 0x3C); // HScroll table at 0xF000
    writeRegister(vdp, 0x0F, 0x02); // auto-increment by one word
    writeRegister(vdp, 0x10, 0x01); // planes are 64 x 32 cells

    setVramWrite(vdp, kHScrollTable);
    vdp.writeDataPort(0);
    vdp.writeDataPort(0);
}

void loadPalette(VDP &vdp, std::uint8_t palette, const std::array<std::uint16_t, 16> &colors) {
    setCramWrite(vdp, static_cast<std::uint16_t>(palette * 32));
    for (const auto color : colors) {
        vdp.writeDataPort(color);
    }
}

void loadTile(VDP &vdp, std::uint16_t tileIndex, const std::array<std::uint32_t, 8> &rows) {
    setVramWrite(vdp, static_cast<std::uint16_t>(tileIndex * 32));
    for (const auto row : rows) {
        vdp.writeDataPort(static_cast<std::uint16_t>(row >> 16));
        vdp.writeDataPort(static_cast<std::uint16_t>(row));
    }
}

void loadFont(VDP &vdp, SystemMemory &memory, std::uint16_t firstTile) {
    constexpr int kPrintableCharacters = 0x7E - 0x20 + 1;
    for (int index = 0; index < kPrintableCharacters; ++index) {
        Font::fontCharToVDPTile(memory,
                               static_cast<std::uint8_t>(0x20 + index),
                               1,
                               0,
                               kFontStagingAddress + static_cast<std::uint32_t>(index * 32));
    }
    dmaFromRam(vdp,
               kFontStagingAddress,
               static_cast<std::uint16_t>(firstTile * 32),
               static_cast<std::uint16_t>(kPrintableCharacters * 16));
}

void fillPlane(VDP &vdp, std::uint16_t planeBase, std::uint16_t descriptor) {
    setVramWrite(vdp, planeBase);
    for (int cell = 0; cell < kPlaneWidth * kPlaneHeight; ++cell) {
        vdp.writeDataPort(descriptor);
    }
}

void writePlaneTile(VDP &vdp,
                    std::uint16_t planeBase,
                    int column,
                    int row,
                    std::uint16_t descriptor) {
    const auto address = static_cast<std::uint16_t>(planeBase + (row * kPlaneWidth + column) * 2);
    setVramWrite(vdp, address);
    vdp.writeDataPort(descriptor);
}

void writeText(VDP &vdp,
               std::uint16_t planeBase,
               int column,
               int row,
               std::string_view text,
               std::uint16_t firstFontTile,
               std::uint8_t palette) {
    for (const char character : text) {
        const auto glyph = static_cast<unsigned char>(character);
        const auto tile = glyph >= 0x20 && glyph <= 0x7E
                              ? static_cast<std::uint16_t>(firstFontTile + glyph - 0x20)
                              : 0;
        writePlaneTile(vdp, planeBase, column++, row, tileDescriptor(tile, palette, true));
    }
}

void writeSprite(VDP &vdp,
                 int spriteIndex,
                 int x,
                 int y,
                 int widthInTiles,
                 int heightInTiles,
                 std::uint16_t firstTile,
                 std::uint8_t palette,
                 int nextSprite) {
    setVramWrite(vdp, static_cast<std::uint16_t>(kSpriteTable + spriteIndex * 8));
    vdp.writeDataPort(static_cast<std::uint16_t>((y + 128) & 0x01FF));

    const auto width = static_cast<std::uint8_t>((widthInTiles - 1) & 3);
    const auto height = static_cast<std::uint8_t>((heightInTiles - 1) & 3);
    vdp.writeDataPort(static_cast<std::uint16_t>(((width << 2) | height) << 8 | (nextSprite & 0x7F)));
    vdp.writeDataPort(tileDescriptor(firstTile, palette, true));
    vdp.writeDataPort(static_cast<std::uint16_t>((x + 128) & 0x01FF));
}

} // namespace sample::vdp

