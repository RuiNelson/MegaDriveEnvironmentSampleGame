/**
 * @file Memory-MD.cpp
 * Inline volatile access to the real 68000 address bus.
 *
 * The ROM builder includes this file first in its generated translation unit,
 * making every definition visible before the shared game sources use it.
 */

#include "MegaDriveEnvironmentSampleGame/Memory.hpp"

#if !defined(MEGADRIVE) || defined(PC)
#error "Memory-MD.cpp requires the MEGADRIVE target"
#endif

namespace sample::memory {
namespace {

#if defined(__GNUC__) || defined(__clang__)
#define SAMPLE_MEMORY_MD_ALWAYS_INLINE inline __attribute__((always_inline))
#else
#define SAMPLE_MEMORY_MD_ALWAYS_INLINE inline
#endif

/** Resolves an address directly on the physical 24-bit bus. */
template <typename Value>
[[nodiscard]] SAMPLE_MEMORY_MD_ALWAYS_INLINE volatile Value &bus(Address address) noexcept {
    return *reinterpret_cast<volatile Value *>(static_cast<std::uintptr_t>(address));
}

} // namespace

SAMPLE_MEMORY_MD_ALWAYS_INLINE std::uint8_t Memory::read8(Address address) const noexcept {
    return bus<std::uint8_t>(address);
}

SAMPLE_MEMORY_MD_ALWAYS_INLINE std::uint16_t Memory::read16(Address address) const noexcept {
    return bus<std::uint16_t>(address);
}

SAMPLE_MEMORY_MD_ALWAYS_INLINE std::uint32_t Memory::read32(Address address) const noexcept {
    return bus<std::uint32_t>(address);
}

SAMPLE_MEMORY_MD_ALWAYS_INLINE void Memory::write8(Address address, std::uint8_t value) const noexcept {
    bus<std::uint8_t>(address) = value;
}

SAMPLE_MEMORY_MD_ALWAYS_INLINE void Memory::write16(Address address, std::uint16_t value) const noexcept {
    bus<std::uint16_t>(address) = value;
}

SAMPLE_MEMORY_MD_ALWAYS_INLINE void Memory::write32(Address address, std::uint32_t value) const noexcept {
    bus<std::uint32_t>(address) = value;
}

SAMPLE_MEMORY_MD_ALWAYS_INLINE bool Memory::waitFor16(Address address,
                                                      std::uint16_t mask,
                                                      std::uint16_t expected) const noexcept {
    volatile std::uint16_t &value = bus<std::uint16_t>(address);
    expected = static_cast<std::uint16_t>(expected & mask);
    while ((value & mask) != expected) {
    }
    return true;
}

#undef SAMPLE_MEMORY_MD_ALWAYS_INLINE

} // namespace sample::memory
