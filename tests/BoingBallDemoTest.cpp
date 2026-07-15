#include "MegaDriveEnvironmentSampleGame/BoingBallDemo.hpp"
#include "MegaDriveEnvironmentSampleGame/VdpUtils.hpp"

#include <cassert>
#include <cstddef>

namespace {

class RecordingMemory final : public sample::memory::Memory {
  public:
    explicit RecordingMemory(bool pal = false) : pal_(pal) {
    }

    std::uint8_t read8(sample::memory::Address address) override {
        return address == 0xA10001 && pal_ ? 0x40 : 0;
    }
    std::uint16_t read16(sample::memory::Address address) override {
        return address == 0xC00008 ? static_cast<std::uint16_t>(verticalCounter << 8) : 0;
    }
    std::uint32_t read32(sample::memory::Address) override {
        return 0;
    }

    void write8(sample::memory::Address, std::uint8_t) override {
    }
    void write16(sample::memory::Address address, std::uint16_t value) override {
        if (address >= kBufferStart && address < kBufferEnd) {
            ++bufferWordWrites;
            for (int shift = 0; shift < 16; shift += 4) {
                sawRasterIndex[(value >> shift) & 0x0F] = true;
            }
            if (address < minimumBufferAddress) {
                minimumBufferAddress = address;
            }
            if (address > maximumBufferAddress) {
                maximumBufferAddress = address;
            }
        }
        if (address == sample::vdp::kControlPort && value == 0x0080) {
            sawDmaCommand = true;
        }
    }
    void write32(sample::memory::Address address, std::uint32_t value) override {
        write16(address, static_cast<std::uint16_t>(value >> 16));
        write16(address + 2, static_cast<std::uint16_t>(value));
    }

    void resetRecording() {
        bufferWordWrites = 0;
        minimumBufferAddress = 0xFFFFFFFF;
        maximumBufferAddress = 0;
        sawDmaCommand = false;
        for (auto &seen : sawRasterIndex) {
            seen = false;
        }
    }

    static constexpr sample::memory::Address kBufferStart = 0xFF1000;
    static constexpr sample::memory::Address kBufferEnd = 0xFF3000;
    bool pal_;
    std::uint8_t verticalCounter = 0;
    std::size_t bufferWordWrites = 0;
    sample::memory::Address minimumBufferAddress = 0xFFFFFFFF;
    sample::memory::Address maximumBufferAddress = 0;
    bool sawDmaCommand = false;
    bool sawRasterIndex[16]{};
};

} // namespace

