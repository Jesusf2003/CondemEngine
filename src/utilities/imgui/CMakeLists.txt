# Dear ImGui (copia de libs/imgui, versión 1.93.0 WIP).
#
# Targets disponibles:
#   imgui::imgui     núcleo (siempre)
#   imgui::sdl2      backend de plataforma SDL2     (CONDEM_IMGUI_SDL2)
#   imgui::sdl3      backend de plataforma SDL3     (CONDEM_IMGUI_SDL3)
#   imgui::opengl2   backend de render OpenGL 2     (CONDEM_IMGUI_OPENGL)
#   imgui::opengl3   backend de render OpenGL 3+    (CONDEM_IMGUI_OPENGL)
#   imgui::vulkan    backend de render Vulkan       (CONDEM_IMGUI_VULKAN)
#
# Cada backend solo se crea si su opción está activa y la dependencia se encuentra;
# si falta la dependencia se avisa y se omite, sin romper la configuración.
# Uso: target_link_libraries(mi_target PRIVATE imgui::sdl3 imgui::opengl3)

option(CONDEM_IMGUI_SDL2   "Backend de plataforma SDL2 para ImGui"      ON)
option(CONDEM_IMGUI_SDL3   "Backend de plataforma SDL3 para ImGui"      ON)
option(CONDEM_IMGUI_OPENGL "Backends de render OpenGL 2/3 para ImGui"  ON)
option(CONDEM_IMGUI_VULKAN "Backend de render Vulkan para ImGui"        ON)

set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR})

# --- Núcleo ------------------------------------------------------------------
add_library(imgui STATIC
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/misc/cpp/imgui_stdlib.cpp
)
add_library(imgui::imgui ALIAS imgui)
target_include_directories(imgui PUBLIC
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${IMGUI_DIR}/misc/cpp
)
target_compile_features(imgui PUBLIC cxx_std_11)
if(WIN32)
    # imgui.cpp usa el portapapeles/IME de Win32 por defecto.
    target_link_libraries(imgui PUBLIC imm32)
endif()

# Crea imgui_<name> + alias imgui::<name> a partir de backends/imgui_impl_<name>.cpp
function(condem_imgui_backend name)
    add_library(imgui_${name} STATIC ${IMGUI_DIR}/backends/imgui_impl_${name}.cpp)
    add_library(imgui::${name} ALIAS imgui_${name})
    target_link_libraries(imgui_${name} PUBLIC imgui ${ARGN})
    message(STATUS "ImGui: backend '${name}' habilitado")
endfunction()

# --- Plataforma: SDL2 --------------------------------------------------------
if(CONDEM_IMGUI_SDL2)
    find_package(SDL2 CONFIG QUIET)
    if(SDL2_FOUND)
        # imgui_impl_sdl2 incluye <SDL.h>, así que se usa el target que expone include/SDL2.
        if(TARGET SDL2::SDL2)
            condem_imgui_backend(sdl2 SDL2::SDL2)
        else()
            condem_imgui_backend(sdl2 SDL2::SDL2-static)
        endif()
    else()
        message(WARNING "ImGui: SDL2 no encontrado, se omite imgui::sdl2")
    endif()
endif()

# --- Plataforma: SDL3 --------------------------------------------------------
if(CONDEM_IMGUI_SDL3)
    # El proyecto raíz puede haber creado ya SDL3::SDL3 (FetchContent).
    if(NOT TARGET SDL3::SDL3)
        find_package(SDL3 CONFIG QUIET)
    endif()
    if(TARGET SDL3::SDL3)
        condem_imgui_backend(sdl3 SDL3::SDL3)
    else()
        message(WARNING "ImGui: SDL3 no encontrado, se omite imgui::sdl3")
    endif()
endif()

# --- Render: OpenGL ----------------------------------------------------------
if(CONDEM_IMGUI_OPENGL)
    find_package(OpenGL QUIET)
    if(OpenGL_FOUND)
        # opengl3 trae su propio loader (imgui_impl_opengl3_loader.h); opengl2 es pipeline fijo.
        condem_imgui_backend(opengl2 OpenGL::GL)
        condem_imgui_backend(opengl3 OpenGL::GL)
    else()
        message(WARNING "ImGui: OpenGL no encontrado, se omiten imgui::opengl2/opengl3")
    endif()
endif()

# --- Render: Vulkan ----------------------------------------------------------
if(CONDEM_IMGUI_VULKAN)
    # El proyecto raíz puede haber creado ya Vulkan::Vulkan (vcpkg).
    if(NOT TARGET Vulkan::Vulkan)
        find_package(Vulkan QUIET)
    endif()
    if(TARGET Vulkan::Vulkan)
        condem_imgui_backend(vulkan Vulkan::Vulkan)
    else()
        message(WARNING "ImGui: Vulkan SDK no encontrado, se omite imgui::vulkan")
    endif()
endif()
