#pragma once

#include <array>
#include <string>

inline constexpr int kSlotCountCfg = 4;

// Configuracao persistida em vac.ini, ao lado do executavel.
//
// Formato deliberadamente simples: "chave = valor", um por linha, sem secoes.
// Linhas iniciadas por # ou ; sao comentario. Chave desconhecida e ignorada e
// chave ausente mantem o padrao, entao um arquivo de uma versao anterior
// continua carregando.
struct Settings {
    std::string targetExe     = "bfv.exe";
    std::string titleFallback = "battlefield";

    bool requireForeground = true;

    // Compensa apenas enquanto o jogo prende o cursor no centro da janela --
    // o que o Battlefield V faz no gameplay e nao faz no menu.
    bool requireCursorPinned = true;

    // Heuristica anterior, desligada por padrao: nao serve para o Battlefield
    // V, que mantem o cursor do sistema visivel tambem durante o gameplay.
    // Quando o jogo se comporta assim, ela bloqueia a compensacao o tempo todo.
    bool suppressWhenCursorVisible = false;

    // px/s por slot de arma. O slot 1 e a primaria; os demais comecam em zero
    // porque pistola e gadget normalmente nao precisam de compensacao.
    // 135 veio de calibracao real no Battlefield V, nao de estimativa.
    std::array<float, kSlotCountCfg> ratePerSlot = {135.0f, 0.0f, 0.0f, 0.0f};

    float stepFine   = 5.0f;   // F11 / F12
    float stepCoarse = 25.0f;  // F9  / F10
    float presetF7   = 135.0f;
    float presetF8   = 335.0f;

    float hudFontSize = 42.0f;

    // Escala do painel. A fonte embutida do ImGui tem 13 px, ilegivel sobre
    // 1920x1080 a distancia de jogo.
    float uiScale = 2.0f;
};

namespace config {

// Caminho de vac.ini, sempre ao lado do executavel -- nao depende do
// diretorio de trabalho de quem iniciou o processo.
std::wstring Path();

// Nunca falha: sem arquivo, devolve os padroes.
Settings Load();

bool Save(const Settings& settings);

std::wstring Widen(const std::string& s);

}  // namespace config
