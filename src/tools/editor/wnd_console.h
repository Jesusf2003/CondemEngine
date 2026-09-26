// wnd_console.h -- ventana de consola del editor
// Basado en WndConsole.h de QuakeEd 3 (docs/quaked3-master/WndConsole.h).

#pragma once

#include "tools/editor/window.h"

class console_window : public editor_window
{
public:
    console_window();

protected:
    void on_draw() override;
    void on_show() override;

private:
    static int completion_callback(ImGuiInputTextCallbackData* data);

    char    input[256];
    bool    scroll_to_end;
    bool    reclaim_focus;
};
