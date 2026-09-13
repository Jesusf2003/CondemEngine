cmake_minimum_required(VERSION 3.10)

project("CondemEngine"
    VERSION 0.1
    DESCRIPTION "A Game Engine")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/../bin")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/../bin")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/../bin")

include(FetchContent)

# --- SDL3 ---
FetchContent_Declare(
    SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG        release-3.4.10
)
FetchContent_MakeAvailable(SDL3)

# --- ImGui (rama docking) ---
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.92.8-docking
)
FetchContent_MakeAvailable(imgui)

add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp

)

target_include_directories(imgui PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)

target_compile_definitions(imgui PUBLIC
    IMGUI_ENABLE_DOCKING
)

target_compile_options(imgui PRIVATE
    -Wno-stringop-overflow
    -Wno-stringop-overread
)

target_link_libraries(imgui PUBLIC SDL3::SDL3)

# --- Vulkan ---
find_package(Vulkan REQUIRED)
target_link_libraries(imgui PUBLIC Vulkan::Vulkan)

find_package(OpenGL REQUIRED)
target_link_libraries(imgui PUBLIC OpenGL::GL)

# --- Ejecutable ---
set(PROJECT_SRC
    src/condemc.cpp
    src/pr_lex.cpp
    src/pr_comp.cpp
    src/cmdlib.cpp
    #src/builtin.cpp
    #src/options.cpp
    #src/window.cpp
    #src/wndMain.cpp
    #src/editor_main.cpp
    #src/vm.cpp
    #src/engine_main.cpp}
)

set(ENGINE_SRC
    src/main.cpp
    src/common/cmdlib.cpp
    src/common/condemfs.cpp
    src/client/sys.cpp
    src/client/host.cpp
    # src/client/cvar.cpp
    # src/client/cmd.cpp
    # src/client/cbuf.cpp
)

#add_executable(${PROJECT_NAME} ${PROJECT_SRC})
add_executable(${PROJECT_NAME} ${ENGINE_SRC})

#target_include_directories(${PROJECT_NAME} PUBLIC src src/cdclib)

target_link_libraries(${PROJECT_NAME} PRIVATE
    imgui
    SDL3::SDL3
    dwmapi
    Vulkan::Vulkan
)