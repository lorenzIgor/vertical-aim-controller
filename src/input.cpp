#include "input.h"

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
std::atomic<bool> g_running{false};
std::atomic<bool> g_enabled{false};
std::atomic<bool> g_isActive{false};
std::atomic<int>  g_currentSlot{0};

std::atomic<float> g_ratePerSlot[kSlotCount];

std::atomic<float> g_stepFine{5.0f};
std::atomic<float> g_stepCoarse{25.0f};
std::atomic<float> g_presetF7{135.0f};
std::atomic<float> g_presetF8{335.0f};

std::atomic<HWND> g_gameWindow{nullptr};
std::atomic<HWND> g_overlayWindow{nullptr};

std::atomic<bool> g_requireForeground{true};
std::atomic<bool> g_requireCursorPinned{true};
std::atomic<bool> g_suppressWhenCursorVisible{false};

std::atomic<bool>   g_ctxForeground{false};
std::atomic<bool>   g_ctxCursorVisible{false};
std::atomic<bool>   g_ctxCursorPinned{false};
std::atomic<bool>   g_ctxSuppressedByKey{false};
std::atomic<bool>   g_ctxCompensating{false};
std::atomic<Status> g_ctxStatus{Status::NoGame};

// Raio em pixels dentro do qual o cursor conta como "no centro".
constexpr int kCenterTolerancePx = 6;

// Tempo continuo fora do centro necessario para declarar menu. O jogo
// recentraliza a cada quadro, entao no gameplay isso nunca acumula; o valor so
// precisa ser maior que um intervalo de quadro com folga.
constexpr double kMenuHysteresisMs = 250.0;

std::thread g_thread;

int ClampSlot(int slot) {
    return std::clamp(slot, 0, kSlotCount - 1);
}

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

void AdjustCurrentRate(float delta) {
    const int slot = ClampSlot(g_currentSlot.load());
    g_ratePerSlot[slot].store(
        std::clamp(g_ratePerSlot[slot].load() + delta, kRateMin, kRateMax));
}

void SetCurrentRate(float rate) {
    const int slot = ClampSlot(g_currentSlot.load());
    g_ratePerSlot[slot].store(std::clamp(rate, kRateMin, kRateMax));
}

void MoveMouseRelative(int dy) {
    INPUT in = {};
    in.type           = INPUT_MOUSE;
    in.mi.dx          = 0;
    in.mi.dy          = dy;
    in.mi.dwFlags     = MOUSEEVENTF_MOVE;  // relativo: sem MOUSEEVENTF_ABSOLUTE
    in.mi.dwExtraInfo = static_cast<ULONG_PTR>(GetMessageExtraInfo());
    SendInput(1, &in, sizeof(INPUT));
}

bool CursorIsVisible() {
    CURSORINFO ci = {};
    ci.cbSize = sizeof(ci);
    if (!GetCursorInfo(&ci)) return false;
    return (ci.flags & CURSOR_SHOWING) != 0;
}

// O jogo esta segurando o cursor no centro da propria janela?
//
// Durante o gameplay o Battlefield V recentraliza o cursor a cada quadro,
// porque a mira vem de raw input; no menu ele solta. Isso distingue os dois
// estados sem depender de visibilidade do cursor -- que neste jogo fica sempre
// visivel -- nem de ClipCursor, que ele nunca usa.
bool CursorPinnedToCenter() {
    const HWND game = g_gameWindow.load();
    if (game == nullptr) return false;

    RECT client;
    if (!GetClientRect(game, &client)) return false;

    POINT center{(client.right - client.left) / 2, (client.bottom - client.top) / 2};
    if (!ClientToScreen(game, &center)) return false;

    POINT cursor;
    if (!GetCursorPos(&cursor)) return false;

    const int dx = cursor.x - center.x;
    const int dy = cursor.y - center.y;
    return (dx * dx + dy * dy) <= (kCenterTolerancePx * kCenterTolerancePx);
}

