#include "core/GameWorld.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace arena {
namespace {

constexpr float kPlayerAttackCooldown = 0.34F;
constexpr float kPlayerAttackRadius = 86.0F;
constexpr int kPlayerAttackDamage = 45;
constexpr float kAttackEffectDuration = 0.10F;
constexpr float kDashDistance = 118.0F;
constexpr float kDashCooldown = 1.15F;
constexpr float kDashInvulnerability = 0.20F;
constexpr int kContactDamage = 10;
constexpr float kContactCooldown = 0.72F;
constexpr float kMaximumDeltaSeconds = 0.25F;
constexpr float kEpsilon = 0.0001F;

float _clampNonNegative(float value) noexcept {
    return std::max(0.0F, value);
}

}  // namespace

Vec2 operator+(Vec2 lhs, Vec2 rhs) noexcept {
    return {lhs.x + rhs.x, lhs.y + rhs.y};
}

Vec2 operator-(Vec2 lhs, Vec2 rhs) noexcept {
    return {lhs.x - rhs.x, lhs.y - rhs.y};
}

Vec2 operator*(Vec2 value, float scalar) noexcept {
    return {value.x * scalar, value.y * scalar};
}

float lengthSquared(Vec2 value) noexcept {
    return value.x * value.x + value.y * value.y;
}

Vec2 normalizedOrZero(Vec2 value) noexcept {
    const float squaredLength = lengthSquared(value);
    if (squaredLength <= kEpsilon) {
        return {};
    }

    const float inverseLength = 1.0F / std::sqrt(squaredLength);
    return value * inverseLength;
}

GameWorld::GameWorld(WorldConfig config, std::uint32_t seed)
    : config_(config), initialSeed_(seed), randomState_(seed) {
    if (!(std::isfinite(config_.width) && std::isfinite(config_.height) &&
          config_.width >= 64.0F && config_.height >= 64.0F)) {
        throw std::invalid_argument("Arena dimensions must be finite and at least 64 by 64.");
    }
    if (!(std::isfinite(config_.playerSpeed) && std::isfinite(config_.enemySpeed) &&
          config_.playerSpeed >= 0.0F && config_.enemySpeed >= 0.0F)) {
        throw std::invalid_argument("Movement speeds must be finite and non-negative.");
    }
    if (config_.automaticSpawning &&
        !(std::isfinite(config_.spawnInterval) && config_.spawnInterval > 0.0F)) {
        throw std::invalid_argument("Automatic spawning requires a finite positive interval.");
    }
    if (config_.enemyHealth <= 0) {
        throw std::invalid_argument("Enemy health must be positive.");
    }

    reset();
}

void GameWorld::reset() {
    randomState_ = initialSeed_;
    nextEnemyId_ = 1;
    score_ = 0;
    elapsedSeconds_ = 0.0F;
    spawnTimer_ = config_.spawnInterval;
    attackEffectRemaining_ = 0.0F;
    enemies_.clear();

    player_ = {};
    player_.position = {config_.width * 0.5F, config_.height * 0.5F};

    const std::size_t enemyCount = std::min(config_.initialEnemies, config_.maximumEnemies);
    enemies_.reserve(config_.maximumEnemies);
    for (std::size_t index = 0; index < enemyCount; ++index) {
        spawnEnemy(_randomEdgePosition());
    }
}

