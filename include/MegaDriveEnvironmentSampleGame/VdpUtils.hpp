#pragma once

#include <array>
#include <cstdint>
#include <string_view>

class VDP;

namespace sample::memory {
class Memory;
}

namespace sample::vdp {

inline constexpr std::uint16_t kPlaneA = 0xC000;
inline constexpr std::uint16_t kSpriteTable = 0xD000;
inline constexpr std::uint16_t kPlaneB = 0xE000;
inline constexpr std::uint16_t kHScrollTable = 0xF000;
inline constexpr int kPlaneWidth = 64;
inline constexpr int kPlaneHeight = 32;

void writeRegister(VDP &vdp, std::uint8_t reg, std::uint8_t value);
void setVramWrite(VDP &vdp, std::uint16_t address);
void setCramWrite(VDP &vdp, std::uint16_t address);
void initialize(VDP &vdp);

void loadPalette(VDP &vdp, std::uint8_t palette, const std::array<std::uint16_t, 16> &colors);
void loadTile(VDP &vdp, std::uint16_t tileIndex, const std::array<std::uint32_t, 8> &rows);
void loadFont(VDP &vdp, memory::Memory &memory, std::uint16_t firstTile);

void fillPlane(VDP &vdp, std::uint16_t planeBase, std::uint16_t tileDescriptor);
void writePlaneTile(VDP &vdp, std::uint16_t planeBase, int column, int row, std::uint16_t tileDescriptor);
void writeText(VDP &vdp,
               std::uint16_t planeBase,
               int column,
               int row,
               std::string_view text,
               std::uint16_t firstFontTile,
               std::uint8_t palette = 0);

void writeSprite(VDP &vdp,
                 int spriteIndex,
                 int x,
                 int y,
                 int widthInTiles,
                 int heightInTiles,
                 std::uint16_t firstTile,
                 std::uint8_t palette,
                 int nextSprite);

[[nodiscard]] constexpr std::uint16_t tileDescriptor(std::uint16_t tile,
                                                     std::uint8_t palette = 0,
                                                     bool priority = false) {
    return static_cast<std::uint16_t>((priority ? 0x8000u : 0u) | ((palette & 3u) << 13) | (tile & 0x07FFu));
}

} // namespace sample::vdp
