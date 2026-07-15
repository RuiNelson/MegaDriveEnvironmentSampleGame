#include "MegaDriveEnvironmentSampleGame/Memory.hpp"

#include "system/memory/SystemMemory.hpp"

#include <cassert>
#include <cstdint>

int main() {
    SystemMemory systemMemory;
    sample::platform::PlatformMemory memory{systemMemory};
    constexpr sample::memory::Address base = 0xFF0000;

    memory.write32(base, 0x12345678);
    assert(memory.read8(base) == 0x12);
    assert(memory.read16(base) == 0x1234);
    assert(memory.read32(base) == 0x12345678);

    assert(sample::memory::Memory::normalize(0xFFFF0000u) == base);
}