void GameWorld::update(const InputFrame& input, float deltaSeconds) {
    if (!(deltaSeconds > 0.0F && deltaSeconds <= kMaximumDeltaSeconds)) {
        throw std::invalid_argument("Delta time must be in the range (0, 0.25].");
    }
    if (gameOver()) {
        return;
    }
    if (!(std::isfinite(input.movement.x) && std::isfinite(input.movement.y))) {
        throw std::invalid_argument("Input movement must contain finite coordinates.");
    }

    elapsedSeconds_ += deltaSeconds;
    player_.attackCooldown = _clampNonNegative(player_.attackCooldown - deltaSeconds);
    player_.dashCooldown = _clampNonNegative(player_.dashCooldown - deltaSeconds);
    player_.invulnerability = _clampNonNegative(player_.invulnerability - deltaSeconds);
    attackEffectRemaining_ = _clampNonNegative(attackEffectRemaining_ - deltaSeconds);
    for (EnemyState& enemy : enemies_) {
        enemy.contactCooldown = _clampNonNegative(enemy.contactCooldown - deltaSeconds);
    }

    _updatePlayer(input, deltaSeconds);
    if (input.attackPressed && player_.attackCooldown <= 0.0F) {
        _performAttack();
    }
    _updateEnemies(deltaSeconds);
    _resolveEnemySeparation();
    _resolveContactDamage();
    _updateSpawning(deltaSeconds);
}

std::uint32_t GameWorld::spawnEnemy(Vec2 position) {
    if (enemies_.size() >= config_.maximumEnemies) {
        throw std::length_error("The arena has reached its configured enemy limit.");
    }
    if (!(std::isfinite(position.x) && std::isfinite(position.y))) {
        throw std::invalid_argument("Enemy position must contain finite coordinates.");
    }
    _clampToArena(position, 14.0F);
    const std::uint32_t id = nextEnemyId_++;
    enemies_.push_back(EnemyState{id, position, 14.0F, config_.enemyHealth, 0.0F});
    return id;
}

const WorldConfig& GameWorld::config() const noexcept {
    return config_;
}

const PlayerState& GameWorld::player() const noexcept {
    return player_;
}

const std::vector<EnemyState>& GameWorld::enemies() const noexcept {
    return enemies_;
}

int GameWorld::score() const noexcept {
    return score_;
}

float GameWorld::elapsedSeconds() const noexcept {
    return elapsedSeconds_;
}

float GameWorld::attackEffectRemaining() const noexcept {
    return attackEffectRemaining_;
}

bool GameWorld::gameOver() const noexcept {
    return player_.health <= 0;
}

float GameWorld::_nextRandomUnit() noexcept {
    randomState_ = randomState_ * 1664525U + 1013904223U;
    return static_cast<float>(randomState_ >> 8U) / 16777216.0F;
}

Vec2 GameWorld::_randomEdgePosition() noexcept {
    const float edge = _nextRandomUnit() * 4.0F;
    const float horizontal = 24.0F + _nextRandomUnit() * (config_.width - 48.0F);
    const float vertical = 24.0F + _nextRandomUnit() * (config_.height - 48.0F);

    if (edge < 1.0F) {
        return {horizontal, 24.0F};
    }
    if (edge < 2.0F) {
        return {config_.width - 24.0F, vertical};
    }
    if (edge < 3.0F) {
        return {horizontal, config_.height - 24.0F};
    }
    return {24.0F, vertical};
}

void GameWorld::_updatePlayer(const InputFrame& input, float deltaSeconds) {
    const Vec2 direction = normalizedOrZero(input.movement);
    player_.position = player_.position + direction * (config_.playerSpeed * deltaSeconds);

    if (input.dashPressed && player_.dashCooldown <= 0.0F && lengthSquared(direction) > 0.0F) {
        player_.position = player_.position + direction * kDashDistance;
        player_.dashCooldown = kDashCooldown;
        player_.invulnerability = kDashInvulnerability;
    }

    _clampToArena(player_.position, player_.radius);
}

void GameWorld::_updateEnemies(float deltaSeconds) {
    for (EnemyState& enemy : enemies_) {
        const Vec2 direction = normalizedOrZero(player_.position - enemy.position);
        enemy.position = enemy.position + direction * (config_.enemySpeed * deltaSeconds);
        _clampToArena(enemy.position, enemy.radius);
    }
}

