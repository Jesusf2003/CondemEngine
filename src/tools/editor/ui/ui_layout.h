// ui_layout.h -- paneles de disposición
//
//   ui_vbox           hijos de arriba a abajo
//   ui_hbox           hijos de izquierda a derecha
//   ui_border_pane    top, bottom, left, right y center, cada uno en su región
//   ui_grid_pane      hijos en filas y columnas (tabla de ImGui)
//
//   auto pane = ui_border_pane::create()
//       ->top(toolbar)
//       ->left(tree)->region_size(UI_LEFT, 200)
//       ->center(view)
//       ->bottom(status);
//
//   auto grid = ui_grid_pane::create()
//       ->add(ui_label::create("Nombre"), 0, 0)->add(name_input, 1, 0)
//       ->add(ui_label::create("Edad"),   0, 1)->add(age_input,  1, 1)
//       ->gap(8, 4);

#pragma once

#include "tools/editor/ui/ui_node.h"

/*
==============================================================================

    ui_vbox / ui_hbox

==============================================================================
*/

class ui_vbox : public ui_container<ui_vbox>
{
public:
    ptr spacing(float v)    { box_spacing = v; return self(); }
    // espacio entre hijos en px; < 0 = el de ImGui (ItemSpacing)

protected:
    ui_vbox() = default;

    void render() override;

    float box_spacing = -1.0f;
};

class ui_hbox : public ui_container<ui_hbox>
{
public:
    ptr spacing(float v)    { box_spacing = v; return self(); }
    // espacio entre hijos en px; < 0 = el de ImGui (ItemSpacing)

protected:
    ui_hbox() = default;

    void render() override;

    float box_spacing = -1.0f;
};

/*
==============================================================================

    ui_border_pane

    Cada región es una ventana hija de ImGui (BeginChild):
      - top y bottom ocupan todo el ancho; left, center y right, la franja
        del medio con el alto que queda
      - una región sin tamaño se ajusta a su contenido (center se queda con
        lo que sobra)
      - una región vacía no ocupa espacio
      - el panel ocupa todo el espacio disponible, salvo width/height

==============================================================================
*/

enum ui_region : int
{
    UI_TOP,
    UI_BOTTOM,
    UI_LEFT,
    UI_RIGHT,
    UI_CENTER,

    UI_REGION_COUNT
};

class ui_border_pane : public ui_fluent<ui_border_pane, ui_parent>
{
public:
    ptr top(ui_ptr node)        { set_region(UI_TOP, std::move(node)); return self(); }
    ptr bottom(ui_ptr node)     { set_region(UI_BOTTOM, std::move(node)); return self(); }
    ptr left(ui_ptr node)       { set_region(UI_LEFT, std::move(node)); return self(); }
    ptr right(ui_ptr node)      { set_region(UI_RIGHT, std::move(node)); return self(); }
    ptr center(ui_ptr node)     { set_region(UI_CENTER, std::move(node)); return self(); }

    ptr region_size(ui_region region, float px);
    // alto de top/bottom o ancho de left/right; 0 = según el contenido
    ptr region_flags(ui_region region, ImGuiWindowFlags flags);
    // flags de la ventana hija (p. ej. ImGuiWindowFlags_HorizontalScrollbar)
    ptr borders(bool v = true)      { region_borders = v; return self(); }
    ptr background(bool v = true)   { region_background = v; return self(); }
    // fondo de las regiones (ImGuiCol_ChildBg); por defecto, transparente

    const ui_ptr&   get_region(ui_region region) const  { return slots[region]; }
    ImVec2          region_min(ui_region region) const  { return rect_min[region]; }
    ImVec2          region_max(ui_region region) const  { return rect_max[region]; }
    // rectángulo en pantalla de la región en el último frame

protected:
    ui_border_pane() = default;

    void render() override;
    void on_child_removed(ui_node* child) override;

    void set_region(ui_region region, ui_ptr node);
    bool has_region(ui_region region) const;
    void draw_region(ui_region region, ImVec2 size, bool fit);

    ui_ptr              slots[UI_REGION_COUNT];
    float               sizes[UI_REGION_COUNT] = {};
    ImGuiWindowFlags    flags[UI_REGION_COUNT] = {};
    float               measured[UI_REGION_COUNT] = {};     // tamaño del último frame
    ImVec2              rect_min[UI_REGION_COUNT] = {};
    ImVec2              rect_max[UI_REGION_COUNT] = {};
    bool                region_borders = false;
    bool                region_background = false;
};

/*
==============================================================================

    ui_grid_pane

    Hijos en celdas (columna, fila) de una tabla de ImGui. Por defecto cada
    columna mide lo que su contenido más ancho; con stretch() se reparten el
    ancho disponible. Varias veces la misma celda = uno debajo de otro.

==============================================================================
*/

class ui_grid_pane : public ui_fluent<ui_grid_pane, ui_parent>
{
public:
    ptr add(ui_ptr node, int column, int row);
    ptr columns(int n)              { num_columns = n; return self(); }
    // mínimo de columnas (si no, las que usen los hijos)
    ptr column_width(int column, float px);
    // ancho fijo de una columna en px; 0 = según el contenido
    ptr gap(float horizontal, float vertical);
    // espacio entre celdas; por defecto, el de ImGui (CellPadding)
    ptr borders(bool v = true)      { grid_borders = v; return self(); }
    ptr stretch(bool v = true)      { grid_stretch = v; return self(); }

protected:
    ui_grid_pane() = default;

    void render() override;
    void on_child_removed(ui_node* child) override;

    struct cell
    {
        ui_node*    node;
        int         column;
        int         row;
    };

    std::vector<cell>   cells;          // ordenadas por fila y columna
    std::vector<float>  widths;
    int                 num_columns = 0;
    ImVec2              cell_gap = ImVec2(-1, -1);
    bool                grid_borders = false;
    bool                grid_stretch = false;
};
