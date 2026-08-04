#include "input.h"

#include <windows.h>
#include <timeapi.h>  // timeBeginPeriod: WIN32_LEAN_AND_MEAN exclui mmsystem.h

#include <algorithm>
#include <atomic>
#include <cmath>
#include <thread>

namespace input {
namespace {

// ---------------------------------------------------------------------------
// Estado compartilhado com a thread de render (somente leitura la).
// ---------------------------------------------------------------------------
std::atomic<bool>  g_running{false};
std::atomic<bool>  g_enabled{false};
std::atomic<bool>  g_isActive{false};
std::atomic<float> g_rate{kPresetF7};

std::thread g_thread;

// ---------------------------------------------------------------------------
// Deteccao de borda
//
// Substitui os seis blocos "bCanTrigger_*" repetidos da versao anterior. Toda
// leitura usa GetAsyncKeyState: GetKeyState reflete o estado da fila de
// mensagens da thread chamadora no ultimo evento processado, o que e pouco
// confiavel para um processo que nao tem foco -- e o overlay nunca tem.
// ---------------------------------------------------------------------------
struct Hotkey {
    int  vk;
    bool wasDown = false;
};

bool Down(int vk) {
    return (GetAsyncKeyState(vk) & 0x8000) != 0;
}

bool Edge(Hotkey& key) {
    const bool down = Down(key.vk);
    const bool edge = down && !key.wasDown;
    key.wasDown = down;
    return edge;
}

// Borda para uma condicao composta (combinacao de teclas).
bool Edge(bool condition, bool& wasDown) {
    const bool edge = condition && !wasDown;
    wasDown = condition;
    return edge;
}

void AdjustRate(float delta) {
    const float updated = std::clamp(g_rate.load() + delta, kRateMin, kRateMax);
    g_rate.store(updated);
}

void MoveMouseRelative(int dy) {
    INPUT in = {};
    in.type         = INPUT_MOUSE;
    in.mi.dx        = 0;
    in.mi.dy        = dy;
    in.mi.dwFlags   = MOUSEEVENTF_MOVE;  // relativo: sem MOUSEEVENTF_ABSOLUTE
    in.mi.dwExtraInfo = static_cast<ULONG_PTR>(GetMessageExtraInfo());
    SendInput(1, &in, sizeof(INPUT));
}

// ---------------------------------------------------------------------------
// Thread
// ---------------------------------------------------------------------------
void ThreadMain() {
    // Sem isto, Sleep(1) dorme cerca de 15,6 ms -- a granularidade padrao do
    // timer do Windows. A versao anterior tinha sleep_for(1ms) achando que
    // dormia 1 ms.
    timeBeginPeriod(1);

    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);
    LARGE_INTEGER prev;
    QueryPerformanceCounter(&prev);

    Hotkey kF12{VK_F12}, kF11{VK_F11}, kF10{VK_F10};
    Hotkey kF9{VK_F9},   kF8{VK_F8},   kF7{VK_F7};

    bool toggleWasDown    = false;
    bool extraGunsWasDown = false;
    bool mainGunWasDown   = false;

    bool prevActiveState     = false;
    bool canStorePrevState   = true;

    // Fracao de pixel que sobrou do tick anterior. Sem isto, uma taxa de
    // 250 px/s com tick de 1 ms daria 0,25 px por tick, truncaria para zero e
    // o mouse nunca sairia do lugar.
    double residual = 0.0;

    while (g_running.load()) {
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        double dt = static_cast<double>(now.QuadPart - prev.QuadPart) /
                    static_cast<double>(freq.QuadPart);
        prev = now;

        // Se a thread ficou sem CPU, dt vira um salto grande e a compensacao
        // daria um tranco. Melhor perder o deslocamento do periodo perdido.
        dt = std::min(dt, 0.05);

        // --- toggle geral: Ctrl+Shift+S ---
        const bool toggleCombo = Down(VK_CONTROL) && Down(VK_SHIFT) && Down('S');
        if (Edge(toggleCombo, toggleWasDown)) {
            g_isActive.store(!g_isActive.load());
        }

        // --- troca de arma ---
        const bool extraGuns = Down('2') || Down('3') || Down('4');
        if (Edge(extraGuns, extraGunsWasDown)) {
            if (canStorePrevState) {
                prevActiveState   = g_isActive.load();
                canStorePrevState = false;
            }
            g_isActive.store(false);
        }
        if (Edge(Down('1'), mainGunWasDown)) {
            g_isActive.store(prevActiveState);
            canStorePrevState = true;
        }

        if (!g_enabled.load() || !g_isActive.load()) {
            residual = 0.0;
            Sleep(1);
            continue;
        }

        // --- ajuste da taxa ---
        if (Edge(kF12)) AdjustRate(+kStepFine);
        if (Edge(kF11)) AdjustRate(-kStepFine);
        if (Edge(kF10)) AdjustRate(+kStepCoarse);
        if (Edge(kF9))  AdjustRate(-kStepCoarse);
        if (Edge(kF8))  g_rate.store(kPresetF8);
        if (Edge(kF7))  g_rate.store(kPresetF7);

        // --- compensacao ---
        const bool firing    = Down(VK_LBUTTON);
        const bool suppressed = Down(VK_HOME) || Down(VK_F2);

        if (firing && !suppressed) {
            residual += static_cast<double>(g_rate.load()) * dt;

            const double whole = std::trunc(residual);
            residual -= whole;

            const int dy = static_cast<int>(whole);
            if (dy != 0) MoveMouseRelative(dy);
        } else {
            // Rajada nova comeca do zero.
            residual = 0.0;
        }

        Sleep(1);
    }

    timeEndPeriod(1);
}

}  // namespace

bool  IsActive() { return g_isActive.load(); }
float Rate()     { return g_rate.load(); }

void SetEnabled(bool enabled) { g_enabled.store(enabled); }

void Start() {
    if (g_running.load()) return;
    g_running.store(true);
    g_thread = std::thread(ThreadMain);
}

void Stop() {
    if (!g_running.load()) return;
    g_running.store(false);
    if (g_thread.joinable()) g_thread.join();
}

}  // namespace input
