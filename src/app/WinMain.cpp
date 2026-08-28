#include "app/AudioSystem.h"
#include "core/GameWorld.h"

#include <windows.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cwchar>
#include <exception>
#include <iterator>
#include <stdexcept>

namespace {

constexpr int kClientWidth = 1280;
constexpr int kClientHeight = 720;
constexpr float kFixedStepSeconds = 1.0F / 60.0F;

struct AppState {
    arena::GameWorld world;
    arena::app::AudioSystem audio;
    bool active{true};
    bool previousAttackDown{};
    bool previousDashDown{};
    bool pendingAttack{};
    bool pendingDash{};
    arena::Vec2 movement{};
};

bool _isKeyDown(int key) noexcept {
    return (GetAsyncKeyState(key) & 0x8000) != 0;
}

void _pollInput(AppState& app) noexcept {
    app.movement.x = (_isKeyDown('D') || _isKeyDown(VK_RIGHT) ? 1.0F : 0.0F) -
                     (_isKeyDown('A') || _isKeyDown(VK_LEFT) ? 1.0F : 0.0F);
    app.movement.y = (_isKeyDown('S') || _isKeyDown(VK_DOWN) ? 1.0F : 0.0F) -
                     (_isKeyDown('W') || _isKeyDown(VK_UP) ? 1.0F : 0.0F);

    const bool attackDown = _isKeyDown(VK_LBUTTON) || _isKeyDown('J');
    const bool dashDown = _isKeyDown(VK_SPACE);
    app.pendingAttack = app.pendingAttack || (attackDown && !app.previousAttackDown);
    app.pendingDash = app.pendingDash || (dashDown && !app.previousDashDown);
    app.previousAttackDown = attackDown;
    app.previousDashDown = dashDown;
}

void _fillCircle(HDC dc, arena::Vec2 center, float radius, COLORREF color) {
    const int left = static_cast<int>(std::lround(center.x - radius));
    const int top = static_cast<int>(std::lround(center.y - radius));
    const int right = static_cast<int>(std::lround(center.x + radius));
    const int bottom = static_cast<int>(std::lround(center.y + radius));
    HBRUSH brush = CreateSolidBrush(color);
    HGDIOBJ previousBrush = SelectObject(dc, brush);
    HGDIOBJ previousPen = SelectObject(dc, GetStockObject(NULL_PEN));
    Ellipse(dc, left, top, right, bottom);
    SelectObject(dc, previousPen);
    SelectObject(dc, previousBrush);
    DeleteObject(brush);
}

void _drawArenaGrid(HDC dc) {
    HPEN pen = CreatePen(PS_SOLID, 1, RGB(33, 41, 58));
    HGDIOBJ previousPen = SelectObject(dc, pen);
    for (int x = 0; x <= kClientWidth; x += 64) {
        MoveToEx(dc, x, 0, nullptr);
        LineTo(dc, x, kClientHeight);
    }
    for (int y = 0; y <= kClientHeight; y += 64) {
        MoveToEx(dc, 0, y, nullptr);
        LineTo(dc, kClientWidth, y);
    }
    SelectObject(dc, previousPen);
    DeleteObject(pen);
}

void _drawHud(HDC dc, const arena::GameWorld& world) {
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(229, 235, 245));

    wchar_t status[160]{};
    std::swprintf(status, 160, L"HP %d    SCORE %d    ENEMIES %zu    TIME %.1fs",
                  world.player().health,
                  world.score(),
                  world.enemies().size(),
                  static_cast<double>(world.elapsedSeconds()));
    TextOutW(dc, 24, 20, status, static_cast<int>(std::wcslen(status)));

    constexpr wchar_t controls[] = L"MOVE  WASD / ARROWS     ATTACK  J / LEFT CLICK     DASH  SPACE";
    TextOutW(dc, 24, kClientHeight - 34, controls,
             static_cast<int>(std::size(controls) - 1));

    if (world.gameOver()) {
        constexpr wchar_t title[] = L"GAME OVER";
        constexpr wchar_t restart[] = L"Press R to restart";
        SetTextColor(dc, RGB(255, 112, 112));
        TextOutW(dc, kClientWidth / 2 - 54, kClientHeight / 2 - 16, title,
                 static_cast<int>(std::size(title) - 1));
        SetTextColor(dc, RGB(229, 235, 245));
        TextOutW(dc, kClientWidth / 2 - 68, kClientHeight / 2 + 12, restart,
                 static_cast<int>(std::size(restart) - 1));
    }
}

