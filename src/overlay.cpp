#include "overlay.h"

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg,
                                                             WPARAM wParam, LPARAM lParam);

#include <algorithm>

namespace {
constexpr wchar_t kWindowClass[] = L"VerticalAimControllerOverlay";
constexpr wchar_t kWindowTitle[] = L"VerticalAimController";

// Estado lido pela WndProc, que e estatica. Ha exatamente um overlay por
// processo, entao um estado de arquivo resolve sem exigir GWLP_USERDATA.
bool g_clickThrough = true;
}  // namespace

Overlay::~Overlay() {
    Shutdown();
}

LRESULT CALLBACK Overlay::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) return 1;

    switch (msg) {
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        // O overlay nunca rouba ativacao do jogo. O painel funciona sem foco
        // porque a posicao do mouse e injetada em FeedMouseFromSystem.
        case WM_MOUSEACTIVATE:
            return MA_NOACTIVATE;

        // Click-through explicito, em conjunto com WS_EX_TRANSPARENT.
        //
        // HTTRANSPARENT manda o Windows repassar o teste para a janela de
        // baixo. E redundante com WS_EX_TRANSPARENT no caso comum, mas nao
        // depende de a janela ser WS_EX_LAYERED -- e esta nao pode ser, porque
        // a transparencia por pixel vem do DirectComposition, que exige
        // WS_EX_NOREDIRECTIONBITMAP.
        case WM_NCHITTEST:
            if (g_clickThrough) return HTTRANSPARENT;
            break;
        default:
            break;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

bool Overlay::CreateHostWindow(HINSTANCE hInstance) {
    WNDCLASSEXW wc = {};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = &Overlay::WndProc;
    wc.hInstance     = hInstance;
    wc.hCursor       = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = kWindowClass;
    if (RegisterClassExW(&wc) == 0) return false;

    // WS_EX_NOREDIRECTIONBITMAP: sem superficie de redirecionamento, exigido
    //   para que o DWM componha o visual do DirectComposition com alfa.
    // WS_EX_TRANSPARENT: cliques atravessam (alternado em SetInteractive).
    // WS_EX_TOOLWINDOW: mantem o overlay fora do Alt-Tab e da barra de tarefas.
    // WS_EX_NOACTIVATE: nunca tira foco do jogo ao aparecer.
    const DWORD exStyle = WS_EX_NOREDIRECTIONBITMAP | WS_EX_TOPMOST |
                          WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE;

    hwnd_ = CreateWindowExW(exStyle, kWindowClass, kWindowTitle, WS_POPUP,
                            posX_, posY_, width_, height_,
                            nullptr, nullptr, hInstance, nullptr);
    return hwnd_ != nullptr;
}

bool Overlay::CreateDeviceAndSwapChain() {
    UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#ifndef NDEBUG
    // A camada de debug so existe se as "Graphics Tools" do Windows estiverem
    // instaladas; se faltar, a criacao e refeita sem ela logo abaixo.
    flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    const D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0,
                                        D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0};

    HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags,
                                   levels, ARRAYSIZE(levels), D3D11_SDK_VERSION,
                                   &device_, nullptr, &context_);
#ifndef NDEBUG
    if (FAILED(hr)) {
        flags &= ~static_cast<UINT>(D3D11_CREATE_DEVICE_DEBUG);
        hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags,
                               levels, ARRAYSIZE(levels), D3D11_SDK_VERSION,
                               &device_, nullptr, &context_);
    }
#endif
    if (FAILED(hr)) return false;

    ComPtr<IDXGIDevice> dxgiDevice;
    if (FAILED(device_.As(&dxgiDevice))) return false;

    ComPtr<IDXGIAdapter> adapter;
    if (FAILED(dxgiDevice->GetAdapter(&adapter))) return false;

    ComPtr<IDXGIFactory2> factory;
    if (FAILED(adapter->GetParent(IID_PPV_ARGS(&factory)))) return false;

    DXGI_SWAP_CHAIN_DESC1 desc = {};
    desc.Width            = static_cast<UINT>(width_);
    desc.Height           = static_cast<UINT>(height_);
    desc.Format           = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.BufferUsage      = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount      = 2;
    // Composicao exige escala STRETCH e um dos modos flip.
    desc.Scaling    = DXGI_SCALING_STRETCH;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    desc.AlphaMode  = DXGI_ALPHA_MODE_PREMULTIPLIED;

    return SUCCEEDED(factory->CreateSwapChainForComposition(device_.Get(), &desc,
                                                            nullptr, &swapChain_));
}

