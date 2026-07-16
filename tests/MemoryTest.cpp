#include "MegaDriveEnvironmentSampleGame/Memory.hpp"

#include "system/memory/SystemMemory.hpp"

#include <cassert>
#include <cstdint>

int main() {
    SystemMemory systemMemory;
    sample::memory::bind(systemMemory);
    constexpr sample::memory::Address base = 0xFF0000;

    sample::memory::write32(base, 0x12345678);
    assert(sample::memory::read8(base) == 0x12);
    assert(sample::memory::read16(base) == 0x1234);
    assert(sample::memory::read32(base) == 0x12345678);

    assert(sample::memory::normalize(0xFFFF0000u) == base);
    sample::memory::unbind();
}
