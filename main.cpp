//#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <iostream>
#include <io.h>
#include <dwmapi.h>
#include <thread>
#include <d3d9.h>
#include <d3dx9.h>
#include <vector>

#pragma comment(lib, "dwmapi.lib");
#pragma comment(lib, "d3d9.lib");
#pragma comment(lib, "d3dx9.lib");

#define WS_EX_LAYERED 0x00080000
#define LWA_ALPHA 0x00000002
#define ARGB_TRANS 0x00000000
#define F7_WEAPON_RATE 238
#define F8_WEAPON_RATE 108

int x, y  = 0;
int rate = F7_WEAPON_RATE;
int acc = 1;

BOOL IS_ACTIVE = FALSE;

BOOL VK_12_PRESSED = false;
BOOL VK_11_PRESSED = false;
BOOL bCanTrigger_VK_12 = true;
BOOL bCanTrigger_VK_11 = true;

BOOL VK_10_PRESSED = false;
BOOL VK_9_PRESSED = false;
BOOL bCanTrigger_VK_10 = true;
BOOL bCanTrigger_VK_9 = true;

BOOL VK_8_PRESSED = false;
BOOL VK_7_PRESSED = false;
BOOL bCanTrigger_VK_8 = true;
BOOL bCanTrigger_VK_7 = true;

LPCSTR wndName = "NVIDIA GeForce Overlay DT";
LPDIRECT3D9EX d3d;              //D3D Object used to create the window
LPDIRECT3DDEVICE9EX d3ddev;     //D3D Device object used to render shit in the window

ID3DXFont *font = nullptr;      //Font used in the window
HRESULT hr = D3D_OK;
MARGINS windowMargins = {-1, -1, -1, -1};
const char *AppName = "IGOROverlay";

HWND BF5_hWnd = nullptr;        //Handle to window (BF5)
DWORD BF5_pID = 0;              //Process ID os that window (BF5 process ID)
HANDLE BF5_hProc = nullptr;     //Handle the process

bool init_ok = false;           //Is everything initialised?
COORD w_pos = {0,0};   //Initial windows position
COORD w_res = {1920,1080}; //Initial windows resolution


struct MonitorInfo {
    HMONITOR hMonitor;
    RECT monitorRect;
};

std::vector<MonitorInfo> monitors;

BOOL CALLBACK MonitorEnumProc(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData) {
    monitors.push_back({ hMonitor, *lprcMonitor });
    return TRUE;
}

void EnumerateMonitors() {
    monitors.clear();
    EnumDisplayMonitors(nullptr, nullptr, MonitorEnumProc, 0);
}

LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

void DrawStringAt(LPCSTR string, int x, int y, COLORREF color, ID3DXFont *Font)
{
    RECT font_rect;
    SetRect(&font_rect, x, y, w_res.X, w_res.Y);
    Font->DrawTextA(nullptr, string, -1, &font_rect, DT_LEFT | DT_NOCLIP | DT_SINGLELINE, color);
}

HRESULT D3DStartup(HWND hWnd)
{
    BOOL bCompOk = false;
    D3DPRESENT_PARAMETERS pp;   //xd
    DWORD msqAAQuality = 0;     //Non-maskeable quality
    HRESULT hr;

    //Make sure that DWM composition is enabled
    DwmIsCompositionEnabled(&bCompOk);
    if(!bCompOk) return E_FAIL;

    hr = Direct3DCreate9Ex(D3D_SDK_VERSION, &d3d);

    if(FAILED(hr))
    {
        MessageBox(nullptr, "Failed to create d3d object", "error", 0);
        return E_FAIL;
    }

    ZeroMemory(&pp, sizeof(pp));
    pp.Windowed = TRUE;
    pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    pp.BackBufferFormat = D3DFMT_A8R8G8B8;

    if(SUCCEEDED(d3d->CheckDeviceMultiSampleType(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, D3DFMT_A8R8G8B8, TRUE, D3DMULTISAMPLE_NONMASKABLE, &msqAAQuality)))
    {
        //Setting the AA quality
        pp.MultiSampleType = D3DMULTISAMPLE_NONMASKABLE;
        pp.MultiSampleQuality = msqAAQuality - 1;

    }else
    {
        //No AA
        pp.MultiSampleType = D3DMULTISAMPLE_NONE;
    }

    hr = d3d->CreateDeviceEx(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd, D3DCREATE_HARDWARE_VERTEXPROCESSING, &pp, nullptr, &d3ddev);

    if(FAILED(hr))
    {
        MessageBox(nullptr, "Failed to create d3d device object", "error", 0);
        return E_FAIL;
    }

    return hr;
}

