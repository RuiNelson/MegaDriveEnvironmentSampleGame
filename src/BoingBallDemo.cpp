/**
 * @file BoingBallDemo.cpp
 * Shared fixed-point simulation and software VDP-tile rasterizer.
 */

#include "MegaDriveEnvironmentSampleGame/BoingBallDemo.hpp"

#include "MegaDriveEnvironmentSampleGame/VdpUtils.hpp"

namespace sample::demo {
namespace {

constexpr std::uint16_t kFontTile = 1;
constexpr std::uint16_t kBallBankTileCount = 256;
constexpr std::uint16_t kBallBank0FirstTile = 128;
constexpr std::uint16_t kBallBank1FirstTile = kBallBank0FirstTile + kBallBankTileCount;
constexpr std::uint16_t kShadowFirstTile = kBallBank1FirstTile + kBallBankTileCount;
constexpr std::uint16_t kWallFirstTile = kShadowFirstTile + 16;
constexpr std::uint16_t kWallTileCount = 3 * 3;
constexpr std::uint16_t kFloorFirstTile = kWallFirstTile + kWallTileCount;
constexpr std::uint16_t kTileBufferTileCapacity = 256;
constexpr int kFloorFirstRow = 20;

// $FF0000-$FF0005 belong to the IRQ bridge. This explicit 8192-byte scratch
// region is safely below the supervisor stack and is the sole dynamic tile
// buffer. Both targets access it through the same memory bus API.
constexpr memory::Address kTileBufferAddress = 0xFF1000;

constexpr int kFixedShift = 8;
constexpr int kFixedOne = 1 << kFixedShift;
constexpr int kLeftEdge = 8;
constexpr int kScreenWidth = 320;
constexpr int kRightInset = 8;
constexpr int kBallFloor = 208;
constexpr int kShadowY = 202;
constexpr std::uint8_t kMinimumZoomSize = 64;
constexpr std::uint8_t kDefaultZoomSize = 96;
constexpr std::uint8_t kMaximumZoomSize = 128;

constexpr memory::Address kHardwareVersionRegister = 0xA10001;
constexpr memory::Address kHvCounter = 0xC00008;
constexpr std::uint8_t kPalVideoBit = 0x40;
constexpr std::uint8_t kRasterDeadlineLine = 192;
constexpr std::uint8_t kVBlankFirstLine = 224;

constexpr int kGeometrySize = 128;
constexpr int kGeometryCenter = 63;
constexpr int kGeometryRadius = 63;
constexpr std::uint16_t kSurfaceKind = 0x0100;
constexpr std::uint16_t kRimKind = 0x0200;

struct PackedSurfaceTable {
    std::uint16_t points[kGeometrySize * kGeometrySize]{};
};

consteval int geometrySquareRoot(int value) {
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

consteval std::uint8_t geometryAngle(int coordinate, int depth) {
    const int magnitude = coordinate < 0 ? -coordinate : coordinate;
    const int denominator = magnitude + depth;
    const int angleMagnitude = denominator == 0 ? 0 : (magnitude << 4) / denominator;
    const int offset = coordinate < 0 ? -angleMagnitude : angleMagnitude;
    return static_cast<std::uint8_t>((16 + offset) & 31);
}

consteval PackedSurfaceTable buildSurfaceTable() {
    PackedSurfaceTable table;
    for (int y = 0; y < kGeometrySize; ++y) {
        const int sphereY = y - kGeometryCenter;
        for (int x = 0; x < kGeometrySize; ++x) {
            const int sphereX = x - kGeometryCenter;
            const int radiusSquared = sphereX * sphereX + sphereY * sphereY;
            if (radiusSquared > kGeometryRadius * kGeometryRadius) {
                continue;
            }

            const int depth = geometrySquareRoot(
                kGeometryRadius * kGeometryRadius - radiusSquared);
            if (depth <= 8) {
                table.points[y * kGeometrySize + x] = kRimKind;
                continue;
            }

            const int light = depth - sphereX / 2 - sphereY / 2;
            const std::uint16_t shade = light >= 64 ? 2 : (light >= 28 ? 1 : 0);
            const auto longitude = geometryAngle(sphereX, depth);
            const auto latitude = geometryAngle(sphereY, depth);
            table.points[y * kGeometrySize + x] = static_cast<std::uint16_t>(
                kSurfaceKind | (shade << 6) |
                ((latitude & 7u) << 3) | (longitude & 7u));
        }
    }
    return table;
}

inline constexpr PackedSurfaceTable kSurfaceTable = buildSurfaceTable();

// CRAM words use the Mega Drive's 0000BBB0GGG0RRR0 channel layout.
constexpr std::uint16_t kTextPalette[16]{
    0x0000, 0x0EEE, 0x0888, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};
constexpr std::uint16_t kBallPalette[16]{
    0x0000, // transparent
    0x0004, 0x000A, 0x000E, // shaded red
    0x0666, 0x0AAA, 0x0EEE, // shaded white
    0x0000,                 // silhouette rim
    0, 0, 0,
    0, 0, 0, 0, 0,
};
constexpr std::uint16_t kShadowPalette[16]{
    0x0000, 0x0222, 0x0444, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};
constexpr std::uint16_t kBackdropPalette[16]{
    0x0000, 0x0808, 0x0A0A, 0x0AAA, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

[[gnu::always_inline]] inline std::uint8_t surfacePixel(
    std::uint16_t point, const std::uint8_t *surfaceColor) {
    return surfaceColor[point];
}

[[gnu::always_inline]] inline std::uint8_t edgeSurfacePixel(
    const std::uint16_t *geometryRow, std::uint8_t sourceX,
    const std::uint8_t *surfaceColor) {
    if (sourceX == 0xFF) {
        return 0;
    }
    return surfacePixel(geometryRow[sourceX], surfaceColor);
}

[[gnu::always_inline]] inline std::uint8_t wallGridPixel(int x, int y) {
    return x == 0 || y == 0 ? 1 : 3;
}

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
    refreshRate_ = (memory_.read8(kHardwareVersionRegister) & kPalVideoBit) != 0 ? 50 : 60;
    uploadBackgroundTiles();
}

void BoingBallDemo::activate() {
    ballSize_ = kDefaultZoomSize;
    ballXFixed_ = kLeftEdge * kFixedOne;
    ballYFixed_ = 80 * kFixedOne;
    velocityXFixed_ = 320; // 1.25 pixels per displayed frame
    velocityYFixed_ = 0;
    thetaPhase_ = 0;
    phiPhase_ = 0;
    framesInSample_ = 0;
    sampleAge_ = 0;
    displayedFps_ = 0;
    displayedBallSize_ = kDefaultZoomSize;
    rasterBallSize_ = kDefaultZoomSize;
    rasterThetaPhase_ = 0;
    rasterPhiPhase_ = 0;
    rotationAxis_ = RotationAxis::Theta;
    displayBank_ = 0;
    rasterTileDimension_ = 12;
    rasterBlockX_ = 0;
    rasterBlockY_ = 0;
    rasterTileX_ = 0;
    rasterTileY_ = 0;
    rasterTileCount_ = 144;
    rasterNextTile_ = 0;
    fpsNeedsRender_ = true;
    shadowNeedsUpload_ = true;
    surfaceVisible_ = false;
    surfaceReadyForDma_ = false;
    setBallSize(kDefaultZoomSize);
    beginSurfaceRaster();

    vdp::loadPalette(memory_, 0, kTextPalette);
    vdp::loadPalette(memory_, 1, kBallPalette);
    vdp::loadPalette(memory_, 2, kShadowPalette);
    vdp::loadPalette(memory_, 3, kBackdropPalette);
    vdp::writeRegister(memory_, 0x07, 0x33); // grey backdrop behind the wall grid

    // Only visible name-table cells need replacing during the transition.
    // Both grid surfaces reference patterns built by uploadBackgroundTiles().
    vdp::fillPlaneArea(memory_, vdp::kPlaneA, 0, 0, 40, 28, vdp::tileDescriptor(0));
    mapWallGrid();
    vdp::beginHorizontalScrollLines(memory_, 0);
    for (int scanline = 0; scanline < 224; ++scanline) {
        vdp::appendHorizontalScrollLine(memory_, 0, 0);
    }
    vdp::writeText(memory_, vdp::kPlaneA, 2, 1, "SOFTWARE 3D BOING BALL", kFontTile);
    vdp::writeText(memory_, vdp::kPlaneA, 31, 1, "FPS", kFontTile);
    vdp::writeText(memory_, vdp::kPlaneA, 7, 3, "UP/DOWN ZOOM   START RETURN", kFontTile);

    // The Window plane replaces Plane A below the horizon, yielding a second
    // perspective surface while Plane B remains the upright wall grid.
    vdp::writeRegister(memory_, 0x11, 0x00);
    vdp::writeRegister(memory_, 0x12, static_cast<std::uint8_t>(0x80 | kFloorFirstRow));

    renderFps();
}

BounceEvents BoingBallDemo::update(bool zoomIn, bool zoomOut) {
    BounceEvents events;

    if (zoomIn != zoomOut) {
        if (zoomIn && ballSize_ < kMaximumZoomSize) {
            setBallSize(static_cast<std::uint8_t>(ballSize_ + 1u));
        } else if (zoomOut && ballSize_ > kMinimumZoomSize) {
            setBallSize(static_cast<std::uint8_t>(ballSize_ - 1u));
        }
    }

    ballXFixed_ += velocityXFixed_;
    if (ballXFixed_ <= kLeftEdge * kFixedOne) {
        ballXFixed_ = kLeftEdge * kFixedOne;
        velocityXFixed_ = -velocityXFixed_;
        rotationAxis_ = RotationAxis::Theta;
        events.hitWall = true;
    } else {
        const int rightEdge = kScreenWidth - kRightInset - ballSize_;
        if (ballXFixed_ >= rightEdge * kFixedOne) {
            ballXFixed_ = rightEdge * kFixedOne;
            velocityXFixed_ = -velocityXFixed_;
            rotationAxis_ = RotationAxis::Phi;
            events.hitWall = true;
        }
    }

    velocityYFixed_ += 48; // 0.1875 pixels/frame^2
    ballYFixed_ += velocityYFixed_;
    const int floorY = kBallFloor - ballSize_;
    if (ballYFixed_ >= floorY * kFixedOne) {
        ballYFixed_ = floorY * kFixedOne;
        velocityYFixed_ = -1408; // 5.5 pixels/frame upwards
        events.hitFloor = true;
    }

    if (rotationAxis_ == RotationAxis::Theta) {
        thetaPhase_ = static_cast<std::uint8_t>((thetaPhase_ + 1u) & 31u);
    } else {
        phiPhase_ = static_cast<std::uint8_t>((phiPhase_ + 1u) & 31u);
    }
    return events;
}

void BoingBallDemo::render() {
    if (surfaceReadyForDma_) {
        const auto inactiveBankFirstTile = displayBank_ == 0
            ? kBallBank1FirstTile : kBallBank0FirstTile;
        vdp::dmaToVram(memory_, kTileBufferAddress,
                       static_cast<std::uint16_t>(inactiveBankFirstTile * 32),
                       static_cast<std::uint16_t>(rasterTileCount_ * 16));

        displayBank_ ^= 1u;
        if (displayedBallSize_ != rasterBallSize_) {
            shadowNeedsUpload_ = true;
        }
        displayedBallSize_ = rasterBallSize_;
        surfaceVisible_ = true;
        ++framesInSample_;
        surfaceReadyForDma_ = false;
        beginSurfaceRaster();
    }

    if (++sampleAge_ >= refreshRate_) {
        displayedFps_ = framesInSample_;
        framesInSample_ = 0;
        sampleAge_ = 0;
        fpsNeedsRender_ = true;
    }
    if (shadowNeedsUpload_) {
        uploadShadowTiles();
    }

    const int centreX = ballX() + (ballSize_ >> 1);
    const int centreY = ballY() + (ballSize_ >> 1);
    const int x = centreX - (displayedBallSize_ >> 1);
    const int y = surfaceVisible_
        ? centreY - (displayedBallSize_ >> 1) : -128;
    const int tileDimension = (displayedBallSize_ + 7) >> 3;
    const int blockCount = (tileDimension + 3) >> 2;
    const int ballSpriteCount = blockCount * blockCount;
    const auto displayedBankFirstTile = displayBank_ == 0
        ? kBallBank0FirstTile : kBallBank1FirstTile;
    int sprite = 0;
    std::uint16_t tileOffset = 0;
    for (int blockY = 0; blockY < blockCount; ++blockY) {
        const int remainingRows = tileDimension - blockY * 4;
        const int height = remainingRows < 4 ? remainingRows : 4;
        for (int blockX = 0; blockX < blockCount; ++blockX) {
            const int remainingColumns = tileDimension - blockX * 4;
            const int width = remainingColumns < 4 ? remainingColumns : 4;
            const int next = sprite + 1 < ballSpriteCount ? sprite + 1 : 16;
            const auto tile = static_cast<std::uint16_t>(displayedBankFirstTile + tileOffset);
            vdp::writeSprite(memory_, sprite, x + blockX * 32, y + blockY * 32,
                             width, height, tile, 1, next);
            tileOffset = static_cast<std::uint16_t>(tileOffset + width * height);
            ++sprite;
        }
    }

    // Lower SAT indices win sprite overlap, so the ball naturally occludes its
    // ground shadow at the bottom of each bounce.
    const int shadowX = centreX - (kMaximumBallSize >> 1);
    const int shadowY = surfaceVisible_ ? kShadowY : -128;
    for (int block = 0; block < 4; ++block) {
        const int shadowSprite = 16 + block;
        const int next = block == 3 ? 0 : shadowSprite + 1;
        vdp::writeSprite(memory_, shadowSprite, shadowX + block * 32, shadowY,
                         4, 1, static_cast<std::uint16_t>(kShadowFirstTile + block * 4), 2, next);
    }

    if (fpsNeedsRender_) {
        renderFps();
    }

    rasterizeBallUntilBudget();
}

int BoingBallDemo::ballX() const {
    return ballXFixed_ >> kFixedShift;
}

int BoingBallDemo::ballY() const {
    return ballYFixed_ >> kFixedShift;
}

int BoingBallDemo::ballSize() const {
    return ballSize_;
}

RotationAxis BoingBallDemo::rotationAxis() const {
    return rotationAxis_;
}

std::uint8_t BoingBallDemo::displayedFps() const {
    return displayedFps_;
}

std::uint8_t BoingBallDemo::refreshRate() const {
    return refreshRate_;
}

void BoingBallDemo::setBallSize(std::uint8_t size) {
    if (size < kMinimumZoomSize) {
        size = kMinimumZoomSize;
    } else if (size > kMaximumZoomSize) {
        size = kMaximumZoomSize;
    }

    const int sizeDelta = static_cast<int>(ballSize_) - size;
    ballXFixed_ += sizeDelta * (kFixedOne >> 1);
    ballYFixed_ += sizeDelta * (kFixedOne >> 1);
    ballSize_ = size;

    const int rightEdge = kScreenWidth - kRightInset - ballSize_;
    if (ballXFixed_ < kLeftEdge * kFixedOne) {
        ballXFixed_ = kLeftEdge * kFixedOne;
    } else if (ballXFixed_ > rightEdge * kFixedOne) {
        ballXFixed_ = rightEdge * kFixedOne;
    }
    const int floorY = kBallFloor - ballSize_;
    if (ballYFixed_ > floorY * kFixedOne) {
        ballYFixed_ = floorY * kFixedOne;
    }

}

void BoingBallDemo::beginSurfaceRaster() {
    rasterBallSize_ = ballSize_;
    rasterThetaPhase_ = thetaPhase_;
    rasterPhiPhase_ = phiPhase_;
    reuseTransparentTiles_ = occupiedBallSize_ == rasterBallSize_;
    if (!reuseTransparentTiles_) {
        for (auto &occupied : occupiedTiles_) {
            occupied = 0;
        }
    }

    // Red/white selection only needs bit 2 of each rotated coordinate. Pack
    // the necessary low three bits into the geometry and resolve kind, shade
    // and both phases here, reducing every final pixel to one direct lookup.
    surfaceColor_[0] = 0;
    surfaceColor_[kRimKind] = 7;
    for (int latitude = 0; latitude < 8; ++latitude) {
        const auto shiftedLatitude = static_cast<std::uint8_t>(
            (latitude + rasterPhiPhase_) & 7u);
        for (int longitude = 0; longitude < 8; ++longitude) {
            const auto shiftedLongitude = static_cast<std::uint8_t>(
                (longitude + rasterThetaPhase_) & 7u);
            const auto baseColour = static_cast<std::uint8_t>(
                ((shiftedLongitude ^ shiftedLatitude) & 4u) != 0 ? 4 : 1);
            const int key = (latitude << 3) | longitude;
            surfaceColor_[kSurfaceKind + key] = baseColour;
            surfaceColor_[kSurfaceKind + 64 + key] =
                static_cast<std::uint8_t>(baseColour + 1u);
            surfaceColor_[kSurfaceKind + 128 + key] =
                static_cast<std::uint8_t>(baseColour + 2u);
        }
    }

    for (int output = 0; output < kMaximumBallSize; ++output) {
        sourceCoordinate_[output] = 0xFF;
    }
    int source = 0;
    int error = 0;
    const int extraSourceSteps = kMaximumBallSize - rasterBallSize_;
    for (int output = 0; output < rasterBallSize_; ++output) {
        sourceCoordinate_[output] = static_cast<std::uint8_t>(source);
        ++source;
        error += extraSourceSteps;
        if (error >= rasterBallSize_) {
            error -= rasterBallSize_;
            ++source;
        }
    }

    rasterTileDimension_ = static_cast<std::uint8_t>((rasterBallSize_ + 7u) >> 3);
    rasterTileCount_ = static_cast<std::uint16_t>(
        rasterTileDimension_ * rasterTileDimension_);
    rasterNextTile_ = 0;
    rasterBlockX_ = 0;
    rasterBlockY_ = 0;
    rasterTileX_ = 0;
    rasterTileY_ = 0;
}

void BoingBallDemo::rasterizeNextBallTile() {
    const int x = (rasterBlockX_ * 4 + rasterTileX_) * 8;
    const int y = (rasterBlockY_ * 4 + rasterTileY_) * 8;
    auto destination = static_cast<memory::Address>(
        kTileBufferAddress + static_cast<memory::Address>(rasterNextTile_) * 32u);
    const auto occupancyBit = static_cast<std::uint8_t>(
        1u << (rasterNextTile_ & 7u));
    const auto occupancyByte = static_cast<std::uint8_t>(rasterNextTile_ >> 3);
    const bool knownTransparent = reuseTransparentTiles_ &&
        (occupiedTiles_[occupancyByte] & occupancyBit) == 0;

    std::uint32_t tilePixels = 0;
    if (!knownTransparent) {
        const bool completeTileWidth = x + 7 < rasterBallSize_;
        for (int row = 0; row < 8; ++row) {
            const auto sourceY = sourceCoordinate_[y + row];
            std::uint32_t packedPixels = 0;
            if (sourceY != 0xFF) {
                const auto *geometryRow = &kSurfaceTable.points[sourceY << 7];
                if (completeTileWidth) {
                    packedPixels =
                    (static_cast<std::uint32_t>(surfacePixel(
                         geometryRow[sourceCoordinate_[x]], surfaceColor_)) << 28) |
                    (static_cast<std::uint32_t>(surfacePixel(
                         geometryRow[sourceCoordinate_[x + 1]], surfaceColor_)) << 24) |
                    (static_cast<std::uint32_t>(surfacePixel(
                         geometryRow[sourceCoordinate_[x + 2]], surfaceColor_)) << 20) |
                    (static_cast<std::uint32_t>(surfacePixel(
                         geometryRow[sourceCoordinate_[x + 3]], surfaceColor_)) << 16) |
                    (static_cast<std::uint32_t>(surfacePixel(
                         geometryRow[sourceCoordinate_[x + 4]], surfaceColor_)) << 12) |
                    (static_cast<std::uint32_t>(surfacePixel(
                         geometryRow[sourceCoordinate_[x + 5]], surfaceColor_)) << 8) |
                    (static_cast<std::uint32_t>(surfacePixel(
                         geometryRow[sourceCoordinate_[x + 6]], surfaceColor_)) << 4) |
                    surfacePixel(geometryRow[sourceCoordinate_[x + 7]], surfaceColor_);
                } else {
                    packedPixels =
                    (static_cast<std::uint32_t>(edgeSurfacePixel(
                         geometryRow, sourceCoordinate_[x], surfaceColor_)) << 28) |
                    (static_cast<std::uint32_t>(edgeSurfacePixel(
                         geometryRow, sourceCoordinate_[x + 1], surfaceColor_)) << 24) |
                    (static_cast<std::uint32_t>(edgeSurfacePixel(
                         geometryRow, sourceCoordinate_[x + 2], surfaceColor_)) << 20) |
                    (static_cast<std::uint32_t>(edgeSurfacePixel(
                         geometryRow, sourceCoordinate_[x + 3], surfaceColor_)) << 16) |
                    (static_cast<std::uint32_t>(edgeSurfacePixel(
                         geometryRow, sourceCoordinate_[x + 4], surfaceColor_)) << 12) |
                    (static_cast<std::uint32_t>(edgeSurfacePixel(
                         geometryRow, sourceCoordinate_[x + 5], surfaceColor_)) << 8) |
                    (static_cast<std::uint32_t>(edgeSurfacePixel(
                         geometryRow, sourceCoordinate_[x + 6], surfaceColor_)) << 4) |
                    edgeSurfacePixel(geometryRow, sourceCoordinate_[x + 7], surfaceColor_);
                }
            }
            tilePixels |= packedPixels;
            memory_.write32(destination, packedPixels);
            destination += 4;
        }
        if (!reuseTransparentTiles_ && tilePixels != 0) {
            occupiedTiles_[occupancyByte] |= occupancyBit;
        }
    }
    ++rasterNextTile_;

    const int remainingRows = rasterTileDimension_ - rasterBlockY_ * 4;
    const int blockHeight = remainingRows < 4 ? remainingRows : 4;
    if (++rasterTileY_ < blockHeight) {
        return;
    }
    rasterTileY_ = 0;

    const int remainingColumns = rasterTileDimension_ - rasterBlockX_ * 4;
    const int blockWidth = remainingColumns < 4 ? remainingColumns : 4;
    if (++rasterTileX_ < blockWidth) {
        return;
    }
    rasterTileX_ = 0;

    const int blockCount = (rasterTileDimension_ + 3) >> 2;
    if (++rasterBlockX_ < blockCount) {
        return;
    }
    rasterBlockX_ = 0;
    ++rasterBlockY_;
}

void BoingBallDemo::rasterizeBallUntilBudget() {
    while (rasterNextTile_ < rasterTileCount_) {
        rasterizeNextBallTile();
        if (videoBudgetExpired()) {
            return;
        }
    }
    occupiedBallSize_ = rasterBallSize_;
    surfaceReadyForDma_ = true;
}

bool BoingBallDemo::videoBudgetExpired() const {
    const auto verticalCounter = static_cast<std::uint8_t>(memory_.read16(kHvCounter) >> 8);
    return verticalCounter >= kRasterDeadlineLine && verticalCounter < kVBlankFirstLine;
}

void BoingBallDemo::uploadShadowTiles() {
    vdp::setVramWrite(memory_, static_cast<std::uint16_t>(kShadowFirstTile * 32));
    const int margin = (kMaximumBallSize - displayedBallSize_) >> 1;
    for (int block = 0; block < 4; ++block) {
        for (int tile = 0; tile < 4; ++tile) {
            for (int y = 0; y < 8; ++y) {
                std::uint16_t words[2]{0, 0};
                const int ellipseInset = y == 0 || y == 7 ? displayedBallSize_ >> 2
                    : (y == 1 || y == 6 ? displayedBallSize_ >> 3
                        : (y == 2 || y == 5 ? displayedBallSize_ >> 4 : 2));
                const int inset = margin + ellipseInset;
                for (int localX = 0; localX < 8; ++localX) {
                    const int x = block * 32 + tile * 8 + localX;
                    if (x < inset || x >= kMaximumBallSize - inset) {
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
    shadowNeedsUpload_ = false;
}

void BoingBallDemo::uploadBackgroundTiles() {
    memory::Address wallDestination = kTileBufferAddress;
    for (int tileRow = 0; tileRow < 3; ++tileRow) {
        for (int tileColumn = 0; tileColumn < 3; ++tileColumn) {
            for (int row = 0; row < 8; ++row) {
                const int x = tileColumn * 8;
                const int y = tileRow * 8 + row;
                const auto left = static_cast<std::uint16_t>(
                    (wallGridPixel(x, y) << 12) | (wallGridPixel(x + 1, y) << 8) |
                    (wallGridPixel(x + 2, y) << 4) | wallGridPixel(x + 3, y));
                const auto right = static_cast<std::uint16_t>(
                    (wallGridPixel(x + 4, y) << 12) | (wallGridPixel(x + 5, y) << 8) |
                    (wallGridPixel(x + 6, y) << 4) | wallGridPixel(x + 7, y));
                memory_.write16(wallDestination, left);
                memory_.write16(wallDestination + 2, right);
                wallDestination += 4;
            }
        }
    }
    vdp::dmaToVram(memory_, kTileBufferAddress,
                   static_cast<std::uint16_t>(kWallFirstTile * 32), kWallTileCount * 16);

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

void BoingBallDemo::mapWallGrid() {
    int tileRow = 0;
    for (int row = 0; row < kFloorFirstRow; ++row) {
        const auto address = static_cast<std::uint16_t>(
            vdp::kPlaneB + (row * vdp::kPlaneWidth) * 2);
        vdp::setVramWrite(memory_, address);
        int tileColumn = 0;
        for (int column = 0; column < 40; ++column) {
            const auto tile = static_cast<std::uint16_t>(
                kWallFirstTile + tileRow * 3 + tileColumn);
            memory_.write16(vdp::kDataPort, vdp::tileDescriptor(tile, 3));
            if (++tileColumn == 3) {
                tileColumn = 0;
            }
        }
        if (++tileRow == 3) {
            tileRow = 0;
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
