/**
 * @file Memory.cpp
 * Byte-oriented bulk operations built on the target-specific bus primitives.
 */

#include "MegaDriveEnvironmentSampleGame/memory/Memory.hpp"

namespace sample::memory {

void Memory::read(Address source, std::span<std::uint8_t> destination) {
    // Using read8/write8 rather than raw host pointers preserves address
    // normalization and memory-mapped device side effects in every backend.
    for (std::size_t index = 0; index < destination.size(); ++index) {
        destination[index] = read8(source + static_cast<Address>(index));
    }
}

void Memory::write(Address destination, std::span<const std::uint8_t> source) {
    for (std::size_t index = 0; index < source.size(); ++index) {
        write8(destination + static_cast<Address>(index), source[index]);
    }
}

void Memory::copy(Address source, Address destination, std::size_t byteCount) {
    if (byteCount == 0 || normalize(source) == normalize(destination)) {
        return;
    }

    // Copy backwards only when destination starts inside the source interval;
    // this gives memmove semantics when objects are compacted in work RAM.
    if (normalize(destination) > normalize(source) &&
        normalize(destination) < normalize(source + static_cast<Address>(byteCount))) {
        for (std::size_t index = byteCount; index > 0; --index) {
            write8(destination + static_cast<Address>(index - 1), read8(source + static_cast<Address>(index - 1)));
        }
        return;
    }

    for (std::size_t index = 0; index < byteCount; ++index) {
        write8(destination + static_cast<Address>(index), read8(source + static_cast<Address>(index)));
    }
}

void Memory::fill(Address destination, std::uint8_t value, std::size_t byteCount) {
    for (std::size_t index = 0; index < byteCount; ++index) {
        write8(destination + static_cast<Address>(index), value);
    }
}

} // namespace sample::memory
