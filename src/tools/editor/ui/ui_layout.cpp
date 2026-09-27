// ui_layout.cpp -- paneles de disposición

#include "tools/editor/ui/ui_layout.h"

#include <algorithm>

/*
==============================================================================

    ui_vbox / ui_hbox

==============================================================================
*/

/*
==================
ui_vbox::render
==================
*/
void ui_vbox::render()
{
    const float item_spacing = ImGui::GetStyle().ItemSpacing.y;
    const std::vector<ui_ptr> list = children;     // un evento puede cambiar los hijos
    bool first = true;

    for (const ui_ptr& child : list)
    {
        if (!child->is_visible())
            continue;

        // ImGui ya dejó ItemSpacing.y bajo el hijo anterior: se cambia por el propio
        if (!first && box_spacing >= 0)
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() - item_spacing + box_spacing);

        child->draw();
        first = false;
    }
}

/*
==================
ui_hbox::render
==================
*/
void ui_hbox::render()
{
    const std::vector<ui_ptr> list = children;
    bool first = true;

    for (const ui_ptr& child : list)
    {
        if (!child->is_visible())
            continue;

        if (!first)
            ImGui::SameLine(0.0f, box_spacing);
        child->draw();
        first = false;
    }
}

/*
==============================================================================

    ui_border_pane

==============================================================================
*/

static const char* const ui_region_names[UI_REGION_COUNT] =
{
    "##top", "##bottom", "##left", "##right", "##center"
};

ui_border_pane::ptr ui_border_pane::region_size(ui_region region, float px)
{
    sizes[region] = std::max(px, 0.0f);
    return self();
}

ui_border_pane::ptr ui_border_pane::region_flags(ui_region region, ImGuiWindowFlags f)
{
    flags[region] = f;
    return self();
}

/*
==================
ui_border_pane::set_region
==================
*/
void ui_border_pane::set_region(ui_region region, ui_ptr node)
{
    if (slots[region])
        remove_child(slots[region]);    // on_child_removed vacía la región
    if (!node)
        return;

    // un nodo solo puede estar en una región
    for (int r = 0; r < UI_REGION_COUNT; r++)
        if (slots[r] == node)
            slots[r] = nullptr;

    add_child(node);
    slots[region] = std::move(node);
}

void ui_border_pane::on_child_removed(ui_node* child)
{
    for (ui_ptr& slot : slots)
        if (slot.get() == child)
            slot = nullptr;
}

bool ui_border_pane::has_region(ui_region region) const
{
    return slots[region] && slots[region]->is_visible();
}

/*
==================
ui_border_pane::draw_region

fit: la región se ajusta a su contenido en su eje (alto o ancho)
==================
*/
void ui_border_pane::draw_region(ui_region region, ImVec2 size, bool fit)
{
    const bool horizontal = region == UI_LEFT || region == UI_RIGHT;

    ImGuiChildFlags child_flags = region_borders ? ImGuiChildFlags_Borders : ImGuiChildFlags_None;
    ImGuiWindowFlags window_flags = flags[region];
    if (fit)
    {
        child_flags |= horizontal ? ImGuiChildFlags_AutoResizeX : ImGuiChildFlags_AutoResizeY;
        window_flags |= ImGuiWindowFlags_NoScrollbar;
    }
    if (!region_background)
        window_flags |= ImGuiWindowFlags_NoBackground;

    if (ImGui::BeginChild(ui_region_names[region], size, child_flags, window_flags))
        slots[region]->draw();
    ImGui::EndChild();

    rect_min[region] = ImGui::GetItemRectMin();
    rect_max[region] = ImGui::GetItemRectMax();
    const ImVec2 measured_size = ImGui::GetItemRectSize();
    measured[region] = horizontal ? measured_size.x : measured_size.y;
}

