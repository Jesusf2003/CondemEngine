// sys_sdl2.cpp -- implementación de sys.h y sys_vid.h con SDL2

#include "sys/sys.h"
#include "sys/sys_vid.h"

// main() es nuestro: no usar SDL2main.
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <SDL_vulkan.h>

#include "imgui.h"
#include "imgui_impl_sdl2.h"

#include <cstdarg>
#include <cstdio>
#include <cstdlib>

static Uint64       sys_timebase;
static SDL_Window*  vid_window;
static bool         vid_imgui;

/*
===============================================================================

                            SISTEMA

===============================================================================
*/

/*
================
Sys_Init
================
*/
void Sys_Init(void)
{
    SDL_SetMainReady();
    if (SDL_Init(0) != 0)
        Sys_Error("SDL_Init: %s", SDL_GetError());

    sys_timebase = SDL_GetPerformanceCounter();
}

/*
================
Sys_Shutdown
================
*/
void Sys_Shutdown(void)
{
    VID_DestroyWindow();
    SDL_Quit();
}

/*
================
Sys_Error
================
*/
void Sys_Error(const char* error, ...)
{
    char    text[1024];
    va_list argptr;

    va_start(argptr, error);
    vsnprintf(text, sizeof(text), error, argptr);
    va_end(argptr);

    fprintf(stderr, "Error: %s\n", text);
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "CondemEngine - Error", text, vid_window);

    Sys_Shutdown();
    exit(1);
}

/*
================
Sys_Printf
================
*/
void Sys_Printf(const char* fmt, ...)
{
    va_list argptr;

    va_start(argptr, fmt);
    vprintf(fmt, argptr);
    va_end(argptr);
    fflush(stdout);
}

/*
================
Sys_FloatTime
================
*/
double Sys_FloatTime(void)
{
    return (double)(SDL_GetPerformanceCounter() - sys_timebase) / (double)SDL_GetPerformanceFrequency();
}

/*
================
Sys_Sleep
================
*/
void Sys_Sleep(int msec)
{
    SDL_Delay((Uint32)msec);
}

/*
================
Sys_PlatformName
================
*/
const char* Sys_PlatformName(void)
{
    return "SDL2";
}

/*
===============================================================================

                            VÍDEO

===============================================================================
*/

/*
================
VID_GetDisplayScale
================
*/
float VID_GetDisplayScale(void)
{
    float ddpi = 0.0f;
    if (SDL_GetDisplayDPI(0, &ddpi, nullptr, nullptr) == 0 && ddpi > 0.0f)
        return ddpi / 96.0f;
    return 1.0f;
}

/*
================
VID_CreateVulkanWindow
================
*/
bool VID_CreateVulkanWindow(const char* title, int width, int height)
{
    // Pedir a Windows coordenadas en píxeles reales para que el escalado lo haga ImGui.
    SDL_SetHint(SDL_HINT_WINDOWS_DPI_AWARENESS, "permonitorv2");

    if (SDL_InitSubSystem(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0)
    {
        Sys_Printf("SDL_InitSubSystem: %s\n", SDL_GetError());
        return false;
    }

    float scale = VID_GetDisplayScale();
    vid_window = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        (int)(width * scale), (int)(height * scale),
        SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!vid_window)
    {
        Sys_Printf("SDL_CreateWindow: %s\n", SDL_GetError());
        return false;
    }
    return true;
}

/*
================
VID_DestroyWindow
================
*/
void VID_DestroyWindow(void)
{
    if (!vid_window)
        return;

    SDL_DestroyWindow(vid_window);
    vid_window = nullptr;
    SDL_QuitSubSystem(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER);
}

/*
================
VID_GetDrawableSize
================
*/
void VID_GetDrawableSize(int* width, int* height)
{
    SDL_Vulkan_GetDrawableSize(vid_window, width, height);
}

/*
================
VID_IsMinimized
================
*/
bool VID_IsMinimized(void)
{
    return (SDL_GetWindowFlags(vid_window) & SDL_WINDOW_MINIMIZED) != 0;
}

/*
================
VID_GetVulkanInstanceExtensions
================
*/
bool VID_GetVulkanInstanceExtensions(std::vector<const char*>& extensions)
{
    unsigned int count = 0;
    if (!SDL_Vulkan_GetInstanceExtensions(vid_window, &count, nullptr))
    {
        Sys_Printf("SDL_Vulkan_GetInstanceExtensions: %s\n", SDL_GetError());
        return false;
    }

    size_t first = extensions.size();
    extensions.resize(first + count);
    if (!SDL_Vulkan_GetInstanceExtensions(vid_window, &count, extensions.data() + first))
    {
        Sys_Printf("SDL_Vulkan_GetInstanceExtensions: %s\n", SDL_GetError());
        extensions.resize(first);
        return false;
    }
    return true;
}

/*
================
VID_CreateVulkanSurface
================
*/
bool VID_CreateVulkanSurface(VkInstance instance, VkSurfaceKHR* surface)
{
    if (!SDL_Vulkan_CreateSurface(vid_window, instance, surface))
    {
        Sys_Printf("SDL_Vulkan_CreateSurface: %s\n", SDL_GetError());
        return false;
    }
    return true;
}

/*
================
VID_PumpEvents
================
*/
bool VID_PumpEvents(void)
{
    bool        running = true;
    SDL_Event   event;

    while (SDL_PollEvent(&event))
    {
        if (vid_imgui)
            ImGui_ImplSDL2_ProcessEvent(&event);

        if (event.type == SDL_QUIT)
            running = false;
        if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE
            && event.window.windowID == SDL_GetWindowID(vid_window))
            running = false;
    }
    return running;
}

/*
===============================================================================

                            IMGUI

===============================================================================
*/

bool VID_ImGui_Init(void)
{
    vid_imgui = ImGui_ImplSDL2_InitForVulkan(vid_window);
    return vid_imgui;
}

void VID_ImGui_NewFrame(void)
{
    ImGui_ImplSDL2_NewFrame();
}

void VID_ImGui_Shutdown(void)
{
    if (!vid_imgui)
        return;

    ImGui_ImplSDL2_Shutdown();
    vid_imgui = false;
}
