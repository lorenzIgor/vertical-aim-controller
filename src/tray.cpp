#include "tray.h"

#include <shellapi.h>

namespace tray {
namespace {

constexpr wchar_t kClassName[] = L"VerticalAimControllerTray";
constexpr UINT    kIconId      = 1;
constexpr UINT    WM_TRAY      = WM_APP + 1;

constexpr UINT kMenuTogglePanel = 100;
constexpr UINT kMenuQuit        = 101;

HWND         g_hwnd = nullptr;
HINSTANCE    g_inst = nullptr;
NOTIFYICONDATAW g_nid = {};
Command      g_pending = Command::None;
std::wstring g_tooltip;

void ShowContextMenu() {
    POINT pt;
    GetCursorPos(&pt);

    HMENU menu = CreatePopupMenu();
    if (menu == nullptr) return;

    AppendMenuW(menu, MF_STRING, kMenuTogglePanel, L"Abrir/fechar painel\tINSERT");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, kMenuQuit, L"Sair");

    // Sem trazer a janela para frente o menu nao fecha ao clicar fora --
    // comportamento documentado do TrackPopupMenu.
    SetForegroundWindow(g_hwnd);
    TrackPopupMenu(menu, TPM_RIGHTBUTTON | TPM_BOTTOMALIGN, pt.x, pt.y, 0, g_hwnd, nullptr);
    PostMessageW(g_hwnd, WM_NULL, 0, 0);

    DestroyMenu(menu);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_TRAY:
            switch (LOWORD(lParam)) {
                case WM_LBUTTONUP:
                case WM_LBUTTONDBLCLK:
                    g_pending = Command::TogglePanel;
                    return 0;
                case WM_RBUTTONUP:
                case WM_CONTEXTMENU:
                    ShowContextMenu();
                    return 0;
                default:
                    break;
            }
            return 0;

        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case kMenuTogglePanel: g_pending = Command::TogglePanel; return 0;
                case kMenuQuit:        g_pending = Command::Quit;        return 0;
                default: break;
            }
            break;

        default:
            break;
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

    // Janela oculta comum, e nao message-only: TrackPopupMenu precisa de uma
    // janela que possa ir para primeiro plano, e HWND_MESSAGE nao pode.
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
    wcscpy_s(g_nid.szTip, L"Vertical Aim Controller");

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
