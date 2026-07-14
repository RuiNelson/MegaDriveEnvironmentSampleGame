/**
 * @file Memory-PC.cpp
 * Delegation from the sample memory API to MegaDriveEnvironment SystemMemory.
 */

#include "MegaDriveEnvironmentSampleGame/Memory.hpp"

#include "system/MegaDriveEnvironment.hpp"
#include "system/memory/SystemMemory.hpp"

#include <SDL3/SDL.h>

#if defined(SAMPLE_FREESTANDING)
#error "Memory-PC.cpp must only be compiled for PC"
#endif

namespace sample::platform {

PlatformMemory::PlatformMemory(SystemMemory &memory, MegaDriveEnvironment *environment) noexcept
    : memory_(memory), environment_(environment) {
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

bool PlatformMemory::waitFor16(memory::Address address,
                               std::uint16_t mask,
                               std::uint16_t expected) noexcept {
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

} // namespace sample::platform
