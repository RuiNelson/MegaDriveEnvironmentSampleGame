#pragma once

#include "MegaDriveEnvironmentSampleGame/memory/Memory.hpp"

namespace sample::platform::megadrive {

/**
 * Direct memory-bus implementation for a real Mega Drive 68000 target.
 *
 * This class must only be executed in a Mega Drive binary. Building it on the
 * host is useful as a compile check, but dereferencing its addresses on a PC is
 * intentionally invalid.
 */
class HardwareMemory final : public memory::Memory {
  public:
    HardwareMemory() = default;

    std::uint8_t read8(memory::Address address) override;
    std::uint16_t read16(memory::Address address) override;
    std::uint32_t read32(memory::Address address) override;

    void write8(memory::Address address, std::uint8_t value) override;
    void write16(memory::Address address, std::uint16_t value) override;
    void write32(memory::Address address, std::uint32_t value) override;
};

} // namespace sample::platform::megadrive
