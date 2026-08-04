#pragma once

#include <windows.h>
#include <string>

// Icone na bandeja do sistema.
//
// Sem ele o programa e invisivel enquanto o jogo nao abre: nao tem console,
// nao aparece na barra de tarefas (WS_EX_TOOLWINDOW) e o overlay so fica
// visivel com o jogo em execucao. Do ponto de vista de quem clicou no atalho,
// isso e indistinguivel de nao ter aberto -- e leva a abrir de novo.
//
// Tambem e a unica forma de encerrar o programa pela interface: clique
// esquerdo abre o painel, clique direito encerra. Nao ha menu de contexto --
// ver o comentario sobre TrackPopupMenu em tray.cpp.
namespace tray {

enum class Command {
    None,
    TogglePanel,
    Quit,
};

bool Init(HINSTANCE hInstance);
void Shutdown();

// Atualiza a dica exibida ao passar o mouse. Chamadas com o mesmo texto sao
// descartadas: Shell_NotifyIcon nao e barato.
void SetTooltip(const std::wstring& text);

// Notificacao de balao, usada uma vez ao iniciar.
void Notify(const std::wstring& title, const std::wstring& message);

// Consome o comando pendente, se houver.
Command PollCommand();

}  // namespace tray
