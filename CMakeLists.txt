cmake_minimum_required(VERSION 3.21)

# Las dependencias (SDL2, SDL3, Vulkan) vienen de vcpkg en modo manifiesto
# (vcpkg.json). Los presets de CMakePresets.json configuran el toolchain de
# vcpkg y el triplet de MinGW de 32 o 64 bits.
#
# Si se configura sin presets (p. ej. CMake Tools en modo kits), se usa vcpkg
# de VCPKG_ROOT o C:/vcpkg y el triplet MinGW según el compilador elegido.
# Debe ir antes de project(), que es cuando se carga el toolchain.
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    if(DEFINED ENV{VCPKG_ROOT})
        set(CONDEM_VCPKG_ROOT "$ENV{VCPKG_ROOT}")
    else()
        set(CONDEM_VCPKG_ROOT "C:/vcpkg")
    endif()
    if(EXISTS "${CONDEM_VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
        set(CMAKE_TOOLCHAIN_FILE "${CONDEM_VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE FILEPATH "")
        if(NOT DEFINED VCPKG_TARGET_TRIPLET)
            if(CMAKE_CXX_COMPILER MATCHES "mingw32/|i686")
                set(VCPKG_TARGET_TRIPLET x86-mingw-dynamic CACHE STRING "")
            else()
                set(VCPKG_TARGET_TRIPLET x64-mingw-dynamic CACHE STRING "")
            endif()
            set(VCPKG_HOST_TRIPLET ${VCPKG_TARGET_TRIPLET} CACHE STRING "")
        endif()
        set(VCPKG_APPLOCAL_DEPS OFF CACHE BOOL "")
    endif()
endif()

project(CondemEngine
    VERSION 0.1
    DESCRIPTION "A Game Engine"
    LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Tipo de compilación" FORCE)
endif()

# --- Arquitectura / salida ---------------------------------------------------
# El ejecutable se genera directamente en bin/ (64 bits es el build principal;
# compilar 32 bits sobrescribe el mismo ejecutable).
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(CONDEM_ARCH x64)
else()
    set(CONDEM_ARCH x86)
endif()
message(STATUS "CondemEngine: ${CONDEM_ARCH} (${CMAKE_BUILD_TYPE}) con ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
if(VCPKG_TARGET_TRIPLET)
    message(STATUS "CondemEngine: vcpkg triplet ${VCPKG_TARGET_TRIPLET}")
endif()

set(CONDEM_BIN_DIR "${CMAKE_SOURCE_DIR}/bin")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CONDEM_BIN_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CONDEM_BIN_DIR})
foreach(cfg Debug Release RelWithDebInfo MinSizeRel)
    string(TOUPPER ${cfg} CFG)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CFG} ${CONDEM_BIN_DIR})
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${CFG} ${CONDEM_BIN_DIR})
endforeach()

# --- Dependencias (vcpkg) ----------------------------------------------------
find_package(SDL2 CONFIG REQUIRED)
find_package(SDL3 CONFIG REQUIRED)
find_package(VulkanHeaders CONFIG REQUIRED)
find_package(VulkanLoader CONFIG REQUIRED)

# Target único de Vulkan que usa ImGui (y que FindVulkan crearía con el SDK).
if(NOT TARGET Vulkan::Vulkan)
    add_library(Vulkan::Vulkan INTERFACE IMPORTED)
    target_link_libraries(Vulkan::Vulkan INTERFACE Vulkan::Loader Vulkan::Headers)
endif()

# --- Subsistemas -------------------------------------------------------------
add_subdirectory(src/utilities/imgui)   # imgui::imgui, imgui::sdl2/sdl3, imgui::vulkan...
add_subdirectory(src/sys)               # condem::sys (SDL2 o SDL3 según CONDEM_PLATFORM)

# --- Ejecutable --------------------------------------------------------------
set(ENGINE_SRC
    src/main.cpp

    src/common/common.cpp
    src/common/common.h

    src/core/cmd.cpp
    src/core/cmd.h
    src/core/cvar.cpp
    src/core/cvar.h

    src/engine/console.cpp
    src/engine/console.h
)

set(EDITOR_SRC
    src/tools/editor/editor_main.cpp
    src/tools/editor/editor.h
    src/tools/editor/layout.cpp
    src/tools/editor/layout.h
    src/tools/editor/menubar.cpp
    src/tools/editor/menubar.h
    src/tools/editor/style.cpp
    src/tools/editor/style.h
    src/tools/editor/window.cpp
    src/tools/editor/window.h
    src/tools/editor/wnd_console.cpp
    src/tools/editor/wnd_console.h
    src/tools/editor/wnd_info.cpp
    src/tools/editor/wnd_info.h
)

add_executable(${PROJECT_NAME} ${ENGINE_SRC} ${EDITOR_SRC})
target_include_directories(${PROJECT_NAME} PRIVATE src)
target_link_libraries(${PROJECT_NAME} PRIVATE
    condem::sys
    imgui::vulkan
    Vulkan::Vulkan
)

if(MINGW)
    # Evita depender de libgcc/libstdc++/winpthread de MSYS2 al ejecutar fuera de su shell.
    target_link_options(${PROJECT_NAME} PRIVATE -static-libgcc -static-libstdc++
        -Wl,-Bstatic,--whole-archive -lwinpthread -Wl,--no-whole-archive,-Bdynamic)
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra)
endif()

# Copia las DLL de vcpkg (SDL) junto al ejecutable.
# vulkan-1.dll se excluye a propósito: se usa el loader del sistema que instala
# el driver de la GPU, como recomienda Khronos.
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<FILTER:$<TARGET_RUNTIME_DLLS:${PROJECT_NAME}>,EXCLUDE,vulkan-1\\.dll$>"
        $<TARGET_FILE_DIR:${PROJECT_NAME}>
    COMMAND_EXPAND_LISTS)
