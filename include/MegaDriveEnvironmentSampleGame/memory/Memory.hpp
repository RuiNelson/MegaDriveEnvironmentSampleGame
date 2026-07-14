#pragma once

/**
 * @file Memory.hpp
 * Target-neutral access to the Mega Drive's 24-bit memory map.
 */

#include "MegaDriveEnvironmentSampleGame/FreestandingStd.hpp"

namespace sample::memory {

using Address = std::uint32_t;

/** Bus mask and address ranges used by the sample's ROM and work RAM. */
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

    /**
     * Performs one bus read. Multi-byte values use 68000 big-endian order.
     * Implementations preserve device side effects at mapped I/O ports.
     */
    virtual std::uint8_t read8(Address address) = 0;
    virtual std::uint16_t read16(Address address) = 0;
    virtual std::uint32_t read32(Address address) = 0;

    /**
     * Performs one bus write. Callers keep word and long-word addresses even,
     * as required by a real 68000.
     */
    virtual void write8(Address address, std::uint8_t value) = 0;
    virtual void write16(Address address, std::uint16_t value) = 0;
    virtual void write32(Address address, std::uint32_t value) = 0;

    /**
     * Waits until the selected bits of a word match `expected`.
     *
     * The default implementation busy-waits, which is appropriate on the real
     * 68000. Host backends may override it to yield to emulated devices. A
     * return value of false means the host cancelled the wait.
     */
    virtual bool waitFor16(Address address, std::uint16_t mask, std::uint16_t expected);

    /** Copies bytes from bus memory into a host or stack buffer. */
    void read(Address source, std::span<std::uint8_t> destination);
    /** Copies bytes from a buffer to bus memory. */
    void write(Address destination, std::span<const std::uint8_t> source);
    /**
     * Copies within bus memory with `memmove`-style overlap handling.
     * Device side effects remain visible because copying uses byte accesses.
     */
    void copy(Address source, Address destination, std::size_t byteCount);
    /** Writes `value` to a consecutive bus-memory range. */
    void fill(Address destination, std::uint8_t value, std::size_t byteCount);

    /** Folds an integer address onto the 68000's 24-bit external bus. */
    [[nodiscard]] static constexpr Address normalize(Address address) {
        return address & kAddressMask;
    }

  protected:
    /** Only concrete target backends can construct the abstraction. */
    Memory() = default;
};

} // namespace sample::memory
