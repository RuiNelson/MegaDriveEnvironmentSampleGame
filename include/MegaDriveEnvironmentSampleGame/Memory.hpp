#pragma once

/**
 * @file Memory.hpp
 * Target-neutral access to the Mega Drive's 24-bit memory map.
 */

#include "MegaDriveEnvironmentSampleGame/StdCompat.hpp"

#if !defined(SAMPLE_FREESTANDING)
class MegaDriveEnvironment;
class SystemMemory;
#endif

namespace sample::memory {

#if defined(SAMPLE_FREESTANDING)
#if defined(__GNUC__) || defined(__clang__)
#define SAMPLE_MEMORY_ALWAYS_INLINE inline __attribute__((always_inline))
#else
#define SAMPLE_MEMORY_ALWAYS_INLINE inline
#endif
#endif

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
#if defined(SAMPLE_FREESTANDING)
    /** Bare-metal memory has no runtime state or virtual dispatch table. */
    Memory() noexcept = default;
    ~Memory() = default;
#else
    virtual ~Memory() = default;
#endif

    Memory(const Memory &) = delete;
    Memory &operator=(const Memory &) = delete;

    /**
     * Performs one bus read. Multi-byte values use 68000 big-endian order.
     * Implementations preserve device side effects at mapped I/O ports.
     */
#if defined(SAMPLE_FREESTANDING)
    SAMPLE_MEMORY_ALWAYS_INLINE std::uint8_t read8(Address address) const noexcept;
    SAMPLE_MEMORY_ALWAYS_INLINE std::uint16_t read16(Address address) const noexcept;
    SAMPLE_MEMORY_ALWAYS_INLINE std::uint32_t read32(Address address) const noexcept;
#else
    virtual std::uint8_t read8(Address address) = 0;
    virtual std::uint16_t read16(Address address) = 0;
    virtual std::uint32_t read32(Address address) = 0;
#endif

    /**
     * Performs one bus write. Callers keep word and long-word addresses even,
     * as required by a real 68000.
     */
#if defined(SAMPLE_FREESTANDING)
    SAMPLE_MEMORY_ALWAYS_INLINE void write8(Address address, std::uint8_t value) const noexcept;
    SAMPLE_MEMORY_ALWAYS_INLINE void write16(Address address, std::uint16_t value) const noexcept;
    SAMPLE_MEMORY_ALWAYS_INLINE void write32(Address address, std::uint32_t value) const noexcept;
#else
    virtual void write8(Address address, std::uint8_t value) = 0;
    virtual void write16(Address address, std::uint16_t value) = 0;
    virtual void write32(Address address, std::uint32_t value) = 0;
#endif

    /**
     * Waits until the selected bits of a word match `expected`.
     *
     * The default implementation busy-waits, which is appropriate on the real
     * 68000. Host backends may override it to yield to emulated devices. A
     * return value of false means the host cancelled the wait.
     */
#if defined(SAMPLE_FREESTANDING)
    SAMPLE_MEMORY_ALWAYS_INLINE bool waitFor16(Address address,
                                               std::uint16_t mask,
                                               std::uint16_t expected) const noexcept;
#else
    virtual bool waitFor16(Address address, std::uint16_t mask, std::uint16_t expected) {
        expected = static_cast<std::uint16_t>(expected & mask);
        while ((read16(address) & mask) != expected) {
        }
        return true;
    }
#endif

    /** Copies bytes from bus memory into a host or stack buffer. */
    void read(Address source, std::span<std::uint8_t> destination) {
        for (std::size_t index = 0; index < destination.size(); ++index) {
            destination[index] = read8(source + static_cast<Address>(index));
        }
    }

    /** Copies bytes from a buffer to bus memory. */
    void write(Address destination, std::span<const std::uint8_t> source) {
        for (std::size_t index = 0; index < source.size(); ++index) {
            write8(destination + static_cast<Address>(index), source[index]);
        }
    }
    /**
     * Copies within bus memory with `memmove`-style overlap handling.
     * Device side effects remain visible because copying uses byte accesses.
     */
    void copy(Address source, Address destination, std::size_t byteCount) {
        if (byteCount == 0 || normalize(source) == normalize(destination)) {
            return;
        }

        // Copy backwards only when destination starts inside the source range.
        if (normalize(destination) > normalize(source) &&
            normalize(destination) < normalize(source + static_cast<Address>(byteCount))) {
            for (std::size_t index = byteCount; index > 0; --index) {
                write8(destination + static_cast<Address>(index - 1),
                       read8(source + static_cast<Address>(index - 1)));
            }
            return;
        }

        for (std::size_t index = 0; index < byteCount; ++index) {
            write8(destination + static_cast<Address>(index),
                   read8(source + static_cast<Address>(index)));
        }
    }

    /** Writes `value` to a consecutive bus-memory range. */
    void fill(Address destination, std::uint8_t value, std::size_t byteCount) {
        for (std::size_t index = 0; index < byteCount; ++index) {
            write8(destination + static_cast<Address>(index), value);
        }
    }

    /** Folds an integer address onto the 68000's 24-bit external bus. */
    [[nodiscard]] static constexpr Address normalize(Address address) {
        return address & kAddressMask;
    }

#if !defined(SAMPLE_FREESTANDING)
  protected:
    /** Only concrete target backends can construct the abstraction. */
    Memory() = default;
#endif
};

#if defined(SAMPLE_FREESTANDING)
#undef SAMPLE_MEMORY_ALWAYS_INLINE
#endif

} // namespace sample::memory

namespace sample::platform {

#if defined(SAMPLE_FREESTANDING)

/** Zero-cost name for the stateless real-hardware memory implementation. */
using PlatformMemory = memory::Memory;

#else

/** PC adapter from the common memory API to MegaDriveEnvironment. */
class PlatformMemory final : public memory::Memory {
  public:
    /** Supplied objects must outlive this adapter. */
    explicit PlatformMemory(SystemMemory &memory, MegaDriveEnvironment *environment = nullptr) noexcept;

    std::uint8_t read8(memory::Address address) noexcept override;
    std::uint16_t read16(memory::Address address) noexcept override;
    std::uint32_t read32(memory::Address address) noexcept override;

    void write8(memory::Address address, std::uint8_t value) noexcept override;
    void write16(memory::Address address, std::uint16_t value) noexcept override;
    void write32(memory::Address address, std::uint32_t value) noexcept override;

    /** Yields to emulated devices instead of busy-waiting the host CPU. */
    bool waitFor16(memory::Address address,
                   std::uint16_t mask,
                   std::uint16_t expected) noexcept override;

  private:
    SystemMemory &memory_;
    MegaDriveEnvironment *environment_;
};

#endif

} // namespace sample::platform
