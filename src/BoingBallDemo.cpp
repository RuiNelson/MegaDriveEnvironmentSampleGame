/**
 * @file BoingBallDemo.cpp
 * Shared fixed-point simulation and software VDP-tile rasterizer.
 */

#include "MegaDriveEnvironmentSampleGame/BoingBallDemo.hpp"

#include "MegaDriveEnvironmentSampleGame/VdpUtils.hpp"

namespace sample::demo {
namespace {

constexpr std::uint16_t kFontTile = 1;
constexpr std::uint16_t kBallFirstTile = 128;
constexpr std::uint16_t kBallTileCount = 144;
constexpr std::uint16_t kShadowFirstTile = kBallFirstTile + kBallTileCount;
constexpr std::uint16_t kBallTileWordCount = kBallTileCount * 16;
constexpr std::uint16_t kFloorFirstTile = 300;
constexpr std::uint16_t kTileBufferTileCapacity = 144;
constexpr int kFloorFirstRow = 20;

// $FF0000-$FF0005 belong to the IRQ bridge. This explicit 4608-byte scratch
// region is safely below the supervisor stack and is the sole dynamic tile
// buffer. Both targets access it through the same memory bus API.
constexpr memory::Address kTileBufferAddress = 0xFF1000;

constexpr int kFixedShift = 8;
constexpr int kFixedOne = 1 << kFixedShift;
constexpr int kLeftEdge = 8;
constexpr int kRightEdge = 216;
constexpr int kFloorY = 112;
constexpr int kShadowY = 202;
constexpr int kSphereCenter = 23;
constexpr int kSphereRadius = 23;

constexpr memory::Address kHardwareVersionRegister = 0xA10001;
constexpr std::uint8_t kPalVideoBit = 0x40;

// Squares for coordinates -23..24. Keeping the products in ROM avoids the
// 68000 compiler's 32-bit multiplication runtime helper during initialization.
constexpr std::uint16_t kCoordinateSquares[48]{
    529, 484, 441, 400, 361, 324, 289, 256, 225, 196, 169, 144,
    121, 100, 81, 64, 49, 36, 25, 16, 9, 4, 1, 0,
    1, 4, 9, 16, 25, 36, 49, 64, 81, 100, 121, 144,
    169, 196, 225, 256, 289, 324, 361, 400, 441, 484, 529, 576,
};

// CRAM words use the Mega Drive's 0000BBB0GGG0RRR0 channel layout.
constexpr std::uint16_t kTextPalette[16]{
    0x0000, 0x0EEE, 0x0888, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};
constexpr std::uint16_t kBallPalette[16]{
    0x0000, // transparent
    0x0004, 0x000A, 0x000E, // shaded red
    0x0666, 0x0AAA, 0x0EEE, // shaded white
    0x0000,                 // silhouette rim
    0x0400, 0x0A00, 0x0E00, // shaded blue
    0, 0, 0, 0, 0,
};
constexpr std::uint16_t kShadowPalette[16]{
    0x0000, 0x0222, 0x0444, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};
constexpr std::uint16_t kBackdropPalette[16]{
    0x0000, 0x0808, 0x0A0A, 0x0AAA, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

[[gnu::always_inline]] inline std::uint8_t floorGridPixel(int x, int y) {
    // Perspective depth lines become progressively farther apart toward the
    // viewer. The eleven rays meet at the wall/floor horizon and end 32 pixels
    // apart, matching the wall grid's cadence.
    const bool horizontal = y == 0 || y == 2 || y == 5 || y == 9 || y == 14 ||
                            y == 21 || y == 29 || y == 38 || y == 49 || y == 61;
    if (horizontal) {
        return 1;
    }

    const int spacing = (y + 2) >> 1;
    int lineX = 160 - (spacing << 2) - spacing;
    for (int line = 0; line < 11; ++line) {
        if (x == lineX) {
            return 1;
        }
        lineX += spacing;
    }
    return 3; // opaque grey hides the upright Plane B grid below the horizon
}

} // namespace

BoingBallDemo::BoingBallDemo(memory::Memory &memory) : memory_(memory) {
}

void BoingBallDemo::initialize() {
    // Build the immutable sphere geometry once. Runtime rasterization then
    // needs no square roots, divisions, heap storage, floating point or trig.
    for (int y = 0; y < kLogicalBallSize; ++y) {
        const int sphereY = y - kSphereCenter;
        for (int x = 0; x < kLogicalBallSize; ++x) {
            const int sphereX = x - kSphereCenter;
            const int radiusSquared = kCoordinateSquares[x] + kCoordinateSquares[y];
            auto &point = surface_[y * kLogicalBallSize + x];
            if (radiusSquared > kSphereRadius * kSphereRadius) {
                point.colors = 0;
                point.longitude = 0;
                point.latitude = 0;
                continue;
            }

            const int depth = integerSquareRoot(kSphereRadius * kSphereRadius - radiusSquared);
            point.longitude = approximateAngle(sphereX, depth);
            point.latitude = approximateAngle(sphereY, depth);

            if (depth <= 3) {
                point.colors = 0x77;
                continue;
            }

            // Lighting never changes with texture rotation. Red and white are
            // packed here; the matching blue shade is four palette indices
            // above white, avoiding another byte in the sphere lookup.
            const int light = depth - sphereX / 2 - sphereY / 2;
            const std::uint8_t shade = light >= 24 ? 2 : (light >= 11 ? 1 : 0);
            const auto red = static_cast<std::uint8_t>(1 + shade);
            const auto white = static_cast<std::uint8_t>(4 + shade);
            point.colors = static_cast<std::uint8_t>((red << 4) | white);
        }
    }

    refreshRate_ = (memory_.read8(kHardwareVersionRegister) & kPalVideoBit) != 0 ? 50 : 60;
    displayedFps_ = static_cast<std::uint8_t>(refreshRate_ >> 1);
    uploadFloorGrid();
}

void BoingBallDemo::activate() {
    ballXFixed_ = kLeftEdge * kFixedOne;
    ballYFixed_ = 80 * kFixedOne;
    velocityXFixed_ = 320; // 1.25 pixels per displayed frame
    velocityYFixed_ = 0;
    thetaPhase_ = 0;
    phiPhase_ = 0;
    framesInSample_ = 0;
    sampleAge_ = 0;
    displayedFps_ = static_cast<std::uint8_t>(refreshRate_ >> 1);
    fpsNeedsRender_ = true;
    rasterizeThisFrame_ = true;
    oddAnimationFrame_ = false;
    oddPhiFrame_ = false;

    vdp::loadPalette(memory_, 0, kTextPalette);
    vdp::loadPalette(memory_, 1, kBallPalette);
    vdp::loadPalette(memory_, 2, kShadowPalette);
    vdp::loadPalette(memory_, 3, kBackdropPalette);
    vdp::writeRegister(memory_, 0x07, 0x33); // grey backdrop behind the wall grid

    // Only visible Plane A cells need clearing during the screen transition.
    // Plane B deliberately retains the sample's bordered floor tile; the demo
    // palette turns it into a subdued grey Amiga-style backdrop grid.
    vdp::fillPlaneArea(memory_, vdp::kPlaneA, 0, 0, 40, 28, vdp::tileDescriptor(0));
    vdp::writeText(memory_, vdp::kPlaneA, 2, 1, "SOFTWARE 3D BOING BALL", kFontTile);
    vdp::writeText(memory_, vdp::kPlaneA, 31, 1, "FPS", kFontTile);
    vdp::writeText(memory_, vdp::kPlaneA, 14, 3, "START RETURN", kFontTile);

    // The Window plane replaces Plane A below the horizon, yielding a second
    // perspective surface while Plane B remains the upright wall grid.
    vdp::writeRegister(memory_, 0x11, 0x00);
    vdp::writeRegister(memory_, 0x12, static_cast<std::uint8_t>(0x80 | kFloorFirstRow));

    uploadShadowTiles();
    renderFps();
}

BounceEvents BoingBallDemo::update() {
    BounceEvents events;
    ballXFixed_ += velocityXFixed_;
    if (ballXFixed_ <= kLeftEdge * kFixedOne) {
        ballXFixed_ = kLeftEdge * kFixedOne;
        velocityXFixed_ = -velocityXFixed_;
        events.hitWall = true;
    } else if (ballXFixed_ >= kRightEdge * kFixedOne) {
        ballXFixed_ = kRightEdge * kFixedOne;
        velocityXFixed_ = -velocityXFixed_;
        events.hitWall = true;
    }

    velocityYFixed_ += 48; // 0.1875 pixels/frame^2
    ballYFixed_ += velocityYFixed_;
    if (ballYFixed_ >= kFloorY * kFixedOne) {
        ballYFixed_ = kFloorY * kFixedOne;
        velocityYFixed_ = -1408; // 5.5 pixels/frame upwards
        events.hitFloor = true;
    }

    // The silhouette still moves at every VBlank, while the relatively costly
    // software texture update runs at 30 Hz (25 Hz PAL). Tile persistence in
    // VRAM makes this temporal decimation free between raster passes.
    oddAnimationFrame_ = !oddAnimationFrame_;
    rasterizeThisFrame_ = !oddAnimationFrame_;
    if (rasterizeThisFrame_) {
        thetaPhase_ = static_cast<std::uint8_t>((thetaPhase_ + 1u) & 31u);
        oddPhiFrame_ = !oddPhiFrame_;
        if (!oddPhiFrame_) {
            phiPhase_ = static_cast<std::uint8_t>((phiPhase_ + 1u) & 31u);
        }
        ++framesInSample_;
    }

    ++sampleAge_;
    if (sampleAge_ >= refreshRate_) {
        displayedFps_ = framesInSample_;
        framesInSample_ = 0;
        sampleAge_ = 0;
        fpsNeedsRender_ = true;
    }
    return events;
}

void BoingBallDemo::render() {
    if (rasterizeThisFrame_) {
        rasterizeBallTiles();
        vdp::dmaToVram(memory_, kTileBufferAddress,
                       static_cast<std::uint16_t>(kBallFirstTile * 32), kBallTileWordCount);
        rasterizeThisFrame_ = false;
    }

    const int x = ballX();
    const int y = ballY();
    // Nine linked 4x4 sprites form the 96x96 image. A scanline intersects only
    // three of them, comfortably below the H40 sprite-per-line limit.
    int sprite = 0;
    for (int blockY = 0; blockY < 3; ++blockY) {
        for (int blockX = 0; blockX < 3; ++blockX) {
            const int next = sprite + 1;
            const auto tile = static_cast<std::uint16_t>(kBallFirstTile + sprite * 16);
            vdp::writeSprite(memory_, sprite, x + blockX * 32, y + blockY * 32,
                             4, 4, tile, 1, next);
            ++sprite;
        }
    }

    // Lower SAT indices win sprite overlap, so the ball naturally occludes its
    // ground shadow at the bottom of each bounce.
    vdp::writeSprite(memory_, 9, x, kShadowY, 4, 1, kShadowFirstTile, 2, 10);
    vdp::writeSprite(memory_, 10, x + 32, kShadowY, 4, 1, kShadowFirstTile + 4, 2, 11);
    vdp::writeSprite(memory_, 11, x + 64, kShadowY, 4, 1, kShadowFirstTile + 8, 2, 0);

    if (fpsNeedsRender_) {
        renderFps();
    }
}

int BoingBallDemo::ballX() const {
    return ballXFixed_ >> kFixedShift;
}

int BoingBallDemo::ballY() const {
    return ballYFixed_ >> kFixedShift;
}

std::uint8_t BoingBallDemo::displayedFps() const {
    return displayedFps_;
}

std::uint8_t BoingBallDemo::refreshRate() const {
    return refreshRate_;
}

int BoingBallDemo::integerSquareRoot(int value) {
    int root = 0;
    int bit = 1 << 14;
    while (bit > value) {
        bit >>= 2;
    }
    while (bit != 0) {
        if (value >= root + bit) {
            value -= root + bit;
            root = (root >> 1) + bit;
        } else {
            root >>= 1;
        }
        bit >>= 2;
    }
    return root;
}

std::uint8_t BoingBallDemo::approximateAngle(int coordinate, int depth) {
    const int magnitude = coordinate < 0 ? -coordinate : coordinate;
    const int denominator = magnitude + depth;
    int angleMagnitude = 0;
    int numerator = magnitude << 4;
    while (denominator != 0 && numerator >= denominator) {
        numerator -= denominator;
        ++angleMagnitude;
    }
    const int offset = coordinate < 0 ? -angleMagnitude : angleMagnitude;
    return static_cast<std::uint8_t>((16 + offset) & 31);
}

void BoingBallDemo::rasterizeBallTiles() {
    memory::Address destination = kTileBufferAddress;

    // VDP sprite patterns are column-major. Each of the nine blocks is a 4x4
    // sprite. Every logical source pixel becomes a 2x2 output block, so the
    // displayed diameter doubles while only 48x48 3D samples are evaluated.
    for (int blockY = 0; blockY < 3; ++blockY) {
        for (int blockX = 0; blockX < 3; ++blockX) {
            for (int tileX = 0; tileX < 4; ++tileX) {
                for (int tileY = 0; tileY < 4; ++tileY) {
                    for (int logicalRow = 0; logicalRow < 4; ++logicalRow) {
                        const int x = (blockX * 32 + tileX * 8) >> 1;
                        const int y = (blockY * 32 + tileY * 8) / 2 + logicalRow;
                        const auto pixel0 = rasterizedPixel(x, y);
                        const auto pixel1 = rasterizedPixel(x + 1, y);
                        const auto pixel2 = rasterizedPixel(x + 2, y);
                        const auto pixel3 = rasterizedPixel(x + 3, y);
                        const auto left = static_cast<std::uint16_t>(
                            (pixel0 << 12) | (pixel0 << 8) | (pixel1 << 4) | pixel1);
                        const auto right = static_cast<std::uint16_t>(
                            (pixel2 << 12) | (pixel2 << 8) | (pixel3 << 4) | pixel3);

                        // Duplicate the row vertically in the tile buffer.
                        memory_.write16(destination, left);
                        memory_.write16(destination + 2, right);
                        memory_.write16(destination + 4, left);
                        memory_.write16(destination + 6, right);
                        destination += 8;
                    }
                }
            }
        }
    }
}

void BoingBallDemo::uploadShadowTiles() {
    vdp::setVramWrite(memory_, static_cast<std::uint16_t>(kShadowFirstTile * 32));
    for (int block = 0; block < 3; ++block) {
        for (int tile = 0; tile < 4; ++tile) {
            for (int y = 0; y < 8; ++y) {
                std::uint16_t words[2]{0, 0};
                const int inset = y == 0 || y == 7 ? 24 : (y == 1 || y == 6 ? 12 : (y == 2 || y == 5 ? 6 : 2));
                for (int localX = 0; localX < 8; ++localX) {
                    const int x = block * 32 + tile * 8 + localX;
                    if (x < inset || x >= kDisplayedBallSize - inset) {
                        continue;
                    }
                    const auto color = static_cast<std::uint16_t>(((x + y) & 1) != 0 ? 1 : 2);
                    const int word = localX >> 2;
                    const int shift = (3 - (localX & 3)) * 4;
                    words[word] = static_cast<std::uint16_t>(words[word] | (color << shift));
                }
                memory_.write16(vdp::kDataPort, words[0]);
                memory_.write16(vdp::kDataPort, words[1]);
            }
        }
    }
}

void BoingBallDemo::uploadFloorGrid() {
    std::uint16_t bufferedTiles = 0;
    std::uint16_t firstBufferedTile = kFloorFirstTile;

    for (int tileRow = 0; tileRow < 8; ++tileRow) {
        for (int tileColumn = 0; tileColumn < 40; ++tileColumn) {
            auto destination = static_cast<memory::Address>(
                kTileBufferAddress + static_cast<memory::Address>(bufferedTiles) * 32u);
            for (int row = 0; row < 8; ++row) {
                const int x = tileColumn * 8;
                const int y = tileRow * 8 + row;
                const auto left = static_cast<std::uint16_t>(
                    (floorGridPixel(x, y) << 12) | (floorGridPixel(x + 1, y) << 8) |
                    (floorGridPixel(x + 2, y) << 4) | floorGridPixel(x + 3, y));
                const auto right = static_cast<std::uint16_t>(
                    (floorGridPixel(x + 4, y) << 12) | (floorGridPixel(x + 5, y) << 8) |
                    (floorGridPixel(x + 6, y) << 4) | floorGridPixel(x + 7, y));
                memory_.write16(destination, left);
                memory_.write16(destination + 2, right);
                destination += 4;
            }

            ++bufferedTiles;
            const bool bufferFull = bufferedTiles == kTileBufferTileCapacity;
            const bool finalTile = tileRow == 7 && tileColumn == 39;
            if (bufferFull || finalTile) {
                vdp::dmaToVram(memory_, kTileBufferAddress,
                               static_cast<std::uint16_t>(firstBufferedTile * 32),
                               static_cast<std::uint16_t>(bufferedTiles * 16));
                firstBufferedTile = static_cast<std::uint16_t>(firstBufferedTile + bufferedTiles);
                bufferedTiles = 0;
            }
        }
    }

    // Window cells use screen-space row numbers, so rows 20..27 line up with
    // the vertical position selected in activate().
    for (int row = 0; row < 8; ++row) {
        const auto address = static_cast<std::uint16_t>(
            vdp::kWindowPlane + ((kFloorFirstRow + row) * vdp::kPlaneWidth) * 2);
        vdp::setVramWrite(memory_, address);
        for (int column = 0; column < 40; ++column) {
            const auto tile = static_cast<std::uint16_t>(kFloorFirstTile + row * 40 + column);
            memory_.write16(vdp::kDataPort, vdp::tileDescriptor(tile, 3));
        }
    }
}

void BoingBallDemo::renderFps() {
    unsigned fps = displayedFps_;
    char tens = '0';
    while (fps >= 10) {
        ++tens;
        fps -= 10;
    }
    const char ones = static_cast<char>('0' + fps);
    vdp::writePlaneTile(memory_, vdp::kPlaneA, 35, 1,
                        vdp::tileDescriptor(static_cast<std::uint16_t>(kFontTile + tens - 0x20), 0, true));
    vdp::writePlaneTile(memory_, vdp::kPlaneA, 36, 1,
                        vdp::tileDescriptor(static_cast<std::uint16_t>(kFontTile + ones - 0x20), 0, true));
    fpsNeedsRender_ = false;
}

} // namespace sample::demo
