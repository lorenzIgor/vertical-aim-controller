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

// Por que a compensacao esta parada num dado instante.
//
// Sem isto, uma compensacao bloqueada e indistinguivel de uma compensacao
// quebrada: nao ha nada na tela dizendo qual das seis condicoes falhou.
enum class Status {
    Compensating,    // atirando e compensando
    Ready,           // tudo liberado, faltando so o gatilho
    NoGame,          // janela do jogo nao encontrada
    NotForeground,   // jogo fora do primeiro plano (camada 1)
    MenuDetected,    // cursor solto: o jogo nao esta prendendo no centro
    CursorVisible,   // cursor do sistema visivel (heuristica antiga)
    SuppressedByKey, // HOME ou F2 segurados
    Inactive,        // desligado neste slot (Ctrl+Shift+S)
};

const char* StatusLabel(Status status);

struct ContextState {
    bool   gameForeground  = false;
    bool   cursorVisible   = false;
    bool   cursorPinned    = false;  // preso no centro da janela do jogo
    bool   suppressedByKey = false;  // HOME ou F2 segurados
    bool   compensating    = false;
    Status status          = Status::NoGame;
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

// Compensa apenas enquanto o jogo estiver prendendo o cursor no centro da
// propria janela.
//
// E o que o Battlefield V faz durante o gameplay: recentraliza o cursor a cada
// quadro, porque a mira vem de raw input. No menu ele solta o cursor. Medido
// nos dois estados: cravado em (960,540) jogando, livre no menu.
//
// A decisao tem histerese de 250 ms. Um unico quadro fora do centro nao conta,
// senao o proprio movimento que enviamos seria lido como menu.
void SetRequireCursorPinned(bool value);
bool RequireCursorPinned();

// Heuristica anterior: suspender enquanto o cursor do sistema estiver visivel.
// Nao serve para o Battlefield V, que mantem o cursor visivel o tempo todo --
// medido. Mantida desligada para outros jogos.
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
