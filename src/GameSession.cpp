/**
 * @file GameSession.cpp
 * Deterministic, allocation-free game simulation shared by both targets.
 */

#include "MegaDriveEnvironmentSampleGame/GameSession.hpp"
#include "MegaDriveEnvironmentSampleGame/GameConfig.hpp"

namespace sample::game {

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
    // Strict inequalities mean merely touching edges is not a collision.
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

Player::Player()
    : Entity(config::kPlayerStartX, config::kPlayerStartY, config::kPlayerSize,
             config::kPlayerSize) {
}

void Player::reset() {
    setPosition(config::kPlayerStartX, config::kPlayerStartY);
}

void Player::update(const controllers::ControllerState &controls) {
    move((controls.right ? config::kPlayerSpeed : 0) -
             (controls.left ? config::kPlayerSpeed : 0),
         (controls.down ? config::kPlayerSpeed : 0) -
             (controls.up ? config::kPlayerSpeed : 0));

    // The HUD occupies the top four tile rows and is not part of the playfield.
    int clampedX = x();
    int clampedY = y();
    if (clampedX < 0) {
        clampedX = 0;
    } else if (clampedX > config::kScreenWidth - width()) {
        clampedX = config::kScreenWidth - width();
    }
    if (clampedY < config::kHudHeight) {
        clampedY = config::kHudHeight;
    } else if (clampedY > config::kScreenHeight - height()) {
        clampedY = config::kScreenHeight - height();
    }
    setPosition(clampedX, clampedY);
}

Collectible::Collectible()
    : Entity(config::kGemStartX, config::kGemStartY, config::kGemSize, config::kGemSize) {
}

void Collectible::reset() {
    stepX_ = config::kGemSequenceStartX;
    stepY_ = config::kGemSequenceStartY;
    setPosition(config::kGemStartX, config::kGemStartY);
}

void Collectible::moveToNextPosition() {
    // The increments are coprime with their ranges, giving long deterministic
    // cycles. Incremental wrapping avoids division helpers in the ROM build.
    stepX_ = static_cast<std::uint16_t>(stepX_ + config::kGemSequenceStepX);
    if (stepX_ >= config::kGemSequenceWidth) {
        stepX_ = static_cast<std::uint16_t>(stepX_ - config::kGemSequenceWidth);
    }
    stepY_ = static_cast<std::uint16_t>(stepY_ + config::kGemSequenceStepY);
    if (stepY_ >= config::kGemSequenceHeight) {
        stepY_ = static_cast<std::uint16_t>(stepY_ - config::kGemSequenceHeight);
    }
    setPosition(config::kGemMarginX + stepX_,
                config::kHudHeight + config::kGemMarginY + stepY_);
}

Enemy::Enemy()
    : Entity(config::kEnemyStartX, config::kEnemyStartY, config::kEnemySize,
             config::kEnemySize) {
}

void Enemy::reset() {
    chaseProgress_ = 0;
    speed_ = config::kEnemyInitialSpeed;
    setPosition(config::kEnemyStartX, config::kEnemyStartY);
}

void Enemy::increaseSpeed() {
    // One unit is 1/24 pixel per frame. Unlike changing an integer frame
    // interval, this makes every collected gem increase the movement rate.
    speed_ += config::kEnemySpeedIncrease;
}

void Enemy::chase(const Player &player) {
    chaseProgress_ += speed_;

    int stepCount = 0;
    while (chaseProgress_ >= config::kEnemySpeedScale) {
        chaseProgress_ -= config::kEnemySpeedScale;
        ++stepCount;
    }
    if (stepCount == 0) {
        return;
    }

    // Clamp each axis to the target so high accumulated speeds cannot overshoot
    // the player. Diagonal pursuit has the same rate on both axes.
    const int distanceX = player.x() - x();
    const int distanceY = player.y() - y();
    const int deltaX =
        distanceX > stepCount ? stepCount : (distanceX < -stepCount ? -stepCount : distanceX);
    const int deltaY =
        distanceY > stepCount ? stepCount : (distanceY < -stepCount ? -stepCount : distanceY);
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
    // Reset is edge-triggered to avoid restarting every frame while held. It is
    // accepted during play as well as game over, making it a general new-round
    // command rather than only a game-over acknowledgement.
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

    // Ordering is intentional: collect after player movement, then advance the
    // enemy and test the potentially updated enemy position for game over.
    player_.update(controls);
    if (player_.overlaps(gem_)) {
        ++score_;
        if (score_ >= config::kScoreLimit) {
            score_ = 0;
        }
        gem_.moveToNextPosition();
        enemy_.increaseSpeed();
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
