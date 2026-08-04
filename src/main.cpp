#include <windows.h>
#include <cstdio>

#include "imgui.h"

#include "gamewindow.h"
#include "input.h"
#include "overlay.h"

namespace {

constexpr wchar_t kTargetExe[]     = L"bfv.exe";
constexpr wchar_t kTitleFallback[] = L"battlefield";

// Carencia antes de encerrar quando a janela do jogo some. Evita fechar por
// uma oscilacao momentanea (troca de resolucao, alt-tab pesado).
constexpr ULONGLONG kGameGoneGraceMs = 3000;

constexpr ImU32 kAccent = IM_COL32(255, 57, 57, 255);
constexpr ImU32 kShadow = IM_COL32(0, 0, 0, 180);
constexpr float kHudFontSize = 42.0f;

bool KeyEdge(int vk, bool& wasDown) {
    const bool down = (GetAsyncKeyState(vk) & 0x8000) != 0;
    const bool edge = down && !wasDown;
    wasDown = down;
    return edge;
}

void DrawPassiveHud() {
    char label[32];
    if (input::IsActive()) {
        std::snprintf(label, sizeof(label), "%d", input::Rate());
    } else {
        std::snprintf(label, sizeof(label), "NONE");
    }

    ImDrawList* dl   = ImGui::GetForegroundDrawList();
    ImFont*     font = ImGui::GetFont();

    // Sombra deslocada: o numero precisa ser legivel tanto sobre ceu claro
    // quanto sobre sombra, e o jogo controla o que esta atras.
    dl->AddText(font, kHudFontSize, ImVec2(12.0f, 32.0f), kShadow, label);
    dl->AddText(font, kHudFontSize, ImVec2(10.0f, 30.0f), kAccent, label);
}

void DrawInteractivePanel() {
    ImGui::SetNextWindowPos(ImVec2(10.0f, 90.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(320.0f, 0.0f), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Vertical Aim Controller")) {
        ImGui::Text("Estado: %s", input::IsActive() ? "ATIVO" : "inativo");
        ImGui::Text("Taxa:   %d", input::Rate());
        ImGui::Separator();
        ImGui::TextWrapped(
            "Modo interativo. INSERT volta para o modo passivo, em que os "
            "cliques atravessam o overlay.");
        ImGui::Spacing();
        ImGui::TextDisabled("Controles editaveis chegam na fase 4.");
    }
    ImGui::End();
}

}  // namespace

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int) {
    // Sem isto o Windows escala as coordenadas da janela quando ha monitor com
    // DPI diferente de 100%, e o overlay fica deslocado do jogo.
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    Overlay overlay;
    if (!overlay.Init(hInstance)) {
        MessageBoxW(nullptr,
                    L"Falha ao inicializar o overlay (D3D11 / DirectComposition).",
                    L"vertical-aim-controller", MB_ICONERROR | MB_OK);
        return 1;
    }

    GameWindowTracker tracker(kTargetExe, kTitleFallback);

    bool      insertWasDown = false;
    bool      running       = true;
    ULONGLONG gameGoneSince = 0;

    while (running) {
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                running = false;
                break;
            }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        if (!running) break;

        tracker.Update();

        if (KeyEdge(VK_INSERT, insertWasDown)) {
            overlay.SetInteractive(!overlay.IsInteractive());
        }

        if (!tracker.Valid()) {
            overlay.Show(false);

            // Encerra so se o jogo chegou a ser visto e depois sumiu; se ainda
            // nao abriu, o programa espera.
            if (tracker.EverFound()) {
                const ULONGLONG now = GetTickCount64();
                if (gameGoneSince == 0) {
                    gameGoneSince = now;
                } else if (now - gameGoneSince > kGameGoneGraceMs) {
                    break;
                }
            }

            // Sem jogo nao ha o que compensar, e sem janela visivel o Present
            // nao serve de limitador -- dorme para nao girar em vazio.
            Sleep(16);
            continue;
        }

        gameGoneSince = 0;
        overlay.SetGeometry(tracker.ClientRectInScreen());
        overlay.Show(true);

        input::Tick();

        if (!overlay.BeginFrame()) break;
        DrawPassiveHud();
        if (overlay.IsInteractive()) DrawInteractivePanel();
        overlay.EndFrame();
    }

    overlay.Shutdown();
    return 0;
}