// O overlay conta como contexto valido: com o painel aberto o foco e dele.
bool InGameContext() {
    const HWND fg      = GetForegroundWindow();
    const HWND game    = g_gameWindow.load();
    const HWND overlay = g_overlayWindow.load();
    return fg != nullptr && (fg == game || fg == overlay);
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
    Hotkey kSlot[kSlotCount] = {Hotkey{'1'}, Hotkey{'2'}, Hotkey{'3'}, Hotkey{'4'}};

    bool toggleWasDown = false;

    // Estado por slot de arma.
    //
    // A versao anterior guardava um unico "estado anterior": as teclas 2/3/4
    // salvavam o estado e desligavam, e a tecla 1 restaurava. O valor salvo
    // ficava obsoleto assim que o usuario usasse o toggle manual -- estando na
    // arma 1 e ligando com Ctrl+Shift+S, apertar 1 de novo desligava sem pedir,
    // porque restaurava um estado capturado antes do toggle.
    //
    // Nao e persistido: comecar com a compensacao ligada sem o usuario ter
    // pedido seria uma surpresa ruim.
    bool activePerSlot[kSlotCount] = {};

    // Fracao de pixel que sobrou do tick anterior. Sem isto, uma taxa de
    // 135 px/s com tick de 1 ms daria 0,135 px por tick, truncaria para zero e
    // o mouse nunca sairia do lugar.
    double residual = 0.0;

    // Tempo continuo com o cursor fora do centro da janela do jogo.
    double offCenterMs = 0.0;

    while (g_running.load()) {
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        double dt = static_cast<double>(now.QuadPart - prev.QuadPart) /
                    static_cast<double>(freq.QuadPart);
        prev = now;

        // Se a thread ficou sem CPU, dt vira um salto grande e a compensacao
        // daria um tranco. Melhor perder o deslocamento do periodo perdido.
        dt = std::min(dt, 0.05);

        const bool foreground    = InGameContext();
        const bool cursorVisible = CursorIsVisible();
        const bool cursorPinned  = CursorPinnedToCenter();
        // Apenas F2: HOME passou a alternar a visibilidade do HUD, e uma tecla
        // com duas funcoes so gera confusao sobre o que ela acabou de fazer.
        const bool keySuppressed = Down(VK_F2);

        // Um quadro solto nao basta: o proprio movimento que enviamos tira o
        // cursor do centro por alguns milissegundos ate o jogo recentralizar.
        if (cursorPinned) {
            offCenterMs = 0.0;
        } else {
            offCenterMs += dt * 1000.0;
        }
        const bool looksLikeMenu = offCenterMs > kMenuHysteresisMs;

        g_ctxForeground.store(foreground);
        g_ctxCursorVisible.store(cursorVisible);
        g_ctxCursorPinned.store(!looksLikeMenu);
        g_ctxSuppressedByKey.store(keySuppressed);

        // Camada 1: fora do jogo, nem as hotkeys valem. Antes, digitar "2" em
        // um chat ou navegador alterava o estado da ferramenta.
        const bool contextOk = !g_requireForeground.load() || foreground;

        if (g_enabled.load() && contextOk) {
            // --- troca de arma ---
            for (int i = 0; i < kSlotCount; ++i) {
                if (Edge(kSlot[i])) {
                    g_currentSlot.store(i);
                    g_isActive.store(activePerSlot[i]);
                }
            }

            // --- toggle geral: Ctrl+Shift+S, aplicado ao slot atual ---
            const bool toggleCombo = Down(VK_CONTROL) && Down(VK_SHIFT) && Down('S');
            if (Edge(toggleCombo, toggleWasDown)) {
                const int slot = ClampSlot(g_currentSlot.load());
                activePerSlot[slot] = !activePerSlot[slot];
                g_isActive.store(activePerSlot[slot]);
            }

            // --- ajuste da taxa do slot atual ---
            if (g_isActive.load()) {
                if (Edge(kF12)) AdjustCurrentRate(+g_stepFine.load());
                if (Edge(kF11)) AdjustCurrentRate(-g_stepFine.load());
                if (Edge(kF10)) AdjustCurrentRate(+g_stepCoarse.load());
                if (Edge(kF9))  AdjustCurrentRate(-g_stepCoarse.load());
                if (Edge(kF8))  SetCurrentRate(g_presetF8.load());
                if (Edge(kF7))  SetCurrentRate(g_presetF7.load());
            }
        } else {
            // Fora de contexto: consome as bordas para que voltar ao jogo com
            // uma tecla ja segurada nao dispare uma acao acumulada.
            for (auto& k : kSlot) Edge(k);
            Edge(kF12); Edge(kF11); Edge(kF10);
            Edge(kF9);  Edge(kF8);  Edge(kF7);
            Edge(Down(VK_CONTROL) && Down(VK_SHIFT) && Down('S'), toggleWasDown);
        }

        // --- compensacao ---
        // Camada 2: cursor visivel indica menu aberto. Durante o gameplay um
        // FPS esconde o cursor do sistema e usa raw input.
        const bool cursorBlocks = g_suppressWhenCursorVisible.load() && cursorVisible;
        const bool menuBlocks   = g_requireCursorPinned.load() && looksLikeMenu;

        const bool compensate = g_enabled.load() && g_isActive.load() && contextOk &&
                                !menuBlocks && !cursorBlocks && !keySuppressed &&
                                Down(VK_LBUTTON);

        g_ctxCompensating.store(compensate);

        // Mesma ordem de precedencia da condicao acima, para que o rotulo
        // exibido aponte a primeira condicao que realmente falhou.
        Status status;
        if (!g_enabled.load())          status = Status::NoGame;
        else if (!contextOk)            status = Status::NotForeground;
        else if (!g_isActive.load())    status = Status::Inactive;
        else if (menuBlocks)            status = Status::MenuDetected;
        else if (cursorBlocks)          status = Status::CursorVisible;
        else if (keySuppressed)         status = Status::SuppressedByKey;
        else if (compensate)            status = Status::Compensating;
        else                            status = Status::Ready;
        g_ctxStatus.store(status);

        if (compensate) {
            const int slot = ClampSlot(g_currentSlot.load());
            residual += static_cast<double>(g_ratePerSlot[slot].load()) * dt;

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

bool IsActive()    { return g_isActive.load(); }
int  CurrentSlot() { return ClampSlot(g_currentSlot.load()); }

float Rate() { return g_ratePerSlot[ClampSlot(g_currentSlot.load())].load(); }

float RateForSlot(int slot) { return g_ratePerSlot[ClampSlot(slot)].load(); }

void SetRateForSlot(int slot, float rate) {
    g_ratePerSlot[ClampSlot(slot)].store(std::clamp(rate, kRateMin, kRateMax));
}

ContextState Context() {
    ContextState s;
    s.gameForeground  = g_ctxForeground.load();
    s.cursorVisible   = g_ctxCursorVisible.load();
    s.cursorPinned    = g_ctxCursorPinned.load();
    s.suppressedByKey = g_ctxSuppressedByKey.load();
    s.compensating    = g_ctxCompensating.load();
    s.status          = g_ctxStatus.load();
    return s;
}

const char* StatusLabel(Status status) {
    switch (status) {
        case Status::Compensating:    return "COMPENSANDO";
        case Status::Ready:           return "PRONTO";
        case Status::NoGame:          return "SEM JOGO";
        case Status::NotForeground:   return "BLOQUEADO: fora de foco";
        case Status::MenuDetected:    return "BLOQUEADO: menu do jogo";
        case Status::CursorVisible:   return "BLOQUEADO: cursor visivel";
        case Status::SuppressedByKey: return "SUSPENSO: F2";
        case Status::Inactive:        return "DESLIGADO (Ctrl+Shift+S)";
    }
    return "?";
}

void SetEnabled(bool enabled)    { g_enabled.store(enabled); }
void SetGameWindow(HWND hwnd)    { g_gameWindow.store(hwnd); }
void SetOverlayWindow(HWND hwnd) { g_overlayWindow.store(hwnd); }

void SetRequireForeground(bool value)         { g_requireForeground.store(value); }
bool RequireForeground()                      { return g_requireForeground.load(); }
void SetRequireCursorPinned(bool value)       { g_requireCursorPinned.store(value); }
bool RequireCursorPinned()                    { return g_requireCursorPinned.load(); }
void SetSuppressWhenCursorVisible(bool value) { g_suppressWhenCursorVisible.store(value); }
bool SuppressWhenCursorVisible()              { return g_suppressWhenCursorVisible.load(); }

void ApplySettings(const Settings& settings) {
    for (int i = 0; i < kSlotCount; ++i) {
        g_ratePerSlot[i].store(
            std::clamp(settings.ratePerSlot[static_cast<size_t>(i)], kRateMin, kRateMax));
    }
    g_stepFine.store(settings.stepFine);
    g_stepCoarse.store(settings.stepCoarse);
    g_presetF7.store(settings.presetF7);
    g_presetF8.store(settings.presetF8);
    g_requireForeground.store(settings.requireForeground);
    g_requireCursorPinned.store(settings.requireCursorPinned);
    g_suppressWhenCursorVisible.store(settings.suppressWhenCursorVisible);
}

void ReadInto(Settings& settings) {
    for (int i = 0; i < kSlotCount; ++i) {
        settings.ratePerSlot[static_cast<size_t>(i)] = g_ratePerSlot[i].load();
    }
    settings.stepFine                  = g_stepFine.load();
    settings.stepCoarse                = g_stepCoarse.load();
    settings.presetF7                  = g_presetF7.load();
    settings.presetF8                  = g_presetF8.load();
    settings.requireForeground         = g_requireForeground.load();
    settings.requireCursorPinned       = g_requireCursorPinned.load();
    settings.suppressWhenCursorVisible = g_suppressWhenCursorVisible.load();
}

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
