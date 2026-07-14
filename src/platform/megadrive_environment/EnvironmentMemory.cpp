/**
 * @file EnvironmentMemory.cpp
 * Delegation from the sample memory API to MegaDriveEnvironment SystemMemory.
 */

#include "MegaDriveEnvironmentSampleGame/platform/megadrive_environment/EnvironmentMemory.hpp"

#include "system/MegaDriveEnvironment.hpp"
#include "system/memory/SystemMemory.hpp"

#include <SDL3/SDL.h>

namespace sample::platform::megadrive_environment {

EnvironmentMemory::EnvironmentMemory(SystemMemory &memory, MegaDriveEnvironment *environment)
    : memory_(memory), environment_(environment) {
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

bool EnvironmentMemory::waitFor16(memory::Address address, std::uint16_t mask, std::uint16_t expected) {
    constexpr memory::Address kVdpControlPort = 0xC00004;
    constexpr std::uint16_t kVBlankStatus = 0x0008;
    const auto normalizedAddress = memory::Memory::normalize(address);

    if (environment_ != nullptr && normalizedAddress == kVdpControlPort && mask == kVBlankStatus) {
        VDP::Interrupt interrupt;
        if ((expected & kVBlankStatus) == 0) {
            // The renderer owns the VDP mutex throughout active display, so a
            // host read cannot reliably observe the short status-bit transition.
            // Draining old events arms the following wait for a fresh frame.
            while (environment_->vdp().popInterrupt(interrupt)) {
            }
            return true;
        }

        while (!environment_->shouldQuit()) {
            while (environment_->vdp().popInterrupt(interrupt)) {
                if (interrupt.type == VDP::Interrupt::VSync) {
                    return true;
                }
            }
            SDL_Delay(1);
        }
        return false;
    }

    const auto value = memory_.waitForWordBits(
        normalizedAddress, mask, expected, [this] {
            SDL_Delay(1);
            return environment_ == nullptr || !environment_->shouldQuit();
        });
    return (value & mask) == (expected & mask);
}

} // namespace sample::platform::megadrive_environment
