#pragma once

/**
 * @file StdCompat.hpp
 * @brief Minimal standard-library compatibility layer for real hardware.
 *
 * The host target uses the normal C++ standard library. The freestanding
 * m68k-elf toolchain used by the real-hardware target provides compiler
 * intrinsic integer types but no target C++ headers, so this file supplies only the
 * handful of names needed by the shared game interfaces.
 *
 * This is deliberately not a general-purpose standard-library replacement.
 */

#if defined(SAMPLE_FREESTANDING)

// The Homebrew m68k-elf compiler intentionally ships without a target C++
// standard library. These are the small compiler-provided types needed by the
// shared memory/controller headers when producing the real-hardware ROM.
namespace std {

using uint8_t = __UINT8_TYPE__;
using uint16_t = __UINT16_TYPE__;
using uint32_t = __UINT32_TYPE__;
using uintptr_t = __UINTPTR_TYPE__;
using size_t = __SIZE_TYPE__;

/**
 * Non-owning contiguous view implementing the subset of `std::span` we use.
 *
 * Bounds checking and dynamic extents beyond this subset are not provided.
 */
template <typename Element>
class span {
  public:
    /** Creates a view over @p size elements beginning at @p data. */
    constexpr span(Element *data, size_t size) : data_(data), size_(size) {
    }

    template <size_t Size>
    /** Creates a view whose extent is inferred from a C array. */
    constexpr span(Element (&data)[Size]) : data_(data), size_(Size) {
    }

    /** Returns the number of elements in the view. */
    [[nodiscard]] constexpr size_t size() const {
        return size_;
    }

    /** Returns an unchecked reference to the element at @p index. */
    [[nodiscard]] constexpr Element &operator[](size_t index) const {
        return data_[index];
    }

  private:
    Element *data_;
    size_t size_;
};

} // namespace std

#else

#include <cstddef>
#include <cstdint>
#include <span>

#endif
