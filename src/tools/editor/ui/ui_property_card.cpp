// ui_property_card.cpp -- tarjeta de propiedades (nombre: valor)

#include "tools/editor/ui/ui_property_card.h"

#include "tools/editor/style.h"

#include "imgui_internal.h"     // GetCurrentWindow: ancho ideal del contenido

#include <algorithm>

/*
==================
ui_property_card
==================
*/
ui_property_card::ui_property_card(std::string title) :
    title_label(ui_label::create(std::move(title))->ellipsis()),
    grid(ui_grid_pane::create()->gap(16, 2))
{
    // por defecto llena el ancho disponible; width(0) = el de su contenido
    props.width = -FLT_MIN;
    title_label->visible(!title_label->get_text().empty());
}

ui_property_card::ptr ui_property_card::title(const std::string& v)
{
    title_label->text(v)->visible(!v.empty());
    return self();
}

ui_property_card::ptr ui_property_card::property(const std::string& name, const std::string& value)
{
    return add_row(name, ui_label::create(value)->ellipsis());
}

ui_property_card::ptr ui_property_card::property(const std::string& name, ui_label::text_fn value)
{
    return add_row(name, ui_label::create(std::move(value))->ellipsis());
}

ui_property_card::ptr ui_property_card::add_row(const std::string& name, ui_label::ptr value)
{
    grid->add(ui_label::create(name)->color(ed_style.text_disabled), 0, rows);
    grid->add(value, 1, rows);
    rows++;
    return self();
}

/*
==================
ui_property_card::render

El fondo se dibuja antes que el contenido con el tamaño del frame anterior,
así queda debajo sin partir la lista de dibujado en canales.
==================
*/
void ui_property_card::render()
{
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    ImDrawList* dl = window->DrawList;
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const float rounding = ImGui::GetStyle().FrameRounding;
    const float pad = card_padding;

    // ancho de la tarjeta: fijo, llenando el disponible o (0) el de su contenido
    const bool fixed = props.width != 0;
    float card_w = last_size.x;
    if (props.width > 0)
        card_w = props.width;
    else if (props.width < 0)
        card_w = std::max(ImGui::GetContentRegionAvail().x + props.width, 1.0f);   // -FLT_MIN = todo

    if (card_w > 0 && last_size.y > 0)
    {
        const ImVec2 end(origin.x + card_w, origin.y + last_size.y);
        dl->AddRectFilled(origin, end, ImGui::GetColorU32(ed_style.frame_bg), rounding);
        dl->AddRect(origin, end, ImGui::GetColorU32(ed_style.border), rounding);
    }

    // contenido con margen interior
    const float pad_y = std::max(pad - ImGui::GetStyle().ItemSpacing.y, 0.0f);
    ImGui::BeginGroup();
    ImGui::Dummy(ImVec2(0, pad_y));
    if (pad > 0)
        ImGui::Indent(pad);     // Indent(0) usaría el sangrado del estilo

    // con ancho fijo o llenando, la columna de valores se queda con lo que
    // sobra y los textos que no caben se recortan con "..."
    const float inner_w = std::max(card_w - pad * 2.0f, 1.0f);
    float grid_w = 0.0f;
    if (props.width > 0)
        grid_w = inner_w;
    else if (props.width < 0)
    {
        // negativo (hasta el borde menos el margen): así la tabla informa de su
        // ancho ideal y la ventana no crece con la tarjeta
        grid_w = inner_w - ImGui::GetContentRegionAvail().x;
        if (grid_w >= 0.0f)
            grid_w = -FLT_MIN;
    }
    grid->width(grid_w)->stretch_column(fixed ? 1 : -1);
    title_label->width(fixed ? inner_w : 0.0f);

    if (title_label->is_visible())
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ed_style.accent);
        title_label->draw();
        ImGui::PopStyleColor();
    }
    grid->draw();

    // margen derecho: se suma al ancho del contenido (el ideal, no el
    // disponible) para que la ventana pueda ajustarse a él
    const float right = window->DC.CursorMaxPos.x + pad;
    window->DC.CursorMaxPos.x = right;
    window->DC.IdealMaxPos.x = std::max(window->DC.IdealMaxPos.x, right);

    if (pad > 0)
        ImGui::Unindent(pad);
    ImGui::Dummy(ImVec2(0, pad_y));
    ImGui::EndGroup();

    last_size = ImGui::GetItemRectSize();
}
