// wnd_info.cpp -- ventana de información del editor ("Hola mundo")

#include "tools/editor/wnd_info.h"

#include "tools/editor/style.h"
#include "tools/editor/ui/ui.h"

#include "common/common.h"
#include "sys/sys.h"

#include <cstdio>

/*
==================
info_window
==================
*/
info_window::info_window(const char* renderer) :
    editor_window("info", ENGINE_NAME, WND_DEFAULT | WND_CLOSABLE | WND_MAX_CONTENT | WND_MIN_CONTENT)
{
    set_default_rect(0.02f, 0.03f, 0.28f, 0.30f);
    // mínimo por código; el máximo es lo que ocupa su contenido (WND_MAX_CONTENT)
    set_min_size(200.0f, 120.0f);

    root = ui_vbox::create({
        ui_label::create("Hola mundo!"),
        ui_separator::create(),
        ui_property_card::create("Sistema")
            ->property("Dear ImGui", IMGUI_VERSION)
            ->property("Plataforma", Sys_PlatformName())
            ->property("Render", std::string("Vulkan - ") + renderer)
            ->property("Arquitectura", std::to_string(sizeof(void*) * 8) + " bits")
            ->property("FPS", []
            {
                char fps[32];
                snprintf(fps, sizeof(fps), "%.1f", ImGui::GetIO().Framerate);
                return std::string(fps);
            }),
    });
}

/*
==================
info_window::on_draw
==================
*/
void info_window::on_draw()
{
    root->draw();
}
