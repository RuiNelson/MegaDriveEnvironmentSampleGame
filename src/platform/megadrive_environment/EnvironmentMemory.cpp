/**
 * @file EnvironmentMemory.cpp
 * Delegation from the sample memory API to MegaDriveEnvironment SystemMemory.
 */

#include "MegaDriveEnvironmentSampleGame/platform/megadrive_environment/EnvironmentMemory.hpp"

#include "system/memory/SystemMemory.hpp"

namespace sample::platform::megadrive_environment {

EnvironmentMemory::EnvironmentMemory(SystemMemory &memory) : memory_(memory) {
}

std::uint8_t EnvironmentMemory::read8(memory::Address address) {
    // SystemMemory owns mapping and device dispatch. Normalize here to mirror
    // the physical 24-bit address bus before handing it the access.
    return memory_.readByte(memory::Memory::normalize(address));
}

std::uint16_t EnvironmentMemory::read16(memory::Address address) {
    return memory_.readWord(memory::Memory::normalize(address));
}

std::uint32_t EnvironmentMemory::read32(memory::Address address) {
    return memory_.readLong(memory::Memory::normalize(address));
}

void EnvironmentMemory::write8(memory::Address address, std::uint8_t value) {
    memory_.writeByte(memory::Memory::normalize(address), value);
}

void EnvironmentMemory::write16(memory::Address address, std::uint16_t value) {
    memory_.writeWord(memory::Memory::normalize(address), value);
}

void EnvironmentMemory::write32(memory::Address address, std::uint32_t value) {
    memory_.writeLong(memory::Memory::normalize(address), value);
}

} // namespace sample::platform::megadrive_environment
