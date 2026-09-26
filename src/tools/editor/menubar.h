// menubar.h -- barra de menús del editor
//
// View
//   Show
//     [x] <ventana>    una casilla por ventana: marcada = visible
//
// Al marcar una ventana se muestra; si no está acoplada, vuelve a su región
// por defecto (editor_window::set_default_dock). Al desmarcarla se oculta y
// su región queda libre. Cerrar la ventana con su botón la desmarca.

#pragma once

class editor_layout;

class editor_menubar
{
public:
    void    draw(editor_layout* layout);
    // Barra de menús principal. Llamar antes de editor_window::draw_all():
    // ocupa la parte de arriba y reduce el área de trabajo del layout.

private:
    void    draw_view_menu(editor_layout* layout);
};
