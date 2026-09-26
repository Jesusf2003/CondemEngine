// sys_vid.h -- ventana, eventos y superficie Vulkan del sistema
// Implementada por sys_sdl3.cpp o sys_sdl2.cpp según CONDEM_PLATFORM.

#pragma once

#include <vulkan/vulkan.h>

#include <vector>

bool VID_CreateVulkanWindow(const char* title, int width, int height);
// Crea la ventana principal preparada para Vulkan (escalada por el DPI).

void VID_DestroyWindow(void);

void VID_GetDrawableSize(int* width, int* height);
// Tamaño en píxeles del área de dibujo.

bool VID_IsMinimized(void);

float VID_GetDisplayScale(void);
// Escala del monitor principal (1.0 = 96 dpi).

bool VID_GetVulkanInstanceExtensions(std::vector<const char*>& extensions);
// Añade las extensiones de instancia necesarias para presentar en la ventana.

bool VID_CreateVulkanSurface(VkInstance instance, VkSurfaceKHR* surface);

bool VID_PumpEvents(void);
// Procesa los eventos pendientes y se los pasa a ImGui si está inicializado.
// Devuelve false cuando se pide cerrar la aplicación.

//============================================================================
// Backend de plataforma de ImGui (imgui_impl_sdl3 / imgui_impl_sdl2)

bool VID_ImGui_Init(void);
void VID_ImGui_NewFrame(void);
void VID_ImGui_Shutdown(void);
