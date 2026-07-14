#pragma once

#include "MegaDriveEnvironmentSampleGame/FreestandingStd.hpp"

namespace sample::memory {

using Address = std::uint32_t;

inline constexpr Address kAddressMask = 0x00FF'FFFF;
inline constexpr Address kRomStart = 0x000000;
inline constexpr Address kRomEnd = 0x3FFFFF;
inline constexpr Address kWorkRamStart = 0xFF0000;
inline constexpr Address kWorkRamEnd = 0xFFFFFF;

/**
 * Common 68000 memory contract used by every game target.
 *
 * Addresses are normalized to the Mega Drive's 24-bit address bus. Words and
 * long words use the 68000's native big-endian byte order. Callers must keep
 * 16-bit and 32-bit accesses word-aligned, just as they must on real hardware.
 */
class Memory {
  public:
    virtual ~Memory() = default;

    Memory(const Memory &) = delete;
    Memory &operator=(const Memory &) = delete;

    virtual std::uint8_t read8(Address address) = 0;
    virtual std::uint16_t read16(Address address) = 0;
    virtual std::uint32_t read32(Address address) = 0;

    virtual void write8(Address address, std::uint8_t value) = 0;
    virtual void write16(Address address, std::uint16_t value) = 0;
    virtual void write32(Address address, std::uint32_t value) = 0;

    void read(Address source, std::span<std::uint8_t> destination);
    void write(Address destination, std::span<const std::uint8_t> source);
    void copy(Address source, Address destination, std::size_t byteCount);
    void fill(Address destination, std::uint8_t value, std::size_t byteCount);

    [[nodiscard]] static constexpr Address normalize(Address address) {
        return address & kAddressMask;
    }

  protected:
    Memory() = default;
};

} // namespace sample::memory
