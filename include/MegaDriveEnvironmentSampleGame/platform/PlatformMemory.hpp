#pragma once

/**
 * @file PlatformMemory.hpp
 * Selects the target's implementation of the common memory API.
 */

#include "MegaDriveEnvironmentSampleGame/memory/Memory.hpp"

#if !defined(SAMPLE_FREESTANDING)
class MegaDriveEnvironment;
class SystemMemory;
#endif

namespace sample::platform {

#if defined(SAMPLE_FREESTANDING)

/**
 * Memory backend used by the game on its current target.
 *
 * On the cartridge target, `memory::Memory` is already a concrete, stateless
 * class whose primitive accesses are defined `always_inline` in its header.
 * This alias keeps target-neutral construction code readable without adding a
 * derived object, virtual table, wrapper, or call boundary.
 */
using PlatformMemory = memory::Memory;

#else

/**
 * PC adapter from the common memory API to MegaDriveEnvironment.
 *
 * Virtual dispatch is intentional on the host: it lets the shared game use
 * the environment backend and lightweight test doubles. The adapter can also
 * yield during waits instead of busy-waiting a host CPU core.
 */
class PlatformMemory final : public memory::Memory {
  public:
    /**
     * Adapts MegaDriveEnvironment's bus and optional runtime services.
     * Supplied objects must outlive this adapter.
     */
    explicit PlatformMemory(SystemMemory &memory, MegaDriveEnvironment *environment = nullptr) noexcept;

    std::uint8_t read8(memory::Address address) noexcept override;
    std::uint16_t read16(memory::Address address) noexcept override;
    std::uint32_t read32(memory::Address address) noexcept override;

    void write8(memory::Address address, std::uint8_t value) noexcept override;
    void write16(memory::Address address, std::uint16_t value) noexcept override;
    void write32(memory::Address address, std::uint32_t value) noexcept override;

    /** Uses the target's appropriate polling or cooperative-wait strategy. */
    bool waitFor16(memory::Address address,
                   std::uint16_t mask,
                   std::uint16_t expected) noexcept override;

  private:
    /** Emulated bus and memory-mapped device dispatcher. */
    SystemMemory &memory_;
    /** Owning runtime, used for VSync and cancellation during shutdown. */
    MegaDriveEnvironment *environment_;
};

#endif

} // namespace sample::platform
