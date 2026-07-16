#pragma once

/**
 * @file Memory.hpp
 * Target-neutral free-function access to the Mega Drive's 24-bit memory map.
 *
 * Shared game code calls sample::memory::read8 / write16 / etc. with 68000 bus addresses.
 * There is no Memory object and no virtual dispatch:
 *
 * - MEGADRIVE: each call is an always-inlined volatile bus access.
 * - PC: calls forward to a bound backend (SystemMemory, or a test double).
 *
 * Words and long words use big-endian order. Keep 16/32-bit accesses
 * word-aligned, as on real hardware.
 */

#include "MegaDriveEnvironmentSampleGame/StdCompat.hpp"

#if defined(PC)
class SystemMemory;
#endif

namespace sample::memory {

using Address = std::uint32_t;

/** Folds an integer address onto the 68000's 24-bit external bus. */
[[nodiscard]] constexpr Address normalize(Address address) noexcept {
    return address & 0x00FF'FFFF;
}

/**
 * One bus read. Multi-byte values use 68000 big-endian order.
 * Implementations preserve device side effects at mapped I/O ports.
 */
[[nodiscard]] std::uint8_t read8(Address address) noexcept;
[[nodiscard]] std::uint16_t read16(Address address) noexcept;
[[nodiscard]] std::uint32_t read32(Address address) noexcept;

/**
 * One bus write. Callers keep word and long-word addresses even, as required
 * by a real 68000.
 */
void write8(Address address, std::uint8_t value) noexcept;
void write16(Address address, std::uint16_t value) noexcept;
void write32(Address address, std::uint32_t value) noexcept;

#if defined(PC)

/**
 * Function-table bus backend for the PC target.
 *
 * Production code binds SystemMemory via bind(SystemMemory&). Unit tests bind a
 * RecordingMemory-style double by filling this table with thunks.
 */
struct Backend {
    std::uint8_t (*read8)(void *context, Address address);
    std::uint16_t (*read16)(void *context, Address address);
    std::uint32_t (*read32)(void *context, Address address);
    void (*write8)(void *context, Address address, std::uint8_t value);
    void (*write16)(void *context, Address address, std::uint16_t value);
    void (*write32)(void *context, Address address, std::uint32_t value);
    void *context;
};

/** Installs @p backend as the target of all subsequent bus read and write calls. */
void bind(const Backend &backend) noexcept;

/** Binds the environment SystemMemory map used by the PC executable. */
void bind(SystemMemory &systemMemory) noexcept;

/** Clears the bound backend; further bus access is undefined until bind(). */
void unbind() noexcept;

#endif

} // namespace sample::memory
