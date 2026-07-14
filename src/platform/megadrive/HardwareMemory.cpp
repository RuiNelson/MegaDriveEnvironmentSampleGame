/**
 * @file HardwareMemory.cpp
 * Volatile reads and writes against the real 68000's external address bus.
 */

#include "MegaDriveEnvironmentSampleGame/platform/megadrive/HardwareMemory.hpp"

namespace sample::platform::megadrive {
namespace {

#if defined(__GNUC__) || defined(__clang__)
#define SAMPLE_ALWAYS_INLINE inline __attribute__((always_inline))
#else
#define SAMPLE_ALWAYS_INLINE inline
#endif

template <typename Value>
[[nodiscard]] SAMPLE_ALWAYS_INLINE volatile Value &bus(memory::Address address) noexcept {
    // volatile prevents the compiler from removing or combining device I/O.
    // Native m68k loads/stores already have the required big-endian byte order.
    // A physical 68000 exposes only address lines A0-A23, so the bus itself
    // performs the normalization. Avoiding a software mask removes one long
    // AND instruction from every hardware access.
    return *reinterpret_cast<volatile Value *>(static_cast<std::uintptr_t>(address));
}

#undef SAMPLE_ALWAYS_INLINE

} // namespace

std::uint8_t HardwareMemory::read8(memory::Address address) noexcept {
    return bus<std::uint8_t>(address);
}

std::uint16_t HardwareMemory::read16(memory::Address address) noexcept {
    return bus<std::uint16_t>(address);
}

std::uint32_t HardwareMemory::read32(memory::Address address) noexcept {
    return bus<std::uint32_t>(address);
}

void HardwareMemory::write8(memory::Address address, std::uint8_t value) noexcept {
    bus<std::uint8_t>(address) = value;
}

void HardwareMemory::write16(memory::Address address, std::uint16_t value) noexcept {
    bus<std::uint16_t>(address) = value;
}

void HardwareMemory::write32(memory::Address address, std::uint32_t value) noexcept {
    bus<std::uint32_t>(address) = value;
}

bool HardwareMemory::waitFor16(memory::Address address,
                               std::uint16_t mask,
                               std::uint16_t expected) noexcept {
    // Resolve the volatile bus address once. Calling Memory::read16() here
    // would perform a virtual dispatch on every spin of the VBlank loop.
    volatile std::uint16_t &value = bus<std::uint16_t>(address);
    expected = static_cast<std::uint16_t>(expected & mask);
    while ((value & mask) != expected) {
    }
    return true;
}

} // namespace sample::platform::megadrive
