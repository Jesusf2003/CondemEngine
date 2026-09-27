// menubar.h -- barra de menús del editor
//
// View
//   Show
//     [x] <ventana>    una casilla por ventana: marcada = visible
//
// Al marcar una ventana se muestra: si estaba acoplada, vuelve a su hoja del
// dock. Al desmarcarla se oculta y su hoja cede el espacio a sus vecinas.
// Cerrar la ventana con su botón la desmarca.

#pragma once

#include <cstddef>
#include <memory>

class ui_menu;
class ui_menu_bar;

class editor_menubar
{
public:
    editor_menubar();

    void    draw();
    // Barra de menús principal. Llamar antes de editor_window::draw_all():
    // ocupa la parte de arriba y reduce el área de trabajo del dock.

private:
    void    rebuild_show_menu();

    std::shared_ptr<ui_menu_bar>    bar;
    std::shared_ptr<ui_menu>        show_menu;
    size_t                          show_count;     // ventanas en show_menu
};
