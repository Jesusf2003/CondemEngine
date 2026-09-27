// wnd_console.h -- ventana de consola del editor
// Basado en WndConsole.h de QuakeEd 3 (docs/quaked3-master/WndConsole.h) y en
// IMGUIQuakeConsole.h de Virtuoso Console (docs/VirtuosoConsole): historial,
// autocompletado con lista de candidatos, filtro, copiar y colores ANSI.
// El log tiene el aspecto de la consola de Quake (docs/WinQuake/console.cpp).
//
// Disposición (ui_border_pane):
//   top      barra de herramientas: Limpiar, Copiar, Opciones, filtro
//   center   log
//   bottom   línea de entrada

#pragma once

#include "tools/editor/window.h"

#include <memory>
#include <string>
#include <vector>

class ui_border_pane;

class console_window : public editor_window
{
public:
    console_window();

protected:
    void on_draw() override;
    void on_show() override;

private:
    static int input_callback(ImGuiInputTextCallbackData* data);
    void complete(ImGuiInputTextCallbackData* data);        // TAB
    void browse_history(ImGuiInputTextCallbackData* data);  // flechas arriba/abajo
    void submit();

    void draw_log();
    void draw_input();
    void draw_background();

    std::shared_ptr<ui_border_pane> root;

    char                        input[256];
    std::vector<std::string>    history;        // comandos escritos, el último al final
    int                         history_pos;    // -1 = línea nueva
    ImGuiTextFilter             filter;

    size_t  last_line_count;
    bool    auto_scroll;
    bool    at_bottom;      // el log estaba al final en el último frame
    bool    scroll_to_end;
    bool    reclaim_focus;
    bool    copy_request;   // copiar el log al portapapeles en este frame
};
