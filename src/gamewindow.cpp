#include "gamewindow.h"

#include <tlhelp32.h>
#include <algorithm>
#include <cwctype>
#include <unordered_set>
#include <vector>

namespace {

std::wstring ToLower(std::wstring s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
    return s;
}

// PIDs de todos os processos cujo executavel bate com exeNameLower.
// Uma unica varredura, sem abrir handle para processo algum.
std::unordered_set<DWORD> PidsForExe(const std::wstring& exeNameLower) {
    std::unordered_set<DWORD> pids;

    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return pids;

    PROCESSENTRY32W entry = {};
    entry.dwSize = sizeof(entry);

    if (Process32FirstW(snap, &entry)) {
        do {
            if (ToLower(entry.szExeFile) == exeNameLower) {
                pids.insert(entry.th32ProcessID);
            }
        } while (Process32NextW(snap, &entry));
    }

    CloseHandle(snap);
    return pids;
}

struct EnumContext {
    const std::unordered_set<DWORD>* pids;
    const std::wstring*              titleFallbackLower;
    HWND                             found;
};

bool IsPlausibleGameWindow(HWND hwnd) {
    if (!IsWindowVisible(hwnd)) return false;
    if (GetWindow(hwnd, GW_OWNER) != nullptr) return false;  // descarta dialogos

    RECT r;
    if (!GetClientRect(hwnd, &r)) return false;
    // Janelas auxiliares invisiveis costumam ter area nula ou minuscula.
    return (r.right - r.left) > 100 && (r.bottom - r.top) > 100;
}

BOOL CALLBACK EnumProc(HWND hwnd, LPARAM lparam) {
    auto* ctx = reinterpret_cast<EnumContext*>(lparam);

    if (!IsPlausibleGameWindow(hwnd)) return TRUE;

    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);

    bool match = ctx->pids->count(pid) > 0;

    // Fallback por titulo, usado quando o executavel nao foi localizado.
    if (!match && ctx->pids->empty() && !ctx->titleFallbackLower->empty()) {
        wchar_t title[512] = {};
        if (GetWindowTextW(hwnd, title, 511) > 0) {
            match = ToLower(title).find(*ctx->titleFallbackLower) != std::wstring::npos;
        }
    }

    if (match) {
        ctx->found = hwnd;
        return FALSE;  // para a enumeracao
    }
    return TRUE;
}

}  // namespace

GameWindowTracker::GameWindowTracker(std::wstring exeName, std::wstring titleFallback)
    : exeName_(ToLower(std::move(exeName))),
      titleFallback_(ToLower(std::move(titleFallback))) {}

void GameWindowTracker::Rescan() {
    const std::unordered_set<DWORD> pids = PidsForExe(exeName_);

    EnumContext ctx{&pids, &titleFallback_, nullptr};
    EnumWindows(EnumProc, reinterpret_cast<LPARAM>(&ctx));

    hwnd_ = ctx.found;
    if (hwnd_ != nullptr) everFound_ = true;
}

bool GameWindowTracker::RefreshGeometry() {
    RECT client;
    if (!GetClientRect(hwnd_, &client)) return false;

    // GetClientRect devolve coordenadas relativas ao cliente; converte para tela
    // mapeando os dois cantos.
    POINT topLeft     = {client.left, client.top};
    POINT bottomRight = {client.right, client.bottom};
    if (!ClientToScreen(hwnd_, &topLeft))     return false;
    if (!ClientToScreen(hwnd_, &bottomRight)) return false;

    clientRect_ = {topLeft.x, topLeft.y, bottomRight.x, bottomRight.y};
    return true;
}

void GameWindowTracker::Update() {
    // Caminho rapido: a janela conhecida ainda existe, so atualiza a geometria.
    if (hwnd_ != nullptr && IsWindow(hwnd_)) {
        if (RefreshGeometry()) return;
    }

    hwnd_       = nullptr;
    clientRect_ = {0, 0, 0, 0};

    const ULONGLONG now = GetTickCount64();
    if (now - lastScanMs_ < kRescanIntervalMs) return;
    lastScanMs_ = now;

    Rescan();
    if (hwnd_ != nullptr && !RefreshGeometry()) {
        hwnd_ = nullptr;
    }
}
