#pragma once

/**
 * @file HardwareMemory.hpp
 * Real-hardware implementation of the common Mega Drive memory interface.
 */

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
    /** No setup is needed: addresses are dereferenced directly when accessed. */
    HardwareMemory() = default;

    std::uint8_t read8(memory::Address address) noexcept override;
    std::uint16_t read16(memory::Address address) noexcept override;
    std::uint32_t read32(memory::Address address) noexcept override;

    void write8(memory::Address address, std::uint8_t value) noexcept override;
    void write16(memory::Address address, std::uint16_t value) noexcept override;
    void write32(memory::Address address, std::uint32_t value) noexcept override;

    /** Tight bare-metal register polling with no virtual call inside the loop. */
    bool waitFor16(memory::Address address,
                   std::uint16_t mask,
                   std::uint16_t expected) noexcept override;
};

} // namespace sample::platform::megadrive
