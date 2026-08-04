# Vertical Aim Controller

Compensador de recuo vertical com HUD para Battlefield V, em C++/Win32.

Enquanto o botão esquerdo do mouse está pressionado, o programa desloca o cursor
para baixo a uma taxa configurável, cancelando a subida da arma. Um overlay
transparente mostra a taxa atual para que a calibração seja feita dentro do jogo,
sem alt-tab.

---

## Requisitos

| Item | Versão |
|---|---|
| Windows | 10 1703 ou superior (DirectComposition + DPI por monitor) |
| Visual Studio Build Tools | 2022, workload C++ |
| Windows SDK | 10.0.22000 ou superior |
| CMake | 3.20+ |

**O jogo precisa estar em borderless ou janela.** Em fullscreen exclusivo nenhum
overlay externo é composto pelo DWM — o HUD não aparece. A compensação em si
continua funcionando, mas você fica sem a leitura na tela.

Não é necessário o DirectX SDK de 2010. Versões anteriores deste projeto
dependiam de `d3dx9`, que nunca foi portado para o Windows SDK; a pilha atual é
D3D11 + DXGI + DirectComposition, tudo já presente no SDK.

## Build

```bash
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

O Dear ImGui é baixado automaticamente pelo `FetchContent` na configuração, em
tag fixa. O binário sai em `build/Release/vertical-aim-controller.exe` e não
depende de nenhuma DLL além das do sistema.

## Atalhos

| Tecla | Ação |
|---|---|
| `Ctrl` + `Shift` + `S` | Liga/desliga a compensação **no slot atual** |
| `1` `2` `3` `4` | Seleciona o slot de arma (acompanha a troca no jogo) |
| `F7` / `F8` | Aplica o preset 1 / preset 2 ao slot atual |
| `F9` / `F10` | Ajuste grosso: −50 / +50 px/s |
| `F11` / `F12` | Ajuste fino: −5 / +5 px/s |
| `HOME` ou `F2` | Suspende a compensação enquanto segurado |
| `INSERT` | Alterna entre HUD passivo e painel interativo |

Os atalhos só respondem com o jogo (ou o painel) em primeiro plano. Digitar `2`
num navegador não altera mais o estado da ferramenta.

## Calibração

A taxa é expressa em **pixels por segundo** e vale por slot de arma. Os valores
padrão são chutes — calibre uma vez por arma e eles ficam gravados.

1. Entre numa partida em borderless e escolha a arma.
2. `INSERT` abre o painel; `Ctrl+Shift+S` liga a compensação.
3. Mire numa parede e segure o tiro observando o rastro:
   - ainda **sobe** → aumente (`F10` grosso, `F12` fino)
   - **desce demais** → diminua (`F9` grosso, `F11` fino)
4. Repita para cada slot que precise de compensação.

Não é preciso salvar: as alterações vão para `vac.ini` automaticamente.

## Detecção de contexto

Duas camadas evitam que a compensação dispare quando você não está atirando:

**Jogo em primeiro plano** — impede que cliques no navegador, na área de
trabalho ou em qualquer outra janela arrastem o cursor.

**Cursor do sistema visível** — durante o gameplay um FPS esconde o cursor e usa
raw input; ao abrir um menu, mostra. É o sinal direto de que há menu aberto.
Depende do jogo não desenhar cursor próprio, então vem com interruptor: abra um
menu e veja se o indicador acende no painel. Se não acender, desligue a opção e
use `HOME`/`F2` manualmente.

## Configuração

`vac.ini`, gravado ao lado do executável. Formato `chave = valor`, um por linha;
`#` e `;` iniciam comentário. Chave ausente mantém o padrão e chave desconhecida
é ignorada, então arquivos de versões anteriores continuam carregando.

| Chave | Padrão | Descrição |
|---|---|---|
| `target.exe` | `bfv.exe` | Executável do jogo a rastrear |
| `target.title_fallback` | `battlefield` | Trecho do título, usado se o executável não for achado |
| `detection.require_foreground` | `1` | Camada 1 |
| `detection.suppress_when_cursor_visible` | `1` | Camada 2 |
| `rate.slot1`…`rate.slot4` | `250`, `0`, `0`, `0` | Compensação por slot, em px/s |
| `step.fine` | `5` | Passo de `F11`/`F12` |
| `step.coarse` | `50` | Passo de `F9`/`F10` |
| `preset.f7` / `preset.f8` | `250` / `620` | Presets |
| `hud.font_size` | `42` | Tamanho do número no HUD |

O programa reescreve o arquivo ao sair, então comentários adicionados à mão se
perdem.

## Arquitetura

| Arquivo | Responsabilidade |
|---|---|
| `src/main.cpp` | Laço principal, HUD, painel, gravação com atraso |
| `src/overlay.*` | Janela transparente, D3D11, DirectComposition, ImGui |
| `src/input.*` | Thread de compensação, hotkeys, detecção de contexto |
| `src/gamewindow.*` | Localização e rastreamento da janela do jogo |
| `src/config.*` | Leitura e gravação de `vac.ini` |

A compensação roda em thread própria com `dt` medido por
`QueryPerformanceCounter`, separada do laço de render — que é ritmado pelo vsync.
Por isso a taxa é px/s e não muda de efeito conforme o FPS do overlay ou a carga
da máquina.

## Aviso

Automação de recuo em multiplayer online é classificada como cheat pelos
sistemas anti-cheat correntes, incluindo o EA Javelin que acompanha o
Battlefield V. Este repositório é material de estudo sobre overlays,
composição no Windows e input sintético; usar em partidas online implica risco
real de banimento da conta.
