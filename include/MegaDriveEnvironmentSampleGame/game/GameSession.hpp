#pragma once

#include "MegaDriveEnvironmentSampleGame/controllers/ControllerReader.hpp"

namespace sample::game {

enum class Phase : std::uint8_t {
    Playing,
    GameOver,
};

class Entity {
  public:
    [[nodiscard]] int x() const;
    [[nodiscard]] int y() const;
    [[nodiscard]] int width() const;
    [[nodiscard]] int height() const;
    [[nodiscard]] bool overlaps(const Entity &other) const;

  protected:
    constexpr Entity(int x, int y, int width, int height) : x_(x), y_(y), width_(width), height_(height) {
    }

    void setPosition(int x, int y);
    void move(int deltaX, int deltaY);

  private:
    int x_;
    int y_;
    int width_;
    int height_;
};

class Player final : public Entity {
  public:
    Player();

    void reset();
    void update(const controllers::ControllerState &controls);
};

class Collectible final : public Entity {
  public:
    Collectible();

    void reset();
    void moveToNextPosition();

  private:
    std::uint16_t stepX_ = 224;
    std::uint16_t stepY_ = 64;
};

class Enemy final : public Entity {
  public:
    Enemy();

    void reset();
    void chase(const Player &player);

  private:
    std::uint8_t chaseFrame_ = 0;
};

class Events final {
  public:
    [[nodiscard]] bool collectedGem() const;
    [[nodiscard]] bool gameOverStarted() const;
    [[nodiscard]] bool restarted() const;

  private:
    friend class GameSession;

    bool collectedGem_ = false;
    bool gameOverStarted_ = false;
    bool restarted_ = false;
};

class GameSession final {
  public:
    GameSession();

    [[nodiscard]] Events update(const controllers::ControllerState &controls);
    void reset();

    [[nodiscard]] const Player &player() const;
    [[nodiscard]] const Collectible &gem() const;
    [[nodiscard]] const Enemy &enemy() const;
    [[nodiscard]] std::uint16_t score() const;
    [[nodiscard]] Phase phase() const;

  private:
    Player player_;
    Collectible gem_;
    Enemy enemy_;
    std::uint16_t score_ = 0;
    Phase phase_ = Phase::Playing;
    bool resetWasPressed_ = false;
};

} // namespace sample::game