bool Overlay::CreateCompositionTree() {
    ComPtr<IDXGIDevice> dxgiDevice;
    if (FAILED(device_.As(&dxgiDevice))) return false;

    if (FAILED(DCompositionCreateDevice(dxgiDevice.Get(), IID_PPV_ARGS(&dcompDevice_))))
        return false;
    if (FAILED(dcompDevice_->CreateTargetForHwnd(hwnd_, TRUE, &dcompTarget_)))
        return false;
    if (FAILED(dcompDevice_->CreateVisual(&dcompVisual_)))
        return false;
    if (FAILED(dcompVisual_->SetContent(swapChain_.Get())))
        return false;
    if (FAILED(dcompTarget_->SetRoot(dcompVisual_.Get())))
        return false;

    return SUCCEEDED(dcompDevice_->Commit());
}

bool Overlay::CreateRenderTarget() {
    ComPtr<ID3D11Texture2D> backBuffer;
    if (FAILED(swapChain_->GetBuffer(0, IID_PPV_ARGS(&backBuffer)))) return false;
    return SUCCEEDED(device_->CreateRenderTargetView(backBuffer.Get(), nullptr, &rtv_));
}

void Overlay::ReleaseRenderTarget() {
    rtv_.Reset();
}

void Overlay::InitImGui() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    // Sem imgui.ini: a persistencia deste projeto sai na fase 4, em formato
    // proprio, e um imgui.ini solto ao lado do exe so confundiria.
    io.IniFilename = nullptr;
    // O jogo controla o cursor; o overlay nunca deve mexer nele.
    io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;

    ImGui::StyleColorsDark();

    ImGui_ImplWin32_Init(hwnd_);
    ImGui_ImplDX11_Init(device_.Get(), context_.Get());
    imguiReady_ = true;
}

void Overlay::ShutdownImGui() {
    if (!imguiReady_) return;
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    imguiReady_ = false;
}

bool Overlay::Init(HINSTANCE hInstance) {
    hInstance_ = hInstance;
    if (!CreateHostWindow(hInstance))  return false;
    if (!CreateDeviceAndSwapChain())   return false;
    if (!CreateCompositionTree())      return false;
    if (!CreateRenderTarget())         return false;
    InitImGui();
    SetInteractive(false);
    return true;
}

void Overlay::Shutdown() {
    ShutdownImGui();
    ReleaseRenderTarget();
    dcompVisual_.Reset();
    dcompTarget_.Reset();
    dcompDevice_.Reset();
    swapChain_.Reset();
    context_.Reset();
    device_.Reset();

    if (hwnd_ != nullptr) {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
        UnregisterClassW(kWindowClass, hInstance_);
    }
}

void Overlay::SetGeometry(const RECT& screenRect) {
    const int w = screenRect.right - screenRect.left;
    const int h = screenRect.bottom - screenRect.top;
    if (w <= 0 || h <= 0) return;

    const bool moved   = (screenRect.left != posX_ || screenRect.top != posY_);
    const bool resized = (w != width_ || h != height_);
    if (!moved && !resized) return;

    posX_   = screenRect.left;
    posY_   = screenRect.top;
    width_  = w;
    height_ = h;

    SetWindowPos(hwnd_, HWND_TOPMOST, posX_, posY_, width_, height_,
                 SWP_NOACTIVATE | SWP_NOREDRAW);

    if (resized) {
        // A swapchain nao acompanha o tamanho da janela sozinha. Sem isto o
        // conteudo sai esticado quando a janela do jogo muda de resolucao --
        // era o comportamento da versao anterior ao mudar de monitor.
        ReleaseRenderTarget();
        swapChain_->ResizeBuffers(0, static_cast<UINT>(width_), static_cast<UINT>(height_),
                                  DXGI_FORMAT_UNKNOWN, 0);
        CreateRenderTarget();
    }
}

void Overlay::Show(bool visible) {
    if (visible == visible_) return;
    visible_ = visible;
    ShowWindow(hwnd_, visible ? SW_SHOWNOACTIVATE : SW_HIDE);
}

// Alterna a resposta de WM_NCHITTEST. Nao mexe em WS_EX_TRANSPARENT: nesta
// janela aquele estilo nao produz click-through, porque ela nao e -- e nao pode
// ser -- WS_EX_LAYERED. Ver o comentario em WM_NCHITTEST.
//
// WS_EX_NOACTIVATE fica permanentemente ligado: o overlay jamais deve roubar
// ativacao do jogo. Nao ha um "modo interativo" que capture a tela toda -- a
// captura dura apenas os quadros em que o cursor esta sobre o painel.
void Overlay::SetClickThrough(bool clickThrough) {
    if (clickThrough == clickThrough_) return;
    clickThrough_  = clickThrough;
    g_clickThrough = clickThrough;
}

