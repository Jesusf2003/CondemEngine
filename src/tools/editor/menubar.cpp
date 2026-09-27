// menubar.cpp -- barra de menús del editor

#include "tools/editor/menubar.h"
#include "tools/editor/window.h"
#include "tools/editor/ui/ui.h"

/*
==================
editor_menubar
==================
*/
editor_menubar::editor_menubar() :
    show_count(0)
{
    show_menu = ui_menu::create("Show");
    bar = ui_menu_bar::create({
        ui_menu::create("View")->add(show_menu),
    });
}

/*
==================
editor_menubar::rebuild_show_menu

View > Show > una casilla por ventana
==================
*/
void editor_menubar::rebuild_show_menu()
{
    show_menu->clear_children();

    for (editor_window* w : editor_window::windex)
    {
        show_menu->add(ui_menu_item::create(w->get_title())->checked(
            [w] { return w->is_open(); },
            [w](bool shown)
            {
                if (shown)
                    w->show();  // acoplada: vuelve a su hoja
                else
                    w->hide();  // su hoja cede el espacio
            }));
    }
    show_count = editor_window::windex.size();
}

/*
==================
editor_menubar::draw
==================
*/
void editor_menubar::draw()
{
    if (show_count != editor_window::windex.size())
        rebuild_show_menu();
    bar->draw();
}
