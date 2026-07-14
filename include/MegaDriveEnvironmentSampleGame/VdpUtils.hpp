#pragma once

/**
 * @file VdpUtils.hpp
 * High-level VDP operations used by the MegaDriveEnvironment build.
 *
 * The cartridge build has an equivalent freestanding implementation because
 * it talks directly to the VDP ports. Keeping the constants and data formats
 * explicit here makes it easier to compare both rendering paths.
 */

#include <array>
#include <cstdint>
#include <string_view>

class VDP;

namespace sample::memory {
class Memory;
}

namespace sample::vdp {

/** Base addresses of the tables reserved in the 64 KiB VDP VRAM. */
inline constexpr std::uint16_t kPlaneA = 0xC000;
inline constexpr std::uint16_t kSpriteTable = 0xD000;
inline constexpr std::uint16_t kPlaneB = 0xE000;
inline constexpr std::uint16_t kHScrollTable = 0xF000;

/** Plane dimensions selected by VDP register 16, measured in 8x8 cells. */
inline constexpr int kPlaneWidth = 64;
inline constexpr int kPlaneHeight = 32;

/** Writes one VDP register through the control port. */
void writeRegister(VDP &vdp, std::uint8_t reg, std::uint8_t value);
/** Selects the byte address at which subsequent data-port writes enter VRAM. */
void setVramWrite(VDP &vdp, std::uint16_t address);
/** Selects the byte address at which subsequent data-port writes enter CRAM. */
void setCramWrite(VDP &vdp, std::uint16_t address);
/** Resets and configures the VDP for 320x224, Mode 5, 64x32 planes. */
void initialize(VDP &vdp);

/**
 * Loads all 16 colors of a hardware palette.
 *
 * @param palette Palette number in the range 0..3.
 * @param colors Mega Drive CRAM words in `0000BBB0GGG0RRR0` format.
 */
void loadPalette(VDP &vdp, std::uint8_t palette, const std::array<std::uint16_t, 16> &colors);
/**
 * Copies packed 4-bpp tiles from the game ROM into VRAM.
 *
 * Each tile occupies 32 bytes in ROM and VRAM. `romAddress` is a byte address
 * on the 68000 bus, while `firstVramTile` is a tile index.
 */
void loadTilesFromRom(VDP &vdp,
                      memory::Memory &memory,
                      std::uint32_t romAddress,
                      std::uint16_t firstVramTile,
                      std::uint16_t tileCount);

/** Fills all 64x32 cells of a plane with the same tile descriptor. */
void fillPlane(VDP &vdp, std::uint16_t planeBase, std::uint16_t tileDescriptor);
/** Writes one in-bounds cell in a plane name table. */
void writePlaneTile(VDP &vdp, std::uint16_t planeBase, int column, int row, std::uint16_t tileDescriptor);
/**
 * Writes printable ASCII using a contiguous 0x20..0x7E font tile set.
 * Unsupported characters use tile zero. Text is written with priority set.
 */
void writeText(VDP &vdp,
               std::uint16_t planeBase,
               int column,
               int row,
               std::string_view text,
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
void writeSprite(VDP &vdp,
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
