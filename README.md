# Abyssal Arena

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
