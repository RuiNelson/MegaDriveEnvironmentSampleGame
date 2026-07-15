#pragma once

/**
 * @file VdpUtils.hpp
 * Target-independent VDP operations implemented through memory-mapped I/O.
 *
 * Both MegaDriveEnvironment and real hardware expose the VDP at the same
 * 68000 addresses. Consequently, this renderer depends only on memory::Memory;
 * the selected memory backend is the sole platform-specific implementation.
 */

#include "MegaDriveEnvironmentSampleGame/Memory.hpp"

namespace sample::vdp {

/** Physical VDP ports on the 68000 bus. */
inline constexpr memory::Address kDataPort = 0xC00000;
inline constexpr memory::Address kControlPort = 0xC00004;

/** Base addresses of the tables reserved in the 64 KiB VDP VRAM. */
inline constexpr std::uint16_t kPlaneA = 0xC000;
inline constexpr std::uint16_t kSpriteTable = 0xD000;
inline constexpr std::uint16_t kPlaneB = 0xE000;
inline constexpr std::uint16_t kHScrollTable = 0xF000;

/** Plane dimensions selected by VDP register 16, measured in 8x8 cells. */
inline constexpr int kPlaneWidth = 64;
inline constexpr int kPlaneHeight = 32;
/** Visible scanlines covered by each HBlank callback. */
inline constexpr int kHSyncLineBatch = 4;

/** Writes one VDP register through the memory-mapped control port. */
void writeRegister(memory::Memory &memory, std::uint8_t reg, std::uint8_t value);

/** Selects the byte address at which subsequent data-port writes enter VRAM. */
void setVramWrite(memory::Memory &memory, std::uint16_t address);

/** Selects the byte address at which subsequent data-port writes enter CRAM. */
void setCramWrite(memory::Memory &memory, std::uint16_t address);

/**
 * Configures the VDP for 320x224 Mode 5 and clears all 64 KiB of VRAM.
 * The display remains disabled until finishInitialization() is called.
 */
void initialize(memory::Memory &memory);

/** Enables the display after palettes, tiles and planes have been populated. */
void finishInitialization(memory::Memory &memory);

/** Updates the Plane A and Plane B horizontal offsets for one scanline. */
void writeHorizontalScrollLine(memory::Memory &memory,
                               int scanline,
                               std::uint16_t planeA,
                               std::uint16_t planeB);

/**
 * Loads all 16 colors of a hardware palette.
 *
 * @param palette Palette number in the range 0..3.
 * @param colors Mega Drive CRAM words in `0000BBB0GGG0RRR0` format.
 */
void loadPalette(memory::Memory &memory, std::uint8_t palette, const std::uint16_t (&colors)[16]);

/**
 * Copies packed 4-bpp tiles from the game ROM into VRAM.
 *
 * Each tile occupies 32 bytes in ROM and VRAM. `romAddress` is a byte address
 * on the 68000 bus, while `firstVramTile` is a tile index.
 */
void loadTilesFromRom(memory::Memory &memory,
                      std::uint32_t romAddress,
                      std::uint16_t firstVramTile,
                      std::uint16_t tileCount);

/** Fills all 64x32 cells of a plane with the same tile descriptor. */
void fillPlane(memory::Memory &memory, std::uint16_t planeBase, std::uint16_t tileDescriptor);

/** Writes one in-bounds cell in a plane name table. */
void writePlaneTile(memory::Memory &memory,
                    std::uint16_t planeBase,
                    int column,
                    int row,
                    std::uint16_t tileDescriptor);

/**
 * Writes a null-terminated printable ASCII string using a contiguous
 * 0x20..0x7E font tile set. Unsupported characters use tile zero.
 */
void writeText(memory::Memory &memory,
               std::uint16_t planeBase,
               int column,
               int row,
               const char *text,
               std::uint16_t firstFontTile,
               std::uint8_t palette = 0);

/**
 * Writes one eight-byte entry in the sprite attribute table.
 *
 * @param spriteIndex Entry to replace, in the range 0..79 in H40 mode.
 * @param x Horizontal top-left position in visible-screen pixels.
 * @param y Vertical top-left position in visible-screen pixels.
 * @param widthInTiles Sprite width in the range 1..4.
 * @param heightInTiles Sprite height in the range 1..4.
 * @param nextSprite Index of the next linked entry; zero terminates the list.
 */
void writeSprite(memory::Memory &memory,
                 int spriteIndex,
                 int x,
                 int y,
                 int widthInTiles,
                 int heightInTiles,
                 std::uint16_t firstTile,
                 std::uint8_t palette,
                 int nextSprite);

/**
 * Packs a tile index, palette number and priority into a plane/sprite word.
 * Horizontal and vertical flip bits are deliberately left clear.
 */
[[nodiscard]] constexpr std::uint16_t tileDescriptor(std::uint16_t tile,
                                                     std::uint8_t palette = 0,
                                                     bool priority = false) {
    return static_cast<std::uint16_t>((priority ? 0x8000u : 0u) | ((palette & 3u) << 13) | (tile & 0x07FFu));
}

} // namespace sample::vdp
