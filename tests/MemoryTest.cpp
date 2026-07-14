#include "MegaDriveEnvironmentSampleGame/platform/megadrive_environment/EnvironmentMemory.hpp"

#include "system/memory/SystemMemory.hpp"

#include <array>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <thread>

int main() {
    SystemMemory systemMemory;
    sample::platform::megadrive_environment::EnvironmentMemory memory{systemMemory};
    constexpr sample::memory::Address base = sample::memory::kWorkRamStart;

    memory.write32(base, 0x12345678);
    assert(memory.read8(base) == 0x12);
    assert(memory.read16(base) == 0x1234);
    assert(memory.read32(base) == 0x12345678);

    constexpr std::array<std::uint8_t, 6> source{1, 2, 3, 4, 5, 6};
    memory.write(base + 16, source);
    memory.copy(base + 16, base + 18, source.size());

    std::array<std::uint8_t, 8> copied{};
    memory.read(base + 16, copied);
    assert((copied == std::array<std::uint8_t, 8>{1, 2, 1, 2, 3, 4, 5, 6}));

    memory.fill(base + 32, 0xA5, 4);
    assert(memory.read32(base + 32) == 0xA5A5A5A5);

    memory.write16(base + 40, 0);
    std::thread producer{[&memory, base] {
        std::this_thread::sleep_for(std::chrono::milliseconds{5});
        memory.write16(base + 40, 0x0008);
    }};
    assert(memory.waitFor16(base + 40, 0x0008, 0x0008));
    producer.join();

    assert(sample::memory::Memory::normalize(0xFFFF0000u) == base);
}
