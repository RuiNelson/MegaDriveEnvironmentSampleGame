#include "MegaDriveEnvironmentSampleGame/platform/megadrive/HardwareMemory.hpp"

namespace sample::platform::megadrive {
namespace {

template <typename Value>
volatile Value &bus(memory::Address address) {
    const auto hostAddress = static_cast<std::uintptr_t>(memory::Memory::normalize(address));
    return *reinterpret_cast<volatile Value *>(hostAddress);
}

} // namespace

std::uint8_t HardwareMemory::read8(memory::Address address) {
    return bus<std::uint8_t>(address);
}

std::uint16_t HardwareMemory::read16(memory::Address address) {
    return bus<std::uint16_t>(address);
}

std::uint32_t HardwareMemory::read32(memory::Address address) {
    return bus<std::uint32_t>(address);
}

void HardwareMemory::write8(memory::Address address, std::uint8_t value) {
    bus<std::uint8_t>(address) = value;
}

void HardwareMemory::write16(memory::Address address, std::uint16_t value) {
    bus<std::uint16_t>(address) = value;
}

void HardwareMemory::write32(memory::Address address, std::uint32_t value) {
    bus<std::uint32_t>(address) = value;
}

} // namespace sample::platform::megadrive