void GameWorld::_resolveEnemySeparation() {
    constexpr int solverPasses = 3;
    for (int pass = 0; pass < solverPasses; ++pass) {
        for (std::size_t first = 0; first < enemies_.size(); ++first) {
            for (std::size_t second = first + 1; second < enemies_.size(); ++second) {
                EnemyState& lhs = enemies_[first];
                EnemyState& rhs = enemies_[second];
                const Vec2 difference = rhs.position - lhs.position;
                const float minimumDistance = lhs.radius + rhs.radius;
                const float squaredDistance = lengthSquared(difference);
                if (squaredDistance >= minimumDistance * minimumDistance) {
                    continue;
                }

                Vec2 normal{0.70710678F, 0.70710678F};
                float distance = 0.0F;
                if (squaredDistance > kEpsilon) {
                    distance = std::sqrt(squaredDistance);
                    normal = difference * (1.0F / distance);
                }

                float remainingCorrection = minimumDistance - distance;
                const Vec2 originalLhs = lhs.position;
                lhs.position = lhs.position - normal * (remainingCorrection * 0.5F);
                _clampToArena(lhs.position, lhs.radius);
                const float lhsTravel = (originalLhs.x - lhs.position.x) * normal.x +
                                        (originalLhs.y - lhs.position.y) * normal.y;
                remainingCorrection = std::max(0.0F, remainingCorrection - lhsTravel);

                const Vec2 originalRhs = rhs.position;
                rhs.position = rhs.position + normal * remainingCorrection;
                _clampToArena(rhs.position, rhs.radius);
                const float rhsTravel = (rhs.position.x - originalRhs.x) * normal.x +
                                        (rhs.position.y - originalRhs.y) * normal.y;
                remainingCorrection = std::max(0.0F, remainingCorrection - rhsTravel);

                if (remainingCorrection > kEpsilon) {
                    lhs.position = lhs.position - normal * remainingCorrection;
                    _clampToArena(lhs.position, lhs.radius);
                }
            }
        }
    }
}

void GameWorld::_resolveContactDamage() {
    if (player_.invulnerability > 0.0F) {
        return;
    }

    for (EnemyState& enemy : enemies_) {
        const float contactDistance = player_.radius + enemy.radius;
        if (enemy.contactCooldown <= 0.0F &&
            lengthSquared(enemy.position - player_.position) <= contactDistance * contactDistance) {
            player_.health = std::max(0, player_.health - kContactDamage);
            player_.invulnerability = 0.16F;
            enemy.contactCooldown = kContactCooldown;
            break;
        }
    }
}

void GameWorld::_performAttack() {
    player_.attackCooldown = kPlayerAttackCooldown;
    attackEffectRemaining_ = kAttackEffectDuration;

    for (EnemyState& enemy : enemies_) {
        const float hitDistance = kPlayerAttackRadius + enemy.radius;
        if (lengthSquared(enemy.position - player_.position) <= hitDistance * hitDistance) {
            enemy.health -= kPlayerAttackDamage;
        }
    }

    const std::size_t previousCount = enemies_.size();
    std::erase_if(enemies_, [](const EnemyState& enemy) { return enemy.health <= 0; });
    const std::size_t defeatedCount = previousCount - enemies_.size();
    score_ += static_cast<int>(defeatedCount) * 100;
}

void GameWorld::_updateSpawning(float deltaSeconds) {
    if (!config_.automaticSpawning || enemies_.size() >= config_.maximumEnemies) {
        return;
    }

    spawnTimer_ -= deltaSeconds;
    while (spawnTimer_ <= 0.0F && enemies_.size() < config_.maximumEnemies) {
        spawnEnemy(_randomEdgePosition());
        const float difficultyScale = std::max(0.48F, 1.0F - elapsedSeconds_ / 150.0F);
        spawnTimer_ += config_.spawnInterval * difficultyScale;
    }
    if (enemies_.size() >= config_.maximumEnemies) {
        spawnTimer_ = std::max(0.0F, spawnTimer_);
    }
}

void GameWorld::_clampToArena(Vec2& position, float radius) const noexcept {
    position.x = std::clamp(position.x, radius, config_.width - radius);
    position.y = std::clamp(position.y, radius, config_.height - radius);
}

}  // namespace arena
