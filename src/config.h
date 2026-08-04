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

    bool requireForeground         = true;
    bool suppressWhenCursorVisible = true;

    // px/s por slot de arma. O slot 1 e a primaria; os demais comecam em zero
    // porque pistola e gadget normalmente nao precisam de compensacao.
    std::array<float, kSlotCountCfg> ratePerSlot = {250.0f, 0.0f, 0.0f, 0.0f};

    float stepFine   = 5.0f;   // F11 / F12
    float stepCoarse = 50.0f;  // F9  / F10
    float presetF7   = 250.0f;
    float presetF8   = 620.0f;

    float hudFontSize = 42.0f;
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
