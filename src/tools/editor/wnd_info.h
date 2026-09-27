// wnd_info.h -- ventana de información del editor ("Hola mundo")

#pragma once

#include "tools/editor/window.h"

#include <memory>

class ui_node;

class info_window : public editor_window
{
public:
    explicit info_window(const char* renderer);

protected:
    void on_draw() override;

private:
    std::shared_ptr<ui_node> root;
};