int main() {
    RecordingMemory ntscMemory;
    sample::demo::BoingBallDemo demo{ntscMemory};
    demo.initialize();
    assert(demo.refreshRate() == 60);
    assert(demo.displayedFps() == 0);
    // Nine wall tiles and 320 perspective-floor tiles are generated in
    // software before being uploaded; no pre-authored graphics are sampled.
    assert(ntscMemory.bufferWordWrites == (9 + 320) * 16);

    demo.activate();
    assert(demo.ballX() == 8);
    assert(demo.ballY() == 80);
    assert(demo.ballSize() == 96);
    assert(demo.rotationAxis() == sample::demo::RotationAxis::Theta);

    // The first (right) wall switches to phi-only rotation; returning to the
    // left wall switches back to theta-only rotation.
    sample::demo::BounceEvents rightWallEvents;
    for (int frame = 0; frame < 300 && !rightWallEvents.hitWall; ++frame) {
        rightWallEvents = demo.update();
    }
    assert(rightWallEvents.hitWall);
    assert(demo.rotationAxis() == sample::demo::RotationAxis::Phi);
    sample::demo::BounceEvents leftWallEvents;
    for (int frame = 0; frame < 300 && !leftWallEvents.hitWall; ++frame) {
        leftWallEvents = demo.update();
    }
    assert(leftWallEvents.hitWall);
    assert(demo.rotationAxis() == sample::demo::RotationAxis::Theta);
    demo.activate();

    ntscMemory.resetRecording();
    demo.render();
    // The default 96-pixel ball generates exactly 12x12 tiles: no fixed
    // 128-pixel transparent canvas is rasterized.
    assert(ntscMemory.bufferWordWrites == 144 * 16);
    assert(ntscMemory.minimumBufferAddress == RecordingMemory::kBufferStart);
    assert(ntscMemory.maximumBufferAddress == RecordingMemory::kBufferStart + 144 * 32 - 2);
    assert(!ntscMemory.sawDmaCommand);
    assert(ntscMemory.sawRasterIndex[1] || ntscMemory.sawRasterIndex[2] ||
           ntscMemory.sawRasterIndex[3]);
    assert(ntscMemory.sawRasterIndex[4] || ntscMemory.sawRasterIndex[5] ||
           ntscMemory.sawRasterIndex[6]);
    assert(!ntscMemory.sawRasterIndex[8] && !ntscMemory.sawRasterIndex[9] &&
           !ntscMemory.sawRasterIndex[10]);

    // The completed Work RAM surface is DMA'd on the next VBlank, then a new
    // surface is built using whatever beam-time budget remains.
    ntscMemory.resetRecording();
    demo.render();
    assert(ntscMemory.sawDmaCommand);
    // Fully transparent corner tiles remain zero across rotations and are
    // skipped after the occupancy mask has been learned from the first surface.
    assert(ntscMemory.bufferWordWrites < 144 * 16);
    assert(ntscMemory.bufferWordWrites > 0);

    // Zoom changes continuously by one pixel per held-button VBlank.
    demo.activate();
    for (int frame = 0; frame < 32; ++frame) {
        (void)demo.update(true, false);
    }
    assert(demo.ballSize() == 128);
    for (int frame = 0; frame < 64; ++frame) {
        (void)demo.update(false, true);
    }
    assert(demo.ballSize() == 64);

    // The 64-pixel display size uses only 8x8 tiles. The first render finishes
    // the already-frozen 96-pixel surface; the second starts the new size.
    ntscMemory.resetRecording();
    demo.render();
    ntscMemory.resetRecording();
    demo.render();
    assert(ntscMemory.bufferWordWrites == 64 * 16);

    // With an unconstrained mock beam counter, PC-like execution completes a
    // surface per callback after the one-frame pipeline fill.
    demo.activate();
    for (int frame = 0; frame < 60; ++frame) {
        (void)demo.update();
        demo.render();
    }
    assert(demo.displayedFps() == 59);

    int floorHits = 0;
    int wallHits = 0;
    for (int frame = 0; frame < 2000; ++frame) {
        const auto events = demo.update();
        floorHits += events.hitFloor ? 1 : 0;
        wallHits += events.hitWall ? 1 : 0;
        assert(demo.ballX() >= 8);
        assert(demo.ballX() <= 216);
        assert(demo.ballY() <= 112);
    }
    assert(floorHits > 0);
    assert(wallHits > 0);
    assert(demo.displayedFps() == 59);

    // A real-MD-like beam deadline yields after one bounded tile;
    // clearing the deadline lets the exact same renderer resume next VBlank.
    RecordingMemory budgetMemory;
    sample::demo::BoingBallDemo budgetDemo{budgetMemory};
    budgetDemo.initialize();
    budgetDemo.activate();
    budgetMemory.resetRecording();
    budgetMemory.verticalCounter = 192;
    budgetDemo.render();
    assert(budgetMemory.bufferWordWrites == 16);
    assert(!budgetMemory.sawDmaCommand);
    budgetMemory.resetRecording();
    budgetMemory.verticalCounter = 0;
    budgetDemo.render();
    assert(budgetMemory.bufferWordWrites == (144 - 1) * 16);
    budgetMemory.resetRecording();
    budgetDemo.render();
    assert(budgetMemory.sawDmaCommand);

    RecordingMemory palMemory{true};
    sample::demo::BoingBallDemo palDemo{palMemory};
    palDemo.initialize();
    palDemo.activate();
    assert(palDemo.refreshRate() == 50);
    for (int frame = 0; frame < 50; ++frame) {
        (void)palDemo.update();
        palDemo.render();
    }
    assert(palDemo.displayedFps() == 49);
    return 0;
}
