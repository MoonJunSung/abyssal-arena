#include "core/GameWorld.h"

#include <cmath>
#include <exception>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string_view>

namespace {

constexpr float kTolerance = 0.01F;

void _require(bool condition, std::string_view message) {
    if (!condition) {
        throw std::runtime_error(message.data());
    }
}

arena::WorldConfig _testConfig() {
    arena::WorldConfig config;
    config.width = 400.0F;
    config.height = 300.0F;
    config.playerSpeed = 100.0F;
    config.enemySpeed = 0.0F;
    config.initialEnemies = 0;
    config.maximumEnemies = 16;
    config.automaticSpawning = false;
    config.enemyHealth = 80;
    return config;
}

void _diagonalMovementIsNormalized() {
    arena::GameWorld straightWorld(_testConfig());
    arena::GameWorld diagonalWorld(_testConfig());

    straightWorld.update({{1.0F, 0.0F}, false, false}, 0.1F);
    diagonalWorld.update({{1.0F, 1.0F}, false, false}, 0.1F);

    const arena::Vec2 center{200.0F, 150.0F};
    const float straightDistance = std::sqrt(arena::lengthSquared(
        straightWorld.player().position - center));
    const float diagonalDistance = std::sqrt(arena::lengthSquared(
        diagonalWorld.player().position - center));
    _require(std::abs(straightDistance - diagonalDistance) < kTolerance,
             "Diagonal movement must not be faster than cardinal movement.");
}

void _playerCannotLeaveArena() {
    arena::GameWorld world(_testConfig());
    for (int step = 0; step < 30; ++step) {
        world.update({{-1.0F, -1.0F}, false, false}, 0.1F);
    }

    _require(world.player().position.x >= world.player().radius,
             "Player crossed the left boundary.");
    _require(world.player().position.y >= world.player().radius,
             "Player crossed the top boundary.");
}

void _attackHasCooldownAndAwardsScore() {
    arena::GameWorld world(_testConfig());
    world.spawnEnemy({235.0F, 150.0F});

    world.update({{}, true, false}, 0.01F);
    _require(world.enemies().size() == 1 && world.enemies().front().health == 35,
             "First attack should damage the nearby enemy once.");

    world.update({{}, true, false}, 0.01F);
    _require(world.enemies().front().health == 35,
             "Attack cooldown should prevent immediate repeated damage.");

    world.update({}, 0.25F);
    world.update({}, 0.10F);
    world.update({{}, true, false}, 0.01F);
    _require(world.enemies().empty(), "Second valid attack should defeat the enemy.");
    _require(world.score() == 100, "Defeating one enemy should award 100 points.");
}

void _dashMovesAndStartsCooldown() {
    arena::GameWorld world(_testConfig());
    const float initialX = world.player().position.x;
    world.update({{1.0F, 0.0F}, false, true}, 0.01F);

    _require(world.player().position.x > initialX + 100.0F,
             "Dash should create a clear burst of movement.");
    _require(world.player().dashCooldown > 1.0F,
             "Dash should start its cooldown.");
    _require(world.player().invulnerability > 0.0F,
             "Dash should briefly protect the player.");
}

void _invalidDeltaTimeIsReported() {
    arena::GameWorld world(_testConfig());
    bool threw = false;
    try {
        world.update({}, 0.0F);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    _require(threw, "Invalid delta time must be reported instead of ignored.");
}

void _invalidWorldAndSpawnInputsAreReported() {
    arena::WorldConfig invalidConfig = _testConfig();
    invalidConfig.width = 20.0F;
    bool invalidWorldThrew = false;
    try {
        arena::GameWorld world(invalidConfig);
    } catch (const std::invalid_argument&) {
        invalidWorldThrew = true;
    }
    _require(invalidWorldThrew, "An arena smaller than its actors must be rejected.");

    arena::GameWorld world(_testConfig());
    bool invalidPositionThrew = false;
    try {
        world.spawnEnemy({std::nanf(""), 10.0F});
    } catch (const std::invalid_argument&) {
        invalidPositionThrew = true;
    }
    _require(invalidPositionThrew, "Non-finite spawn coordinates must be rejected.");

    arena::WorldConfig infiniteSpeedConfig = _testConfig();
    infiniteSpeedConfig.playerSpeed = std::numeric_limits<float>::infinity();
    bool infiniteSpeedThrew = false;
    try {
        arena::GameWorld invalidWorld(infiniteSpeedConfig);
    } catch (const std::invalid_argument&) {
        infiniteSpeedThrew = true;
    }
    _require(infiniteSpeedThrew, "Infinite movement speed must be rejected.");
}

void _spawnTimingDoesNotDependOnFramePartitioning() {
    arena::WorldConfig config = _testConfig();
    config.automaticSpawning = true;
    config.spawnInterval = 0.1F;

    arena::GameWorld singleUpdateWorld(config);
    arena::GameWorld partitionedWorld(config);
    singleUpdateWorld.update({}, 0.25F);
    for (int step = 0; step < 5; ++step) {
        partitionedWorld.update({}, 0.05F);
    }

    _require(singleUpdateWorld.enemies().size() == partitionedWorld.enemies().size(),
             "Equal simulated time must produce the same number of enemies.");
}

void _spawnLimitAndSeparationAreEnforced() {
    arena::WorldConfig config = _testConfig();
    config.maximumEnemies = 2;
    arena::GameWorld world(config);
    world.spawnEnemy({14.0F, 14.0F});
    world.spawnEnemy({14.0F, 14.0F});

    bool limitThrew = false;
    try {
        world.spawnEnemy({100.0F, 100.0F});
    } catch (const std::length_error&) {
        limitThrew = true;
    }
    _require(limitThrew, "The public spawn seam must enforce the configured enemy limit.");

    world.update({}, 0.01F);
    const arena::Vec2 difference = world.enemies()[1].position - world.enemies()[0].position;
    _require(std::sqrt(arena::lengthSquared(difference)) >= 27.99F,
             "Coincident enemies must be separated even at an arena boundary.");
}

}  // namespace

int main() {
    try {
        _diagonalMovementIsNormalized();
        _playerCannotLeaveArena();
        _attackHasCooldownAndAwardsScore();
        _dashMovesAndStartsCooldown();
        _invalidDeltaTimeIsReported();
        _invalidWorldAndSpawnInputsAreReported();
        _spawnTimingDoesNotDependOnFramePartitioning();
        _spawnLimitAndSeparationAreEnforced();
        std::cout << "All arena_core tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Test failure: " << error.what() << '\n';
        return 1;
    }
}
