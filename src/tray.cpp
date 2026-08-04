#include "tray.h"

#include <shellapi.h>

namespace tray {
namespace {

constexpr wchar_t kClassName[] = L"VerticalAimControllerTray";
constexpr UINT    kIconId      = 1;
constexpr UINT    WM_TRAY      = WM_APP + 1;

HWND            g_hwnd = nullptr;
HINSTANCE       g_inst = nullptr;
NOTIFYICONDATAW g_nid  = {};
Command         g_pending = Command::None;
std::wstring    g_tooltip;

// Nao ha menu de contexto aqui, e a ausencia e deliberada.
//
// TrackPopupMenu entra num laco modal que CAPTURA O MOUSE. A janela dona
// precisa ser uma janela que possa ir a primeiro plano de verdade; a daqui e
// WS_POPUP de tamanho zero e nunca exibida, entao SetForegroundWindow nao
// surtia efeito, o menu nao recebia o input que o fecharia, e o laco ficava
// presa segurando a captura.
//
// Captura de mouse e global: enquanto uma janela a detem, NENHUMA janela do
// sistema recebe clique. O sintoma era o desktop inteiro parar de responder ao
// mouse ate o processo ser morto -- observado, com a captura registrada nesta
// janela da bandeja.
//
// Duas acoes distintas por botao cobrem o mesmo que o menu cobria, sem laco
// modal e sem captura.
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_TRAY) {
        switch (LOWORD(lParam)) {
            case WM_LBUTTONUP:
            case WM_LBUTTONDBLCLK:
                g_pending = Command::TogglePanel;
                break;
            case WM_RBUTTONUP:
                g_pending = Command::Quit;
                break;
            default:
                break;
        }
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

}  // namespace

bool Init(HINSTANCE hInstance) {
    g_inst = hInstance;

    WNDCLASSEXW wc = {};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = &WndProc;
    wc.hInstance     = hInstance;
    wc.lpszClassName = kClassName;
    RegisterClassExW(&wc);

    // Janela oculta, apenas para receber as mensagens do icone. Nao precisa ir
    // a primeiro plano nem exibir nada: era o menu de contexto que exigia isso,
    // e ele foi removido.
    g_hwnd = CreateWindowExW(0, kClassName, kClassName, WS_POPUP,
                             0, 0, 0, 0, nullptr, nullptr, hInstance, nullptr);
    if (g_hwnd == nullptr) return false;

    g_nid.cbSize           = sizeof(g_nid);
    g_nid.hWnd             = g_hwnd;
    g_nid.uID              = kIconId;
    g_nid.uFlags           = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAY;
    // Mesmo icone embutido pelo app.rc.
    g_nid.hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(1));
    if (g_nid.hIcon == nullptr) {
        g_nid.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    }
    wcscpy_s(g_nid.szTip, L"Vertical Aim Controller\nClique: painel | Direito: sair");

    return Shell_NotifyIconW(NIM_ADD, &g_nid) != FALSE;
}

void Shutdown() {
    if (g_hwnd == nullptr) return;

    Shell_NotifyIconW(NIM_DELETE, &g_nid);
    DestroyWindow(g_hwnd);
    g_hwnd = nullptr;
    UnregisterClassW(kClassName, g_inst);
}

void SetTooltip(const std::wstring& text) {
    if (g_hwnd == nullptr || text == g_tooltip) return;
    g_tooltip = text;

    g_nid.uFlags = NIF_TIP;
    wcsncpy_s(g_nid.szTip, text.c_str(), _TRUNCATE);
    Shell_NotifyIconW(NIM_MODIFY, &g_nid);
}

void Notify(const std::wstring& title, const std::wstring& message) {
    if (g_hwnd == nullptr) return;

    g_nid.uFlags      = NIF_INFO;
    g_nid.dwInfoFlags = NIIF_INFO;
    wcsncpy_s(g_nid.szInfoTitle, title.c_str(), _TRUNCATE);
    wcsncpy_s(g_nid.szInfo, message.c_str(), _TRUNCATE);
    Shell_NotifyIconW(NIM_MODIFY, &g_nid);
}

Command PollCommand() {
    const Command cmd = g_pending;
    g_pending = Command::None;
    return cmd;
}

}  // namespace tray
