#pragma once

#include <cstdint>
#include <vector>

namespace arena {

struct Vec2 {
    float x{};
    float y{};
};

[[nodiscard]] Vec2 operator+(Vec2 lhs, Vec2 rhs) noexcept;
[[nodiscard]] Vec2 operator-(Vec2 lhs, Vec2 rhs) noexcept;
[[nodiscard]] Vec2 operator*(Vec2 value, float scalar) noexcept;
[[nodiscard]] float lengthSquared(Vec2 value) noexcept;
[[nodiscard]] Vec2 normalizedOrZero(Vec2 value) noexcept;

struct InputFrame {
    Vec2 movement{};
    bool attackPressed{};
    bool dashPressed{};
};

enum class GameEvent : std::uint8_t {
    AttackStarted,
    DashStarted,
    // Aggregated per update: emitted once when one or more enemies are defeated.
    EnemyDefeated,
    PlayerDamaged,
    GameOver,
};

struct PlayerState {
    Vec2 position{};
    float radius{18.0F};
    int health{100};
    float attackCooldown{};
    float dashCooldown{};
    float invulnerability{};
};

struct EnemyState {
    std::uint32_t id{};
    Vec2 position{};
    float radius{14.0F};
    int health{40};
    float contactCooldown{};
};

struct WorldConfig {
    float width{1280.0F};
    float height{720.0F};
    float playerSpeed{260.0F};
    float enemySpeed{92.0F};
    float spawnInterval{1.15F};
    std::size_t maximumEnemies{48};
    std::size_t initialEnemies{5};
    int enemyHealth{40};
    bool automaticSpawning{true};
};

class GameWorld {
public:
    explicit GameWorld(WorldConfig config = {}, std::uint32_t seed = 0xC0FFEEU);

    void reset();
    void update(const InputFrame& input, float deltaSeconds);

    // Kept public as a level-scripting seam and as a deterministic test hook.
    std::uint32_t spawnEnemy(Vec2 position);

    [[nodiscard]] const WorldConfig& config() const noexcept;
    [[nodiscard]] const PlayerState& player() const noexcept;
    [[nodiscard]] const std::vector<EnemyState>& enemies() const noexcept;
    // Events produced by the latest update. Consume before calling update again.
    [[nodiscard]] const std::vector<GameEvent>& events() const noexcept;
    [[nodiscard]] int score() const noexcept;
    [[nodiscard]] float elapsedSeconds() const noexcept;
    [[nodiscard]] float attackEffectRemaining() const noexcept;
    [[nodiscard]] bool gameOver() const noexcept;

private:
    [[nodiscard]] float _nextRandomUnit() noexcept;
    [[nodiscard]] Vec2 _randomEdgePosition() noexcept;
    void _updatePlayer(const InputFrame& input, float deltaSeconds);
    void _updateEnemies(float deltaSeconds);
    void _resolveEnemySeparation();
    void _resolveContactDamage();
    void _performAttack();
    void _updateSpawning(float deltaSeconds);
    void _clampToArena(Vec2& position, float radius) const noexcept;

    WorldConfig config_;
    std::uint32_t initialSeed_;
    std::uint32_t randomState_;
    std::uint32_t nextEnemyId_{1};
    PlayerState player_;
    std::vector<EnemyState> enemies_;
    std::vector<GameEvent> events_;
    int score_{};
    float elapsedSeconds_{};
    float spawnTimer_{};
    float attackEffectRemaining_{};
};

}  // namespace arena
