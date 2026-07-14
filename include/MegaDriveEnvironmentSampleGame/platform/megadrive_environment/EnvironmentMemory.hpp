#pragma once

#include "MegaDriveEnvironmentSampleGame/memory/Memory.hpp"

class SystemMemory;

namespace sample::platform::megadrive_environment {

/** Adapts MegaDriveEnvironment's emulated address space to the common API. */
class EnvironmentMemory final : public memory::Memory {
  public:
    explicit EnvironmentMemory(SystemMemory &memory);

    std::uint8_t read8(memory::Address address) override;
    std::uint16_t read16(memory::Address address) override;
    std::uint32_t read32(memory::Address address) override;

    void write8(memory::Address address, std::uint8_t value) override;
    void write16(memory::Address address, std::uint16_t value) override;
    void write32(memory::Address address, std::uint32_t value) override;

  private:
    SystemMemory &memory_;
};

} // namespace sample::platform::megadrive_environment
