#include <windows.h>
#include <cstdio>

#include "imgui.h"

#include "config.h"
#include "gamewindow.h"
#include "input.h"
#include "overlay.h"

namespace {

// Carencia antes de encerrar quando a janela do jogo some. Evita fechar por
// uma oscilacao momentanea (troca de resolucao, alt-tab pesado).
constexpr ULONGLONG kGameGoneGraceMs = 3000;

// Gravacao com atraso: ajustar a taxa com F9-F12 dispara varias mudancas em
// sequencia, e nao ha motivo para escrever em disco a cada uma.
constexpr ULONGLONG kSaveDebounceMs = 2000;

constexpr ImU32 kAccent = IM_COL32(255, 57, 57, 255);
constexpr ImU32 kShadow = IM_COL32(0, 0, 0, 180);
constexpr ImU32 kMuted  = IM_COL32(210, 210, 210, 200);

bool KeyEdge(int vk, bool& wasDown) {
    const bool down = (GetAsyncKeyState(vk) & 0x8000) != 0;
    const bool edge = down && !wasDown;
    wasDown = down;
    return edge;
}

// Compara so o que muda em tempo de execucao. Serve para decidir se ha algo
// novo a gravar, inclusive mudancas feitas pelas hotkeys -- que acontecem na
// thread de input e nao passam pelo painel.
bool TunablesEqual(const Settings& a, const Settings& b) {
    for (size_t i = 0; i < a.ratePerSlot.size(); ++i) {
        if (a.ratePerSlot[i] != b.ratePerSlot[i]) return false;
    }
    return a.stepFine == b.stepFine && a.stepCoarse == b.stepCoarse &&
           a.presetF7 == b.presetF7 && a.presetF8 == b.presetF8 &&
           a.requireForeground == b.requireForeground &&
           a.suppressWhenCursorVisible == b.suppressWhenCursorVisible;
}

void DrawPassiveHud(float fontSize) {
    ImDrawList* dl   = ImGui::GetForegroundDrawList();
    ImFont*     font = ImGui::GetFont();

    char slotLabel[24];
    std::snprintf(slotLabel, sizeof(slotLabel), "SLOT %d", input::CurrentSlot() + 1);

    char rateLabel[32];
    if (input::IsActive()) {
        std::snprintf(rateLabel, sizeof(rateLabel), "%.0f", input::Rate());
    } else {
        std::snprintf(rateLabel, sizeof(rateLabel), "NONE");
    }

    // Nao chamar esta variavel de "small": rpcndr.h, puxado por windows.h,
    // define small como macro para char.
    const float labelSize = fontSize * 0.38f;

    // Sombra deslocada: o numero precisa ser legivel tanto sobre ceu claro
    // quanto sobre sombra, e o jogo controla o que esta atras.
    dl->AddText(font, labelSize, ImVec2(12.0f, 14.0f), kShadow, slotLabel);
    dl->AddText(font, labelSize, ImVec2(10.0f, 12.0f), kMuted,  slotLabel);

    dl->AddText(font, fontSize, ImVec2(12.0f, 32.0f), kShadow, rateLabel);
    dl->AddText(font, fontSize, ImVec2(10.0f, 30.0f), kAccent, rateLabel);
}

// Devolve true se algo foi alterado pelo usuario nesta passagem.
bool DrawInteractivePanel(Settings& settings) {
    bool changed = false;

    ImGui::SetNextWindowPos(ImVec2(10.0f, 110.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(380.0f, 0.0f), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Vertical Aim Controller")) {
        const int current = input::CurrentSlot();

        ImGui::Text("Slot ativo: %d", current + 1);
        ImGui::Text("Compensacao: %s", input::IsActive() ? "LIGADA" : "desligada");

        ImGui::SeparatorText("Taxa por slot (px/s)");

        for (int slot = 0; slot < input::kSlotCount; ++slot) {
            float rate = input::RateForSlot(slot);

            char label[32];
            std::snprintf(label, sizeof(label), "Slot %d%s", slot + 1,
                          slot == current ? " *" : "");

            ImGui::PushID(slot);
            if (ImGui::SliderFloat(label, &rate, input::kRateMin, 1500.0f, "%.0f")) {
                input::SetRateForSlot(slot, rate);
                changed = true;
            }
            ImGui::PopID();
        }
        ImGui::TextDisabled("Atire numa parede e ajuste ate o rastro ficar reto.");

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
            changed = true;
        }

        bool suppressCursor = input::SuppressWhenCursorVisible();
        if (ImGui::Checkbox("Suspender com cursor visivel", &suppressCursor)) {
            input::SetSuppressWhenCursorVisible(suppressCursor);
            changed = true;
        }
        ImGui::TextDisabled(
            "Abra um menu do jogo e veja se o indicador de cursor visivel\n"
            "acende. Se nao acender, o BF5 desenha cursor proprio e esta\n"
            "opcao nao serve -- desligue e use HOME/F2.");

        ImGui::SeparatorText("Aparencia");
        if (ImGui::SliderFloat("Tamanho do HUD", &settings.hudFontSize, 16.0f, 96.0f, "%.0f")) {
            changed = true;
        }

        ImGui::Separator();
        ImGui::TextWrapped(
            "INSERT volta ao modo passivo, em que os cliques atravessam o "
            "overlay. As alteracoes sao gravadas em vac.ini automaticamente.");
    }
    ImGui::End();

    return changed;
}

}  // namespace

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int) {
    // Sem isto o Windows escala as coordenadas da janela quando ha monitor com
    // DPI diferente de 100%, e o overlay fica deslocado do jogo.
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    Settings settings = config::Load();
    Settings saved    = settings;

    Overlay overlay;
    if (!overlay.Init(hInstance)) {
        MessageBoxW(nullptr,
                    L"Falha ao inicializar o overlay (D3D11 / DirectComposition).",
                    L"vertical-aim-controller", MB_ICONERROR | MB_OK);
        return 1;
    }

    GameWindowTracker tracker(config::Widen(settings.targetExe),
                              config::Widen(settings.titleFallback));

    // A compensacao roda em thread propria: precisa de cadencia estavel, e o
    // laco de render e ritmado pelo vsync do monitor.
    input::ApplySettings(settings);
    input::SetOverlayWindow(overlay.Hwnd());
    input::Start();

    bool      insertWasDown = false;
    bool      running       = true;
    ULONGLONG gameGoneSince = 0;
    ULONGLONG dirtySince    = 0;

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

        // Gravacao com atraso, comparando o estado vivo com o ultimo gravado.
        // Pega tanto mudancas do painel quanto das hotkeys.
        const ULONGLONG now = GetTickCount64();
        Settings live = settings;
        input::ReadInto(live);
        if (!TunablesEqual(live, saved) || live.hudFontSize != saved.hudFontSize) {
            if (dirtySince == 0) dirtySince = now;
            if (now - dirtySince > kSaveDebounceMs) {
                settings = live;
                if (config::Save(settings)) saved = settings;
                dirtySince = 0;
            }
        } else {
            dirtySince = 0;
        }

        if (!tracker.Valid()) {
            overlay.Show(false);

            // Encerra so se o jogo chegou a ser visto e depois sumiu; se ainda
            // nao abriu, o programa espera.
            if (tracker.EverFound()) {
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
        DrawPassiveHud(settings.hudFontSize);
        if (overlay.IsInteractive()) DrawInteractivePanel(settings);
        overlay.EndFrame();
    }

    input::Stop();
    input::ReadInto(settings);
    config::Save(settings);

    overlay.Shutdown();
    return 0;
}