/*
==================
ui_border_pane::render
==================
*/
void ui_border_pane::render()
{
    const ImGuiStyle& style = ImGui::GetStyle();

    ImVec2 avail = ImGui::GetContentRegionAvail();
    if (props.width > 0)
        avail.x = props.width;
    if (props.height > 0)
        avail.y = props.height;

    for (int r = 0; r < UI_REGION_COUNT; r++)
        if (!has_region((ui_region)r))
            rect_min[r] = rect_max[r] = ImVec2(0, 0);

    const float start_y = ImGui::GetCursorPosY();

    // arriba
    if (has_region(UI_TOP))
        draw_region(UI_TOP, ImVec2(avail.x, sizes[UI_TOP]), sizes[UI_TOP] <= 0);

    // abajo: se reserva su alto (el del último frame si se ajusta al contenido)
    float bottom_h = 0.0f;
    if (has_region(UI_BOTTOM))
    {
        bottom_h = sizes[UI_BOTTOM] > 0 ? sizes[UI_BOTTOM] : measured[UI_BOTTOM];
        if (bottom_h <= 0)
            bottom_h = ImGui::GetFrameHeightWithSpacing();  // primer frame: una línea
        bottom_h += style.ItemSpacing.y;
    }
    const float used_top = ImGui::GetCursorPosY() - start_y;
    const float middle_h = std::max(avail.y - used_top - bottom_h, 1.0f);

    // franja del medio: left | center | right
    const bool left = has_region(UI_LEFT);
    const bool right = has_region(UI_RIGHT);
    const bool center = has_region(UI_CENTER);

    if (left)
    {
        draw_region(UI_LEFT, ImVec2(sizes[UI_LEFT], middle_h), sizes[UI_LEFT] <= 0);
        if (center || right)
            ImGui::SameLine();
    }

    if (center || right)
    {
        // center se queda con el ancho que no usa right
        float right_w = 0.0f;
        if (right)
            right_w = (sizes[UI_RIGHT] > 0 ? sizes[UI_RIGHT] : measured[UI_RIGHT]) + style.ItemSpacing.x;
        const float center_w = right_w > 0 ? -right_w : 0.0f;

        if (center)
            draw_region(UI_CENTER, ImVec2(center_w, middle_h), false);
        else
            ImGui::Dummy(ImVec2(std::max(ImGui::GetContentRegionAvail().x - right_w, 0.0f), middle_h));

        if (right)
        {
            ImGui::SameLine();
            draw_region(UI_RIGHT, ImVec2(sizes[UI_RIGHT], middle_h), sizes[UI_RIGHT] <= 0);
        }
    }
    else if (!left)
        ImGui::Dummy(ImVec2(0, middle_h));    // bottom sigue abajo aunque no haya nada en medio

    // abajo
    if (has_region(UI_BOTTOM))
        draw_region(UI_BOTTOM, ImVec2(avail.x, sizes[UI_BOTTOM]), sizes[UI_BOTTOM] <= 0);
}

/*
==============================================================================

    ui_grid_pane

==============================================================================
*/

/*
==================
ui_grid_pane::add
==================
*/
ui_grid_pane::ptr ui_grid_pane::add(ui_ptr node, int column, int row)
{
    if (!node || column < 0 || row < 0)
        return self();

    ui_node* raw = node.get();
    if (raw->get_parent() == this)
        on_child_removed(raw);      // se mueve de celda
    else
        add_child(std::move(node));

    // insertar ordenado por fila y columna, detrás de los de la misma celda
    const cell c = { raw, column, row };
    auto it = std::upper_bound(cells.begin(), cells.end(), c, [](const cell& a, const cell& b)
    {
        return a.row != b.row ? a.row < b.row : a.column < b.column;
    });
    cells.insert(it, c);
    return self();
}

ui_grid_pane::ptr ui_grid_pane::column_width(int column, float px)
{
    if (column < 0)
        return self();
    if ((int)widths.size() <= column)
        widths.resize(column + 1, 0.0f);
    widths[column] = std::max(px, 0.0f);
    return self();
}

ui_grid_pane::ptr ui_grid_pane::gap(float horizontal, float vertical)
{
    cell_gap = ImVec2(horizontal, vertical);
    return self();
}

void ui_grid_pane::on_child_removed(ui_node* child)
{
    cells.erase(std::remove_if(cells.begin(), cells.end(),
        [child](const cell& c) { return c.node == child; }), cells.end());
}

/*
==================
ui_grid_pane::render
==================
*/
void ui_grid_pane::render()
{
    if (cells.empty())
        return;

    int columns = std::max(num_columns, (int)widths.size());
    for (const cell& c : cells)
        columns = std::max(columns, c.column + 1);
    columns = std::min(columns, 512);   // límite de las tablas de ImGui

    ImGuiTableFlags table_flags = ImGuiTableFlags_NoSavedSettings;
    table_flags |= grid_stretch ? ImGuiTableFlags_SizingStretchProp
                                : ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoHostExtendX;
    if (grid_borders)
        table_flags |= ImGuiTableFlags_Borders;

    const bool gap = cell_gap.x >= 0 && cell_gap.y >= 0;
    if (gap)
        ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(cell_gap.x * 0.5f, cell_gap.y * 0.5f));

    if (ImGui::BeginTable("##grid", columns, table_flags, ImVec2(props.width, props.height)))
    {
        for (int i = 0; i < columns; i++)
        {
            const float w = i < (int)widths.size() ? widths[i] : 0.0f;
            ImGui::TableSetupColumn(nullptr, w > 0 ? ImGuiTableColumnFlags_WidthFixed : ImGuiTableColumnFlags_None, w);
        }

        // copia: un evento puede cambiar las celdas mientras se dibujan
        const std::vector<cell> list = cells;
        const std::vector<ui_ptr> keep = children;     // que no se destruyan a mitad del dibujado

        int row = -1;
        for (const cell& c : list)
        {
            while (row < c.row)
            {
                ImGui::TableNextRow();
                row++;
            }
            if (c.column < columns)
            {
                ImGui::TableSetColumnIndex(c.column);
                c.node->draw();
            }
        }
        ImGui::EndTable();
    }

    if (gap)
        ImGui::PopStyleVar();
}
