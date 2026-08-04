cmake_minimum_required(VERSION 3.20)
project(vertical-aim-controller LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR "Projeto especifico para Windows (Win32 + D3D11 + DirectComposition).")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------------------
# Dear ImGui
#
# Nao ha CMakeLists no repositorio do ImGui, entao FetchContent apenas baixa os
# fontes e nos compilamos a lista que usamos. Tag fixa: atualizar e uma decisao
# explicita, nao um efeito colateral de rebuildar.
# ---------------------------------------------------------------------------
include(FetchContent)
FetchContent_Declare(
    imgui_src
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.91.5
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(imgui_src)

add_library(imgui STATIC
    ${imgui_src_SOURCE_DIR}/imgui.cpp
    ${imgui_src_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_src_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_src_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_src_SOURCE_DIR}/backends/imgui_impl_win32.cpp
    ${imgui_src_SOURCE_DIR}/backends/imgui_impl_dx11.cpp
)
target_include_directories(imgui PUBLIC
    ${imgui_src_SOURCE_DIR}
    ${imgui_src_SOURCE_DIR}/backends
)
target_link_libraries(imgui PUBLIC d3d11 dxgi dxguid)
target_compile_definitions(imgui PUBLIC UNICODE _UNICODE)

# ---------------------------------------------------------------------------
# Executavel
#
# WIN32 define o subsystem como "Windows", que e o que faz o linker procurar
# wWinMain. Sem isso o MSVC gera um executavel de console e abre um terminal
# junto -- era o motivo do ShowWindow(GetConsoleWindow(), 0) na versao antiga.
# ---------------------------------------------------------------------------
add_executable(vertical-aim-controller WIN32
    src/main.cpp
    src/overlay.cpp
    src/gamewindow.cpp
    src/input.cpp
    src/config.cpp
)

target_link_libraries(vertical-aim-controller PRIVATE
    imgui
    d3d11      # dispositivo e contexto de render
    dxgi       # swapchain de composicao
    dcomp      # DirectComposition: transparencia por pixel
    winmm      # timeBeginPeriod (resolucao do timer, usado a partir da fase 2)
)

target_compile_definitions(vertical-aim-controller PRIVATE
    UNICODE _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

if(MSVC)
    target_compile_options(vertical-aim-controller PRIVATE /W4 /permissive-)
endif()
