#include "input.h"

#include <windows.h>

namespace input {
namespace {

constexpr int kF7WeaponRate = 121;
constexpr int kF8WeaponRate = 301;

int g_x    = 0;
int g_y    = 0;
int g_rate = kF7WeaponRate;
int g_acc  = 1;

bool g_isActive = false;

bool g_f12Down = false, g_canTriggerF12 = true;
bool g_f11Down = false, g_canTriggerF11 = true;
bool g_f10Down = false, g_canTriggerF10 = true;
bool g_f9Down  = false, g_canTriggerF9  = true;
bool g_f8Down  = false, g_canTriggerF8  = true;
bool g_f7Down  = false, g_canTriggerF7  = true;

bool g_canToggleActive    = true;
bool g_canToggleExtraGuns = true;
bool g_canToggleMainGun   = true;

bool g_prevActiveState        = false;
bool g_canTogglePrevActive    = true;

}  // namespace

bool IsActive() { return g_isActive; }
int  Rate()     { return g_rate; }

void Tick() {
    const bool ctrlPressed  = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
    const bool shiftPressed = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
    const bool sPressed     = (GetAsyncKeyState('S') & 0x8000) != 0;
    const bool sMainGun     = (GetAsyncKeyState('1') & 0x8000) != 0;
    const bool sExtraGuns   = ((GetAsyncKeyState('2') & 0x8000) != 0) ||
                              ((GetAsyncKeyState('3') & 0x8000) != 0) ||
                              ((GetAsyncKeyState('4') & 0x8000) != 0);

    if (ctrlPressed && shiftPressed && sPressed) {
        if (g_canToggleActive) {
            g_isActive = !g_isActive;
            g_canToggleActive = false;
        }
    } else {
        g_canToggleActive = true;
    }

    if (sExtraGuns) {
        if (g_canToggleExtraGuns) {
            if (g_canTogglePrevActive) {
                g_prevActiveState = g_isActive;
                g_canTogglePrevActive = false;
            }
            g_isActive = false;
            g_canToggleExtraGuns = false;
        }
    } else {
        g_canToggleExtraGuns = true;
    }

    if (sMainGun) {
        if (g_canToggleMainGun) {
            g_isActive = g_prevActiveState;
            g_canTogglePrevActive = true;
            g_canToggleMainGun = false;
        }
    } else {
        g_canToggleMainGun = true;
    }

    if (!g_isActive) return;

    g_f12Down = (GetKeyState(VK_F12) & 0x8000) != 0;
    g_f11Down = (GetKeyState(VK_F11) & 0x8000) != 0;
    g_f10Down = (GetKeyState(VK_F10) & 0x8000) != 0;
    g_f9Down  = (GetKeyState(VK_F9)  & 0x8000) != 0;
    g_f8Down  = (GetKeyState(VK_F8)  & 0x8000) != 0;
    g_f7Down  = (GetKeyState(VK_F7)  & 0x8000) != 0;

    if (g_f12Down && g_canTriggerF12) {
        g_rate += g_acc;
        g_canTriggerF12 = false;
    } else if (!g_f12Down && !g_canTriggerF12) {
        g_canTriggerF12 = true;
    }

    if (g_f11Down && g_canTriggerF11) {
        g_rate -= g_acc;
        g_canTriggerF11 = false;
    } else if (!g_f11Down && !g_canTriggerF11) {
        g_canTriggerF11 = true;
    }

    if (g_f10Down && g_canTriggerF10) {
        g_rate += g_acc * 10;
        g_canTriggerF10 = false;
    } else if (!g_f10Down && !g_canTriggerF10) {
        g_canTriggerF10 = true;
    }

    if (g_f9Down && g_canTriggerF9) {
        g_rate -= g_acc * 10;
        g_canTriggerF9 = false;
    } else if (!g_f9Down && !g_canTriggerF9) {
        g_canTriggerF9 = true;
    }

    if (g_f8Down && g_canTriggerF8) {
        g_rate = kF8WeaponRate;
        g_canTriggerF8 = false;
    } else if (!g_f8Down && !g_canTriggerF8) {
        g_canTriggerF8 = true;
    }

    if (g_f7Down && g_canTriggerF7) {
        g_rate = kF7WeaponRate;
        g_canTriggerF7 = false;
    } else if (!g_f7Down && !g_canTriggerF7) {
        g_canTriggerF7 = true;
    }

    if (GetKeyState(VK_LBUTTON) & 0x8000) {
        if ((GetKeyState(VK_HOME) & 0x8000) == 0 && (GetKeyState(VK_F2) & 0x8000) == 0) {
            mouse_event(MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK,
                        static_cast<DWORD>(g_x), static_cast<DWORD>(g_y), 0,
                        static_cast<ULONG_PTR>(GetMessageExtraInfo()));
            g_y += g_rate;
        }
    }
}

}  // namespace input
