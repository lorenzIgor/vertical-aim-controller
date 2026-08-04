#include <windows.h>
#include <cfloat>
#include <cstdio>
#include <cwchar>
#include <string>

#include "imgui.h"

#include "config.h"
#include "gamewindow.h"
#include "input.h"
#include "overlay.h"
#include "tray.h"

namespace {

constexpr wchar_t kInstanceMutex[] = L"Local\\VerticalAimController.SingleInstance";

// Carencia antes de encerrar quando a janela do jogo some. Evita fechar por
// uma oscilacao momentanea (troca de resolucao, alt-tab pesado).
constexpr ULONGLONG kGameGoneGraceMs = 3000;

// Gravacao com atraso: ajustar a taxa com F9-F12 dispara varias mudancas em
// sequencia, e nao ha motivo para escrever em disco a cada uma.
constexpr ULONGLONG kSaveDebounceMs = 2000;


// Somente para o rotulo do HUD; o valor real vive em wWinMain.
bool g_inputDisabled = false;

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

std::wstring BuildTooltip(bool gameFound, bool hudVisible) {
    if (!hudVisible) {
        return L"Vertical Aim Controller\nHUD oculto (HOME mostra)";
    }
    if (!gameFound) {
        return L"Vertical Aim Controller\nAguardando o Battlefield V";
    }

    wchar_t buffer[128];
    if (input::IsActive()) {
        std::swprintf(buffer, 128, L"Vertical Aim Controller\nSlot %d: %.0f px/s",
                      input::CurrentSlot() + 1, input::Rate());
    } else {
        std::swprintf(buffer, 128, L"Vertical Aim Controller\nSlot %d: desligado",
                      input::CurrentSlot() + 1);
    }
    return buffer;
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

    // Motivo pelo qual a compensacao esta parada.
    //
    // Sem esta linha, bloqueado e quebrado tem exatamente a mesma aparencia na
    // tela, e a unica saida e adivinhar qual das seis condicoes falhou.
    const input::Status status = input::Context().status;

    ImU32 statusColor = kMuted;
    switch (status) {
        case input::Status::Compensating: statusColor = IM_COL32( 80, 230,  90, 235); break;
        case input::Status::Ready:        statusColor = IM_COL32(200, 200, 200, 210); break;
        default:                          statusColor = IM_COL32(255, 190,  60, 240); break;
    }

    const float y = 34.0f + fontSize;
    dl->AddText(font, labelSize, ImVec2(12.0f, y + 2.0f), kShadow, input::StatusLabel(status));
    dl->AddText(font, labelSize, ImVec2(10.0f, y), statusColor, input::StatusLabel(status));

    if (g_inputDisabled) {
        const float y2 = y + labelSize + 4.0f;
        dl->AddText(font, labelSize, ImVec2(12.0f, y2 + 2.0f), kShadow, "--no-input");
        dl->AddText(font, labelSize, ImVec2(10.0f, y2), IM_COL32(120, 190, 255, 235), "--no-input");
    }
}

// Aviso de que o painel esta aberto.
//
// Nao ha moldura de tela cheia: fora do retangulo do painel os cliques
// continuam indo para o jogo, entao marcar a tela toda daria a impressao
// errada de que tudo esta capturado.
void DrawInteractiveFrame() {
    ImDrawList*  dl   = ImGui::GetForegroundDrawList();
    ImFont*      font = ImGui::GetFont();
    const ImVec2 size = ImGui::GetIO().DisplaySize;

    const ImU32 warn = IM_COL32(255, 190, 60, 230);

    const char* text = "PAINEL ABERTO - so o painel captura o mouse - INSERT ou ESC para fechar";
    const ImVec2 extent = font->CalcTextSizeA(20.0f, FLT_MAX, 0.0f, text);
    const ImVec2 pos(size.x * 0.5f - extent.x * 0.5f, 12.0f);

    dl->AddRectFilled(ImVec2(pos.x - 12.0f, pos.y - 6.0f),
                      ImVec2(pos.x + extent.x + 12.0f, pos.y + extent.y + 6.0f),
                      IM_COL32(0, 0, 0, 170), 4.0f);
    dl->AddText(font, 20.0f, pos, warn, text);
}

// Devolve true se algo foi alterado pelo usuario nesta passagem.
bool DrawInteractivePanel(Settings& settings, bool& quitRequested) {
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
        flag("cursor preso no centro (gameplay)", ctx.cursorPinned);
        flag("cursor do sistema visivel", ctx.cursorVisible);
        flag("suspenso por HOME/F2", ctx.suppressedByKey);
        flag("compensando agora", ctx.compensating);
        ImGui::TextDisabled("Estado: %s", input::StatusLabel(ctx.status));

        ImGui::SeparatorText("Deteccao");

        bool requireFg = input::RequireForeground();
        if (ImGui::Checkbox("Exigir jogo em primeiro plano", &requireFg)) {
            input::SetRequireForeground(requireFg);
            changed = true;
        }

        bool requirePinned = input::RequireCursorPinned();
        if (ImGui::Checkbox("Compensar so com o cursor preso no centro", &requirePinned)) {
            input::SetRequireCursorPinned(requirePinned);
            changed = true;
        }
        ImGui::TextDisabled(
            "O BF5 prende o cursor no centro durante o gameplay e solta no\n"
            "menu. E o que impede a compensacao de arrastar o ponteiro\n"
            "enquanto voce clica na interface do jogo.");

        bool suppressCursor = input::SuppressWhenCursorVisible();
        if (ImGui::Checkbox("Suspender com cursor visivel (obsoleta)", &suppressCursor)) {
            input::SetSuppressWhenCursorVisible(suppressCursor);
            changed = true;
        }
        ImGui::TextDisabled(
            "Nao serve para o BF5, que mantem o cursor visivel tambem\n"
            "durante o gameplay. Ligar isto bloqueia tudo.");

        ImGui::SeparatorText("Aparencia");
        if (ImGui::SliderFloat("Tamanho do HUD", &settings.hudFontSize, 16.0f, 96.0f, "%.0f")) {
            changed = true;
        }
        if (ImGui::SliderFloat("Escala do painel", &settings.uiScale, 1.0f, 4.0f, "%.1fx")) {
            changed = true;
        }

        ImGui::Separator();
        ImGui::TextWrapped(
            "INSERT ou ESC fecha o painel. Fora do painel os cliques continuam "
            "indo para o jogo. As alteracoes vao para vac.ini sozinhas.");
        ImGui::Spacing();
        if (ImGui::Button("Sair do programa")) {
            quitRequested = true;
        }
    }
    ImGui::End();

