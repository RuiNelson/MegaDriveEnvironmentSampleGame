#pragma once

#if defined(SAMPLE_FREESTANDING)

// The Homebrew m68k-elf compiler intentionally ships without a target C++
// standard library. These are the small compiler-provided types needed by the
// shared memory/controller headers when producing the cartridge binary.
namespace std {

using uint8_t = __UINT8_TYPE__;
using uint16_t = __UINT16_TYPE__;
using uint32_t = __UINT32_TYPE__;
using uintptr_t = __UINTPTR_TYPE__;
using size_t = __SIZE_TYPE__;

template <typename Element>
class span {
  public:
    constexpr span(Element *data, size_t size) : data_(data), size_(size) {
    }

    template <size_t Size>
    constexpr span(Element (&data)[Size]) : data_(data), size_(Size) {
    }

    [[nodiscard]] constexpr size_t size() const {
        return size_;
    }

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
