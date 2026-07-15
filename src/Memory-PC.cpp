/**
 * @file Memory-PC.cpp
 * Delegation from the sample memory API to MegaDriveEnvironment SystemMemory.
 */

#include "MegaDriveEnvironmentSampleGame/Memory.hpp"

#include "system/memory/SystemMemory.hpp"

#if !defined(PC) || defined(MEGADRIVE)
#error "Memory-PC.cpp requires the PC target"
#endif

namespace sample::platform {

PlatformMemory::PlatformMemory(SystemMemory &memory) noexcept : memory_(memory) {
}

std::uint8_t PlatformMemory::read8(memory::Address address) noexcept {
    // SystemMemory owns mapping and device dispatch. Normalize here to mirror
    // the physical 24-bit address bus before handing it the access.
    return memory_.readByte(memory::Memory::normalize(address));
}

std::uint16_t PlatformMemory::read16(memory::Address address) noexcept {
    return memory_.readWord(memory::Memory::normalize(address));
}

std::uint32_t PlatformMemory::read32(memory::Address address) noexcept {
    return memory_.readLong(memory::Memory::normalize(address));
}

void PlatformMemory::write8(memory::Address address, std::uint8_t value) noexcept {
    memory_.writeByte(memory::Memory::normalize(address), value);
}

void PlatformMemory::write16(memory::Address address, std::uint16_t value) noexcept {
    memory_.writeWord(memory::Memory::normalize(address), value);
}

void PlatformMemory::write32(memory::Address address, std::uint32_t value) noexcept {
    memory_.writeLong(memory::Memory::normalize(address), value);
}

} // namespace sample::platform