VOID Render(VOID)
{
    COLORREF color = 0xFFFF3939;

    if(d3ddev == nullptr) return;

    d3ddev->Clear(0, nullptr, D3DCLEAR_TARGET, ARGB_TRANS, 1.0f, 0);

    //begin render scene
    d3ddev->BeginScene();

    auto rateLabel = std::to_string(rate);

    //render text in our window

    DrawStringAt(IS_ACTIVE ? rateLabel.c_str() : "NONE", 10, 30, color, font);

    d3ddev->EndScene();

    //Update the display
    d3ddev->PresentEx(nullptr, nullptr, nullptr, nullptr, 0);
}

void D3DShutdown()
{
    if(d3ddev != nullptr)
        d3ddev->Release();
    if(d3d != nullptr)
        d3d->Release();
}

void setIsActive() {
    IS_ACTIVE = !IS_ACTIVE;
}

bool bCanToggleActive = true;

void checkInput()
{
    std::this_thread::sleep_for(std::chrono::milliseconds(1));

    bool ctrlPressed = (GetAsyncKeyState(VK_CONTROL) & 0x8000);
    bool shiftPressed = (GetAsyncKeyState(VK_SHIFT) & 0x8000);
    bool sPressed = (GetAsyncKeyState('S') & 0x8000);

    if (ctrlPressed && shiftPressed && sPressed) {
        if (bCanToggleActive) {
            setIsActive();
            bCanToggleActive = false;
        }
    } else {
        // Libera o toggle quando a combinação for solta
        bCanToggleActive = true;
    }

    if(!IS_ACTIVE) return;

    VK_12_PRESSED = ((GetKeyState(VK_F12) & 0x8000) != 0);
    VK_11_PRESSED = ((GetKeyState(VK_F11) & 0x8000) != 0);
    VK_10_PRESSED = ((GetKeyState(VK_F10) & 0x8000) != 0);
    VK_9_PRESSED = ((GetKeyState(VK_F9) & 0x8000) != 0);
    VK_8_PRESSED = ((GetKeyState(VK_F8) & 0x8000) != 0);
    VK_7_PRESSED = ((GetKeyState(VK_F7) & 0x8000) != 0);

    if(VK_12_PRESSED && bCanTrigger_VK_12){
        rate += acc;
        bCanTrigger_VK_12 = false;
    } else if (!VK_12_PRESSED && !bCanTrigger_VK_12){
        bCanTrigger_VK_12 = true;
    }

    if(VK_11_PRESSED && bCanTrigger_VK_11){
        rate -= acc;
        bCanTrigger_VK_11 = false;
    } else if (!VK_11_PRESSED && !bCanTrigger_VK_11){
        bCanTrigger_VK_11 = true;
    }

    if(VK_10_PRESSED && bCanTrigger_VK_10){
        rate += (acc * 10);
        bCanTrigger_VK_10 = false;
    } else if (!VK_10_PRESSED && !bCanTrigger_VK_10){
        bCanTrigger_VK_10 = true;
    }

    if(VK_9_PRESSED && bCanTrigger_VK_9){
        rate -= (acc * 10);
        bCanTrigger_VK_9 = false;
    } else if (!VK_9_PRESSED && !bCanTrigger_VK_9){
        bCanTrigger_VK_9 = true;
    }

    if(VK_8_PRESSED && bCanTrigger_VK_8){
        rate = F8_WEAPON_RATE;
        bCanTrigger_VK_8 = false;
    } else if (!VK_8_PRESSED && !bCanTrigger_VK_8){
        bCanTrigger_VK_8 = true;
    }

    if(VK_7_PRESSED && bCanTrigger_VK_7){
        rate = F7_WEAPON_RATE;
        bCanTrigger_VK_7 = false;
    } else if (!VK_7_PRESSED && !bCanTrigger_VK_7){
        bCanTrigger_VK_7 = true;
    }

    if(GetKeyState(VK_LBUTTON) & 0x8000) {
        if ((GetKeyState(VK_HOME) & 0x8000) == 0 && (GetKeyState(VK_F2) & 0x8000) == 0) {
            mouse_event(MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK,
                        x,
                        y,
                        0,
                        GetMessageExtraInfo());

            y += rate;
        }
    }


}

