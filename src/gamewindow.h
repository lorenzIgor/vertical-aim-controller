#pragma once

#include <windows.h>
#include <string>

// Rastreia a janela principal do jogo alvo.
//
// A identificacao e feita pelo nome do executavel, nao pelo titulo da janela:
// o titulo muda com localizacao e entre patches, enquanto o executavel nao.
// O titulo fica como fallback.
//
// A busca do PID usa Toolhelp32, que le a tabela de processos sem abrir handle
// para nenhum deles. A versao anterior chamava OpenProcess a cada iteracao do
// laco e nunca fechava o handle resultante.
class GameWindowTracker {
public:
    GameWindowTracker(std::wstring exeName, std::wstring titleFallback);

    // Reavalia o alvo. Barato quando a janela ja e conhecida e continua valida;
    // so revarre a tabela de processos a cada rescanIntervalMs enquanto procura.
    void Update();

    bool     Valid() const { return hwnd_ != nullptr; }
    HWND     Hwnd()  const { return hwnd_; }

    // True depois que a janela foi encontrada ao menos uma vez. Permite
    // distinguir "o jogo ainda nao abriu" de "o jogo fechou".
    bool EverFound() const { return everFound_; }

    // Area de cliente em coordenadas de tela. Vazia se Valid() for false.
    RECT ClientRectInScreen() const { return clientRect_; }

private:
    void Rescan();
    bool RefreshGeometry();

    std::wstring exeName_;
    std::wstring titleFallback_;
    HWND         hwnd_        = nullptr;
    RECT         clientRect_  = {0, 0, 0, 0};
    bool         everFound_   = false;
    ULONGLONG    lastScanMs_  = 0;

    static constexpr ULONGLONG kRescanIntervalMs = 500;
};
