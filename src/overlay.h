#pragma once

#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <dcomp.h>
#include <wrl/client.h>

// Overlay transparente sobre a janela do jogo.
//
// A pilha e D3D11 + DXGI + DirectComposition. Nao da para usar uma swapchain
// de HWND aqui: DXGI so aceita DXGI_ALPHA_MODE_PREMULTIPLIED em swapchain de
// composicao, entao o conteudo vai para um IDCompositionVisual que o DWM
// compoe sobre a tela. A versao anterior conseguia transparencia via D3D9Ex +
// DwmExtendFrameIntoClientArea, caminho que nao tem equivalente em D3D11.
class Overlay {
public:
    Overlay() = default;
    ~Overlay();

    Overlay(const Overlay&)            = delete;
    Overlay& operator=(const Overlay&) = delete;

    bool Init(HINSTANCE hInstance);
    void Shutdown();

    // Reposiciona sobre o retangulo informado (coordenadas de tela).
    // Redimensiona os buffers apenas quando o tamanho muda de fato.
    void SetGeometry(const RECT& screenRect);

    void Show(bool visible);

    // Passivo: WS_EX_TRANSPARENT e WS_EX_NOACTIVATE ligados, cliques
    // atravessam para o jogo e a janela nunca rouba foco.
    // Interativo: ambos desligados e a janela vai a primeiro plano.
    //
    // Os dois estilos precisam andar juntos. O backend Win32 do ImGui so
    // alimenta a posicao do mouse quando GetForegroundWindow() e esta janela,
    // e WS_EX_NOACTIVATE impede exatamente isso -- entao deixar so o
    // TRANSPARENT sair cria uma zona morta: a janela para de deixar o clique
    // passar para o jogo, mas o ImGui tambem nao o recebe.
    void SetInteractive(bool interactive);
    bool IsInteractive() const { return interactive_; }

    // Janela para a qual devolver o foco ao sair do modo interativo.
    void SetFocusReturnWindow(HWND hwnd) { focusReturn_ = hwnd; }

    // Escala da interface. Reconstroi o atlas de fontes, entao nao pode ser
    // chamada entre BeginFrame e EndFrame.
    void  ApplyUiScale(float scale);
    float UiScale() const { return uiScale_; }

    // false quando o dispositivo foi perdido e nao foi possivel recriar.
    bool BeginFrame();
    void EndFrame();

    HWND Hwnd() const { return hwnd_; }

    int Width()  const { return width_; }
    int Height() const { return height_; }

private:
    template <typename T>
    using ComPtr = Microsoft::WRL::ComPtr<T>;

    bool CreateHostWindow(HINSTANCE hInstance);
    bool CreateDeviceAndSwapChain();
    bool CreateCompositionTree();
    bool CreateRenderTarget();
    void ReleaseRenderTarget();
    bool RecreateAfterDeviceLoss();
    void InitImGui();
    void ShutdownImGui();

    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

    HINSTANCE hInstance_ = nullptr;
    HWND      hwnd_      = nullptr;

    ComPtr<ID3D11Device>           device_;
    ComPtr<ID3D11DeviceContext>    context_;
    ComPtr<IDXGISwapChain1>        swapChain_;
    ComPtr<ID3D11RenderTargetView> rtv_;

    ComPtr<IDCompositionDevice> dcompDevice_;
    ComPtr<IDCompositionTarget> dcompTarget_;
    ComPtr<IDCompositionVisual> dcompVisual_;

    int   width_       = 1280;
    int   height_      = 720;
    int   posX_        = 0;
    int   posY_        = 0;
    bool  visible_     = false;
    bool  interactive_ = false;
    bool  imguiReady_  = false;
    float uiScale_     = 1.0f;
    HWND  focusReturn_ = nullptr;
};
