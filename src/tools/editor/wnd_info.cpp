// wnd_info.cpp -- ventana de información del editor ("Hola mundo")

#include "tools/editor/wnd_info.h"

#include "common/common.h"
#include "sys/sys.h"

/*
==================
info_window
==================
*/
info_window::info_window(const char* renderer) :
    editor_window("info", ENGINE_NAME, WND_DEFAULT | WND_CLOSABLE | WND_MAX_CONTENT),
    renderer(renderer)
{
    set_default_rect(0.02f, 0.03f, 0.28f, 0.30f);
    // mínimo por código; el máximo es lo que ocupa su contenido (WND_MAX_CONTENT)
    set_min_size(200.0f, 120.0f);
}

/*
==================
info_window::on_draw
==================
*/
void info_window::on_draw()
{
    ImGui::Text("Hola mundo!");
    ImGui::Separator();
    ImGui::Text("Dear ImGui %s", IMGUI_VERSION);
    ImGui::Text("Plataforma: %s", Sys_PlatformName());
    ImGui::Text("Render: Vulkan - %s", renderer.c_str());
    ImGui::Text("Arquitectura: %d bits", (int)(sizeof(void*) * 8));
    ImGui::Text("%.1f FPS", ImGui::GetIO().Framerate);
}
