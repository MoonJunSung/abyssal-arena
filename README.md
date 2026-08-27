# Abyssal Arena

넥토리얼 게임 프로그래머 지원을 목표로 만드는 **C++20 탑다운 액션 게임 포트폴리오**입니다. 작은 게임을 끝까지 완성하면서 게임 루프, 충돌, AI, 성능 측정과 설계 의사결정을 코드와 문서로 함께 보여주는 것이 목표입니다.

현재 버전은 외부 엔진이나 에셋 없이 Win32 GDI만으로 실행되는 첫 번째 플레이 가능 버전입니다. 게임 규칙은 운영체제 코드와 분리되어 Linux에서도 단위 테스트할 수 있습니다.

## 현재 구현

- 60 Hz 고정 시간 간격 게임 시뮬레이션
- 정규화된 8방향 이동과 대시
- 재사용 대기시간이 있는 범위 공격
- 플레이어를 추적하는 적과 적 간 겹침 보정
- 접촉 피해, 무적 시간, 점수, 난이도에 따른 스폰 간격
- 시드 기반의 재현 가능한 난수
- 더블 버퍼링 GDI 렌더링과 HUD
- 창 비활성화·최소화 시 입력과 시뮬레이션 일시 정지
- 핵심 규칙 단위 테스트와 GitHub Actions 빌드

## 조작법

| 동작 | 키 |
| --- | --- |
| 이동 | `WASD` 또는 방향키 |
| 공격 | `J` 또는 마우스 왼쪽 버튼 |
| 대시 | `Space` |
| 재시작 | 게임 오버 후 `R` |
| 종료 | `Esc` |

## 빌드

Windows에서 CMake 3.24 이상과 C++20 컴파일러가 필요합니다.

```powershell
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

MinGW Makefiles를 명시하려면 다음과 같이 실행합니다.

```powershell
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

실행 파일은 생성기 설정에 따라 `build/abyssal_arena.exe` 또는 `build/Release/abyssal_arena.exe`에 만들어집니다.

## 포트폴리오 로드맵

1. **Vertical Slice** — 고정 게임 루프, 전투, 적 스폰, 테스트
2. **AI & Navigation** — 타일 맵과 A* 길 찾기, 시야 디버그 표시
3. **Performance** — 공간 분할과 오브젝트 풀, 전후 벤치마크
4. **Tooling** — 맵 직렬화, 입력 기록 기반 리플레이
5. **Presentation** — 플레이 영상, 성능 그래프, 트러블슈팅 문서

구조와 설계 원칙은 [docs/architecture.md](docs/architecture.md)에 기록합니다.
