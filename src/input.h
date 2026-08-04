#pragma once

// Compensacao de recuo e hotkeys.
//
// Roda em thread propria, com cadencia medida por QueryPerformanceCounter. A
// taxa e expressa em PIXELS POR SEGUNDO e o deslocamento de cada tick e
// rate * dt, entao a compensacao independe da velocidade do laco de render --
// que e o que a versao anterior nao tinha: ela somava um delta fixo por
// iteracao, e a iteracao durava o que o render demorasse.
namespace input {

// Taxas iniciais, em px/s. Substituem os antigos 121 e 301, que estavam em
// "unidades absolutas normalizadas por iteracao" e nao tem conversao para ca.
// Preservam a proporcao entre as duas armas; os valores exatos precisam de uma
// calibracao unica.
inline constexpr float kPresetF7 = 250.0f;
inline constexpr float kPresetF8 = 620.0f;

inline constexpr float kStepFine   = 5.0f;   // F11 / F12
inline constexpr float kStepCoarse = 50.0f;  // F9  / F10

inline constexpr float kRateMin = 0.0f;
inline constexpr float kRateMax = 5000.0f;

void Start();
void Stop();

// Liga/desliga a compensacao de fora. main() usa isto para so compensar
// enquanto a janela do jogo existe.
void SetEnabled(bool enabled);

bool  IsActive();
float Rate();

}  // namespace input
