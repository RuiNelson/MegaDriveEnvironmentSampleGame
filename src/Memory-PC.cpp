/**
 * @file Memory-PC.cpp
 * Free-function bus access delegated to a bound PC backend.
 */

#include "MegaDriveEnvironmentSampleGame/Memory.hpp"

#include "system/memory/SystemMemory.hpp"

#include <cassert>

#if !defined(PC) || defined(MEGADRIVE)
#error "Memory-PC.cpp requires the PC target"
#endif

namespace sample::memory {
namespace {

Backend g_backend{};

std::uint8_t systemRead8(void *context, Address address) {
    return static_cast<SystemMemory *>(context)->readByte(normalize(address));
}

std::uint16_t systemRead16(void *context, Address address) {
    return static_cast<SystemMemory *>(context)->readWord(normalize(address));
}

std::uint32_t systemRead32(void *context, Address address) {
    return static_cast<SystemMemory *>(context)->readLong(normalize(address));
}

void systemWrite8(void *context, Address address, std::uint8_t value) {
    static_cast<SystemMemory *>(context)->writeByte(normalize(address), value);
}

void systemWrite16(void *context, Address address, std::uint16_t value) {
    static_cast<SystemMemory *>(context)->writeWord(normalize(address), value);
}

void systemWrite32(void *context, Address address, std::uint32_t value) {
    static_cast<SystemMemory *>(context)->writeLong(normalize(address), value);
}

} // namespace

void bind(const Backend &backend) noexcept {
    g_backend = backend;
}

void bind(SystemMemory &systemMemory) noexcept {
    g_backend = Backend{
        systemRead8,
        systemRead16,
        systemRead32,
        systemWrite8,
        systemWrite16,
        systemWrite32,
        &systemMemory,
    };
}

void unbind() noexcept {
    g_backend = Backend{};
}

std::uint8_t read8(Address address) noexcept {
    assert(g_backend.read8 != nullptr);
    return g_backend.read8(g_backend.context, address);
}

std::uint16_t read16(Address address) noexcept {
    assert(g_backend.read16 != nullptr);
    return g_backend.read16(g_backend.context, address);
}

std::uint32_t read32(Address address) noexcept {
    assert(g_backend.read32 != nullptr);
    return g_backend.read32(g_backend.context, address);
}

void write8(Address address, std::uint8_t value) noexcept {
    assert(g_backend.write8 != nullptr);
    g_backend.write8(g_backend.context, address, value);
}

void write16(Address address, std::uint16_t value) noexcept {
    assert(g_backend.write16 != nullptr);
    g_backend.write16(g_backend.context, address, value);
}

void write32(Address address, std::uint32_t value) noexcept {
    assert(g_backend.write32 != nullptr);
    g_backend.write32(g_backend.context, address, value);
}

} // namespace sample::memory
