/**
 * @file VdpUtils.cpp
 * MegaDriveEnvironment VDP command encoding and table-writing helpers.
 */

#include "MegaDriveEnvironmentSampleGame/VdpUtils.hpp"

#include "MegaDriveEnvironmentSampleGame/memory/Memory.hpp"
#include "system/graphics/VDP.hpp"

namespace sample::vdp {
namespace {

constexpr std::uint8_t kVramWrite = 0x01;
constexpr std::uint8_t kCramWrite = 0x03;

// A VDP address command is split over two 16-bit control-port writes. The code
// selects VRAM/CRAM and read/write mode; address bits are interleaved around it.
std::uint16_t commandWord1(std::uint8_t code, std::uint16_t address) {
    return static_cast<std::uint16_t>(((code & 0x03u) << 14) | (address & 0x3FFFu));
}

std::uint16_t commandWord2(std::uint8_t code, std::uint16_t address) {
    return static_cast<std::uint16_t>(((code & 0x3Cu) << 2) | ((address >> 14) & 0x03u));
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
    // Select the full 3-bit-per-channel CRAM palette. With bit 2 clear,
    // real VDPs only use one bit from each colour component.
    writeRegister(vdp, 0x00, 0x04);
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

    // Full-screen scroll mode consumes one horizontal offset per plane.
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

void loadTilesFromRom(VDP &vdp,
                      memory::Memory &memory,
                      std::uint32_t romAddress,
                      std::uint16_t firstVramTile,
                      std::uint16_t tileCount) {
    setVramWrite(vdp, static_cast<std::uint16_t>(firstVramTile * 32));
    // One 8x8 4-bpp tile is 32 bytes, or 16 big-endian words.
    const auto wordCount = static_cast<std::uint32_t>(tileCount) * 16;
    for (std::uint32_t word = 0; word < wordCount; ++word) {
        vdp.writeDataPort(memory.read16(romAddress + word * 2));
    }
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
    // Plane name tables are row-major arrays of 16-bit descriptors.
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
    // Hardware coordinates are biased by 128 so off-screen negative positions
    // remain representable in the nine-bit SAT fields.
    setVramWrite(vdp, static_cast<std::uint16_t>(kSpriteTable + spriteIndex * 8));
    vdp.writeDataPort(static_cast<std::uint16_t>((y + 128) & 0x01FF));

    const auto width = static_cast<std::uint8_t>((widthInTiles - 1) & 3);
    const auto height = static_cast<std::uint8_t>((heightInTiles - 1) & 3);
    vdp.writeDataPort(static_cast<std::uint16_t>(((width << 2) | height) << 8 | (nextSprite & 0x7F)));
    vdp.writeDataPort(tileDescriptor(firstTile, palette, true));
    vdp.writeDataPort(static_cast<std::uint16_t>((x + 128) & 0x01FF));
}

} // namespace sample::vdp
