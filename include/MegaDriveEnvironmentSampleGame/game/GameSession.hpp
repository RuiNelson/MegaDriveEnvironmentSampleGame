#pragma once

/**
 * @file GameSession.hpp
 * Platform-independent rules and object model for the sample game.
 *
 * Coordinates are integer pixels measured from the visible screen's top-left.
 * The first 32 vertical pixels are reserved for the HUD.
 */

#include "MegaDriveEnvironmentSampleGame/controllers/ControllerReader.hpp"

namespace sample::game {

/** High-level state controlling whether normal simulation may advance. */
enum class Phase : std::uint8_t {
    Playing,
    GameOver,
};

/** Axis-aligned rectangular object in the 320x224 playfield. */
class Entity {
  public:
    /** Top-left position and dimensions, all measured in pixels. */
    [[nodiscard]] int x() const;
    [[nodiscard]] int y() const;
    [[nodiscard]] int width() const;
    [[nodiscard]] int height() const;
    /** Returns true when the two rectangles have a positive-area overlap. */
    [[nodiscard]] bool overlaps(const Entity &other) const;

  protected:
    /** Creates an entity at a fixed position and size. */
    constexpr Entity(int x, int y, int width, int height) : x_(x), y_(y), width_(width), height_(height) {
    }

    /** Replaces or offsets the top-left position. */
    void setPosition(int x, int y);
    void move(int deltaX, int deltaY);

  private:
    int x_;
    int y_;
    int width_;
    int height_;
};

/** Player-controlled 16x16 entity. */
class Player final : public Entity {
  public:
    /** Creates the player at its starting position. */
    Player();

    /** Restores the starting position. */
    void reset();
    /** Applies held directions at two pixels per frame and clamps to play. */
    void update(const controllers::ControllerState &controls);
};

/** Eight-pixel gem that moves through a deterministic position sequence. */
class Collectible final : public Entity {
  public:
    /** Creates the gem at its starting position. */
    Collectible();

    /** Restores both the position and deterministic sequence state. */
    void reset();
    /** Advances to the next in-bounds pseudo-random-looking position. */
    void moveToNextPosition();

  private:
    /** Sequence accumulators relative to the playable area's origin. */
    std::uint16_t stepX_ = 224;
    std::uint16_t stepY_ = 64;
};

/** Autonomous 16x16 entity that follows the player and accelerates with gems. */
class Enemy final : public Entity {
  public:
    /** Creates the enemy at its starting position. */
    Enemy();

    /** Restores its position, movement progress, and initial speed. */
    void reset();
    /** Adds a small permanent speed increase for the current round. */
    void increaseSpeed();
    /** Advances toward the player according to the fractional movement rate. */
    void chase(const Player &player);

  private:
    /** Fixed-point progress and rate, measured in 1/24-pixel units. */
    std::uint32_t chaseProgress_ = 0;
    std::uint32_t speed_ = 8;
};

/** One-frame notifications emitted by GameSession::update(). */
class Events final {
  public:
    /** A gem was collected and the score changed this frame. */
    [[nodiscard]] bool collectedGem() const;
    /** Player/enemy contact changed the phase to game over this frame. */
    [[nodiscard]] bool gameOverStarted() const;
    /** A rising edge on A or Start reset the session this frame. */
    [[nodiscard]] bool restarted() const;

  private:
    friend class GameSession;

    bool collectedGem_ = false;
    bool gameOverStarted_ = false;
    bool restarted_ = false;
};

/** Owns every gameplay object, score, phase and reset-edge state. */
class GameSession final {
  public:
    /** Creates a fresh session with all entities at their start positions. */
    GameSession();

    /** Advances one frame and returns only the events caused by that frame. */
    [[nodiscard]] Events update(const controllers::ControllerState &controls);
    /** Starts a new round without reconstructing the session object. */
    void reset();

    /** Read-only views used by renderers for both supported targets. */
    [[nodiscard]] const Player &player() const;
    [[nodiscard]] const Collectible &gem() const;
    [[nodiscard]] const Enemy &enemy() const;
    /** Current three-digit score; it wraps from 999 back to zero. */
    [[nodiscard]] std::uint16_t score() const;
    /** Whether the world is advancing or waiting for a restart. */
    [[nodiscard]] Phase phase() const;

  private:
    /** Owned world objects. */
    Player player_;
    Collectible gem_;
    Enemy enemy_;
    /** Persistent session state. */
    std::uint16_t score_ = 0;
    Phase phase_ = Phase::Playing;
    bool resetWasPressed_ = false;
};

} // namespace sample::game
