// sys_sdl3.cpp -- implementación de sys.h y sys_vid.h con SDL3

#include "sys/sys.h"
#include "sys/sys_vid.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#include "imgui.h"
#include "imgui_impl_sdl3.h"

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
    if (!SDL_Init(0))
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
    return "SDL3";
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
    float scale = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());
    return scale > 0.0f ? scale : 1.0f;
}

/*
================
VID_CreateVulkanWindow
================
*/
bool VID_CreateVulkanWindow(const char* title, int width, int height)
{
    if (!SDL_InitSubSystem(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD))
    {
        Sys_Printf("SDL_InitSubSystem: %s\n", SDL_GetError());
        return false;
    }

    float scale = VID_GetDisplayScale();
    vid_window = SDL_CreateWindow(title, (int)(width * scale), (int)(height * scale),
        SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (!vid_window)
    {
        Sys_Printf("SDL_CreateWindow: %s\n", SDL_GetError());
        return false;
    }

    SDL_SetWindowPosition(vid_window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    SDL_ShowWindow(vid_window);
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
    SDL_QuitSubSystem(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD);
}

/*
================
VID_GetDrawableSize
================
*/
void VID_GetDrawableSize(int* width, int* height)
{
    SDL_GetWindowSizeInPixels(vid_window, width, height);
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
    Uint32 count = 0;
    const char* const* names = SDL_Vulkan_GetInstanceExtensions(&count);
    if (!names)
    {
        Sys_Printf("SDL_Vulkan_GetInstanceExtensions: %s\n", SDL_GetError());
        return false;
    }

    for (Uint32 i = 0; i < count; i++)
        extensions.push_back(names[i]);
    return true;
}

/*
================
VID_CreateVulkanSurface
================
*/
bool VID_CreateVulkanSurface(VkInstance instance, VkSurfaceKHR* surface)
{
    if (!SDL_Vulkan_CreateSurface(vid_window, instance, nullptr, surface))
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
            ImGui_ImplSDL3_ProcessEvent(&event);

        if (event.type == SDL_EVENT_QUIT)
            running = false;
        if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID == SDL_GetWindowID(vid_window))
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
    vid_imgui = ImGui_ImplSDL3_InitForVulkan(vid_window);
    return vid_imgui;
}

void VID_ImGui_NewFrame(void)
{
    ImGui_ImplSDL3_NewFrame();
}

void VID_ImGui_Shutdown(void)
{
    if (!vid_imgui)
        return;

    ImGui_ImplSDL3_Shutdown();
    vid_imgui = false;
}
