#pragma once

/**
 * @file StdCompat.hpp
 * @brief Minimal standard-library compatibility layer for real hardware.
 *
 * The PC target uses the normal C++ standard library. The freestanding
 * m68k-elf toolchain used by the real-hardware target provides compiler
 * intrinsic integer types but no target C++ headers, so this file supplies only the
 * handful of names needed by the shared game interfaces.
 *
 * This is deliberately not a general-purpose standard-library replacement.
 */

#if defined(MEGADRIVE) && defined(PC)
#error "Define only one sample-game target: MEGADRIVE or PC"
#elif !defined(MEGADRIVE) && !defined(PC)
#error "Define one sample-game target: MEGADRIVE or PC"
#endif

#if defined(MEGADRIVE)

// The Homebrew m68k-elf compiler intentionally ships without a target C++
// standard library. These are the small compiler-provided types needed by the
// shared memory/controller headers when producing the real-hardware ROM.
namespace std {

using uint8_t = __UINT8_TYPE__;
using uint16_t = __UINT16_TYPE__;
using uint32_t = __UINT32_TYPE__;
using uintptr_t = __UINTPTR_TYPE__;

} // namespace std

#else

#include <cstdint>

#endif
