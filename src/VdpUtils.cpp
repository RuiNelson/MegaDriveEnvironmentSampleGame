/**
 * @file VdpUtils.cpp
 * VDP command encoding and table-writing through the common memory bus.
 */

#include "MegaDriveEnvironmentSampleGame/VdpUtils.hpp"

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

void clearVram(memory::Memory &memory) {
    setVramWrite(memory, 0);
    for (std::uint32_t word = 0; word < 32768; ++word) {
        memory.write16(kDataPort, 0);
    }
}

} // namespace

void writeRegister(memory::Memory &memory, std::uint8_t reg, std::uint8_t value) {
    memory.write16(kControlPort, static_cast<std::uint16_t>(0x8000u | (reg << 8) | value));
}

void setVramWrite(memory::Memory &memory, std::uint16_t address) {
    memory.write16(kControlPort, commandWord1(kVramWrite, address));
    memory.write16(kControlPort, commandWord2(kVramWrite, address));
}

void setCramWrite(memory::Memory &memory, std::uint16_t address) {
    memory.write16(kControlPort, commandWord1(kCramWrite, address));
    memory.write16(kControlPort, commandWord2(kCramWrite, address));
}

void initialize(memory::Memory &memory) {
    // Configure both targets exactly like the physical VDP. The display stays
    // disabled while tables are populated. H/V interrupt callbacks drive the
    // shared game once initialization has completed.
    writeRegister(memory, 0x00, 0x14); // full CRAM palette + HBlank IRQ
    writeRegister(memory, 0x01, 0x14); // display disabled, DMA, Mode 5
    writeRegister(memory, 0x02, 0x30); // Plane A at 0xC000
    writeRegister(memory, 0x03, 0x2C); // Window at 0xB000
    writeRegister(memory, 0x04, 0x07); // Plane B at 0xE000
    writeRegister(memory, 0x05, 0x68); // sprite table at 0xD000
    writeRegister(memory, 0x07, 0x00); // backdrop palette 0, color 0
    writeRegister(memory, 0x0A, 0x07); // HBlank IRQ every eight scanlines
    writeRegister(memory, 0x0B, 0x00); // full-screen scrolling
    writeRegister(memory, 0x0C, 0x81); // H40 (320 pixels), non-interlaced
    writeRegister(memory, 0x0D, 0x3C); // HScroll table at 0xF000
    writeRegister(memory, 0x0F, 0x02); // auto-increment by one word
    writeRegister(memory, 0x10, 0x01); // planes are 64 x 32 cells
    writeRegister(memory, 0x11, 0x00);
    writeRegister(memory, 0x12, 0x00);

    clearVram(memory);

    // Full-screen scroll mode consumes one horizontal offset per plane.
    setVramWrite(memory, kHScrollTable);
    memory.write16(kDataPort, 0);
    memory.write16(kDataPort, 0);
}

void finishInitialization(memory::Memory &memory) {
    writeRegister(memory, 0x01, 0x74); // display, DMA, Mode 5, VBlank IRQ
}

void writeHorizontalScroll(memory::Memory &memory, std::uint16_t planeA, std::uint16_t planeB) {
    setVramWrite(memory, kHScrollTable);
    memory.write16(kDataPort, planeA);
    memory.write16(kDataPort, planeB);
}

void loadPalette(memory::Memory &memory, std::uint8_t palette, const std::uint16_t (&colors)[16]) {
    setCramWrite(memory, static_cast<std::uint16_t>(palette * 32));
    for (const auto color : colors) {
        memory.write16(kDataPort, color);
    }
}

void loadTilesFromRom(memory::Memory &memory,
                      std::uint32_t romAddress,
                      std::uint16_t firstVramTile,
                      std::uint16_t tileCount) {
    setVramWrite(memory, static_cast<std::uint16_t>(firstVramTile * 32));
    // One 8x8 4-bpp tile is 32 bytes, or 16 big-endian words.
    const auto wordCount = static_cast<std::uint32_t>(tileCount) * 16;
    for (std::uint32_t word = 0; word < wordCount; ++word) {
        memory.write16(kDataPort, memory.read16(romAddress + word * 2));
    }
}

void fillPlane(memory::Memory &memory, std::uint16_t planeBase, std::uint16_t descriptor) {
    setVramWrite(memory, planeBase);
    for (int cell = 0; cell < kPlaneWidth * kPlaneHeight; ++cell) {
        memory.write16(kDataPort, descriptor);
    }
}

void writePlaneTile(memory::Memory &memory,
                    std::uint16_t planeBase,
                    int column,
                    int row,
                    std::uint16_t descriptor) {
    // Plane name tables are row-major arrays of 16-bit descriptors.
    const auto address = static_cast<std::uint16_t>(planeBase + (row * kPlaneWidth + column) * 2);
    setVramWrite(memory, address);
    memory.write16(kDataPort, descriptor);
}

void writeText(memory::Memory &memory,
               std::uint16_t planeBase,
               int column,
               int row,
               const char *text,
               std::uint16_t firstFontTile,
               std::uint8_t palette) {
    while (*text != '\0') {
        const auto glyph = static_cast<unsigned char>(*text++);
        const auto tile = glyph >= 0x20 && glyph <= 0x7E
                              ? static_cast<std::uint16_t>(firstFontTile + glyph - 0x20)
                              : 0;
        writePlaneTile(memory, planeBase, column++, row, tileDescriptor(tile, palette, true));
    }
}

void writeSprite(memory::Memory &memory,
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
    setVramWrite(memory, static_cast<std::uint16_t>(kSpriteTable + spriteIndex * 8));
    memory.write16(kDataPort, static_cast<std::uint16_t>((y + 128) & 0x01FF));

    const auto width = static_cast<std::uint8_t>((widthInTiles - 1) & 3);
    const auto height = static_cast<std::uint8_t>((heightInTiles - 1) & 3);
    memory.write16(kDataPort,
                   static_cast<std::uint16_t>(((width << 2) | height) << 8 | (nextSprite & 0x7F)));
    memory.write16(kDataPort, tileDescriptor(firstTile, palette, true));
    memory.write16(kDataPort, static_cast<std::uint16_t>((x + 128) & 0x01FF));
}

} // namespace sample::vdp
