#pragma once

/**
 * @file Memory.hpp
 * Target-neutral access to the Mega Drive's 24-bit memory map.
 */

#include "MegaDriveEnvironmentSampleGame/StdCompat.hpp"

#if defined(PC)
class SystemMemory;
#endif

namespace sample::memory {

#if defined(MEGADRIVE)
#if defined(__GNUC__) || defined(__clang__)
#define SAMPLE_MEMORY_ALWAYS_INLINE inline __attribute__((always_inline))
#else
#define SAMPLE_MEMORY_ALWAYS_INLINE inline
#endif
#endif

using Address = std::uint32_t;

/**
 * Common 68000 memory contract used by every game target.
 *
 * Addresses are normalized to the Mega Drive's 24-bit address bus. Words and
 * long words use the 68000's native big-endian byte order. Callers must keep
 * 16-bit and 32-bit accesses word-aligned, just as they must on real hardware.
 */
class Memory {
  public:
#if defined(MEGADRIVE)
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
#if defined(MEGADRIVE)
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
#if defined(MEGADRIVE)
    SAMPLE_MEMORY_ALWAYS_INLINE void write8(Address address, std::uint8_t value) const noexcept;
    SAMPLE_MEMORY_ALWAYS_INLINE void write16(Address address, std::uint16_t value) const noexcept;
    SAMPLE_MEMORY_ALWAYS_INLINE void write32(Address address, std::uint32_t value) const noexcept;
#else
    virtual void write8(Address address, std::uint8_t value) = 0;
    virtual void write16(Address address, std::uint16_t value) = 0;
    virtual void write32(Address address, std::uint32_t value) = 0;
#endif

    /** Folds an integer address onto the 68000's 24-bit external bus. */
    [[nodiscard]] static constexpr Address normalize(Address address) {
        return address & 0x00FF'FFFF;
    }

#if defined(PC)
  protected:
    /** Only concrete target backends can construct the abstraction. */
    Memory() = default;
#endif
};

#if defined(MEGADRIVE)
#undef SAMPLE_MEMORY_ALWAYS_INLINE
#endif

} // namespace sample::memory

namespace sample::platform {

#if defined(MEGADRIVE)

/** Zero-cost name for the stateless real-hardware memory implementation. */
using PlatformMemory = memory::Memory;

#else

/** PC adapter from the common memory API to MegaDriveEnvironment. */
class PlatformMemory final : public memory::Memory {
  public:
    explicit PlatformMemory(SystemMemory &memory) noexcept;

    std::uint8_t read8(memory::Address address) noexcept override;
    std::uint16_t read16(memory::Address address) noexcept override;
    std::uint32_t read32(memory::Address address) noexcept override;

    void write8(memory::Address address, std::uint8_t value) noexcept override;
    void write16(memory::Address address, std::uint16_t value) noexcept override;
    void write32(memory::Address address, std::uint32_t value) noexcept override;

  private:
    SystemMemory &memory_;
};

#endif

} // namespace sample::platform
