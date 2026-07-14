#pragma once

/**
 * @file PlatformMemory.hpp
 * Single target-memory API with one implementation selected by each build.
 */

#include "MegaDriveEnvironmentSampleGame/memory/Memory.hpp"

#if !defined(SAMPLE_FREESTANDING)
class MegaDriveEnvironment;
class SystemMemory;
#endif

namespace sample::platform {

/**
 * Memory backend used by the game on its current target.
 *
 * The class and public API are shared. The PC build links the implementation
 * under `platform/megadrive_environment`, while the cartridge build links the
 * bare-metal implementation under `platform/megadrive`.
 */
class PlatformMemory final : public memory::Memory {
  public:
#if defined(SAMPLE_FREESTANDING)
    /** Real hardware needs no state: addresses map directly to the 68000 bus. */
    PlatformMemory() noexcept = default;
#else
    /**
     * Adapts MegaDriveEnvironment's bus and optional runtime services.
     * Supplied objects must outlive this adapter.
     */
    explicit PlatformMemory(SystemMemory &memory, MegaDriveEnvironment *environment = nullptr) noexcept;
#endif

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

#if !defined(SAMPLE_FREESTANDING)
  private:
    /** Emulated bus and memory-mapped device dispatcher. */
    SystemMemory &memory_;
    /** Owning runtime, used for VSync and cancellation during shutdown. */
    MegaDriveEnvironment *environment_;
#endif
};

} // namespace sample::platform