INT WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
    ShowWindow(GetConsoleWindow(), 0);
    HWND hWnd = nullptr;
    MSG uMsg;
    HRESULT _hr;
    long Loops = 0;
    WNDCLASSEX wc = { sizeof(WNDCLASSEX),
                      CS_HREDRAW | CS_VREDRAW, WindowProc,
                      0, 0, hInstance,
                      LoadIconA(nullptr, IDI_APPLICATION),
                      LoadCursorA(nullptr, IDC_ARROW),
                      nullptr, nullptr, AppName,
                      LoadIconA(nullptr, IDI_APPLICATION)
                      };

    RegisterClassEx(&wc);

    //Making the transparent window (Overlay) that passes input through overlay into the window behind it (like a game)
    hWnd = CreateWindowExA( WS_EX_TOPMOST | WS_EX_COMPOSITED | WS_EX_TRANSPARENT | WS_EX_LAYERED,
                          AppName, AppName,  WS_POPUP | WS_VISIBLE | WS_CAPTION | WS_DISABLED, w_pos.X, w_pos.Y, w_res.X, w_res.Y,
                          nullptr, nullptr, hInstance, nullptr);

    long lCurStyle = GetWindowLong(hWnd, -16);  // GWL_STYLE=-16
    int a = 12582912;  //WS_CAPTION = 0x00C00000L
    int b = 1048576;  //WS_HSCROLL = 0x00100000L
    int c = 2097152;  //WS_VSCROLL = 0x00200000L
    int d = 524288;  //WS_SYSMENU = 0x00080000L
    int e = 16777216;  //WS_MAXIMIZE = 0x01000000L

    lCurStyle &= ~(a | b | c | d);
    lCurStyle &= e;
    SetWindowLong(hWnd, -16, lCurStyle);// GWL_STYLE=-16
    SetWindowLong(hWnd, -20, 524288 | 32);
    SetLayeredWindowAttributes(hWnd, 0, 200, LWA_ALPHA);// Transparency=51=20%, LWA_ALPHA=2

    _hr = DwmExtendFrameIntoClientArea(hWnd, &windowMargins);

    EnumerateMonitors();



    //Initialise D3D
    if(SUCCEEDED(D3DStartup(hWnd))){

        D3DXCreateFontA(d3ddev, 80, 40, FW_BOLD, 0, FALSE,
                       DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                       ANTIALIASED_QUALITY, DEFAULT_PITCH || FF_DONTCARE,
                       TEXT("Franklin Gothic"), &font);

        //Show the window
        ShowWindow(hWnd, SW_SHOWDEFAULT);
        UpdateWindow(hWnd);

        while(TRUE){

            checkInput();

            //Check for message
            if(PeekMessage(&uMsg, nullptr, 0, 0, PM_REMOVE))
            {
                //if message is WM_QUIT
                if(uMsg.message == WM_QUIT)
                {
                    //break out of the loop
                    break;
                }

                //Pump the message
                TranslateMessage(&uMsg);
                DispatchMessage(&uMsg);
            }


            BF5_hWnd = FindWindowA(nullptr, wndName);
            GetWindowThreadProcessId(BF5_hWnd, &BF5_pID);
            BF5_hProc = OpenProcess(PROCESS_VM_READ, false, BF5_pID);

            if(BF5_hProc != nullptr)
            {
                if(init_ok || (Loops % 20) == 0){

                    RECT client_rect;
                    GetClientRect(BF5_hWnd, &client_rect);      //Get the resolution
                    w_res.X = client_rect.right;
                    w_res.Y = client_rect.bottom;

                    RECT bounding_rect;
                    GetWindowRect(BF5_hWnd, &bounding_rect);    //Get the position
                    if(!init_ok)
                    {
                        if(w_pos.X != bounding_rect.left || w_pos.Y != bounding_rect.top)
                        {
                            MoveWindow(hWnd, bounding_rect.left, bounding_rect.top, client_rect.right, client_rect.bottom, false);
                            w_pos.X = bounding_rect.left;
                            w_pos.Y = bounding_rect.top;
                        }
                    } else {
                        // MoveWindow(hWnd, bounding_rect.left + 3850, bounding_rect.top + 450, 1920, 1080, false);

                        if (monitors.size() >= 2) {
                            RECT secondMonitor = monitors[0].monitorRect; //Mude aqui o index do segundo monitor!!!

                            MoveWindow(
                                hWnd,
                                secondMonitor.left, secondMonitor.top,
                                secondMonitor.right - secondMonitor.left,
                                secondMonitor.bottom - secondMonitor.top,
                                false
                            );
                        } else {
                            // fallback: tela principal
                            RECT primary = monitors[0].monitorRect;
                            MoveWindow(hWnd, primary.left, primary.top, primary.right - primary.left, primary.bottom - primary.top, false);
                        }

                    }

                    init_ok = true;     //Finally initialised

                }
            }

            //Check if window has closed
            if(Loops % 10 == 0)
            {
                if(FindWindowA(nullptr, wndName) == nullptr)
                {
                    SendMessage(hWnd, WM_CLOSE, 0, 0);        //Quit if game closed
                }
            }
            Loops++;
            if(Loops > 100) Loops = 0;

            //RENDERRRRRRRRRR
            Render();

        }

    }

    //Shutdown D3D
    D3DShutdown();

    //Quit
    return 0;
}

LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam){

    switch(message){
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;

    }

    return DefWindowProc(hWnd, message, wParam, lParam);

}