// Injeta mouse direto do sistema.
//
// ImGui_ImplWin32_NewFrame so entrega posicao quando GetForegroundWindow() e
// esta janela, e o overlay nunca esta em primeiro plano. Lendo do sistema o
// painel funciona sem foco, o que dispensa toda a manobra de ativacao que a
// versao anterior fazia -- e era ela que produzia a zona morta.
void Overlay::FeedMouseFromSystem() {
    if (!interactive_) return;

    POINT p;
    if (!GetCursorPos(&p)) return;
    if (!ScreenToClient(hwnd_, &p)) return;

    ImGuiIO& io = ImGui::GetIO();
    io.AddMousePosEvent(static_cast<float>(p.x), static_cast<float>(p.y));
    io.AddMouseButtonEvent(0, (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0);
    io.AddMouseButtonEvent(1, (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0);
}

void Overlay::SetInteractive(bool interactive) {
    interactive_ = interactive;

    if (imguiReady_) {
        ImGuiIO& io = ImGui::GetIO();
        if (interactive) {
            io.ConfigFlags &= ~static_cast<int>(ImGuiConfigFlags_NoMouse);
        } else {
            io.ConfigFlags |= ImGuiConfigFlags_NoMouse;
        }
    }

    // Fechando o painel, volta a ser click-through imediatamente em vez de
    // esperar o proximo quadro decidir.
    if (!interactive) SetClickThrough(true);
}

void Overlay::ApplyUiScale(float scale) {
    uiScale_ = std::clamp(scale, 1.0f, 4.0f);
    if (!imguiReady_) return;

    // A fonte embutida do ImGui tem 13 px. Sobre 1920x1080 isso e ilegivel a
    // distancia de jogo, entao o atlas e reconstruido no tamanho pedido em vez
    // de apenas esticar a textura, que sairia borrada.
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->Clear();

    ImFontConfig cfg;
    cfg.SizePixels = 13.0f * uiScale_;
    io.Fonts->AddFontDefault(&cfg);
    io.Fonts->Build();
    ImGui_ImplDX11_InvalidateDeviceObjects();  // recria a textura no proximo frame

    // ScaleAllSizes e cumulativo, entao o estilo volta ao padrao antes.
    ImGuiStyle& style = ImGui::GetStyle();
    style = ImGuiStyle();
    ImGui::StyleColorsDark();
    style.ScaleAllSizes(uiScale_);
}

bool Overlay::RecreateAfterDeviceLoss() {
    ShutdownImGui();
    ReleaseRenderTarget();
    dcompVisual_.Reset();
    dcompTarget_.Reset();
    dcompDevice_.Reset();
    swapChain_.Reset();
    context_.Reset();
    device_.Reset();

    if (!CreateDeviceAndSwapChain()) return false;
    if (!CreateCompositionTree())    return false;
    if (!CreateRenderTarget())       return false;
    InitImGui();
    SetInteractive(interactive_);
    return true;
}

bool Overlay::BeginFrame() {
    if (!rtv_ && !CreateRenderTarget()) return false;

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    FeedMouseFromSystem();  // depois do backend, para sobrepor o que ele nao deu
    ImGui::NewFrame();
    return true;
}

void Overlay::EndFrame() {
    ImGui::Render();

    // Captura o mouse apenas enquanto o cursor estiver sobre algum elemento do
    // painel. Fora dele o overlay continua click-through e o jogo recebe os
    // cliques normalmente, mesmo com o painel aberto.
    SetClickThrough(!(interactive_ && ImGui::GetIO().WantCaptureMouse));

    // Limpar com alfa zero e o que torna o fundo realmente transparente: o DWM
    // compoe o resultado premultiplicado por cima da tela.
    const float clear[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    ID3D11RenderTargetView* rtvs[] = {rtv_.Get()};
    context_->OMSetRenderTargets(1, rtvs, nullptr);
    context_->ClearRenderTargetView(rtv_.Get(), clear);

    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    // Present com intervalo 1 sincroniza com o monitor. Alem de evitar tearing,
    // e o que limita o laco principal: a versao anterior girava sem limite e
    // ocupava um core inteiro para desenhar um numero.
    const HRESULT hr = swapChain_->Present(1, 0);

    if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET) {
        RecreateAfterDeviceLoss();
        return;
    }

    dcompDevice_->Commit();
}
