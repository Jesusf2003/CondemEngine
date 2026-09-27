// ui_property_card.h -- tarjeta de propiedades (nombre: valor)
//
// Ejemplo de control propio: hereda de ui_fluent, añade sus setters
// encadenables y en render() combina otros nodos (un ui_grid_pane de
// ui_label) con dibujado directo (el fondo de la tarjeta).
//
//   ui_property_card::create("Sistema")
//       ->property("Plataforma", Sys_PlatformName())
//       ->property("FPS", [] { return std::to_string(fps); });

#pragma once

#include "tools/editor/ui/ui_controls.h"
#include "tools/editor/ui/ui_layout.h"

class ui_property_card : public ui_fluent<ui_property_card>
{
public:
    ptr title(const std::string& v);
    ptr property(const std::string& name, const std::string& value);
    ptr property(const std::string& name, ui_label::text_fn value);
    // valor calculado cada frame
    ptr padding(float px)       { card_padding = px; return self(); }

protected:
    explicit ui_property_card(std::string title = "");

    void render() override;
    bool is_composite() const override  { return true; }

    ptr add_row(const std::string& name, ui_label::ptr value);

    ui_label::ptr       title_label;
    ui_grid_pane::ptr   grid;
    int                 rows = 0;
    float               card_padding = 8.0f;
    ImVec2              last_size = ImVec2(0, 0);   // fondo: tamaño del frame anterior
};