void _render(HWND window, const arena::GameWorld& world) {
    PAINTSTRUCT paint{};
    HDC windowDc = BeginPaint(window, &paint);
    HDC bufferDc = CreateCompatibleDC(windowDc);
    HBITMAP bufferBitmap = CreateCompatibleBitmap(windowDc, kClientWidth, kClientHeight);
    HGDIOBJ previousBitmap = SelectObject(bufferDc, bufferBitmap);

    RECT clientRect{0, 0, kClientWidth, kClientHeight};
    HBRUSH background = CreateSolidBrush(RGB(16, 21, 31));
    FillRect(bufferDc, &clientRect, background);
    DeleteObject(background);
    _drawArenaGrid(bufferDc);

    if (world.attackEffectRemaining() > 0.0F) {
        HPEN attackPen = CreatePen(PS_SOLID, 4, RGB(255, 210, 92));
        HGDIOBJ previousPen = SelectObject(bufferDc, attackPen);
        HGDIOBJ previousBrush = SelectObject(bufferDc, GetStockObject(HOLLOW_BRUSH));
        const arena::Vec2 center = world.player().position;
        constexpr int radius = 86;
        Ellipse(bufferDc,
                static_cast<int>(center.x) - radius,
                static_cast<int>(center.y) - radius,
                static_cast<int>(center.x) + radius,
                static_cast<int>(center.y) + radius);
        SelectObject(bufferDc, previousBrush);
        SelectObject(bufferDc, previousPen);
        DeleteObject(attackPen);
    }

    for (const arena::EnemyState& enemy : world.enemies()) {
        _fillCircle(bufferDc, enemy.position, enemy.radius, RGB(229, 75, 92));
        const float healthRatio = std::clamp(
            static_cast<float>(enemy.health) / static_cast<float>(world.config().enemyHealth),
            0.0F,
            1.0F);
        RECT healthBar{
            static_cast<int>(enemy.position.x - 15.0F),
            static_cast<int>(enemy.position.y - enemy.radius - 8.0F),
            static_cast<int>(enemy.position.x - 15.0F + 30.0F * healthRatio),
            static_cast<int>(enemy.position.y - enemy.radius - 5.0F)};
        HBRUSH healthBrush = CreateSolidBrush(RGB(255, 190, 85));
        FillRect(bufferDc, &healthBar, healthBrush);
        DeleteObject(healthBrush);
    }

    const COLORREF playerColor = world.player().invulnerability > 0.0F
                                    ? RGB(125, 220, 255)
                                    : RGB(69, 156, 255);
    _fillCircle(bufferDc, world.player().position, world.player().radius, playerColor);
    _drawHud(bufferDc, world);

    BitBlt(windowDc, 0, 0, kClientWidth, kClientHeight, bufferDc, 0, 0, SRCCOPY);
    SelectObject(bufferDc, previousBitmap);
    DeleteObject(bufferBitmap);
    DeleteDC(bufferDc);
    EndPaint(window, &paint);
}

LRESULT CALLBACK _windowProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_NCCREATE) {
        const auto* creation = reinterpret_cast<CREATESTRUCTW*>(lParam);
        SetWindowLongPtrW(window, GWLP_USERDATA,
                          reinterpret_cast<LONG_PTR>(creation->lpCreateParams));
    }

    auto* app = reinterpret_cast<AppState*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    switch (message) {
        case WM_PAINT:
            if (app != nullptr) {
                _render(window, app->world);
                return 0;
            }
            break;
        case WM_ERASEBKGND:
            return 1;
        case WM_KEYDOWN:
            if (app != nullptr && wParam == 'R' && app->world.gameOver()) {
                app->world.reset();
                return 0;
            }
            if (wParam == VK_ESCAPE) {
                DestroyWindow(window);
                return 0;
            }
            break;
        case WM_ACTIVATEAPP:
            if (app != nullptr) {
                app->active = wParam != FALSE;
                if (!app->active) {
                    app->movement = {};
                    app->pendingAttack = false;
                    app->pendingDash = false;
                    app->previousAttackDown = false;
                    app->previousDashDown = false;
                }
            }
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        default:
            break;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

int _run(HINSTANCE instance, int showCommand) {
    AppState app;
    constexpr wchar_t className[] = L"AbyssalArenaWindow";

    WNDCLASSW windowClass{};
    windowClass.lpfnWndProc = _windowProcedure;
    windowClass.hInstance = instance;
    windowClass.lpszClassName = className;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_CROSS);
    windowClass.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    if (RegisterClassW(&windowClass) == 0) {
        throw std::runtime_error("Could not register the game window class.");
    }

    constexpr DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    RECT windowRect{0, 0, kClientWidth, kClientHeight};
    AdjustWindowRect(&windowRect, style, FALSE);
    HWND window = CreateWindowExW(
        0,
        className,
        L"Abyssal Arena - C++ Portfolio Prototype",
        style,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        windowRect.right - windowRect.left,
        windowRect.bottom - windowRect.top,
        nullptr,
        nullptr,
        instance,
        &app);
    if (window == nullptr) {
        throw std::runtime_error("Could not create the game window.");
    }

    ShowWindow(window, showCommand);
    UpdateWindow(window);

    using Clock = std::chrono::steady_clock;
    auto previousTime = Clock::now();
    float accumulator = 0.0F;
    bool running = true;

    while (running) {
        MSG message{};
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
            if (message.message == WM_QUIT) {
                running = false;
                break;
            }
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        if (!running) {
            break;
        }

        const auto currentTime = Clock::now();
        const std::chrono::duration<float> frameDuration = currentTime - previousTime;
        previousTime = currentTime;
        if (!app.active || IsIconic(window) != FALSE) {
            accumulator = 0.0F;
            Sleep(10);
            continue;
        }
        accumulator += std::min(frameDuration.count(), 0.25F);

        _pollInput(app);
        arena::InputFrame input{app.movement, app.pendingAttack, app.pendingDash};
        bool advancedSimulation = false;
        while (accumulator >= kFixedStepSeconds) {
            if (advancedSimulation) {
                input.attackPressed = false;
                input.dashPressed = false;
            }
            app.world.update(input, kFixedStepSeconds);
            app.audio.playEvents(app.world.events());
            advancedSimulation = true;
            accumulator -= kFixedStepSeconds;
        }
        if (advancedSimulation) {
            app.pendingAttack = false;
            app.pendingDash = false;
        }

        if (advancedSimulation) {
            InvalidateRect(window, nullptr, FALSE);
        }
        Sleep(1);
    }
    return 0;
}

}  // namespace

int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int showCommand) {
    try {
        return _run(instance, showCommand);
    } catch (const std::exception& error) {
        MessageBoxA(nullptr, error.what(), "Abyssal Arena - Fatal error", MB_OK | MB_ICONERROR);
        return 1;
    }
}
