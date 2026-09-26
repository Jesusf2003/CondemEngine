// menubar.cpp -- barra de menús del editor

#include "tools/editor/menubar.h"
#include "tools/editor/layout.h"
#include "tools/editor/window.h"

#include "imgui.h"

/*
==================
editor_menubar::draw
==================
*/
void editor_menubar::draw(editor_layout* layout)
{
    if (!ImGui::BeginMainMenuBar())
        return;

    if (ImGui::BeginMenu("View"))
    {
        draw_view_menu(layout);
        ImGui::EndMenu();
    }

    ImGui::EndMainMenuBar();
}

/*
==================
editor_menubar::draw_view_menu

View > Show > una casilla por ventana
==================
*/
void editor_menubar::draw_view_menu(editor_layout* layout)
{
    if (!ImGui::BeginMenu("Show"))
        return;

    for (editor_window* w : editor_window::windex)
    {
        bool shown = w->is_open();
        if (!ImGui::MenuItem(w->get_title(), nullptr, &shown))
            continue;

        if (shown)
        {
            // si no está acoplada, a su región por defecto (la consola, abajo)
            if (layout)
                layout->dock_default(w);
            w->show();
        }
        else
            w->hide();  // su región del layout queda libre
    }

    ImGui::EndMenu();
}
