#pragma once

#include <windows.h>

#include "config.h"

// Compensacao de recuo e hotkeys.
//
// Roda em thread propria, com cadencia medida por QueryPerformanceCounter. A
// taxa e expressa em PIXELS POR SEGUNDO e o deslocamento de cada tick e
// rate * dt, entao a compensacao independe da velocidade do laco de render --
// que e o que a versao anterior nao tinha: ela somava um delta fixo por
// iteracao, e a iteracao durava o que o render demorasse.
namespace input {

inline constexpr int kSlotCount = kSlotCountCfg;

inline constexpr float kRateMin = 0.0f;
inline constexpr float kRateMax = 5000.0f;

// Por que a compensacao esta parada num dado instante. Alimenta o painel: e
// como se descobre, por exemplo, se a heuristica do cursor funciona no jogo.
struct ContextState {
    bool gameForeground  = false;
    bool cursorVisible   = false;
    bool suppressedByKey = false;  // HOME ou F2 segurados
    bool compensating    = false;
};

void Start();
void Stop();

// Carrega taxas, passos e presets vindos do arquivo de configuracao.
void ApplySettings(const Settings& settings);

// Copia o estado corrente de volta para a struct, para gravacao.
void ReadInto(Settings& settings);

// Janelas que contam como "estou no contexto certo". A do overlay entra na
// conta porque, com o painel aberto, o foco e dele e nao do jogo.
void SetGameWindow(HWND hwnd);
void SetOverlayWindow(HWND hwnd);

// Liga/desliga por fora. main() usa para so compensar enquanto a janela do
// jogo existe.
void SetEnabled(bool enabled);

// --- Camadas de deteccao de contexto ---

// So compensa com o jogo em primeiro plano. Impede que um clique no navegador
// ou na area de trabalho arraste o cursor, e que as teclas 1-4 digitadas fora
// do jogo mexam no estado.
void SetRequireForeground(bool value);
bool RequireForeground();

// Suspende quando o cursor do sistema esta visivel. Durante o gameplay um FPS
// esconde o cursor; ao abrir menu, mostra. Serve para dispensar o HOME/F2
// manual -- mas depende do jogo nao desenhar cursor proprio, entao e
// desligavel.
void SetSuppressWhenCursorVisible(bool value);
bool SuppressWhenCursorVisible();

bool IsActive();
int  CurrentSlot();  // 0..kSlotCount-1

// Taxa do slot atualmente selecionado.
float Rate();

float RateForSlot(int slot);
void  SetRateForSlot(int slot, float rate);

ContextState Context();

}  // namespace input
