// ui.h -- nodos de interfaz del editor (todo en un include)
//
// Uso básico en una ventana del editor:
//
//   class my_window : public editor_window
//   {
//       ui_ptr root;
//
//       my_window() : editor_window("mine", "Mi ventana")
//       {
//           root = ui_vbox::create({
//               ui_label::create("Título")->color(ed_style.accent),
//               ui_separator::create(),
//               ui_hbox::create({
//                   ui_button::create("Guardar")->on_click([this] { save(); }),
//                   ui_checkbox::create("Auto")->bind(&auto_save),
//               }),
//           })->spacing(6);
//       }
//
//       void on_draw() override { root->draw(); }
//   };
//
// Ver ui_node.h (base y controles propios), ui_layout.h (paneles) y
// ui_controls.h (controles).

#pragma once

#include "tools/editor/ui/ui_node.h"
#include "tools/editor/ui/ui_layout.h"
#include "tools/editor/ui/ui_controls.h"
#include "tools/editor/ui/ui_property_card.h"
