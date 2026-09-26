# src/sys -- capa de plataforma (SDL2 / SDL3)
#
# Target: condem::sys
#   Implementa sys.h y sys_vid.h con la versión de SDL elegida en CONDEM_PLATFORM.
#   El resto del motor solo ve la API Sys_* / VID_*, nunca SDL directamente.
#
# SDL2 y SDL3 vienen de vcpkg (vcpkg.json) y los busca el CMakeLists.txt raíz.
# También enlaza el backend de plataforma de ImGui correspondiente (imgui::sdl3
# o imgui::sdl2), porque sys_vid.h es quien lo inicializa.

set(CONDEM_PLATFORM "SDL3" CACHE STRING "Backend de plataforma (SDL3 o SDL2)")
set_property(CACHE CONDEM_PLATFORM PROPERTY STRINGS SDL3 SDL2)

if(CONDEM_PLATFORM STREQUAL "SDL3")
    if(NOT TARGET SDL3::SDL3)
        message(FATAL_ERROR "CONDEM_PLATFORM=SDL3 pero SDL3 no está disponible (vcpkg: sdl3)")
    endif()
    set(SYS_SRC sys_sdl3.cpp)
    set(SYS_SDL SDL3::SDL3)
elseif(CONDEM_PLATFORM STREQUAL "SDL2")
    if(NOT TARGET SDL2::SDL2)
        message(FATAL_ERROR "CONDEM_PLATFORM=SDL2 pero SDL2 no está disponible (vcpkg: sdl2)")
    endif()
    set(SYS_SRC sys_sdl2.cpp)
    set(SYS_SDL SDL2::SDL2)
else()
    message(FATAL_ERROR "CONDEM_PLATFORM desconocido: ${CONDEM_PLATFORM}")
endif()

message(STATUS "CondemEngine: plataforma ${CONDEM_PLATFORM}")

string(TOLOWER ${CONDEM_PLATFORM} SYS_IMGUI)

add_library(condem_sys STATIC
    sys.h
    sys_vid.h
    ${SYS_SRC}
)
add_library(condem::sys ALIAS condem_sys)

target_include_directories(condem_sys PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(condem_sys PUBLIC ${SYS_SDL} imgui::${SYS_IMGUI} Vulkan::Headers)
target_compile_definitions(condem_sys PUBLIC CONDEM_${CONDEM_PLATFORM})
