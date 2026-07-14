#include "MegaDriveEnvironmentSampleGame/game/GameSession.hpp"

namespace sample::game {
namespace {

constexpr int kScreenWidth = 320;
constexpr int kScreenHeight = 224;
constexpr int kHudHeight = 32;
constexpr int kPlayerSize = 16;
constexpr int kGemSize = 8;
constexpr int kEnemySize = 16;

} // namespace

int Entity::x() const {
    return x_;
}

int Entity::y() const {
    return y_;
}

int Entity::width() const {
    return width_;
}

int Entity::height() const {
    return height_;
}

bool Entity::overlaps(const Entity &other) const {
    return x_ < other.x_ + other.width_ && x_ + width_ > other.x_ && y_ < other.y_ + other.height_ &&
           y_ + height_ > other.y_;
}

void Entity::setPosition(int x, int y) {
    x_ = x;
    y_ = y;
}

void Entity::move(int deltaX, int deltaY) {
    x_ += deltaX;
    y_ += deltaY;
}

Player::Player() : Entity(40, 104, kPlayerSize, kPlayerSize) {
}

void Player::reset() {
    setPosition(40, 104);
}

void Player::update(const controllers::ControllerState &controls) {
    constexpr int speed = 2;
    move((controls.right ? speed : 0) - (controls.left ? speed : 0),
         (controls.down ? speed : 0) - (controls.up ? speed : 0));

    int clampedX = x();
    int clampedY = y();
    if (clampedX < 0) {
        clampedX = 0;
    } else if (clampedX > kScreenWidth - width()) {
        clampedX = kScreenWidth - width();
    }
    if (clampedY < kHudHeight) {
        clampedY = kHudHeight;
    } else if (clampedY > kScreenHeight - height()) {
        clampedY = kScreenHeight - height();
    }
    setPosition(clampedX, clampedY);
}

Collectible::Collectible() : Entity(240, 104, kGemSize, kGemSize) {
}

void Collectible::reset() {
    stepX_ = 224;
    stepY_ = 64;
    setPosition(240, 104);
}

void Collectible::moveToNextPosition() {
    // Incremental wrapping avoids division helpers in the freestanding build.
    stepX_ = static_cast<std::uint16_t>(stepX_ + 73);
    if (stepX_ >= 280) {
        stepX_ = static_cast<std::uint16_t>(stepX_ - 280);
    }
    stepY_ = static_cast<std::uint16_t>(stepY_ + 47);
    if (stepY_ >= 168) {
        stepY_ = static_cast<std::uint16_t>(stepY_ - 168);
    }
    setPosition(16 + stepX_, kHudHeight + 8 + stepY_);
}

Enemy::Enemy() : Entity(288, 184, kEnemySize, kEnemySize) {
}

void Enemy::reset() {
    chaseFrame_ = 0;
    setPosition(288, 184);
}

void Enemy::chase(const Player &player) {
    ++chaseFrame_;
    if (chaseFrame_ < 3) {
        return;
    }
    chaseFrame_ = 0;

    const int deltaX = player.x() > x() ? 1 : (player.x() < x() ? -1 : 0);
    const int deltaY = player.y() > y() ? 1 : (player.y() < y() ? -1 : 0);
    move(deltaX, deltaY);
}

bool Events::collectedGem() const {
    return collectedGem_;
}

bool Events::gameOverStarted() const {
    return gameOverStarted_;
}

bool Events::restarted() const {
    return restarted_;
}

GameSession::GameSession() = default;

Events GameSession::update(const controllers::ControllerState &controls) {
    Events events;
    const bool resetPressed = controls.a || controls.start;
    if (resetPressed && !resetWasPressed_) {
        reset();
        resetWasPressed_ = true;
        events.restarted_ = true;
        return events;
    }
    resetWasPressed_ = resetPressed;

    if (phase_ == Phase::GameOver) {
        return events;
    }

    player_.update(controls);
    if (player_.overlaps(gem_)) {
        ++score_;
        if (score_ >= 1000) {
            score_ = 0;
        }
        gem_.moveToNextPosition();
        events.collectedGem_ = true;
    }

    enemy_.chase(player_);
    if (player_.overlaps(enemy_)) {
        phase_ = Phase::GameOver;
        events.gameOverStarted_ = true;
    }
    return events;
}

void GameSession::reset() {
    player_.reset();
    gem_.reset();
    enemy_.reset();
    score_ = 0;
    phase_ = Phase::Playing;
}

const Player &GameSession::player() const {
    return player_;
}

const Collectible &GameSession::gem() const {
    return gem_;
}

const Enemy &GameSession::enemy() const {
    return enemy_;
}

std::uint16_t GameSession::score() const {
    return score_;
}

Phase GameSession::phase() const {
    return phase_;
}

} // namespace sample::game