    return changed;
}

}  // namespace

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR lpCmdLine, int) {
    // --no-input sobe o overlay sem a thread de compensacao, portanto sem
    // emitir SendInput algum. Serve para separar defeitos de janela de
    // defeitos de input sem precisar de dois binarios.
    const bool inputDisabled =
        (lpCmdLine != nullptr && wcsstr(lpCmdLine, L"--no-input") != nullptr);

    // Instancia unica.
    //
    // Duas copias rodando enviariam SendInput em paralelo e a compensacao
    // sairia dobrada -- um erro que se manifesta como calibracao errada, sem
    // pista alguma da causa real. E facil chegar nisso: como o programa fica
    // invisivel ate o jogo abrir, clicar no atalho de novo e a reacao natural.
    HANDLE instanceLock = CreateMutexW(nullptr, TRUE, kInstanceMutex);
    if (instanceLock == nullptr || GetLastError() == ERROR_ALREADY_EXISTS) {
        MessageBoxW(nullptr,
                    L"O Vertical Aim Controller ja esta em execucao.\n\n"
                    L"O icone fica na bandeja do sistema, ao lado do relogio "
                    L"(pode estar escondido na setinha). Clique nele com o "
                    L"botao direito para abrir o painel ou sair.",
                    L"Vertical Aim Controller", MB_ICONINFORMATION | MB_OK);
        if (instanceLock != nullptr) CloseHandle(instanceLock);
        return 0;
    }

    // Sem isto o Windows escala as coordenadas da janela quando ha monitor com
    // DPI diferente de 100%, e o overlay fica deslocado do jogo.
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    g_inputDisabled = inputDisabled;

    Settings settings = config::Load();
    Settings saved    = settings;

    Overlay overlay;
    if (!overlay.Init(hInstance)) {
        MessageBoxW(nullptr,
                    L"Falha ao inicializar o overlay (D3D11 / DirectComposition).",
                    L"vertical-aim-controller", MB_ICONERROR | MB_OK);
        CloseHandle(instanceLock);
        return 1;
    }

    // Unico sinal de que o programa esta vivo enquanto o jogo nao abre, e
    // unica forma de encerrar pela interface: o overlay fica oculto e
    // WS_EX_TOOLWINDOW o mantem fora da barra de tarefas e do Alt-Tab.
    tray::Init(hInstance);
    tray::Notify(L"Vertical Aim Controller",
                 L"Rodando. Clique no icone para abrir o painel, "
                 L"botao direito para sair.");

    GameWindowTracker tracker(config::Widen(settings.targetExe),
                              config::Widen(settings.titleFallback));

    // A compensacao roda em thread propria: precisa de cadencia estavel, e o
    // laco de render e ritmado pelo vsync do monitor.
    input::ApplySettings(settings);
    input::SetOverlayWindow(overlay.Hwnd());
    overlay.ApplyUiScale(settings.uiScale);
    if (!inputDisabled) input::Start();

    bool      insertWasDown = false;
    bool      escapeWasDown = false;
    bool      homeWasDown   = false;
    bool      hudVisible    = true;
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
        input::SetEnabled(tracker.Valid() && !inputDisabled);

        // Reconstroi o atlas de fontes fora do par BeginFrame/EndFrame.
        if (settings.uiScale != overlay.UiScale()) {
            overlay.ApplyUiScale(settings.uiScale);
        }

        switch (tray::PollCommand()) {
            case tray::Command::TogglePanel:
                overlay.SetInteractive(!overlay.IsInteractive());
                break;
            case tray::Command::Quit:
                running = false;
                break;
            case tray::Command::None:
                break;
        }
        if (!running) break;

        const ULONGLONG now = GetTickCount64();

        // Rede de seguranca contra captura de mouse presa.
        //
        // Enquanto uma janela detem a captura, NENHUMA janela do sistema recebe
        // clique -- o desktop inteiro para de responder ao mouse ate o processo
        // morrer. Nada neste programa tem motivo legitimo para capturar o
        // mouse, entao qualquer captura pendente nesta thread e defeito.
        //
        // Fica no laco principal, e nao no render: com o jogo fechado o render
        // nao roda, e o defeito que originou esta guarda estava na janela da
        // bandeja, que funciona independente do jogo.
        if (GetCapture() != nullptr) ReleaseCapture();

        if (KeyEdge(VK_INSERT, insertWasDown)) {
            overlay.SetInteractive(!overlay.IsInteractive());
        }

        // HOME esconde e mostra o HUD.
        //
        // Com o HUD escondido a janela do overlay e ocultada por inteiro, e nao
        // apenas apagada: assim ela sai completamente do caminho, sem depender
        // de click-through nem de qualquer outra propriedade da janela.
        if (KeyEdge(VK_HOME, homeWasDown)) {
            hudVisible = !hudVisible;
            if (!hudVisible) overlay.SetInteractive(false);
        }

        // Segunda saida do modo interativo. Nele o overlay cobre a tela inteira
        // e o icone da bandeja fica inacessivel, entao depender de uma unica
        // tecla para sair e arriscado demais.
        if (KeyEdge(VK_ESCAPE, escapeWasDown) && overlay.IsInteractive()) {
            overlay.SetInteractive(false);
        }

        tray::SetTooltip(BuildTooltip(tracker.Valid(), hudVisible));

        // Gravacao com atraso, comparando o estado vivo com o ultimo gravado.
        // Pega tanto mudancas do painel quanto das hotkeys.
        Settings live = settings;
        input::ReadInto(live);
        if (!TunablesEqual(live, saved) || live.hudFontSize != saved.hudFontSize ||
            live.uiScale != saved.uiScale) {
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

        // O overlay so aparece com o jogo EM FOCO, nao apenas aberto.
        //
        // A janela e TOPMOST e cobre toda a area do jogo. Verificando apenas a
        // existencia do jogo, alternar para outro aplicativo deixava o HUD
        // desenhado por cima dele -- uma camada vermelha sobre a tela inteira,
        // que nao passa de ruido fora do jogo e faz o overlay parecer travado.
        if (!hudVisible || GetForegroundWindow() != tracker.Hwnd()) {
            overlay.Show(false);
            Sleep(16);
            continue;
        }

        overlay.SetGeometry(tracker.ClientRectInScreen());
        overlay.Show(true);

        if (!overlay.BeginFrame()) break;
        DrawPassiveHud(settings.hudFontSize);
        if (overlay.IsInteractive()) {
            DrawInteractiveFrame();
            bool quitFromPanel = false;
            DrawInteractivePanel(settings, quitFromPanel);
            if (quitFromPanel) running = false;
        }
        overlay.EndFrame();
    }

    input::Stop();
    input::ReadInto(settings);
    config::Save(settings);

    tray::Shutdown();
    overlay.Shutdown();
    CloseHandle(instanceLock);
    return 0;
}
