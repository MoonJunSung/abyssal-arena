# Abyssal Arena

C++20으로 구현한 top-down action game prototype.

## Preview

게임 실행 화면
<img width="1268" height="707" alt="image" src="https://github.com/user-attachments/assets/b8aa5b16-e9ef-496e-b508-06c5909948fa" />
<img width="1280" height="718" alt="image" src="https://github.com/user-attachments/assets/88586eae-a974-4f82-b21b-8854272d7503" />

## Development Goals
- 게임 로직과 플랫폼 코드 분리
- Fixed timestep 기반 deterministic simulation
- 테스트 가능한 게임 시스템 설계
- 성능 병목을 측정하고 개선하는 구조

## Core Systems
- Player movement
- Attack / cooldown
- Dash / invulnerability
- Enemy chase AI
- Enemy spawning
- Collision separation
- Score / game over
- Event system

## Architecture

Win32 Platform
      ↓
   InputFrame
      ↓
   GameWorld
      ↓
   GameEvent

## Testing

- Movement normalization
- Cooldown
- Combat
- Spawn determinism
- Invalid input
- Collision separation

## Future Optimization

Current enemy separation:
O(n²)

Planned:
Uniform Grid Spatial Partitioning

## 조작법

| 동작 | 키 |
| --- | --- |
| 이동 | `WASD` 또는 방향키 |
| 공격 | `J` 또는 마우스 왼쪽 버튼 |
| 대시 | `Space` |
| 재시작 | 게임 오버 후 `R` |
| 종료 | `Esc` |

## 빌드

Windows에서 CMake 3.24 이상과 C++20 컴파일러가 필요.

```powershell
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

MinGW Makefiles를 명시하려면 다음과 같이 실행하여야 합니다.

```powershell
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

실행 파일은 생성기 설정에 따라 `build/abyssal_arena.exe` 또는 `build/Release/abyssal_arena.exe`에 만들어집니다.
