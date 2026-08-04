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
        std::snprintf(label, sizeof(label), "%.0f", input::Rate());
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
        ImGui::Text("Slot:   %d", input::CurrentSlot() + 1);
        ImGui::Text("Estado: %s", input::IsActive() ? "ATIVO" : "inativo");
        ImGui::Text("Taxa:   %.0f px/s", input::Rate());

        ImGui::SeparatorText("Contexto");

        const input::ContextState ctx = input::Context();
        auto flag = [](const char* name, bool value) {
            ImGui::TextColored(value ? ImVec4(0.35f, 0.85f, 0.35f, 1.0f)
                                     : ImVec4(0.55f, 0.55f, 0.55f, 1.0f),
                               "%s %s", value ? "[x]" : "[ ]", name);
        };
        flag("jogo em primeiro plano", ctx.gameForeground);
        flag("cursor do sistema visivel", ctx.cursorVisible);
        flag("suspenso por HOME/F2", ctx.suppressedByKey);
        flag("compensando agora", ctx.compensating);

        ImGui::SeparatorText("Deteccao");

        bool requireFg = input::RequireForeground();
        if (ImGui::Checkbox("Exigir jogo em primeiro plano", &requireFg)) {
            input::SetRequireForeground(requireFg);
        }

        bool suppressCursor = input::SuppressWhenCursorVisible();
        if (ImGui::Checkbox("Suspender com cursor visivel", &suppressCursor)) {
            input::SetSuppressWhenCursorVisible(suppressCursor);
        }
        ImGui::TextDisabled(
            "Abra um menu do jogo e veja se 'cursor do sistema visivel'\n"
            "acende. Se nao acender, o BF5 desenha cursor proprio e esta\n"
            "opcao nao serve -- desligue e use HOME/F2.");

        ImGui::Separator();
        ImGui::TextWrapped(
            "INSERT volta para o modo passivo, em que os cliques atravessam "
            "o overlay.");
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

    // A compensacao roda em thread propria: precisa de cadencia estavel, e o
    // laco de render e ritmado pelo vsync do monitor.
    input::SetOverlayWindow(overlay.Hwnd());
    input::Start();

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
        input::SetGameWindow(tracker.Hwnd());
        input::SetEnabled(tracker.Valid());

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

            // Sem janela visivel o Present nao serve de limitador -- dorme
            // para nao girar em vazio.
            Sleep(16);
            continue;
        }

        gameGoneSince = 0;
        overlay.SetGeometry(tracker.ClientRectInScreen());
        overlay.Show(true);

        if (!overlay.BeginFrame()) break;
        DrawPassiveHud();
        if (overlay.IsInteractive()) DrawInteractivePanel();
        overlay.EndFrame();
    }

    input::Stop();
    overlay.Shutdown();
    return 0;
}
